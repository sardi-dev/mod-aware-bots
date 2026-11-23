/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Finder manners (see LfgMannersAction.h).
 */

#include "LfgMannersAction.h"

#include "AiChat.h"
#include "LikingStore.h"
#include "BotLfm.h"
#include "Config.h"
#include "Event.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LfgCrawlAction.h"
#include "LfgFill.h"
#include "LfgLeavePolicy.h"
#include "BotVoice.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Timer.h"

#include <atomic>
#include <ctime>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct Settings
    {
        bool enable;
        uint32 helloChance;     // percent of bots that greet
        uint32 questChance;     // percent of bots that would ask about quests (one per group asks)
        uint32 readyChance;
        uint32 thanksChance;
        uint32 groupGapMs;      // between two lines of the group
        uint32 botGapMs;        // between two lines of one bot
        uint32 leaveMinMs;      // after the final boss, the earliest a bot leaves
        uint32 leaveMaxMs;
        uint32 orphanMs;        // no player in the group for this long: leave
    };

    Settings const& Config()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enable = sConfigMgr->GetBoolDefault("AiPlayerbot.LfgManners.Enable", true);
            s.helloChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.HelloChance", 60))));
            s.questChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.QuestChance", 35))));
            s.readyChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.ReadyChance", 60))));
            s.thanksChance = uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.ThanksChance", 75))));
            s.groupGapMs = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.GroupGapSeconds", 6))) * IN_MILLISECONDS;
            s.botGapMs = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.BotGapSeconds", 20))) * IN_MILLISECONDS;
            s.leaveMinMs = uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.LeaveDelayMinSeconds", 12))) * IN_MILLISECONDS;
            s.leaveMaxMs = std::max(s.leaveMinMs,
                uint32(std::max(0, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.LeaveDelayMaxSeconds", 35))) * IN_MILLISECONDS);
            s.orphanMs = uint32(std::max(1, sConfigMgr->GetIntDefault("AiPlayerbot.LfgManners.OrphanLeaveSeconds", 90))) * IN_MILLISECONDS;
            return s;
        }();
        return settings;
    }

    // Lines are planned once when a bot comes into the dungeon, so the trigger and the action agree.
    uint32 const HELLO_MIN_MS = 1500;
    uint32 const HELLO_MAX_MS = 8000;
    uint32 const QUEST_MIN_MS = 12000;
    uint32 const QUEST_MAX_MS = 30000;
    uint32 const READY_MIN_MS = 15000;
    uint32 const READY_MAX_MS = 40000;
    uint32 const THANKS_MIN_MS = 3000;
    uint32 const THANKS_MAX_MS = 9000;
    // "Ready" needs health and mana at least this high, in percent.
    float const READY_PERCENT = 80.0f;
    // Entries of groups that were never cleaned up are dropped after this long.
    uint32 const GROUP_FORGET_MS = 3 * 60 * 60 * IN_MILLISECONDS;

    struct BotState
    {
        uint64 groupGuid = 0;
        bool entered = false;
        bool willHello = false, helloDone = false;
        bool willQuest = false, questDone = false;
        bool willReady = false, readyDone = false;
        bool finished = false;
        bool willThank = false, thanksDone = false;
        bool orphan = false;
        uint32 seenAt = 0;  // when this run of the bot in this group was first seen
        uint32 helloAt = 0, questAt = 0, readyAt = 0, thanksAt = 0, leaveAt = 0, orphanSince = 0;
        uint32 lastLineAt = 0;
        bool anyLine = false;
    };

    struct GroupState
    {
        uint32 lastLineAt = 0;
        bool anyLine = false;
        bool questAsked = false;
        uint32 touchedAt = 0;
    };

    std::mutex stateLock;
    std::unordered_map<uint32, BotState> botStates;
    std::unordered_map<uint64, GroupState> groupStates;
    std::atomic<uint32> tracked{ 0 };

    enum class Step
    {
        None,
        Hello,
        Quest,
        Ready,
        Thanks,
        LeaveWithBye,
        LeaveQuietly
    };

    // Milliseconds from an earlier getMSTime() to a later one (the clock wraps, the subtraction does not mind).
    uint32 Elapsed(uint32 earlier, uint32 later) { return later - earlier; }

    bool Reached(uint32 now, uint32 at) { return int32(now - at) >= 0; }

    bool Roll(uint32 percent) { return urand(1, 100) <= percent; }

    bool GroupHasPlayer(Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->GetSession() && !member->GetSession()->IsBot())
                return true;
        }
        return false;
    }

    // Both gaps hold: nobody else spoke a moment ago and this bot did not either.
    bool MayTalk(BotState const& bot, GroupState const& group, uint32 now, Settings const& s)
    {
        if (group.anyLine && Elapsed(group.lastLineAt, now) < s.groupGapMs)
            return false;
        return !bot.anyLine || Elapsed(bot.lastLineAt, now) >= s.botGapMs;
    }

    bool HealthAndManaFull(Player* bot)
    {
        if (bot->GetHealthPct() < READY_PERCENT)
            return false;
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        return !maxMana || float(bot->GetPower(POWER_MANA)) * 100.0f / float(maxMana) >= READY_PERCENT;
    }

    // Out of the fight and nothing left to loot.
    bool ReadyToLeave(PlayerbotAI* botAI, Player* bot)
    {
        return !bot->IsInCombat() && !botAI->GetAiObjectContext()->GetValue<bool>("has available loot")->Get();
    }

    // BT-206: the bot's own fight is over in all but the flag: no creature that has it as its victim stands at it, no
    // player or player's pet attacks it, and it took no damage for a while.
    bool FightIsOver(PlayerbotAI* botAI, Player* bot)
    {
        bool attackerAtHand = false;
        bool playerAttacker = false;
        for (Unit* attacker : bot->getAttackers())
        {
            if (!attacker || !attacker->IsAlive())
                continue;
            if (attacker->IsControlledByPlayer())
                playerAttacker = true;
            if (attacker->ToCreature() && attacker->GetVictim() == bot &&
                bot->GetDistance(attacker) <= LfgLeavePolicy::ATTACKER_AT_HAND_YARDS)
                attackerAtHand = true;
        }

        time_t const hurtAt = botAI->lastDamageTakenAt.load(std::memory_order_relaxed);
        time_t const now = time(nullptr);
        uint32 const quiet = !hurtAt ? std::numeric_limits<uint32>::max() : (now > hurtAt ? uint32(now - hurtAt) : 0);
        return LfgLeavePolicy::FightIsOver(attackerAtHand, playerAttacker, quiet);
    }

    void Forget(uint32 low)
    {
        std::lock_guard<std::mutex> guard(stateLock);
        botStates.erase(low);
        tracked.store(uint32(botStates.size()), std::memory_order_relaxed);
    }

    // reason (optional): why a leave step was chosen, for the [LFGLEAVE] line.
    Step Plan(PlayerbotAI* botAI, Player* bot, Group* group, char const** reason = nullptr)
    {
        char const* unusedReason = nullptr;
        char const*& why = reason ? *reason : unusedReason;
        Settings const& s = Config();
        uint32 const low = bot->GetGUIDLow();
        uint32 const now = getMSTime();
        bool const human = GroupHasPlayer(group);

        // Asked outside the lock: LfgFill and LfgCrawl have their own. A party the bots formed by hand (BotLfm) is no
        // finder group: it is a bot run as long as no player is in it, it is wrapped up after the same time limit as a
        // finder run (below), and it is finished when its leader says the final boss is dead (LfgCrawl).
        bool const partyRun = !group->isLFGGroup();
        bool const wrapUp = LfgFill::WrapUpRequested(low);
        bool const botRun = partyRun || LfgFill::IsBotRun(low);
        bool const finishedRun = partyRun ? LfgCrawl::PartyFinished(group->GetGUID()) :
            sLFGMgr->GetState(group->GetGUID()) == lfg::LFG_STATE_FINISHED_DUNGEON;
        Map* map = bot->GetMap();
        bool const inDungeon = map && map->IsDungeon();
        // BT-184: an open loot roll of the group holds the goodbye (LfgLeavePolicy.h).
        bool const rollOpen = finishedRun && group->isRollLootActive();
        // BT-206: a bot held in its combat engine ("lfg combat leave") plans only the leaves of a group without a player,
        // once its fight is over.
        bool const combatEngine = botAI->GetState() == BOT_STATE_COMBAT;
        bool const combatHold = combatEngine && !LfgLeavePolicy::MayLeaveFromCombat(human, !human && FightIsOver(botAI, bot));

        std::lock_guard<std::mutex> guard(stateLock);
        BotState& st = botStates[low];
        tracked.store(uint32(botStates.size()), std::memory_order_relaxed);
        if (st.groupGuid != group->GetGUID())
        {
            st = BotState();
            st.groupGuid = group->GetGUID();
            st.seenAt = now;
        }

        GroupState& gs = groupStates[group->GetGUID()];
        gs.touchedAt = now;
        if (groupStates.size() > 64)
            for (auto itr = groupStates.begin(); itr != groupStates.end();)
                itr = Elapsed(itr->second.touchedAt, now) > GROUP_FORGET_MS ? groupStates.erase(itr) : std::next(itr);

        if (LfgLeavePolicy::MayTimeOut(wrapUp, human))
        {
            if (combatHold)
                return Step::None;
            why = "bot run wrapped up";
            return Step::LeaveQuietly;
        }

        // A manually assembled bot-only party has the same time limit as a finder bot run.
        if (partyRun && LfgLeavePolicy::MayTimeOut(
                Elapsed(st.seenAt, now) >= LfgFill::MaxRunSecondsFor(bot->GetMapId()) * IN_MILLISECONDS, human))
        {
            if (combatHold)
                return Step::None;
            why = "bot party over its time limit";
            return Step::LeaveQuietly;
        }

        // A group the player left (or never had one): nothing keeps this bot there, unless it is a bot-only run.
        if (!human && !botRun)
        {
            if (!st.orphan)
            {
                st.orphan = true;
                st.orphanSince = now;
            }
            else if (Elapsed(st.orphanSince, now) >= s.orphanMs)
            {
                if (combatHold)
                    return Step::None;
                why = "no player in the group";
                return Step::LeaveQuietly;
            }
        }
        else
            st.orphan = false;

        // Thanks, the goodbye after the last boss and greetings wait until the bot is out of its combat engine.
        if (combatEngine)
            return Step::None;

        if (finishedRun)
        {
            if (!st.finished)
            {
                st.finished = true;
                // Design decision 0031: a run finished together goes to the AI layer's memory (it sends only to humans).
                if (human)
                {
                    AiChat::OnDungeonDone(bot);
                    LikingStore::OnDungeonDone(bot);  // liking score of the bot for the humans of the group
                }
                st.willThank = human && Roll(s.thanksChance);
                st.thanksAt = now + urand(THANKS_MIN_MS, THANKS_MAX_MS);
                st.leaveAt = now + urand(s.leaveMinMs, s.leaveMaxMs);
            }

            if (st.willThank && !st.thanksDone && Reached(now, st.thanksAt))
            {
                if (MayTalk(st, gs, now, s))
                    return Step::Thanks;
            }

            bool const timeReached = Reached(now, st.leaveAt);
            if (LfgLeavePolicy::MayLeave(timeReached, timeReached && ReadyToLeave(botAI, bot),
                                         timeReached ? Elapsed(st.leaveAt, now) : 0, rollOpen))
            {
                why = "run finished";
                return human ? Step::LeaveWithBye : Step::LeaveQuietly;
            }

            return Step::None;
        }

        // Greetings only inside the dungeon and only with a player to greet.
        if (!human || !inDungeon)
            return Step::None;

        if (!st.entered)
        {
            st.entered = true;
            st.willHello = Roll(s.helloChance);
            st.helloAt = now + urand(HELLO_MIN_MS, HELLO_MAX_MS);
            st.willQuest = Roll(s.questChance);
            st.questAt = now + urand(QUEST_MIN_MS, QUEST_MAX_MS);
            st.willReady = Roll(s.readyChance);
            st.readyAt = now + urand(READY_MIN_MS, READY_MAX_MS);
        }

        if (!MayTalk(st, gs, now, s))
            return Step::None;

        if (st.willHello && !st.helloDone && Reached(now, st.helloAt))
            return Step::Hello;

        if (st.willQuest && !st.questDone && !gs.questAsked && Reached(now, st.questAt))
            return Step::Quest;

        if (st.willReady && !st.readyDone && Reached(now, st.readyAt) && !bot->IsInCombat() && HealthAndManaFull(bot))
            return Step::Ready;

        return Step::None;
    }

    // Marks the step as said and starts both gaps; false when another bot took it meanwhile.
    bool Commit(Player* bot, Group* group, Step step)
    {
        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);
        BotState& st = botStates[bot->GetGUIDLow()];
        GroupState& gs = groupStates[group->GetGUID()];

        switch (step)
        {
            case Step::Hello:
                st.helloDone = true;
                break;
            case Step::Quest:
                st.questDone = true;
                if (gs.questAsked)
                    return false;
                gs.questAsked = true;
                break;
            case Step::Ready:
                st.readyDone = true;
                break;
            case Step::Thanks:
                st.thanksDone = true;
                break;
            default:
                break;
        }

        st.lastLineAt = gs.lastLineAt = now;
        st.anyLine = gs.anyLine = true;
        return true;
    }

    std::vector<std::string> const& Defaults(Step step)
    {
        static std::vector<std::string> const hello = { "hi", "hello", "hey all", "hi all", "Hi!" };
        static std::vector<std::string> const quest = { "anyone need a quest in here?", "does anybody have a quest for this place?",
                                                         "need to do any quests in here?" };
        static std::vector<std::string> const ready = { "ready when you are", "ready", "rdy", "good to go" };
        static std::vector<std::string> const thanks = { "thanks all", "ty everyone", "thanks for the run", "gg, thanks" };
        static std::vector<std::string> const bye = { "bye!", "cya", "see you around", "gl with everything, bye" };

        switch (step)
        {
            case Step::Hello:
                return hello;
            case Step::Quest:
                return quest;
            case Step::Ready:
                return ready;
            case Step::Thanks:
                return thanks;
            default:
                return bye;
        }
    }

    char const* TextName(Step step)
    {
        switch (step)
        {
            case Step::Hello:
                return "lfg_hello";
            case Step::Quest:
                return "lfg_quest";
            case Step::Ready:
                return "lfg_ready";
            case Step::Thanks:
                return "lfg_thanks";
            default:
                return "lfg_bye";
        }
    }

    // One line to the party through the bot voice (personality variants "<name>_<personality>", then the text table row
    // "<name>", then a variant from the code). The goodbye is a notice (always said), the rest is chatter that a quiet
    // personality holds back more often.
    void Say(PlayerbotAI* botAI, Step step)
    {
        std::vector<std::string> const& defaults = Defaults(step);
        bool const notice = step == Step::LeaveWithBye;
        BotVoice::Say(botAI, TextName(step), defaults[urand(0, uint32(defaults.size() - 1))], {},
                      BotVoice::Channel::PARTY, notice ? BotVoice::Priority::NOTICE : BotVoice::Priority::CHATTER, 100);
    }

    // Out of the group: the core takes the bot out of the dungeon (LFGGroupScript::OnRemoveMember), the bot goes back to
    // being a free random bot.
    void LeaveGroup(PlayerbotAI* botAI, Player* bot)
    {
        if (bot->GetGroup())
            bot->RemoveFromGroup();

        botAI->SetMaster(nullptr);
        botAI->ResetStrategies();
        botAI->Reset();
        Forget(bot->GetGUIDLow());
    }

    bool Eligible(Player* bot, Group*& group)
    {
        if (!Config().enable || !bot->IsInWorld() || !sRandomPlayerbotMgr->IsRandomBot(bot))
            return false;

        group = bot->GetGroup();
        if (!group)
            return false;

        if (group->isLFGGroup())
            return true;

        // A party the bots formed by hand (BotLfm), once it has walked into its dungeon.
        Map* map = bot->GetMap();
        return map && map->IsDungeon() && BotLfm::InsideParty(bot);
    }
}  // namespace

bool LfgManners::Due(PlayerbotAI* botAI, Player* bot)
{
    Group* group = nullptr;
    if (!Eligible(bot, group))
    {
        if (tracked.load(std::memory_order_relaxed) && !bot->GetGroup())
            Forget(bot->GetGUIDLow());
        return false;
    }

    return bot->IsAlive() && Plan(botAI, bot, group) != Step::None;
}

bool LfgMannersAction::Execute(Event /*event*/)
{
    Group* group = nullptr;
    if (!Eligible(bot, group) || !bot->IsAlive())
        return false;

    char const* reason = nullptr;
    Step const step = Plan(botAI, bot, group, &reason);
    if (step == Step::LeaveWithBye || step == Step::LeaveQuietly)
        SF_LOG_INFO("playerbots", "[LFGLEAVE] %s leaves group %u (%s%s, finder group %u, players online %u)",
            bot->GetName().c_str(), group->GetLowGUID(), reason ? reason : "unknown",
            botAI->GetState() == BOT_STATE_COMBAT ? ", flagged in combat" : "", uint32(group->isLFGGroup()),
            uint32(GroupHasPlayer(group)));
    switch (step)
    {
        case Step::None:
            return false;
        case Step::LeaveWithBye:
            Say(botAI, step);
            LeaveGroup(botAI, bot);
            return true;
        case Step::LeaveQuietly:
            LeaveGroup(botAI, bot);
            return true;
        default:
            if (!Commit(bot, group, step))
                return false;

            Say(botAI, step);
            return true;
    }
}
