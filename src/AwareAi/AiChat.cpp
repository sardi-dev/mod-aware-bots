/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AiChat.h"
#include "AiChatLines.h"
#include "AiChatPolicy.h"
#include "AiObjectContext.h"
#include "AwareAiClient.h"
#include "AwareAiConfig.h"
#include "AwareAiProtocol.h"
#include "AwareAiRuntime.h"
#include "Battleground.h"
#include "BotVoice.h"
#include "BroadcastHelper.h"
#include "DBCStores.h"
#include "LikingStore.h"
#include "GridObject.h"  // before CellImpl.h
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Random.h"
#include "Timer.h"
#include "World.h"
#include "WorldSession.h"
#include <algorithm>
#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace
{
    // Aliases: the core has its own Channel class and Roll struct.
    using ChatChannel = AiChatPolicy::Channel;
    using AiChatPolicy::Kind;

    // A lost answer (it never should be: the client always calls back) gives up this long after the deadline.
    constexpr uint32 LOST_ANSWER_GRACE_MS = 2000;
    // The HTTP timeout leaves the sidecar room to answer its own "deadline" hint.
    constexpr Milliseconds HTTP_MARGIN = 300ms;
    // Tickets of bots that logged out before saying their line are dropped after this: longer than any late answer or
    // after-combat hold.
    constexpr uint32 STALE_TICKET_MS = 240000;
    // Never more open tickets than this, whatever happens in the world.
    constexpr size_t MAX_OPEN_TICKETS = 256;
    // The second guild bot types a little after the first.
    constexpr uint32 SECOND_GUILD_BOT_MIN_MS = 1500;
    constexpr uint32 SECOND_GUILD_BOT_MAX_MS = 4000;
    constexpr uint32 JITTER_ROLL_MAX = 100;
    constexpr uint32 ROLL_MAX = 100;
    // Any spread works: the pools wrap the pick round.
    constexpr uint32 PICK_MAX = 1000;
    constexpr uint32 VARIATION_ROLL_MAX = 99;

    struct Ticket
    {
        uint64 id = 0;
        uint64 botGuid = 0;
        uint64 humanGuid = 0;
        std::string humanName;
        Kind kind = Kind::WHISPER_REPLY;
        ChatChannel channel = ChatChannel::WHISPER;
        std::string fallback;
        std::string canned;  // said when the sidecar has no line and the moment no rule line
        size_t humanChars = 0;
        uint32 createdMs = 0;
        uint32 deadlineMs = 0;
        uint32 extraDelayMs = 0;
        uint32 jitterRoll = 0;
        uint32 timingRoll = 0;
        uint32 lateMs = 0;
        uint32 guildId = 0;
        uint32 guildWindowMs = 0;
        uint32 bgInstanceId = 0;  // the match a battleground line is about
        // Filled by the answer (IO thread) or by the deadline.
        bool answered = false;
        std::string source;
        std::string reason;
        std::string reply;
        // Chosen once on the bot's update. delayMs counts from createdMs.
        bool chosen = false;
        AiChatPolicy::Line line;
        uint32 delayMs = 0;
        AiChatPolicy::Timing timing = AiChatPolicy::Timing::NOW;
        bool waitingCombat = false;
        // Conversation mode: the request is kept to send it once the gathering is over and to ask again with merged
        // lines. Ages count from createdMs.
        AwareAiChatRequest request;
        std::vector<std::string> lines;      // the human lines in the request (empty for a moment)
        std::vector<std::string> followUps;  // lines that came after the request went out
        bool sent = false;
        uint32 sendAfterMs = 0;
        uint32 roundStartMs = 0;             // when the current request went out
        bool reasked = false;                // a held answer was asked again with its follow-ups
    };

    std::mutex ticketLock;
    // One open ticket per bot, by the bot's guid.
    std::unordered_map<uint64, Ticket> tickets;
    std::atomic<uint32> openTickets{ 0 };
    std::atomic<uint64> nextTicketId{ 1 };

    bool LinkUp()
    {
        return sAwareAiClient.IsRunning() && !sAwareAiClient.IsBreakerOpen();
    }

    // Memory events never take more than this many of the client's requests in flight, so chat keeps its room.
    constexpr uint32 MEMORY_IN_FLIGHT_LIMIT = 8;
    constexpr Milliseconds MEMORY_TIMEOUT = 2000ms;
    // The wipe scans the log files, so it gets longer than a memory event.
    constexpr Milliseconds WIPE_TIMEOUT = 10000ms;
    // The same thanks or insult from a human to a bot counts once in this time.
    constexpr uint32 TONE_REPEAT_MS = 30000;
    // A bot's wipe counts once in this time (both of its release paths note it).
    constexpr uint32 WIPE_REPEAT_MS = 60000;
    // How long a bot remembers that it wanted an item, or that a human passed on it, while the roll runs.
    constexpr uint32 LOOT_NOTE_MS = 300000;
    constexpr size_t MEMORY_BOOK_TRIM_SIZE = 512;

    std::mutex memoryLock;
    std::unordered_map<uint64, uint32> wipeStamps;                        // bot -> its last wipe
    std::map<std::pair<uint64, uint64>, uint32> toneStamps;               // (bot, human) -> last thanks or insult
    std::map<std::pair<uint64, uint32>, uint32> lootWishes;               // (bot, item) -> when it wanted to need it
    std::map<std::pair<uint64, uint32>, std::pair<uint64, uint32>> lootPasses;  // (bot, item) -> (human, when)

    // Openers (design decision 0031 item 3): the bot "types" a moment longer than for a reply, a level-up is congratulated by
    // group mates up to this far away, a duel is asked only at this distance and level gap.
    constexpr uint32 OPENER_EXTRA_MIN_MS = 1000;
    constexpr uint32 OPENER_EXTRA_MAX_MS = 3000;
    constexpr float LEVEL_GRATS_GROUP_RANGE = 100.0f;
    constexpr float DUEL_ASK_RANGE = 20.0f;
    constexpr uint32 DUEL_ASK_LEVEL_GAP = 5;

    // Per online human, from their login: openers and friendly whispers (design decision 0031 items 3 and 14).
    struct HumanState
    {
        bool hadOpener = false;
        uint32 lastOpenerMs = 0;
        uint32 duelAskTimerMs = 0;
        uint32 friendTimerMs = 0;
        uint32 friendCap = 0;      // friendly whispers this session may hold, rolled at login
        uint32 friendSaid = 0;
    };

    std::mutex humanLock;
    std::unordered_map<uint64, HumanState> humans;

    HumanState NewSession()
    {
        HumanState state;
        uint32 const max = sAwareAiConfig.ChatFriendWhisperMaxPerSession;
        state.friendCap = max ? urand(1, max) : 0;
        return state;
    }

    // The human's state, a new session when they have none yet. Caller holds humanLock.
    HumanState& StateOf(uint64 humanGuid)
    {
        auto const [itr, fresh] = humans.try_emplace(humanGuid);
        if (fresh)
            itr->second = NewSession();
        return itr->second;
    }

    // True, and noted, when the human may get an opener now: the chance passed and their last one is past the cooldown.
    bool TakeOpener(uint64 humanGuid, uint32 chancePct)
    {
        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(humanLock);
        HumanState& state = StateOf(humanGuid);
        AiChatPolicy::OpenerCue cue;
        cue.chancePct = chancePct;
        cue.hadOpener = state.hadOpener;
        cue.sinceLastOpenerMs = getMSTimeDiff(state.lastOpenerMs, now);
        cue.cooldownMs = sAwareAiConfig.ChatOpenerCooldownSeconds * IN_MILLISECONDS;
        if (!AiChatPolicy::MayOpen(cue, urand(1, ROLL_MAX)))
            return false;

        state.hadOpener = true;
        state.lastOpenerMs = now;
        return true;
    }

    // The bots each human grouped or duelled with since the server started, newest last (friendly whisper candidates).
    constexpr size_t MAX_COMPANIONS = 16;
    std::unordered_map<uint64, std::vector<uint64>> companions;  // under humanLock

    void NoteCompanion(uint64 humanGuid, uint64 botGuid)
    {
        std::lock_guard<std::mutex> guard(humanLock);
        std::vector<uint64>& known = companions[humanGuid];
        known.erase(std::remove(known.begin(), known.end(), botGuid), known.end());
        if (known.size() >= MAX_COMPANIONS)
            known.erase(known.begin());
        known.push_back(botGuid);
    }

    // When each human last wrote to each bot (design decision 0031 item 12), by (bot guid, human guid).
    std::mutex conversationLock;
    std::map<std::pair<uint64, uint64>, uint32> lastLines;
    constexpr size_t CONVERSATION_TRIM_SIZE = 256;

    // Notes a human line to the bot; true when it continues a conversation.
    bool NoteHumanLine(uint64 botGuid, uint64 humanGuid, uint32 now)
    {
        std::lock_guard<std::mutex> guard(conversationLock);
        if (lastLines.size() >= CONVERSATION_TRIM_SIZE)
            for (auto itr = lastLines.begin(); itr != lastLines.end();)
                itr = getMSTimeDiff(itr->second, now) >= AiChatPolicy::CONVERSATION_WINDOW_MS ? lastLines.erase(itr)
                                                                                               : std::next(itr);

        auto const [itr, fresh] = lastLines.try_emplace({ botGuid, humanGuid }, now);
        uint32 const sinceMs = fresh ? 0 : getMSTimeDiff(itr->second, now);
        itr->second = now;
        return AiChatPolicy::InConversation(!fresh, sinceMs);
    }

    bool TalksWith(uint64 botGuid, uint64 humanGuid, uint32 now)
    {
        std::lock_guard<std::mutex> guard(conversationLock);
        auto const itr = lastLines.find({ botGuid, humanGuid });
        return itr != lastLines.end() && AiChatPolicy::InConversation(true, getMSTimeDiff(itr->second, now));
    }

    AiChatPolicy::Gate CurrentGate()
    {
        AwareAiConfig const& config = sAwareAiConfig;
        AiChatPolicy::Gate gate;
        gate.layerOn = config.Enabled;
        gate.chatOn = config.ChatEnabled;
        gate.linkUp = LinkUp();
        gate.nearbyChancePct = config.ChatNearbyChancePct;
        gate.partyChancePct = config.ChatPartyChancePct;
        return gate;
    }

    // The cheap test every hook makes first: the layer and its chat are switched on. With the sidecar down a bot still
    // answers from the template layer and its canned lines (design decision 0031: a failure never leaves a human unanswered).
    bool ChatOn()
    {
        return sAwareAiConfig.Enabled && sAwareAiConfig.ChatEnabled;
    }

    // The loot wish book is kept for the AI layer's memory and for the liking score (design decision 0033 point 14).
    bool TracksLoot()
    {
        return ChatOn() || LikingStore::Enabled();
    }

    // ChatOn and the sidecar link is up: the bot voice keeps its own rule line at once otherwise.
    bool Available()
    {
        return ChatOn() && LinkUp();
    }

    bool IsHuman(Player* player)
    {
        return player && player->GetSession() && !player->GetSession()->IsBot();
    }

    // A module bot that can talk (a human with a bot master object is not one).
    PlayerbotAI* BotAiOf(Player* player)
    {
        if (!player || !player->IsInWorld() || !player->GetSession() || !player->GetSession()->IsBot())
            return nullptr;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
        return botAI && !botAI->IsRealPlayer() ? botAI : nullptr;
    }

    AiChatPolicy::CommandSyntax Syntax()
    {
        return { sPlayerbotAIConfig->commandSeparator, sPlayerbotAIConfig->commandPrefix };
    }

    // The bot's own trigger and action names, looked up without creating or caching anything.
    bool IsCommandFor(PlayerbotAI* botAI, std::string const& message)
    {
        AiObjectContext* context = botAI ? botAI->GetAiObjectContext() : nullptr;
        AiChatPolicy::NameLookup isTrigger;
        AiChatPolicy::NameLookup isAction;
        if (context)
        {
            isTrigger = [context](std::string const& name) { return context->HasTrigger(name); };
            isAction = [context](std::string const& name) { return context->HasAction(name); };
        }

        return AiChatPolicy::IsCommandLike(message, Syntax(), isTrigger, isAction);
    }

    AwareAiChatCharacter CharacterOf(Player* player)
    {
        AwareAiChatCharacter character;
        character.Name = player->GetName();
        character.ClassName = AiChatPolicy::ClassToken(player->getClass());
        character.Race = AiChatPolicy::RaceToken(player->getRace());
        character.Level = player->getLevel();
        return character;
    }

    std::string ZoneOf(Player* bot)
    {
        AreaTableEntry const* zone = GetAreaEntryByAreaID(bot->GetZoneId());
        return zone && zone->m_AreaName ? std::string(zone->m_AreaName) : std::string();
    }

    AiChatPolicy::Archetype ArchetypeOfBot(Player* bot)
    {
        return AiChatPolicy::ArchetypeOf(AiChatPolicy::PersonalityTraits(uint8(BotVoice::PersonalityOf(bot))));
    }

    void StoreAnswer(uint64 botGuid, uint64 ticketId, AwareAiResponse const& response)
    {
        std::string source = "error";
        std::string reason;
        std::string reply;
        if (!response.Ok)
            reason = response.Error.empty() ? "http_error" : response.Error;
        else if (std::optional<AwareAiChatResponse> parsed = ParseChatBody(response.Body))
        {
            source = parsed->Source;
            reason = parsed->Reason;
            reply = std::move(parsed->Reply);
        }
        else
            reason = "bad_response";

        std::lock_guard<std::mutex> guard(ticketLock);
        auto const itr = tickets.find(botGuid);
        if (itr == tickets.end() || itr->second.id != ticketId || itr->second.answered)
            return;

        itr->second.answered = true;
        itr->second.source = std::move(source);
        itr->second.reason = std::move(reason);
        itr->second.reply = std::move(reply);
    }

    // Caller holds ticketLock.
    void DropStaleTickets(uint32 now)
    {
        for (auto itr = tickets.begin(); itr != tickets.end();)
        {
            uint32 const ageMs = getMSTimeDiff(itr->second.createdMs, now);
            if (ageMs > STALE_TICKET_MS)
            {
                sAwareAiRuntime.LogChat(AiChatPolicy::KindName(itr->second.kind), "rules",
                    itr->second.reason.empty() ? "timeout" : itr->second.reason, "bot_gone", ageMs);
                itr = tickets.erase(itr);
                --openTickets;
            }
            else
                ++itr;
        }
    }

    struct Ask
    {
        Player* bot = nullptr;
        Player* human = nullptr;
        Kind kind = Kind::WHISPER_REPLY;
        ChatChannel channel = ChatChannel::WHISPER;
        std::string message;      // the human's line, or the moment described for the model
        std::string fallback;     // the rule line, or empty
        AiChatLines::Canned canned = AiChatLines::Canned::NONE;  // the canned line's moment when there is no rule line
        bool templated = false;   // a trivial line: answered from the template layer, the sidecar is not asked
        std::string templateReply;
        size_t humanChars = 0;    // how much the bot "reads" before it types
        uint32 extraDelayMs = 0;
        uint32 guildId = 0;
        uint32 bgInstanceId = 0;
    };

    // A human's line to the bot: "reads" it, and a trivial one ("hi", "ty", "gg") is answered from the template layer
    // without asking the sidecar.
    void SetHumanLine(Ask& ask, std::string const& message)
    {
        ask.message = message;
        ask.humanChars = message.size();
        if (!ask.bot)
            return;

        AiChatLines::Trivial const trivial = AiChatLines::ClassifyTrivial(message, ask.bot->GetName());
        if (trivial == AiChatLines::Trivial::NONE)
            return;

        ask.templated = true;
        ask.templateReply = AiChatLines::TemplateReply(trivial, ArchetypeOfBot(ask.bot), urand(0, PICK_MAX),
            urand(0, VARIATION_ROLL_MAX), ask.channel == ChatChannel::WHISPER);
    }

    // Answers to a human's line follow the reply timing rules; moments (duels, voice moments) come at once.
    bool IsHumanLine(Ticket const& ticket)
    {
        return ticket.humanChars > 0;
    }

    void SendRequest(uint64 botGuid, uint64 ticketId, AwareAiChatRequest const& request)
    {
        if (!LinkUp())
        {
            AwareAiResponse down;
            down.Error = "link_down";
            StoreAnswer(botGuid, ticketId, down);
            return;
        }

        bool const sent = sAwareAiClient.Send(AwareAiHttpMethod::Post, "/v1/chat", BuildChatBody(request),
            Milliseconds(request.DeadlineMs) + HTTP_MARGIN,
            [botGuid, ticketId](AwareAiResponse response) { StoreAnswer(botGuid, ticketId, response); });
        if (!sent)
        {
            AwareAiResponse refused;
            refused.Error = "client_busy";
            StoreAnswer(botGuid, ticketId, refused);
        }
    }

    size_t CharsOf(std::vector<std::string> const& lines)
    {
        size_t chars = 0;
        for (std::string const& line : lines)
            chars += line.size();
        return chars;
    }

    // Conversation mode: another line from the same human on the same channel while the bot still reads or types.
    // Before the request went out it joins it; after, it waits as a follow-up for one more answer. Caller holds
    // ticketLock.
    bool MergeLine(Ticket& open, uint64 humanGuid, ChatChannel channel, std::string const& line, uint32 now)
    {
        if (open.humanGuid != humanGuid || open.channel != channel || !IsHumanLine(open))
            return false;

        if (!open.sent)
        {
            open.lines.push_back(line);
            open.request.Message = AiChatPolicy::JoinLines(open.lines);
            open.request.Conversation = true;
            open.humanChars = CharsOf(open.lines);
            open.sendAfterMs = AiChatPolicy::ExtendedGatherMs(open.sendAfterMs, getMSTimeDiff(open.createdMs, now));
            return true;
        }

        if (open.followUps.size() >= AiChatPolicy::MAX_MERGED_LINES)
            open.followUps.erase(open.followUps.begin());
        open.followUps.push_back(line);
        return true;
    }

    // The module's liking score of this bot for the human goes with the request while AwareAi.Chat.SendLiking is on (design decision 0033
    // point 14): the percent and the band word, nothing else. The sidecar then uses it as the one score; a pair whose rows are
    // not loaded yet sends neither field and the sidecar falls back to its own.
    void FillLiking(AwareAiChatRequest& request, Player* bot, Player* human)
    {
        if (!sAwareAiConfig.ChatSendLiking)
            return;

        std::optional<double> const value = LikingStore::ValueOf(bot, human);
        if (!value)
            return;

        request.LikingPercent = LikingPolicy::Percent(*value);
        request.Relation = LikingPolicy::BandName(LikingPolicy::BandOf(sPlayerbotAIConfig->socialLiking, *value));
    }

    // Files the ticket and sends the request: at once for a moment, after a short gathering for a human's line (more
    // lines may follow), never for a template answer. False when nothing was filed (chat off, the bot already waits
    // for another line, too many open tickets): the caller then behaves as without the layer.
    bool Send(Ask const& ask)
    {
        if (!ChatOn() || !ask.bot || !ask.human)
            return false;

        AwareAiConfig const& config = sAwareAiConfig;
        uint32 const now = getMSTime();
        uint32 const deadlineMs = config.ChatDeadlineMs;
        uint64 const botGuid = ask.bot->GetGUID();
        uint64 const humanGuid = ask.human->GetGUID();
        bool const humanLine = ask.humanChars > 0;
        bool const conversation = humanLine && NoteHumanLine(botGuid, humanGuid, now);
        {
            std::lock_guard<std::mutex> guard(ticketLock);
            auto const itr = tickets.find(botGuid);
            if (itr != tickets.end())
                return humanLine && MergeLine(itr->second, humanGuid, ask.channel, ask.message, now);
        }

        AwareAiChatRequest request;
        request.BotGuid = botGuid;
        request.PlayerGuid = humanGuid;
        request.Bot = CharacterOf(ask.bot);
        request.Player = CharacterOf(ask.human);
        AiChatPolicy::Traits const traits =
            AiChatPolicy::PersonalityTraits(uint8(BotVoice::PersonalityOf(ask.bot)));
        request.Aggression = traits.aggression;
        request.Sociability = traits.sociability;
        request.Chattiness = traits.chattiness;
        request.Honor = traits.honor;
        request.SameFaction = ask.bot->GetTeamId() == ask.human->GetTeamId();
        request.Zone = ZoneOf(ask.bot);
        request.Kind = AiChatPolicy::KindName(ask.kind);
        request.Channel = AiChatPolicy::ChannelName(ask.channel);
        request.Message = ask.message;
        request.DeadlineMs = deadlineMs;
        request.Conversation = conversation;
        FillLiking(request, ask.bot, ask.human);
        request.DenyBeingBot = config.ChatDenyBeingBot;
        // Sent in the tick the human wrote; the window is checked again before the bot speaks.
        if (ask.kind == Kind::GUILD_REPLY)
            request.GuildHumanMessageAgeS = 0.0;

        Ticket ticket;
        ticket.id = nextTicketId++;
        ticket.botGuid = botGuid;
        ticket.humanGuid = humanGuid;
        ticket.humanName = ask.human->GetName();
        ticket.kind = ask.kind;
        ticket.channel = ask.channel;
        ticket.fallback = ask.fallback;
        ticket.canned = AiChatLines::CannedLine(ask.canned, ArchetypeOfBot(ask.bot), urand(0, PICK_MAX));
        ticket.humanChars = ask.humanChars;
        ticket.createdMs = now;
        ticket.deadlineMs = deadlineMs;
        ticket.extraDelayMs = ask.extraDelayMs;
        ticket.jitterRoll = urand(0, JITTER_ROLL_MAX);
        ticket.timingRoll = urand(1, ROLL_MAX);
        ticket.lateMs = urand(AiChatPolicy::LATE_MIN_MS, AiChatPolicy::LATE_MAX_MS);
        ticket.guildId = ask.guildId;
        ticket.guildWindowMs = config.ChatGuildWindowSeconds * IN_MILLISECONDS;
        ticket.bgInstanceId = ask.bgInstanceId;
        ticket.request = request;
        if (humanLine)
            ticket.lines.push_back(ask.message);

        if (ask.templated)
        {
            ticket.sent = true;
            ticket.answered = true;
            ticket.source = std::string(AiChatPolicy::TEMPLATE_SOURCE);
            ticket.reply = ask.templateReply;
        }
        else if (humanLine)
            ticket.sendAfterMs = AiChatPolicy::FirstGatherMs(ask.humanChars);
        else
            ticket.sent = true;

        bool const sendNow = ticket.sent && !ask.templated;
        uint64 const ticketId = ticket.id;
        {
            std::lock_guard<std::mutex> guard(ticketLock);
            DropStaleTickets(now);
            if (tickets.count(botGuid) || tickets.size() >= MAX_OPEN_TICKETS)
                return false;

            tickets.emplace(botGuid, std::move(ticket));
            ++openTickets;
        }

        // A human's line goes out from the bot's update once the gathering is over.
        if (sendNow)
            SendRequest(botGuid, ticketId, request);

        return true;
    }

    // The module bots of the human's faction within range (map thread of the human: a grid search).
    std::vector<Player*> NearbyBots(Player* human, float range)
    {
        std::list<Player*> nearby;
        Skyfire::AnyPlayerInObjectRangeCheck check(human, range, true);
        Skyfire::PlayerListSearcher<Skyfire::AnyPlayerInObjectRangeCheck> searcher(human, nearby, check);
        human->VisitNearbyWorldObject(range, searcher);

        std::vector<Player*> bots;
        for (Player* player : nearby)
            if (player != human && BotAiOf(player) && player->GetTeamId() == human->GetTeamId())
                bots.push_back(player);

        return bots;
    }

    // One bot out of several, the talkative ones more often (the bot voice's chattiness).
    Player* PickSpeaker(std::vector<Player*> const& candidates)
    {
        uint32 total = 0;
        for (Player* candidate : candidates)
            total += BotVoice::ChattinessPercent(candidate);

        if (!total)
            return nullptr;

        uint32 roll = urand(1, total);
        for (Player* candidate : candidates)
        {
            uint32 const weight = BotVoice::ChattinessPercent(candidate);
            if (roll <= weight)
                return candidate;

            roll -= weight;
        }

        return candidates.back();
    }

    Player* NamedIn(std::vector<Player*> const& candidates, std::string const& message)
    {
        for (Player* candidate : candidates)
            if (AiChatPolicy::MentionsName(message, candidate->GetName()))
                return candidate;

        return nullptr;
    }

    // The human's target among the candidates.
    Player* TargetedIn(std::vector<Player*> const& candidates, Player* human)
    {
        uint64 const target = human->GetTarget();
        if (!target)
            return nullptr;

        for (Player* candidate : candidates)
            if (candidate->GetGUID() == target)
                return candidate;

        return nullptr;
    }

    // The candidate the human is in a conversation with (their last line to it under 2 minutes old), or none.
    Player* PartnerIn(std::vector<Player*> const& candidates, Player* human)
    {
        uint32 const now = getMSTime();
        for (Player* candidate : candidates)
            if (TalksWith(candidate->GetGUID(), human->GetGUID(), now))
                return candidate;

        return nullptr;
    }

    // The nearest candidate within reach, or none.
    Player* NearestWithin(std::vector<Player*> const& candidates, Player* human, float reach)
    {
        Player* nearest = nullptr;
        float best = reach;
        for (Player* candidate : candidates)
        {
            float const distance = human->GetDistance(candidate);
            if (distance <= best)
            {
                best = distance;
                nearest = candidate;
            }
        }

        return nearest;
    }

    bool RollFor(Kind kind, bool certain)
    {
        // The caller checked ChatOn; a dead link still answers from the template layer or a canned line.
        return urand(1, ROLL_MAX) <= AiChatPolicy::ChancePct(CurrentGate(), kind, certain);
    }

    // Says the chosen line where the human still hears it; the outcome goes to the events log.
    char const* SayLine(PlayerbotAI* botAI, Player* bot, Ticket const& ticket, uint32 ageMs)
    {
        std::string const& text = ticket.line.text;
        if (text.empty())
            return "silent";

        switch (ticket.channel)
        {
            case ChatChannel::WHISPER:
                return botAI->Whisper(text, ticket.humanName) ? "said" : "human_gone";
            case ChatChannel::SAY:
            {
                Player* human = ObjectAccessor::GetPlayer(*bot, ticket.humanGuid);
                float const range = sWorld->GetFloatConfig(WorldFloatConfigs::CONFIG_LISTEN_RANGE_SAY);
                if (!human || !human->IsInWorld() || !bot->IsWithinDistInMap(human, range))
                    return "out_of_range";

                botAI->Say(text);
                return "said";
            }
            case ChatChannel::PARTY:
            case ChatChannel::RAID:
            {
                Group* group = bot->GetGroup();
                if (!group || !group->IsMember(ticket.humanGuid))
                    return "left_group";

                bool const sent = ticket.channel == ChatChannel::RAID && group->isRaidGroup() ? botAI->SayToRaid(text)
                                                                                         : botAI->SayToParty(text);
                return sent ? "said" : "left_group";
            }
            case ChatChannel::GUILD:
                if (!ticket.guildId || bot->GetGuildId() != ticket.guildId)
                    return "left_guild";

                if (!AiChatPolicy::StillFresh(ageMs, ticket.guildWindowMs))
                    return "stale";

                return BroadcastHelper::SayToChannel(bot, text, BroadcastHelper::TO_GUILD) ? "said" : "left_guild";
            case ChatChannel::BATTLEGROUND:
            {
                // Only while the bot is still in that match and the human it speaks for is still on its team there.
                Group* group = bot->GetGroup();
                Player* human = ObjectAccessor::GetPlayer(*bot, ticket.humanGuid);
                bool const humanOnTeam = human && IsHuman(human) && human->IsInWorld() &&
                    human->GetBattlegroundId() == ticket.bgInstanceId && human->GetBGTeam() == bot->GetBGTeam() &&
                    group && group->isBGGroup() && group->IsMember(ticket.humanGuid);
                if (char const* block = AiChatPolicy::BgDeliveryBlock(bot->InBattleground(), bot->GetBattlegroundId(),
                        ticket.bgInstanceId, humanOnTeam))
                    return block;

                return BotVoice::SayToBattleground(bot, text) ? "said" : "left_battleground";
            }
        }

        return "silent";
    }

    AiChatPolicy::TimingCue TimingCueOf(Ticket const& ticket, Player* bot)
    {
        AwareAiConfig const& config = sAwareAiConfig;
        AiChatPolicy::TimingCue cue;
        cue.humanLine = IsHumanLine(ticket);
        cue.whisper = ticket.channel == ChatChannel::WHISPER;
        cue.lateAllowed = ticket.channel != ChatChannel::GUILD;
        cue.inCombat = bot->IsInCombat();
        cue.latePct = config.ChatLateChancePct;
        cue.combatWaitPct = config.ChatCombatWaitPct;
        cue.combatSkipPct = config.ChatCombatSkipPct;
        return cue;
    }

    uint32 TypingMs(Ticket const& ticket)
    {
        return AiChatPolicy::ReplyDelayMs(ticket.humanChars, ticket.line.text.size(), ticket.jitterRoll) +
            ticket.extraDelayMs;
    }

    // Moves an answered ticket on: picks the line and its timing once, holds it through a fight, and says whether it is
    // due now. Caller holds ticketLock.
    bool Advance(Ticket& ticket, Player* bot, uint32 ageMs)
    {
        if (!ticket.answered)
        {
            if (ageMs - ticket.roundStartMs < ticket.deadlineMs + LOST_ANSWER_GRACE_MS)
                return false;

            ticket.answered = true;
            ticket.source = "error";
            ticket.reason = "timeout";
        }

        if (!ticket.chosen)
        {
            ticket.line = AiChatPolicy::ChooseLine(ticket.source, ticket.reply, ticket.fallback, ticket.canned);
            // A held answer asked again comes at once: one late line only.
            ticket.timing = ticket.reasked ? AiChatPolicy::Timing::NOW
                                           : AiChatPolicy::PlanTiming(TimingCueOf(ticket, bot), ticket.timingRoll);
            // The typing counts from the human's line, so the sidecar's own time is hidden in it; a second request
            // types from when it went out.
            uint32 const typingMs = (ticket.reasked ? ticket.roundStartMs : 0) + TypingMs(ticket);
            switch (ticket.timing)
            {
                case AiChatPolicy::Timing::LATE:
                    ticket.delayMs = typingMs + ticket.lateMs;
                    break;
                case AiChatPolicy::Timing::AFTER_COMBAT:
                    ticket.waitingCombat = true;
                    break;
                case AiChatPolicy::Timing::SKIP:
                    ticket.delayMs = 0;
                    break;
                default:
                    ticket.delayMs = typingMs;
                    break;
            }

            ticket.chosen = true;
        }

        // Held for after the fight: the typing starts once the bot is out of combat, or the line is dropped when the
        // fight lasts too long.
        if (ticket.waitingCombat)
        {
            if (bot->IsInCombat() && ageMs < AiChatPolicy::COMBAT_HOLD_MAX_MS)
                return false;

            if (bot->IsInCombat())
                ticket.timing = AiChatPolicy::Timing::SKIP;

            ticket.waitingCombat = false;
            ticket.delayMs = ticket.timing == AiChatPolicy::Timing::SKIP ? 0 : ageMs + TypingMs(ticket);
        }

        return ageMs >= ticket.delayMs;
    }

    // One more answer to the lines a human sent while the bot typed: a new ticket that goes out on the bot's next
    // update, or a template answer when it is a single trivial line.
    void FileFollowUp(Player* bot, Ticket const& done)
    {
        uint32 const now = getMSTime();
        Ticket ticket;
        ticket.id = nextTicketId++;
        ticket.botGuid = done.botGuid;
        ticket.humanGuid = done.humanGuid;
        ticket.humanName = done.humanName;
        ticket.kind = done.kind;
        ticket.channel = done.channel;
        ticket.canned = done.canned;
        ticket.lines = done.followUps;
        ticket.humanChars = CharsOf(ticket.lines);
        ticket.createdMs = now;
        ticket.deadlineMs = done.deadlineMs;
        ticket.jitterRoll = urand(0, JITTER_ROLL_MAX);
        ticket.timingRoll = urand(1, ROLL_MAX);
        ticket.lateMs = urand(AiChatPolicy::LATE_MIN_MS, AiChatPolicy::LATE_MAX_MS);
        ticket.guildId = done.guildId;
        ticket.guildWindowMs = done.guildWindowMs;
        ticket.bgInstanceId = done.bgInstanceId;
        ticket.request = done.request;
        ticket.request.Message = AiChatPolicy::JoinLines(ticket.lines);
        ticket.request.Conversation = true;
        AiChatLines::Trivial const trivial = ticket.lines.size() == 1 ?
            AiChatLines::ClassifyTrivial(ticket.lines.front(), bot->GetName()) : AiChatLines::Trivial::NONE;
        if (trivial != AiChatLines::Trivial::NONE)
        {
            ticket.sent = true;
            ticket.answered = true;
            ticket.source = std::string(AiChatPolicy::TEMPLATE_SOURCE);
            ticket.reply = AiChatLines::TemplateReply(trivial, ArchetypeOfBot(bot), urand(0, PICK_MAX),
                urand(0, VARIATION_ROLL_MAX), ticket.channel == ChatChannel::WHISPER);
        }

        std::lock_guard<std::mutex> guard(ticketLock);
        if (tickets.count(ticket.botGuid) || tickets.size() >= MAX_OPEN_TICKETS)
            return;

        tickets.emplace(ticket.botGuid, std::move(ticket));
        ++openTickets;
    }

    // One memory event for a bot and a human pair, fire and forget. Grouping and duelling also make the bot one the
    // human knows, for the friendly whispers.
    void SendMemory(Player* bot, Player* human, AiChatPolicy::MemoryEvent event, std::string const& detail = {})
    {
        if (!ChatOn() || !BotAiOf(bot) || !IsHuman(human))
            return;

        switch (event)
        {
            case AiChatPolicy::MemoryEvent::GROUPED:
            case AiChatPolicy::MemoryEvent::DUEL_WON:
            case AiChatPolicy::MemoryEvent::DUEL_LOST:
                NoteCompanion(human->GetGUID(), bot->GetGUID());
                break;
            default:
                break;
        }

        if (!LinkUp())
            return;

        char const* name = AiChatPolicy::MemoryEventName(event);
        if (sAwareAiClient.InFlight() >= MEMORY_IN_FLIGHT_LIMIT)
        {
            SF_LOG_DEBUG("playerbots.ai", "[AIMEMORY] %s %s dropped: client busy", bot->GetName().c_str(), name);
            return;
        }

        AwareAiMemoryEvent body;
        body.BotGuid = bot->GetGUID();
        body.PlayerGuid = human->GetGUID();
        body.Event = name;
        body.Detail = detail;
        sAwareAiClient.Send(AwareAiHttpMethod::Post, "/v1/memory/event", BuildMemoryEventBody(body), MEMORY_TIMEOUT,
            [](AwareAiResponse) {});
        SF_LOG_DEBUG("playerbots.ai", "[AIMEMORY] %s %s", bot->GetName().c_str(), name);
    }

    // An obvious thanks or insult in a human's line to the bot (AiChatLines::ToneOf).
    void NoteTone(Player* bot, Player* human, std::string const& message)
    {
        AiChatLines::Tone const tone = AiChatLines::ToneOf(message);
        if (tone == AiChatLines::Tone::NONE || !bot || !human)
            return;

        uint32 const now = getMSTime();
        {
            std::lock_guard<std::mutex> guard(memoryLock);
            if (toneStamps.size() >= MEMORY_BOOK_TRIM_SIZE)
                toneStamps.clear();

            auto const [itr, fresh] = toneStamps.try_emplace({ bot->GetGUID(), human->GetGUID() }, now);
            if (!fresh && getMSTimeDiff(itr->second, now) < TONE_REPEAT_MS)
                return;

            itr->second = now;
        }

        SendMemory(bot, human, tone == AiChatLines::Tone::THANKED ? AiChatPolicy::MemoryEvent::THANKED
                                                                  : AiChatPolicy::MemoryEvent::INSULTED);
    }

    // Every bot and human pair of the group on the given map.
    void ForEachPairOnMap(Group* group, Map* map, std::function<void(Player*, Player*)> const& visit)
    {
        std::vector<Player*> bots;
        std::vector<Player*> humans;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || member->GetMap() != map)
                continue;

            if (BotAiOf(member))
                bots.push_back(member);
            else if (IsHuman(member))
                humans.push_back(member);
        }

        for (Player* bot : bots)
            for (Player* human : humans)
                visit(bot, human);
    }

    // A bot speaks first to a human (an opener), with a canned line when the sidecar has none.
    void SendOpener(Player* bot, Player* human, ChatChannel channel, std::string const& moment,
        AiChatLines::Canned canned)
    {
        Ask ask;
        ask.bot = bot;
        ask.human = human;
        ask.kind = Kind::OPENER;
        ask.channel = channel;
        ask.message = moment;
        ask.canned = canned;
        ask.extraDelayMs = urand(OPENER_EXTRA_MIN_MS, OPENER_EXTRA_MAX_MS);
        Send(ask);
    }

    // A human joined a group with bots (not a finder group: its bots greet on their own, LfgMannersAction).
    void GreetJoin(Group* group, Player* human)
    {
        if (!sAwareAiConfig.ChatOpenersEnabled || group->isLFGGroup())
            return;

        std::vector<Player*> candidates;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member != human && BotAiOf(member) && !member->IsInCombat())
                candidates.push_back(member);

        if (candidates.empty() || !TakeOpener(human->GetGUID(), sAwareAiConfig.ChatOpenerGreetChancePct))
            return;

        if (Player* speaker = PickSpeaker(candidates))
            SendOpener(speaker, human, group->isRaidGroup() ? ChatChannel::RAID : ChatChannel::PARTY,
                human->GetName() + " just joined your group. Greet them in a few words.", AiChatLines::Canned::GREET);
    }

    // Idle: standing out of combat, not casting, mounted, in a vehicle, a duel or an instance.
    bool IsIdle(Player* human)
    {
        Map* map = human->GetMap();
        return human->IsAlive() && !human->IsInCombat() && !human->duel && !human->isMoving() &&
            !human->IsNonMeleeSpellCasted(false) && !human->IsMounted() && !human->GetVehicle() && map &&
            !map->Instanceable();
    }

    // Now and then a rough or chatty bot standing near an idle human asks for a duel.
    void TryDuelAsk(Player* human)
    {
        AwareAiConfig const& config = sAwareAiConfig;
        if (!config.ChatOpenersEnabled || !config.ChatOpenerDuelAskChancePct || !IsIdle(human))
            return;

        std::vector<Player*> candidates;
        for (Player* bot : NearbyBots(human, DUEL_ASK_RANGE))
        {
            uint32 const gap = uint32(std::abs(int32(bot->getLevel()) - int32(human->getLevel())));
            if (AiChatPolicy::AsksForDuels(ArchetypeOfBot(bot)) && bot->IsAlive() && !bot->IsInCombat() && !bot->duel &&
                gap <= DUEL_ASK_LEVEL_GAP)
                candidates.push_back(bot);
        }

        if (candidates.empty() || !TakeOpener(human->GetGUID(), config.ChatOpenerDuelAskChancePct))
            return;

        Player* asker = candidates[urand(0, uint32(candidates.size()) - 1)];
        SendOpener(asker, human, ChatChannel::SAY,
            human->GetName() + " is standing around near you. Ask them for a duel, in a few words.",
            AiChatLines::Canned::DUEL_ASK);
    }

    // A bot the human knows, online, out of combat, out of instances and out of the human's group, newest first.
    Player* FriendFor(Player* human)
    {
        std::vector<uint64> known;
        {
            std::lock_guard<std::mutex> guard(humanLock);
            auto const itr = companions.find(human->GetGUID());
            if (itr == companions.end())
                return nullptr;

            known = itr->second;
        }

        Group* group = human->GetGroup();
        for (auto itr = known.rbegin(); itr != known.rend(); ++itr)
        {
            Player* bot = ObjectAccessor::FindPlayer(*itr);
            Map* map = bot ? bot->GetMap() : nullptr;
            if (BotAiOf(bot) && bot->IsAlive() && !bot->IsInCombat() && map && !map->Instanceable() &&
                !(group && group->IsMember(bot->GetGUID())))
                return bot;
        }

        return nullptr;
    }

    // The rare timer rolled: a bot that knows the human whispers first; the sidecar may refuse by relation.
    void TryFriendWhisper(Player* human)
    {
        AwareAiConfig const& config = sAwareAiConfig;
        if (!config.ChatFriendWhisperEnabled)
            return;

        AiChatPolicy::FriendWhisperCue cue;
        {
            std::lock_guard<std::mutex> guard(humanLock);
            HumanState const& state = StateOf(human->GetGUID());
            cue.saidThisSession = state.friendSaid;
            cue.sessionCap = state.friendCap;
        }

        Map* map = human->GetMap();
        cue.humanInCombat = human->IsInCombat();
        cue.humanInInstance = !map || map->Instanceable();
        cue.chancePct = config.ChatFriendWhisperChancePct;
        if (!AiChatPolicy::MayFriendWhisper(cue, urand(1, ROLL_MAX)))
            return;

        Player* bot = FriendFor(human);
        if (!bot)
            return;

        Ask ask;
        ask.bot = bot;
        ask.human = human;
        ask.kind = Kind::FRIEND_WHISPER;
        ask.channel = ChatChannel::WHISPER;
        ask.message = "You know " + human->GetName() + " from before. Whisper them first, like a friend saying hi.";
        ask.canned = AiChatLines::Canned::NONE;
        Send(ask);
    }

    void NoteFriendWhisperSaid(uint64 humanGuid)
    {
        std::lock_guard<std::mutex> guard(humanLock);
        if (auto const itr = humans.find(humanGuid); itr != humans.end())
            ++itr->second.friendSaid;
    }

    // Battleground chat (design decision 0029, phase 2). The first bot of a team that sees its match end files the team's lines;
    // the second speaker types a little after the first.
    constexpr uint32 SECOND_BG_SPEAKER_MIN_MS = 1500;
    constexpr uint32 SECOND_BG_SPEAKER_MAX_MS = 4000;
    std::mutex matchEndLock;
    AiChatPolicy::MatchEndBook matchEnds;  // under matchEndLock

    uint32 TeamIndexOf(Player* player)
    {
        return player->GetBGTeam() == ALLIANCE ? 0 : 1;
    }

    struct BgTeam
    {
        std::vector<Player*> bots;
        Player* human = nullptr;
    };

    // The module bots and the first human of the bot's team in its match, on its map (the battleground map thread: the
    // team's players are all on it).
    BgTeam TeamOf(Player* bot, Battleground* bg)
    {
        BgTeam team;
        uint32 const side = bot->GetBGTeam();
        for (auto const& [guid, info] : bg->GetPlayers())
        {
            if (info.Team != side)
                continue;

            Player* member = ObjectAccessor::GetPlayer(*bot, guid);
            if (!member || !member->IsInWorld() || member->GetBattlegroundId() != bg->GetInstanceID() || member->GetMap() != bot->GetMap())
                continue;

            if (BotAiOf(member))
                team.bots.push_back(member);
            else if (!team.human && IsHuman(member))
                team.human = member;
        }

        return team;
    }

    AiChatPolicy::BgChatCue BgCueOf(Battleground* bg, bool humanOnTeam)
    {
        AiChatPolicy::BgChatCue cue;
        cue.chatOn = ChatOn();
        cue.battlegroundOn = sAwareAiConfig.ChatBattlegroundEnabled;
        cue.arena = bg->isArena();
        cue.humanOnTeam = humanOnTeam;
        return cue;
    }

    std::string MatchName(Battleground* bg)
    {
        char const* name = bg->GetName();
        return name && *name ? std::string(name) : std::string("the battleground");
    }

    void FileBgLines(Player* bot, Battleground* bg, Kind kind)
    {
        bool const won = kind == Kind::BG_WIN;
        BgTeam const team = TeamOf(bot, bg);
        if (team.bots.empty() || !AiChatPolicy::MayBgChat(BgCueOf(bg, team.human != nullptr), kind))
            return;

        std::vector<uint32> weights;
        std::vector<uint32> rolls;
        for (Player* member : team.bots)
        {
            weights.push_back(BotVoice::ChattinessPercent(member));
            rolls.push_back(urand(0, PICK_MAX));
        }

        uint32 const speakers = AiChatPolicy::BgSpeakerCount(int32(sAwareAiConfig.ChatBattlegroundSpeakers.load()),
            urand(1, ROLL_MAX));
        std::string const moment = kind == Kind::BG_CAPTURE ? "Your team captured the flag in " + MatchName(bg) + "." :
            "The match in " + MatchName(bg) + " is over and your team " + (won ? "won." : "lost.");
        uint32 filed = 0;
        for (size_t index : AiChatPolicy::PickSpeakers(weights, weights.size(), rolls))
        {
            if (filed >= speakers)
                break;

            Ask ask;
            ask.bot = team.bots[index];
            ask.human = team.human;
            ask.kind = kind;
            ask.channel = ChatChannel::BATTLEGROUND;
            ask.message = moment;
            ask.canned = kind == Kind::BG_CAPTURE ? AiChatLines::Canned::BG_CAPTURE :
                (won ? AiChatLines::Canned::BG_WIN : AiChatLines::Canned::BG_LOSS);
            ask.extraDelayMs = urand(OPENER_EXTRA_MIN_MS, OPENER_EXTRA_MAX_MS) +
                (filed ? urand(SECOND_BG_SPEAKER_MIN_MS, SECOND_BG_SPEAKER_MAX_MS) : 0);
            ask.bgInstanceId = bg->GetInstanceID();
            if (Send(ask))
                ++filed;
        }

        SF_LOG_DEBUG("playerbots.ai", "[AICHAT] %s match %u: %s, %u of %u bot(s) speak", MatchName(bg).c_str(),
            bg->GetInstanceID(), AiChatPolicy::KindName(kind), filed, uint32(team.bots.size()));
    }

    // Once per team and match, the first bot to leave files the result.
    void FileMatchEnd(Player* bot, Battleground* bg)
    {
        AiChatPolicy::BgResult const result = AiChatPolicy::ResultFor(bg->GetWinner(), bot->GetBGTeam() == ALLIANCE);
        if (result == AiChatPolicy::BgResult::NONE)
            return;
        {
            std::lock_guard<std::mutex> guard(matchEndLock);
            if (!matchEnds.Claim(bg->GetInstanceID(), TeamIndexOf(bot), getMSTime()))
                return;
        }
        FileBgLines(bot, bg, result == AiChatPolicy::BgResult::WIN ? Kind::BG_WIN : Kind::BG_LOSS);
    }

    std::mutex captureLock;
    AiChatPolicy::BgCaptureBook captures;

    // The two flag battlegrounds count only flag captures in their team score.
    void ObserveCapture(Player* bot, Battleground* bg)
    {
        if ((bg->GetMapId() != 489 && bg->GetMapId() != 726) || bg->GetStatus() != STATUS_IN_PROGRESS)
            return;
        uint32 const team = TeamIndexOf(bot);
        uint32 const score = bg->GetTeamScore(team);
        bool captured;
        {
            std::lock_guard<std::mutex> guard(captureLock);
            captured = captures.Observe(bg->GetInstanceID(), team, score, getMSTime());
        }
        if (captured)
            FileBgLines(bot, bg, Kind::BG_CAPTURE);
    }

    // The bot still has its end-of-match line to say (and not for longer than the hold).
    bool EndLineOpen(Player* bot)
    {
        if (!openTickets.load(std::memory_order_relaxed))
            return false;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(ticketLock);
        auto const itr = tickets.find(bot->GetGUID());
        if (itr == tickets.end())
            return false;

        Ticket const& ticket = itr->second;
        bool const endLine = ticket.kind == Kind::BG_WIN || ticket.kind == Kind::BG_LOSS;
        return AiChatPolicy::HoldsLeave(endLine, getMSTimeDiff(ticket.createdMs, now));
    }

    std::string DuelMoment(Kind kind, std::string const& human, bool fled)
    {
        switch (kind)
        {
            case Kind::DUEL_WIN:
                return fled ? human + " ran out of the duel area, so you won the duel."
                            : "You just won a duel against " + human + ".";
            case Kind::DUEL_LOSS:
                return fled ? "You left the duel area, so " + human + " won the duel."
                            : human + " just beat you in a duel.";
            default:
                return {};
        }
    }
}

namespace AiChat
{
    void OnDuelAnswered(Player* bot, Player* challenger, bool accepted)
    {
        if (!ChatOn() || !BotAiOf(bot) || !IsHuman(challenger) || challenger == bot)
            return;

        Ask ask;
        ask.bot = bot;
        ask.human = challenger;
        ask.kind = Kind::DUEL_CHALLENGE;
        ask.channel = ChatChannel::SAY;
        ask.canned = accepted ? AiChatLines::Canned::DUEL_ACCEPT : AiChatLines::Canned::DUEL_DECLINE;
        ask.message = challenger->GetName() + (accepted ? " challenges you to a duel and you accept."
                                                        : " challenges you to a duel but you are hurt and decline.");
        Send(ask);
    }

    void OnDuelEnd(Player* winner, Player* loser, bool decided, bool fled)
    {
        if (!decided || !ChatOn() || !winner || !loser)
            return;

        Ask ask;
        ask.channel = ChatChannel::SAY;
        if (BotAiOf(winner) && IsHuman(loser))
        {
            ask.bot = winner;
            ask.human = loser;
            ask.kind = Kind::DUEL_WIN;
            ask.canned = AiChatLines::Canned::DUEL_WIN;
        }
        else if (BotAiOf(loser) && IsHuman(winner))
        {
            ask.bot = loser;
            ask.human = winner;
            ask.kind = Kind::DUEL_LOSS;
            ask.canned = AiChatLines::Canned::DUEL_LOSS;
        }
        else
            return;

        SendMemory(ask.bot, ask.human, ask.kind == Kind::DUEL_WIN ? AiChatPolicy::MemoryEvent::DUEL_WON
                                                                  : AiChatPolicy::MemoryEvent::DUEL_LOST);
        ask.message = DuelMoment(ask.kind, ask.human->GetName(), fled);
        Send(ask);
    }

    bool OnWhisper(Player* human, Player* bot, std::string const& message)
    {
        if (!ChatOn() || !IsHuman(human))
            return false;

        PlayerbotAI* botAI = BotAiOf(bot);
        if (!botAI)
            return false;

        NoteTone(bot, human, message);
        if (IsCommandFor(botAI, message))
            return false;

        // Small talk: there is no rule reply to it, so a failed request says a canned line.
        Ask ask;
        ask.bot = bot;
        ask.human = human;
        ask.kind = Kind::WHISPER_REPLY;
        ask.channel = ChatChannel::WHISPER;
        ask.canned = AiChatLines::Canned::WHISPER;
        SetHumanLine(ask, message);
        Send(ask);
        return true;
    }

    void OnSay(Player* human, std::string const& message)
    {
        if (!IsHuman(human) || !ChatOn() || !human->IsInWorld() ||
            AiChatPolicy::IsCommandLike(message, Syntax(), {}))
            return;

        // Only bots of the human's faction understand the line.
        std::vector<Player*> const candidates =
            NearbyBots(human, sWorld->GetFloatConfig(WorldFloatConfigs::CONFIG_LISTEN_RANGE_SAY));

        if (candidates.empty())
            return;

        // Who the line is for: the human's target first (the strongest signal), then a bot named in it, then the bot
        // the human is talking with; else the bot standing very close, else a talkative one. The chance comes from
        // that and the bot's personality.
        Player* speaker = TargetedIn(candidates, human);
        if (!speaker)
            speaker = NamedIn(candidates, message);

        // Conversation mode: the bot the human is talking with counts as named.
        if (!speaker)
            speaker = PartnerIn(candidates, human);

        bool const addressed = speaker != nullptr;
        if (addressed)
            NoteTone(speaker, human, message);

        if (!speaker)
            speaker = NearestWithin(candidates, human, AiChatPolicy::VERY_CLOSE_YARDS);

        if (!speaker)
            speaker = PickSpeaker(candidates);

        if (!speaker)
            return;

        AiChatPolicy::NearbyCue cue;
        cue.basePct = sAwareAiConfig.ChatNearbyChancePct;
        cue.archetype = ArchetypeOfBot(speaker);
        cue.named = addressed;
        cue.veryClose = human->GetDistance(speaker) <= AiChatPolicy::VERY_CLOSE_YARDS;
        if (urand(1, ROLL_MAX) > AiChatPolicy::NearbyChancePct(cue))
            return;

        Ask ask;
        ask.bot = speaker;
        ask.human = human;
        ask.kind = Kind::NEARBY_REPLY;
        ask.channel = ChatChannel::SAY;
        ask.canned = AiChatLines::Canned::NEARBY;
        SetHumanLine(ask, message);
        Send(ask);
    }

    void OnGroupChat(Group* group, Player* human, uint32 chatType, std::string const& message)
    {
        if (!group || group->isBGGroup() || !IsHuman(human) || !ChatOn())
            return;

        bool const raid = chatType == CHAT_MSG_RAID || chatType == CHAT_MSG_RAID_LEADER;
        if (!raid && chatType != CHAT_MSG_PARTY && chatType != CHAT_MSG_PARTY_LEADER)
            return;

        std::vector<Player*> candidates;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); member != human && BotAiOf(member))
                candidates.push_back(member);

        // The group's bots run every party line as a command too (mod_playerbots.cpp): a command gets no chat reply.
        if (candidates.empty() || IsCommandFor(BotAiOf(candidates.front()), message))
            return;

        // A bot named in the line, or the one the human is talking with, always answers.
        Player* speaker = NamedIn(candidates, message);
        if (!speaker)
            speaker = PartnerIn(candidates, human);

        if (speaker)
            NoteTone(speaker, human, message);

        if (!RollFor(Kind::PARTY_MOMENT, speaker != nullptr))
            return;

        Ask ask;
        ask.bot = speaker ? speaker : PickSpeaker(candidates);
        ask.human = human;
        ask.kind = Kind::PARTY_MOMENT;
        ask.channel = raid && group->isRaidGroup() ? ChatChannel::RAID : ChatChannel::PARTY;
        ask.canned = AiChatLines::Canned::PARTY;
        SetHumanLine(ask, message);
        Send(ask);
    }

    bool OnPartyMoment(PlayerbotAI* botAI, std::string const& moment, std::string const& fallback)
    {
        if (!Available() || !botAI)
            return false;

        Player* bot = botAI->GetBot();
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || group->isBGGroup() || !BotAiOf(bot))
            return false;

        // The human the moment is shared with: the bot's master when it is one, else the first human of the group.
        Player* human = nullptr;
        Player* master = botAI->GetMaster();
        if (IsHuman(master) && group->IsMember(master->GetGUID()))
            human = master;

        for (GroupReference* ref = group->GetFirstMember(); ref && !human; ref = ref->next())
            if (IsHuman(ref->GetSource()))
                human = ref->GetSource();

        if (!human)
            return false;

        Ask ask;
        ask.bot = bot;
        ask.human = human;
        ask.kind = Kind::PARTY_MOMENT;
        ask.channel = ChatChannel::PARTY;
        ask.message = moment;
        ask.fallback = fallback;
        return Send(ask);
    }

    void OnGuildChat(Player* human, std::string const& message)
    {
        uint32 const guildId = human ? human->GetGuildId() : 0;
        if (!guildId || !IsHuman(human) || !ChatOn())
            return;

        // SkyFire's Guild keeps its member list private, so the online bots of the guild are found through the sessions
        // (random and alt bots alike). Guild chat is read on the world thread, outside the map updates.
        std::vector<Player*> candidates;
        for (auto const& [accountId, session] : sWorld->GetAllSessions())
        {
            Player* player = session && session->IsBot() ? session->GetPlayer() : nullptr;
            if (player && player != human && player->GetGuildId() == guildId && BotAiOf(player))
                candidates.push_back(player);
        }

        if (candidates.empty() || IsCommandFor(BotAiOf(candidates.front()), message))
            return;

        uint32 const answerers = AiChatPolicy::GuildAnswerers(int32(sAwareAiConfig.ChatGuildBots.load()));
        std::vector<Player*> speakers;
        if (Player* named = NamedIn(candidates, message))
        {
            NoteTone(named, human, message);
            speakers.push_back(named);
            candidates.erase(std::find(candidates.begin(), candidates.end(), named));
        }

        while (speakers.size() < answerers && !candidates.empty())
        {
            Player* picked = PickSpeaker(candidates);
            if (!picked)
                break;

            speakers.push_back(picked);
            candidates.erase(std::find(candidates.begin(), candidates.end(), picked));
        }

        for (size_t i = 0; i < speakers.size(); ++i)
        {
            Ask ask;
            ask.bot = speakers[i];
            ask.human = human;
            ask.kind = Kind::GUILD_REPLY;
            ask.channel = ChatChannel::GUILD;
            ask.canned = AiChatLines::Canned::GUILD;
            SetHumanLine(ask, message);
            ask.extraDelayMs = i ? urand(SECOND_GUILD_BOT_MIN_MS, SECOND_GUILD_BOT_MAX_MS) : 0;
            ask.guildId = guildId;
            Send(ask);
        }
    }

    void Deliver(PlayerbotAI* botAI)
    {
        if (!openTickets.load(std::memory_order_relaxed))
            return;

        Player* bot = botAI->GetBot();
        uint64 const botGuid = bot->GetGUID();
        uint32 const now = getMSTime();
        std::optional<Ticket> due;
        std::optional<std::pair<uint64, AwareAiChatRequest>> request;
        {
            std::lock_guard<std::mutex> guard(ticketLock);
            auto const itr = tickets.find(botGuid);
            if (itr == tickets.end())
                return;

            Ticket& ticket = itr->second;
            uint32 const ageMs = getMSTimeDiff(ticket.createdMs, now);
            if (!ticket.sent)
            {
                // The gathering is over: the human's lines so far go out as one request.
                if (ageMs < ticket.sendAfterMs)
                    return;

                ticket.sent = true;
                ticket.roundStartMs = ageMs;
                request.emplace(ticket.id, ticket.request);
            }
            else if (!Advance(ticket, bot, ageMs))
                return;
            else if (ticket.timing != AiChatPolicy::Timing::NOW && ticket.timing != AiChatPolicy::Timing::SKIP &&
                     !ticket.followUps.empty() && !ticket.reasked &&
                     ticket.line.source != AiChatPolicy::Source::TEMPLATE)
            {
                // One late line: a held answer that collected more lines meanwhile is asked again with all of them,
                // once.
                ticket.lines.insert(ticket.lines.end(), ticket.followUps.begin(), ticket.followUps.end());
                ticket.followUps.clear();
                ticket.request.Message = AiChatPolicy::JoinLines(ticket.lines);
                ticket.request.Conversation = true;
                ticket.humanChars = CharsOf(ticket.lines);
                ticket.id = nextTicketId++;
                ticket.reasked = true;
                ticket.answered = false;
                ticket.chosen = false;
                ticket.source.clear();
                ticket.reason.clear();
                ticket.reply.clear();
                ticket.roundStartMs = ageMs;
                request.emplace(ticket.id, ticket.request);
            }
            else
            {
                due.emplace(std::move(ticket));
                tickets.erase(itr);
                --openTickets;
            }
        }

        if (request)
        {
            SendRequest(botGuid, request->first, request->second);
            return;
        }

        if (!due)
            return;

        uint32 const ageMs = getMSTimeDiff(due->createdMs, now);
        char const* outcome =
            due->timing == AiChatPolicy::Timing::SKIP ? "busy_in_combat" : SayLine(botAI, bot, *due, ageMs);
        char const* source = AiChatPolicy::SourceName(due->line.source);
        char const* kind = AiChatPolicy::KindName(due->kind);
        // A reply from the AI has no reason; a fallback keeps the sidecar's or the client's reason.
        std::string reason;
        if (due->line.source == AiChatPolicy::Source::RULES || due->line.source == AiChatPolicy::Source::CANNED)
            reason = due->reason.empty() ? std::string("empty_reply") : due->reason;
        sAwareAiRuntime.LogChat(kind, source, reason, outcome, ageMs);
        if (due->kind == Kind::FRIEND_WHISPER && std::string_view(outcome) == "said")
            NoteFriendWhisperSaid(due->humanGuid);
        SF_LOG_DEBUG("playerbots.ai", "[AICHAT] %s %s source=%s reason=%s timing=%s lines=%u outcome=%s after %u ms",
            bot->GetName().c_str(), kind, source, reason.empty() ? "-" : reason.c_str(),
            AiChatPolicy::TimingName(due->timing), uint32(due->lines.size()), outcome, ageMs);

        // The lines that came while it typed get one more answer, all of them in one request.
        if (!due->followUps.empty() && due->timing != AiChatPolicy::Timing::SKIP)
            FileFollowUp(bot, *due);
    }

    void OnGroupMemberAdded(Group* group, uint64 guid)
    {
        if (!ChatOn() || !group || group->isBGGroup() || group->isBFGroup())
            return;

        Player* joined = ObjectAccessor::FindPlayer(guid);
        bool const joinedHuman = IsHuman(joined);
        bool const joinedBot = !joinedHuman && BotAiOf(joined);
        if (!joinedHuman && !joinedBot)
            return;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == joined)
                continue;

            if (joinedHuman && BotAiOf(member))
                SendMemory(member, joined, AiChatPolicy::MemoryEvent::GROUPED);
            else if (joinedBot && IsHuman(member))
                SendMemory(joined, member, AiChatPolicy::MemoryEvent::GROUPED);
        }

        if (joinedHuman)
            GreetJoin(group, joined);
    }

    void OnBossKilled(Player* killer, Creature* boss)
    {
        if (!ChatOn() || !killer || !boss || !(boss->IsDungeonBoss() || boss->isWorldBoss()))
            return;

        Group* group = killer->GetGroup();
        if (!group || group->isBGGroup() || group->isBFGroup())
            return;

        std::string const bossName = boss->GetName();
        ForEachPairOnMap(group, boss->GetMap(), [&bossName](Player* bot, Player* human)
            { SendMemory(bot, human, AiChatPolicy::MemoryEvent::BOSS_KILLED_TOGETHER, bossName); });
    }

    void OnWipe(Player* bot)
    {
        if (!ChatOn() || !BotAiOf(bot))
            return;

        Group* group = bot->GetGroup();
        if (!group || group->isBGGroup())
            return;

        uint32 const now = getMSTime();
        {
            std::lock_guard<std::mutex> guard(memoryLock);
            if (wipeStamps.size() >= MEMORY_BOOK_TRIM_SIZE)
                wipeStamps.clear();

            auto const [itr, fresh] = wipeStamps.try_emplace(bot->GetGUID(), now);
            if (!fresh && getMSTimeDiff(itr->second, now) < WIPE_REPEAT_MS)
                return;

            itr->second = now;
        }

        Map* map = bot->GetMap();
        std::string const place = map && map->Instanceable() ? std::string(map->GetMapName()) : std::string();
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); IsHuman(member) && member->GetMap() == map)
                SendMemory(bot, member, AiChatPolicy::MemoryEvent::WIPE_TOGETHER, place);
    }

    void OnDungeonDone(Player* bot)
    {
        if (!ChatOn() || !BotAiOf(bot))
            return;

        Group* group = bot->GetGroup();
        if (!group)
            return;

        Map* map = bot->GetMap();
        std::string const dungeon = map && map->IsDungeon() ? std::string(map->GetMapName()) : std::string();
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            if (Player* member = ref->GetSource(); IsHuman(member))
                SendMemory(bot, member, AiChatPolicy::MemoryEvent::DUNGEON_DONE, dungeon);
    }

    void NoteLootRoll(Player* bot, Roll const* roll, ItemTemplate const* proto, bool wantedNeed, bool needs)
    {
        // The wish book also serves the liking score (loot ninja), which works with the AI layer off.
        if (!wantedNeed || !TracksLoot() || !roll || !proto || !BotAiOf(bot))
            return;

        // A human who passed or greeded while able to use it left it to the bot.
        uint64 passer = 0;
        if (needs)
            for (auto const& [guid, vote] : roll->playerVote)
            {
                if (vote != RollType::ROLL_PASS && vote != RollType::ROLL_GREED && vote != RollType::ROLL_DISENCHANT)
                    continue;

                Player* member = ObjectAccessor::FindPlayer(guid);
                if (IsHuman(member) && member->CanUseItem(proto) == EQUIP_ERR_OK)
                {
                    passer = guid;
                    break;
                }
            }

        uint32 const now = getMSTime();
        std::pair<uint64, uint32> const key{ bot->GetGUID(), proto->ItemId };
        std::lock_guard<std::mutex> guard(memoryLock);
        if (lootWishes.size() >= MEMORY_BOOK_TRIM_SIZE)
            lootWishes.clear();
        if (lootPasses.size() >= MEMORY_BOOK_TRIM_SIZE)
            lootPasses.clear();

        lootWishes[key] = now;
        if (passer)
            lootPasses[key] = { passer, now };
    }

    void OnLootRollWon(Player* bot, uint64 winnerGuid, uint32 itemId, uint8 rollType)
    {
        if (!TracksLoot() || !BotAiOf(bot))
            return;

        uint32 const now = getMSTime();
        bool wished = false;
        uint64 passer = 0;
        {
            std::pair<uint64, uint32> const key{ bot->GetGUID(), itemId };
            std::lock_guard<std::mutex> guard(memoryLock);
            if (auto const itr = lootWishes.find(key); itr != lootWishes.end())
            {
                wished = getMSTimeDiff(itr->second, now) < LOOT_NOTE_MS;
                lootWishes.erase(itr);
            }

            if (auto const itr = lootPasses.find(key); itr != lootPasses.end())
            {
                if (getMSTimeDiff(itr->second.second, now) < LOOT_NOTE_MS)
                    passer = itr->second.first;
                lootPasses.erase(itr);
            }
        }

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!wished || !proto || rollType != uint8(RollType::ROLL_NEED))
            return;

        if (winnerGuid == bot->GetGUID())
        {
            if (passer)
                SendMemory(bot, ObjectAccessor::FindPlayer(passer), AiChatPolicy::MemoryEvent::LOOT_PASSED,
                    proto->Name1);
            return;
        }

        Player* winner = ObjectAccessor::FindPlayer(winnerGuid);
        Group* group = bot->GetGroup();
        if (IsHuman(winner) && group && group->IsMember(winnerGuid) &&
            AiChatPolicy::IsOffArmorNeed(winner->getClass(), winner->getLevel(), proto->Class, proto->SubClass,
                proto->InventoryType))
        {
            LikingStore::NoteEvent(bot, winner, LikingPolicy::Event::LootNinja);
            SendMemory(bot, winner, AiChatPolicy::MemoryEvent::LOOT_NINJA, proto->Name1);
        }
    }

    bool OnOpener(PlayerbotAI* botAI, Player* human, std::string const& moment, std::string const& fallback)
    {
        if (!Available() || !botAI || !IsHuman(human))
            return false;

        Player* bot = botAI->GetBot();
        Group* group = bot ? bot->GetGroup() : nullptr;
        if (!group || group->isBGGroup() || !BotAiOf(bot) || !group->IsMember(human->GetGUID()))
            return false;

        Ask ask;
        ask.bot = bot;
        ask.human = human;
        ask.kind = Kind::OPENER;
        ask.channel = ChatChannel::PARTY;
        ask.message = moment;
        ask.fallback = fallback;
        return Send(ask);
    }

    void OnLevelUp(Player* player, uint8 oldLevel)
    {
        if (!ChatOn() || !sAwareAiConfig.ChatOpenersEnabled || !IsHuman(player) || !player->IsInWorld() ||
            player->getLevel() <= oldLevel)
            return;

        // Group mates near first, in party chat; else a bot standing near, in say.
        std::vector<Player*> candidates;
        ChatChannel channel = ChatChannel::SAY;
        Group* group = player->GetGroup();
        if (group && !group->isBGGroup() && !group->isBFGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                if (Player* member = ref->GetSource(); member != player && BotAiOf(member) && !member->IsInCombat() &&
                    member->GetMap() == player->GetMap() && member->IsWithinDistInMap(player, LEVEL_GRATS_GROUP_RANGE))
                    candidates.push_back(member);

            channel = group->isRaidGroup() ? ChatChannel::RAID : ChatChannel::PARTY;
        }

        if (candidates.empty())
        {
            channel = ChatChannel::SAY;
            for (Player* bot : NearbyBots(player, sWorld->GetFloatConfig(WorldFloatConfigs::CONFIG_LISTEN_RANGE_SAY)))
                if (!bot->IsInCombat())
                    candidates.push_back(bot);
        }

        if (candidates.empty() || !TakeOpener(player->GetGUID(), sAwareAiConfig.ChatOpenerLevelChancePct))
            return;

        if (Player* speaker = PickSpeaker(candidates))
            SendOpener(speaker, player, channel,
                player->GetName() + " just reached level " + std::to_string(player->getLevel()) + ".",
                AiChatLines::Canned::GRATS);
    }

    void OnHumanUpdate(Player* player, uint32 diff)
    {
        if (!ChatOn() || !IsHuman(player) || !player->IsInWorld())
            return;

        uint32 const duelCheckMs = sAwareAiConfig.ChatOpenerDuelAskCheckSeconds * IN_MILLISECONDS;
        uint32 const friendCheckMs = sAwareAiConfig.ChatFriendWhisperCheckSeconds * IN_MILLISECONDS;
        bool duelCheck = false;
        bool friendCheck = false;
        {
            std::lock_guard<std::mutex> guard(humanLock);
            HumanState& state = StateOf(player->GetGUID());
            state.duelAskTimerMs += diff;
            state.friendTimerMs += diff;
            if (state.duelAskTimerMs >= duelCheckMs)
            {
                state.duelAskTimerMs = 0;
                duelCheck = true;
            }

            if (state.friendTimerMs >= friendCheckMs)
            {
                state.friendTimerMs = 0;
                friendCheck = true;
            }
        }

        if (duelCheck)
            TryDuelAsk(player);

        if (friendCheck)
            TryFriendWhisper(player);
    }

    void OnHumanLogin(Player* player)
    {
        if (!IsHuman(player))
            return;

        std::lock_guard<std::mutex> guard(humanLock);
        humans[player->GetGUID()] = NewSession();
    }

    void OnHumanLogout(Player* player)
    {
        if (!player)
            return;

        std::lock_guard<std::mutex> guard(humanLock);
        humans.erase(player->GetGUID());
    }

    void OnBattlegroundUpdate(Player* bot)
    {
        if (!ChatOn() || !sAwareAiConfig.ChatBattlegroundEnabled || !BotAiOf(bot))
            return;
        if (Battleground* bg = bot->GetBattleground())
            ObserveCapture(bot, bg);
    }

    bool HoldForMatchEnd(Player* bot)
    {
        if (!ChatOn() || !sAwareAiConfig.ChatBattlegroundEnabled || !BotAiOf(bot))
            return false;

        Battleground* bg = bot->GetBattleground();
        if (!bg || bg->isArena() || bg->GetStatus() != STATUS_WAIT_LEAVE)
            return false;

        FileMatchEnd(bot, bg);
        return EndLineOpen(bot);
    }

    std::string WipeSidecar(std::optional<uint64> playerGuid)
    {
        if (!sAwareAiConfig.Enabled)
            return "AI layer is off: nothing was sent to the sidecar.";

        if (!LinkUp())
        {
            SF_LOG_WARN("playerbots.ai", "[AIWIPE] the sidecar link is down: its history was not wiped");
            return "The sidecar is not reachable: its own copy of the history was NOT wiped. Run the command again when it is up.";
        }

        AwareAiWipe wipe;
        wipe.PlayerGuid = playerGuid;
        bool const everyone = !playerGuid.has_value();
        bool const sent = sAwareAiClient.Send(AwareAiHttpMethod::Post, "/v1/memory/wipe", BuildWipeBody(wipe), WIPE_TIMEOUT,
            [everyone](AwareAiResponse response)
            {
                // Counts only: the log never names the human.
                if (!response.Ok)
                {
                    SF_LOG_WARN("playerbots.ai", "[AIWIPE] the sidecar did not confirm the wipe (%s %u): run the command again",
                        response.Error.c_str(), response.Status);
                    return;
                }

                AwareAiWipeResult const result = ParseWipeBody(response.Body).value_or(AwareAiWipeResult{});
                SF_LOG_INFO("playerbots.ai", "[AIWIPE] the sidecar wiped %s: %u memory events, %u chat turns, %u social events, %u pair notes, %u log lines",
                    everyone ? "everyone" : "one human", result.MemoryEvents, result.ChatTurns, result.SocialEvents,
                    result.PairNotes, result.LogLines);
            });
        if (!sent)
        {
            SF_LOG_WARN("playerbots.ai", "[AIWIPE] the sidecar client is busy: its history was not wiped");
            return "The sidecar client is busy: its own copy of the history was NOT wiped. Run the command again.";
        }

        SF_LOG_INFO("playerbots.ai", "[AIWIPE] asked the sidecar to forget %s", everyone ? "everyone" : "one human");
        return "Asked the sidecar to forget the history (the counts are in the server log, [AIWIPE]).";
    }
}
