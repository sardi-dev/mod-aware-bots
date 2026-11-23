/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See BotVoice.h for what this is and its credit.
 */

#include "BotVoice.h"

#include "AiChat.h"
#include "AiChatPolicy.h"
#include "AwareAiConfig.h"
#include "BroadcastHelper.h"
#include "Chat.h"
#include "Config.h"
#include "Group.h"
#include "PlayerbotAI.h"
#include "PlayerbotTextMgr.h"
#include "Playerbots.h"
#include "Random.h"
#include "Timer.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace
{
    struct VoiceSettings
    {
        bool enabled;
        uint32 botCooldownMs;
        uint32 groupCooldownMs;
        uint32 momentChance[5];
    };

    VoiceSettings const& Settings()
    {
        static VoiceSettings const settings = []
        {
            VoiceSettings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.Voice.Enable", true);
            s.botCooldownMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.Voice.CooldownSeconds", 20)) * IN_MILLISECONDS;
            s.groupCooldownMs =
                uint32(sConfigMgr->GetIntDefault("AiPlayerbot.Voice.GroupCooldownSeconds", 6)) * IN_MILLISECONDS;
            s.momentChance[uint32(BotVoice::Moment::JOIN_GROUP)] = sConfigMgr->GetIntDefault("AiPlayerbot.Voice.JoinChance", 80);
            s.momentChance[uint32(BotVoice::Moment::BEFORE_PULL)] = sConfigMgr->GetIntDefault("AiPlayerbot.Voice.PullChance", 12);
            s.momentChance[uint32(BotVoice::Moment::BOSS_KILLED)] = sConfigMgr->GetIntDefault("AiPlayerbot.Voice.BossChance", 60);
            s.momentChance[uint32(BotVoice::Moment::WIPE)] = sConfigMgr->GetIntDefault("AiPlayerbot.Voice.WipeChance", 50);
            s.momentChance[uint32(BotVoice::Moment::LEAVE_GROUP)] = sConfigMgr->GetIntDefault("AiPlayerbot.Voice.LeaveChance", 50);
            return s;
        }();
        return settings;
    }

    // How much each personality talks (quiet, friendly, joker, grumpy). The shares are AiChatPolicy::PersonalityIndex.
    uint32 const CHATTINESS_PERCENT[4] = { 40, 100, 120, 80 };
    uint32 const MAX_PERCENT = 100;
    // With blended pools: how often the personality's own variant is used when it exists.
    uint32 const PERSONAL_POOL_PERCENT = 60;

    // A wipe note older than this is stale: the group has long moved on.
    uint32 const WIPE_NOTE_MS = 10 * 60 * IN_MILLISECONDS;
    // The bookkeeping maps are trimmed past this size.
    size_t const TRIM_SIZE = 512;

    struct MomentLine
    {
        char const* key;
        char const* fallback;
        BotVoice::Channel channel;
    };

    MomentLine const& LineOf(BotVoice::Moment moment)
    {
        static MomentLine const lines[5] = {
            { "voice_join", "Hi! What are we doing today?", BotVoice::Channel::PARTY },
            { "voice_pull", "Pulling, get ready", BotVoice::Channel::PARTY },
            { "voice_boss_down", "That went well", BotVoice::Channel::PARTY },
            { "voice_wipe", "Ouch. Let's try again", BotVoice::Channel::PARTY },
            { "voice_leave", "Gotta go, see you around", BotVoice::Channel::PARTY }
        };
        return lines[uint32(moment)];
    }

    // The moments whose words the AI layer may write (AiChat::OnPartyMoment), described for the model. The pull call
    // must come at once and the goodbye comes as the bot leaves, so both keep their rule lines.
    char const* AiMomentOf(BotVoice::Moment moment)
    {
        switch (moment)
        {
            case BotVoice::Moment::JOIN_GROUP: return "You just joined this group. Greet them.";
            case BotVoice::Moment::BOSS_KILLED: return "Your group just killed a boss.";
            case BotVoice::Moment::WIPE: return "Your whole group just died and is back on its feet.";
            default: return nullptr;
        }
    }

    std::mutex stateLock;
    std::unordered_map<uint32, uint32> botLastSay;
    std::unordered_map<uint32, uint32> groupLastSay;
    std::unordered_map<uint32, uint32> wipeNotes;
    std::atomic<uint32> pendingWipes{ 0 };

    void Trim(std::unordered_map<uint32, uint32>& stamps, uint32 maxAgeMs)
    {
        if (stamps.size() < TRIM_SIZE)
            return;

        for (auto itr = stamps.begin(); itr != stamps.end();)
        {
            if (getMSTimeDiff(itr->second, getMSTime()) > maxAgeMs)
                itr = stamps.erase(itr);
            else
                ++itr;
        }
    }

    // Only a random bot in a group with a player talks unprompted: nobody else would read it.
    bool MayChatter(PlayerbotAI* botAI, Player* bot)
    {
        if (!botAI->HasRealPlayerMaster() || !bot->GetGroup() || bot->InBattleground())
            return false;

        return sRandomPlayerbotMgr->IsRandomBot(bot);
    }

    // Looks up "<key>_<personality>", then "<key>", then the default.
    std::string TextFor(Player* bot, std::string const& key, std::string const& fallback,
                        std::map<std::string, std::string> const& placeholders, bool personal, bool blend)
    {
        PlayerbotTextMgr& texts = PlayerbotTextMgr::instance();
        if (personal && (!blend || urand(1, MAX_PERCENT) <= PERSONAL_POOL_PERCENT))
        {
            std::string const personalKey = key + "_" + BotVoice::PersonalityName(BotVoice::PersonalityOf(bot));
            if (texts.HasBotText(personalKey))
            {
                std::string const text = texts.GetBotText(personalKey, placeholders);
                if (!text.empty())
                    return text;
            }
        }

        if (texts.HasBotText(key))
        {
            std::string const text = texts.GetBotText(key, placeholders);
            if (!text.empty())
                return text;
        }

        std::string text = fallback;
        for (auto const& placeholder : placeholders)
            PlayerbotTextMgr::replaceAll(text, placeholder.first, placeholder.second);
        return text;
    }

    // The last lines each group heard (BT-116): a line is drawn again when the group heard it a moment ago, so a
    // small pool does not repeat itself back to back. Its own lock: groups talk on different map threads.
    size_t const RECENT_LINES = 10;
    size_t const RECENT_GROUPS_MAX = 256;
    uint32 const FRESH_DRAWS = 4;
    std::mutex recentLock;
    std::unordered_map<uint32, std::deque<size_t>> recentLines;

    std::string FreshTextFor(Player* bot, std::string const& key, std::string const& fallback,
                             std::map<std::string, std::string> const& placeholders, bool personal, bool blend)
    {
        Group* group = bot->GetGroup();
        std::string text;
        for (uint32 draw = 0; draw < FRESH_DRAWS; ++draw)
        {
            text = TextFor(bot, key, fallback, placeholders, personal, blend);
            if (!group)
                return text;

            size_t const hash = std::hash<std::string>()(text);
            std::lock_guard<std::mutex> guard(recentLock);
            if (recentLines.size() > RECENT_GROUPS_MAX)
                recentLines.clear();

            std::deque<size_t>& recent = recentLines[group->GetLowGUID()];
            if (draw + 1 < FRESH_DRAWS && std::find(recent.begin(), recent.end(), hash) != recent.end())
                continue;

            recent.push_back(hash);
            if (recent.size() > RECENT_LINES)
                recent.pop_front();
            return text;
        }
        return text;
    }

    // How a line that passed every check goes out: by default straight to its channel.
    using Sender = std::function<bool(PlayerbotAI*, Player*, std::string const&)>;

    bool Send(PlayerbotAI* botAI, Player* bot, std::string const& text, BotVoice::Channel channel, Player* whisperTo)
    {
        if (text.empty())
            return false;

        switch (channel)
        {
            case BotVoice::Channel::PARTY:
                return botAI->SayToParty(text);
            case BotVoice::Channel::WHISPER:
                return whisperTo && botAI->Whisper(text, whisperTo->GetName());
            case BotVoice::Channel::SAY:
                return botAI->Say(text);
            case BotVoice::Channel::LFG:
                return BroadcastHelper::SayToChannel(bot, text, BroadcastHelper::TO_LOOKING_FOR_GROUP);
            case BotVoice::Channel::GENERAL:
                return BroadcastHelper::SayToChannel(bot, text, BroadcastHelper::TO_GENERAL);
            case BotVoice::Channel::GUILD:
                return BroadcastHelper::SayToChannel(bot, text, BroadcastHelper::TO_GUILD);
            case BotVoice::Channel::BATTLEGROUND:
                return BotVoice::SayToBattleground(bot, text);
        }

        return false;
    }
}

namespace BotVoice
{
    bool Enabled() { return Settings().enabled; }

    // BT-153: the battleground group has its own chat type in 5.4.8, instance chat (ChatMsg::CHAT_MSG_INSTANCE, 0x42; the
    // core routes a human's instance chat in a battleground to the BG group the same way, Handlers/ChatHandler.cpp
    // "CHAT_MSG_INSTANCE"). Raid chat is not used: the core keeps /raid out of battleground groups. To check live that the
    // client shows the line in the battleground tab.
    bool SayToBattleground(Player* bot, std::string const& text)
    {
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || !group->isBGGroup())
            return false;

        ChatMsg const type = group->IsLeader(bot->GetGUID()) ? ChatMsg::CHAT_MSG_INSTANCE_LEADER : ChatMsg::CHAT_MSG_INSTANCE;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, type, Language::LANG_UNIVERSAL, bot, nullptr, text);
        group->BroadcastPacket(&data, false);
        return true;
    }

    Personality PersonalityOf(Player* bot)
    {
        // With AI chat on, random bots get the chat spec's mix (design decision 0031); a human's alt bots, and every bot while
        // chat is off, keep the split they always had. AwareAi.Chat.Enable is read once at start-up, so a bot's
        // personality never changes while the server runs.
        WorldSession* session = bot->GetSession();
        bool const randomBot = sAwareAiConfig.ChatEnabled && session &&
            sPlayerbotAIConfig->IsInRandomAccountList(session->GetAccountId());
        return Personality(AiChatPolicy::PersonalityIndex(bot->GetGUID(), randomBot));
    }

    uint32 ChattinessPercent(Player* bot) { return CHATTINESS_PERCENT[uint32(PersonalityOf(bot))]; }

    char const* PersonalityName(Personality personality)
    {
        switch (personality)
        {
            case Personality::QUIET: return "quiet";
            case Personality::FRIENDLY: return "friendly";
            case Personality::JOKER: return "joker";
            case Personality::GRUMPY: return "grumpy";
        }

        return "friendly";
    }

    // Say() with the way out chosen by the caller (the moments hand theirs to the AI layer).
    bool SayVia(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
                std::map<std::string, std::string> const& placeholders, Channel channel, Priority priority,
                uint32 chancePct, Player* whisperTo, bool blendPools, Sender const& sender);

    bool Say(PlayerbotAI* botAI, Moment moment)
    {
        MomentLine const& line = LineOf(moment);
        char const* aiMoment = AiMomentOf(moment);
        // The rolls and cooldowns stay the voice's own; only the words of a moment that speaks may come from the AI
        // layer, with the rule line as its fallback.
        Sender const viaAiLayer = [aiMoment, channel = line.channel](PlayerbotAI* ai, Player* bot, std::string const& text)
        {
            return AiChat::OnPartyMoment(ai, aiMoment, text) || Send(ai, bot, text, channel, nullptr);
        };
        return SayVia(botAI, line.key, line.fallback, {}, line.channel, Priority::CHATTER,
                      Settings().momentChance[uint32(moment)], nullptr, true, aiMoment ? viaAiLayer : Sender());
    }

    bool Say(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
             std::map<std::string, std::string> const& placeholders, Channel channel, Priority priority,
             uint32 chancePct, Player* whisperTo, bool blendPools)
    {
        return SayVia(botAI, key, fallback, placeholders, channel, priority, chancePct, whisperTo, blendPools, Sender());
    }

    bool SayToHuman(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
                    std::map<std::string, std::string> const& placeholders, Player* human, std::string const& aiMoment,
                    uint32 chancePct)
    {
        Sender const viaAiLayer = [human, aiMoment](PlayerbotAI* ai, Player* bot, std::string const& text)
        {
            return AiChat::OnOpener(ai, human, aiMoment, text) || Send(ai, bot, text, Channel::PARTY, nullptr);
        };
        return SayVia(botAI, key, fallback, placeholders, Channel::PARTY, Priority::CHATTER, chancePct, nullptr, false,
                      viaAiLayer);
    }

    bool SayVia(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
                std::map<std::string, std::string> const& placeholders, Channel channel, Priority priority,
                uint32 chancePct, Player* whisperTo, bool blendPools, Sender const& sender)
    {
        if (!botAI)
            return false;

        Player* bot = botAI->GetBot();
        if (!bot || !bot->IsInWorld())
            return false;

        VoiceSettings const& settings = Settings();

        if (!settings.enabled)
        {
            // The old behaviour: plain default lines for what the bot has to tell, nothing else.
            if (priority == Priority::CHATTER)
                return false;

            return Send(botAI, bot, FreshTextFor(bot, key, fallback, placeholders, false, false), channel, whisperTo);
        }

        if (priority == Priority::CHATTER)
        {
            if (!MayChatter(botAI, bot))
                return false;

            uint32 const percent = std::min(MAX_PERCENT, chancePct * CHATTINESS_PERCENT[uint32(PersonalityOf(bot))] / MAX_PERCENT);
            uint32 const now = getMSTime();
            uint32 const groupId = bot->GetGroup()->GetLowGUID();
            {
                std::lock_guard<std::mutex> guard(stateLock);
                auto const mine = botLastSay.find(bot->GetGUIDLow());
                auto const theirs = groupLastSay.find(groupId);
                if ((mine != botLastSay.end() && getMSTimeDiff(mine->second, now) < settings.botCooldownMs) ||
                    (theirs != groupLastSay.end() && getMSTimeDiff(theirs->second, now) < settings.groupCooldownMs))
                    return false;
            }

            if (urand(1, MAX_PERCENT) > percent)
                return false;
        }

        std::string const text = FreshTextFor(bot, key, fallback, placeholders, true, blendPools);
        if (!(sender ? sender(botAI, bot, text) : Send(botAI, bot, text, channel, whisperTo)))
            return false;

        if (priority != Priority::REPLY)
        {
            uint32 const now = getMSTime();
            std::lock_guard<std::mutex> guard(stateLock);
            botLastSay[bot->GetGUIDLow()] = now;
            Trim(botLastSay, settings.botCooldownMs);
            if (Group* group = bot->GetGroup())
            {
                groupLastSay[group->GetLowGUID()] = now;
                Trim(groupLastSay, settings.groupCooldownMs);
            }
        }

        SF_LOG_DEBUG("playerbots", "[VOICE] %s (%s) says %s", bot->GetName().c_str(), PersonalityName(PersonalityOf(bot)),
                     key.c_str());
        return true;
    }

    void NoteDeath(Player* bot)
    {
        if (!bot)
            return;

        Group* group = bot->GetGroup();
        if (!group || group->GetMembersCount() < 2 || group->isBGGroup())
            return;

        // The whole group is dead, not only this bot.
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member != bot && member->IsAlive() && member->GetMapId() == bot->GetMapId())
                return;
        }

        // The wipe is also a shared moment for the AI layer's memory, whatever the voice switch says.
        AiChat::OnWipe(bot);
        if (!Settings().enabled)
            return;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);
        if (wipeNotes.emplace(bot->GetGUIDLow(), now).second)
            ++pendingWipes;
        else
            wipeNotes[bot->GetGUIDLow()] = now;

        if (wipeNotes.size() >= TRIM_SIZE)
        {
            for (auto itr = wipeNotes.begin(); itr != wipeNotes.end();)
            {
                if (getMSTimeDiff(itr->second, now) > WIPE_NOTE_MS)
                {
                    itr = wipeNotes.erase(itr);
                    --pendingWipes;
                }
                else
                    ++itr;
            }
        }
    }

    bool HasPendingWipe(Player* bot)
    {
        if (!pendingWipes.load(std::memory_order_relaxed))
            return false;

        std::lock_guard<std::mutex> guard(stateLock);
        return wipeNotes.find(bot->GetGUIDLow()) != wipeNotes.end();
    }

    bool SayAfterWipe(PlayerbotAI* botAI)
    {
        Player* bot = botAI->GetBot();
        uint32 noteMs = 0;
        {
            std::lock_guard<std::mutex> guard(stateLock);
            auto itr = wipeNotes.find(bot->GetGUIDLow());
            if (itr == wipeNotes.end())
                return false;

            noteMs = itr->second;
            wipeNotes.erase(itr);
            --pendingWipes;
        }

        if (getMSTimeDiff(noteMs, getMSTime()) > WIPE_NOTE_MS)
            return false;

        return Say(botAI, Moment::WIPE);
    }
}
