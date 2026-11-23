/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See DungeonChat.h for what this is.
 */

#include "DungeonChat.h"

#include "BotLfm.h"
#include "BotVoice.h"
#include "Config.h"
#include "Creature.h"
#include "EmoteAction.h"
#include "Event.h"
#include "Group.h"
#include "HealerGuardrails.h"
#include "Map.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "Random.h"
#include "SharedDefines.h"
#include "Timer.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    // One moment or one answer. The text table key and the emote of each are in KindOf().
    enum class Kind : uint8
    {
        ENTER,
        ROLE,
        BOSS_PULL,
        BOSS_KILL,
        DEATH_OTHER,
        DEATH_SELF,
        LONG_FIGHT,
        LOW_MANA,
        DRINK,
        LOOT_NEED,
        LOOT_GREED,
        END,
        IDLE,
        WAITING,
        REPLY_READY,
        REPLY_NOT_READY,
        REPLY_WAIT,
        REPLY_THANKS,
        REPLY_LAUGH,
        REPLY_PRAISE,
        REPLY_GREET,
        REPLY_SORRY,
        REPLY_BYE,
        NONE
    };

    enum Role : uint8
    {
        ROLE_TANK,
        ROLE_HEALER,
        ROLE_DAMAGE
    };

    struct Settings
    {
        bool enabled;
        bool replies;
        uint32 groupGapMs;
        uint32 botGapMs;
        uint32 replyGapMs;
        uint32 emoteChance;
        uint32 enterChance;
        uint32 roleChance;
        uint32 bossPullChance;
        uint32 bossKillChance;
        uint32 deathChance;
        uint32 selfDeathChance;
        uint32 longFightMs;
        uint32 longFightChance;
        uint32 lowManaChance;
        uint32 healerDrinkPct;
        uint32 drinkPct;
        uint32 lootChance;
        uint32 endChance;
        uint32 idleChance;
        uint32 idleMinMs;
        uint32 idleMaxMs;
        uint32 waitingMs;
        uint32 waitingChance;
        uint32 replyChance;
    };

    uint32 Percent(char const* key, int32 fallback)
    {
        return uint32(std::min(100, std::max(0, sConfigMgr->GetIntDefault(key, fallback))));
    }

    uint32 Millis(char const* key, int32 fallbackSeconds)
    {
        return uint32(std::max(0, sConfigMgr->GetIntDefault(key, fallbackSeconds))) * IN_MILLISECONDS;
    }

    Settings const& Cfg()
    {
        static Settings const settings = []
        {
            Settings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.DungeonChat.Enable", true);
            s.replies = sConfigMgr->GetBoolDefault("AiPlayerbot.DungeonChat.Replies", true);
            s.groupGapMs = Millis("AiPlayerbot.DungeonChat.GroupGapSeconds", 12);
            s.botGapMs = Millis("AiPlayerbot.DungeonChat.BotGapSeconds", 45);
            s.replyGapMs = Millis("AiPlayerbot.DungeonChat.ReplyGapSeconds", 8);
            s.emoteChance = Percent("AiPlayerbot.DungeonChat.EmoteChance", 60);
            s.enterChance = Percent("AiPlayerbot.DungeonChat.EnterChance", 70);
            s.roleChance = Percent("AiPlayerbot.DungeonChat.RoleChance", 50);
            s.bossPullChance = Percent("AiPlayerbot.DungeonChat.BossPullChance", 45);
            s.bossKillChance = Percent("AiPlayerbot.DungeonChat.BossKillChance", 85);
            s.deathChance = Percent("AiPlayerbot.DungeonChat.DeathChance", 40);
            s.selfDeathChance = Percent("AiPlayerbot.DungeonChat.SelfDeathChance", 35);
            s.longFightMs = Millis("AiPlayerbot.DungeonChat.LongFightSeconds", 60);
            s.longFightChance = Percent("AiPlayerbot.DungeonChat.LongFightChance", 60);
            s.lowManaChance = Percent("AiPlayerbot.DungeonChat.LowManaChance", 70);
            s.healerDrinkPct = Percent("AiPlayerbot.DungeonChat.HealerDrinkPercent", 50);
            s.drinkPct = Percent("AiPlayerbot.DungeonChat.DrinkPercent", 50);
            s.lootChance = Percent("AiPlayerbot.DungeonChat.LootChance", 12);
            s.endChance = Percent("AiPlayerbot.DungeonChat.EndChance", 70);
            s.idleChance = Percent("AiPlayerbot.DungeonChat.IdleChance", 35);
            s.idleMinMs = Millis("AiPlayerbot.DungeonChat.IdleMinSeconds", 90);
            s.idleMaxMs = std::max(s.idleMinMs, Millis("AiPlayerbot.DungeonChat.IdleMaxSeconds", 210));
            s.waitingMs = Millis("AiPlayerbot.DungeonChat.WaitingSeconds", 90);
            s.waitingChance = Percent("AiPlayerbot.DungeonChat.WaitingChance", 60);
            s.replyChance = Percent("AiPlayerbot.DungeonChat.ReplyChance", 85);
            return s;
        }();
        return settings;
    }

    // The timings of a run. The first lines come after the finder manners' greetings (1.5 to 8 s).
    uint32 const EVAL_GAP_MS = 1000;
    uint32 const ENTER_MIN_MS = 9000;
    uint32 const ENTER_MAX_MS = 18000;
    uint32 const ROLE_MIN_MS = 14000;
    uint32 const ROLE_MAX_MS = 26000;
    uint32 const ROLE_STEP_MIN_MS = 12000;
    uint32 const ROLE_STEP_MAX_MS = 22000;
    uint32 const REACT_MIN_MS = 900;
    uint32 const REACT_MAX_MS = 2800;
    uint32 const KILL_MIN_MS = 1800;
    uint32 const KILL_MAX_MS = 4200;
    uint32 const REPLY_MIN_MS = 1200;
    uint32 const REPLY_MAX_MS = 3500;
    // A planned line is dropped when it was not said this long after it was due (the bot died, was busy).
    uint32 const PENDING_GRACE_MS = 8000;
    // A boss that left the fight alive (reset) is forgotten after this long without combat.
    uint32 const BOSS_RESET_MS = 20000;
    // Idle jokes need this long without combat, and a short run is not worth a goodbye.
    uint32 const IDLE_QUIET_MS = 20000;
    uint32 const FIRST_IDLE_QUIET_MS = 45000;
    uint32 const IDLE_RETRY_MS = 10000;
    // A line that is not about the fight waits for the fight to end: this often, this long each time.
    uint8 const COMBAT_POSTPONES = 5;
    uint32 const POSTPONE_MIN_MS = 4000;
    uint32 const POSTPONE_MAX_MS = 8000;
    uint32 const MIN_RUN_FOR_GOODBYE_MS = 3 * 60 * IN_MILLISECONDS;
    uint32 const COMBAT_END_MS = 8000;
    uint32 const LOW_MANA_COMBAT_MIN_MS = 15000;
    uint32 const MANA_REPEAT_MS = 100000;
    // A roll for a mat or a greed is only remarked on for items of at least this quality.
    uint32 const LOOT_REMARK_QUALITY = ITEM_QUALITY_UNCOMMON;
    uint32 const MIN_READY_HEALTH_PCT = 70;
    uint32 const MIN_READY_MANA_PCT = 60;
    uint32 const GROUP_FORGET_MS = 3 * 60 * 60 * IN_MILLISECONDS;
    size_t const TRIM_SIZE = 64;
    size_t const BOT_TRIM_SIZE = 512;
    size_t const MAX_REMEMBERED_BOSSES = 32;
    size_t const MAX_MESSAGE_LENGTH = 60;

    struct KindInfo
    {
        char const* key;
        uint32 emote;
        std::vector<char const*> defaults;
    };

    // The in-code fallback, used when the text table has no row of the name (the SQL file is not applied yet).
    KindInfo const& KindOf(Kind kind)
    {
        static std::vector<KindInfo> const infos = {
            { "dgn_enter", TEXT_EMOTE_WAVE, { "Alright, %dungeon. Stay together and watch the patrols",
                "Here we go, %dungeon. Tell me when you are ready", "In we go. Buffs are up, so whenever you are" } },
            { "dgn_role", 0, { "Checking in. Let me know when to start" } },
            { "dgn_boss_pull", 0, { "Here comes %boss, get ready!", "%boss engaged, watch your positions",
                "Boss fight, stay sharp everyone" } },
            { "dgn_boss_kill", TEXT_EMOTE_CHEER, { "%boss is down, nice work", "That is %boss done. Good fight everyone",
                "Down it goes, well played" } },
            { "dgn_death_other", 0, { "%name went down, we will get you up", "Ouch, %name is down. Careful out there",
                "Lost %name, let us keep it tight" } },
            { "dgn_death_self", TEXT_EMOTE_SIGH, { "Well, that did not go to plan", "I regret that last pull",
                "Back on my feet, sorry about that" } },
            { "dgn_long_fight", 0, { "This fight is dragging on, keep it up", "Still going, stay focused",
                "That is a long one, hang in there" } },
            { "dgn_low_mana", 0, { "Running low on mana, bear with me", "My mana is getting low, careful with pulls",
                "Low on mana here, I will need a moment" } },
            { "dgn_drink", TEXT_EMOTE_DRINK, { "Taking a quick drink, one moment please", "Stopping to drink, please wait a moment",
                "Need to drink before the next pull" } },
            { "dgn_loot_need", 0, { "Rolling need on %item, it is an upgrade for me", "I will need %item, it fits me well",
                "Need on %item, sorry if I take it" } },
            { "dgn_loot_greed", 0, { "Greed on %item, I will not fight over it", "Just greed on %item, it is not for me",
                "Greeding %item, it can be sold or disenchanted" } },
            { "dgn_end", TEXT_EMOTE_BYE, { "That was a good run, thanks everyone", "Thanks for the group, see you around",
                "Good run all, take care" } },
            { "dgn_idle", TEXT_EMOTE_LAUGH, { "I swear that last corridor was longer yesterday", "I hope the next pack is a small one",
                "Anyone else hungry or is it just me?" } },
            { "dgn_waiting", 0, { "all good? ready when you are", "we waiting for something?", "no rush, say when" } },
            { "dgn_reply_ready", TEXT_EMOTE_READY, { "Ready", "Ready when you are", "All set here" } },
            { "dgn_reply_not_ready", 0, { "Not quite yet, give me a moment", "One moment, still recovering",
                "Almost ready, please hold on a second" } },
            { "dgn_reply_wait", 0, { "No problem, we will wait", "Sure, take your time", "Okay, holding here" } },
            { "dgn_reply_thanks", 0, { "No problem", "You are welcome", "Anytime" } },
            { "dgn_reply_laugh", TEXT_EMOTE_LAUGH, { "Haha", "Ha, good one", "Heh" } },
            { "dgn_reply_praise", TEXT_EMOTE_BOW, { "Thanks, great teamwork", "Thank you, you did well too", "Team effort" } },
            { "dgn_reply_greet", TEXT_EMOTE_WAVE, { "Hello", "Hi there", "Hey" } },
            { "dgn_reply_sorry", 0, { "No worries", "It happens", "Do not worry about it" } },
            { "dgn_reply_bye", TEXT_EMOTE_BYE, { "Bye", "See you around", "Take care" } },
        };
        return infos[uint32(kind)];
    }

    struct Pending
    {
        uint32 botLow = 0;
        uint32 groupLow = 0;
        Kind kind = Kind::NONE;
        Role role = ROLE_DAMAGE;
        uint32 dueAt = 0;
        uint8 postponed = 0;
        bool speak = true;
        std::string name;   // %name: the player who died
        std::string boss;   // %boss
        std::string item;   // %item
    };

    struct GroupState
    {
        uint64 instanceKey = 0;
        uint32 touchedAt = 0;
        uint32 evalAt = 0;
        uint32 runStartAt = 0;
        bool active = false;
        bool managed = false;     // a finder group or a bot-formed party: their own manners greet and say goodbye
        bool endDone = false;
        bool enterDone = false;
        uint32 enterAt = 0;
        uint8 roleStage = 0;      // 0: the tank introduces itself, 1: the healer, 2: done
        uint32 roleAt = 0;
        bool hasBusy = false;
        uint32 busyUntil = 0;
        bool hasReplyBusy = false;
        uint32 replyBusyUntil = 0;
        bool inCombat = false;
        uint32 combatSince = 0;
        uint32 lastCombatAt = 0;
        bool longFightSaid = false;
        uint64 boss = 0;
        std::string bossName;
        uint32 bossSeenAt = 0;
        std::vector<uint64> pulled;
        std::vector<uint64> killed;
        std::vector<uint32> deadSeen;
        bool wiped = false;
        uint32 nextIdleAt = 0;
        bool leaderStill = false;  // the player stands still out of combat since stillSince
        uint32 stillSince = 0;
        bool waitingSaid = false;  // one question per pause
    };

    std::mutex stateLock;
    std::unordered_map<uint32, GroupState> groups;
    std::unordered_map<uint32, uint32> botSpokeAt;
    std::unordered_map<uint32, uint32> botManaAt;
    std::vector<Pending> pendings;

    // Milliseconds from an earlier getMSTime() to a later one (the clock wraps, the subtraction does not mind).
    uint32 Elapsed(uint32 earlier, uint32 later) { return later - earlier; }

    bool Passed(uint32 now, uint32 at) { return int32(now - at) >= 0; }

    bool Roll(uint32 percent) { return urand(1, 100) <= percent; }

    bool Remember(std::vector<uint64>& list, uint64 guid)
    {
        if (std::find(list.begin(), list.end(), guid) != list.end())
            return false;

        if (list.size() >= MAX_REMEMBERED_BOSSES)
            list.erase(list.begin());
        list.push_back(guid);
        return true;
    }

    bool Busy(GroupState const& gs, uint32 now) { return gs.hasBusy && !Passed(now, gs.busyUntil); }

    Role RoleOf(Player* player)
    {
        if (PlayerBotSpec::IsTank(player, true))
            return ROLE_TANK;
        if (PlayerBotSpec::IsHeal(player, true))
            return ROLE_HEALER;
        return ROLE_DAMAGE;
    }

    // The people of the group that stand on the dungeon's map.
    struct Roster
    {
        Map* map = nullptr;
        Player* human = nullptr;
        std::vector<Player*> members;
        std::vector<Player*> speakers;  // alive random bots that have a player as master
        bool anyCombat = false;
    };

    void Collect(Group* group, Map* map, Roster& roster)
    {
        roster.map = map;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsInWorld() || member->GetMap() != map)
                continue;

            roster.members.push_back(member);
            if (member->IsInCombat())
                roster.anyCombat = true;

            if (member->GetSession() && !member->GetSession()->IsBot())
            {
                // The leader when there are two players: the waiting question is about who sets the pace.
                if (!roster.human || member->GetGUID() == group->GetLeaderGUID())
                    roster.human = member;
                continue;
            }

            PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
            if (memberAI && member->IsAlive() && sRandomPlayerbotMgr->IsRandomBot(member) && memberAI->HasRealPlayerMaster())
                roster.speakers.push_back(member);
        }
    }

    // A bot out of the candidates, weighted by how talkative its personality is. roleOnly picks only a bot of that role.
    // honourBotGap leaves out a bot that spoke a moment ago. exclude is a bot guid that must not speak (0 for none).
    Player* ChooseSpeaker(std::vector<Player*> const& candidates, uint32 now, bool honourBotGap, int roleOnly, uint32 exclude)
    {
        uint32 total = 0;
        std::vector<std::pair<Player*, uint32>> weighted;
        for (Player* candidate : candidates)
        {
            if (candidate->GetGUIDLow() == exclude)
                continue;

            if (roleOnly >= 0 && RoleOf(candidate) != Role(roleOnly))
                continue;

            if (honourBotGap)
            {
                auto const itr = botSpokeAt.find(candidate->GetGUIDLow());
                if (itr != botSpokeAt.end() && Elapsed(itr->second, now) < Cfg().botGapMs)
                    continue;
            }

            uint32 const weight = std::max<uint32>(1, BotVoice::ChattinessPercent(candidate));
            weighted.emplace_back(candidate, weight);
            total += weight;
        }

        if (!total)
            return nullptr;

        uint32 pick = urand(1, total);
        for (auto const& entry : weighted)
        {
            if (pick <= entry.second)
                return entry.first;
            pick -= entry.second;
        }
        return weighted.back().first;
    }

    enum class Scheduled
    {
        YES,
        ROLLED_OUT,   // the roll failed: the moment is over
        BUSY,         // the group spoke a moment ago
        NO_SPEAKER
    };

    struct Plan
    {
        Kind kind = Kind::NONE;
        uint32 chance = 100;
        uint32 delayMin = REACT_MIN_MS;
        uint32 delayMax = REACT_MAX_MS;
        bool ignoreGap = false;       // the group gap does not hold it back (a boss kill is rare and always welcome)
        bool honourBotGap = true;
        int roleOnly = -1;
        Player* forced = nullptr;     // this bot speaks, nobody else
        uint32 exclude = 0;
        Role role = ROLE_DAMAGE;
        std::string name, boss, item;
    };

    // Called with stateLock held. One roll and one speaker for the whole group.
    Scheduled Schedule(GroupState& gs, std::vector<Player*> const& candidates, Plan const& plan, uint32 now)
    {
        Settings const& s = Cfg();
        if (!plan.ignoreGap && Busy(gs, now))
            return Scheduled::BUSY;

        Player* speaker = plan.forced ? plan.forced :
            ChooseSpeaker(candidates, now, plan.honourBotGap, plan.roleOnly, plan.exclude);
        if (!speaker)
            return Scheduled::NO_SPEAKER;

        if (!Roll(plan.chance))
            return Scheduled::ROLLED_OUT;

        Pending pending;
        pending.botLow = speaker->GetGUIDLow();
        pending.groupLow = 0;
        if (Group* group = speaker->GetGroup())
            pending.groupLow = group->GetLowGUID();
        pending.kind = plan.kind;
        pending.role = plan.roleOnly >= 0 ? Role(plan.roleOnly) : plan.role;
        pending.dueAt = now + urand(plan.delayMin, plan.delayMax);
        pending.name = plan.name;
        pending.boss = plan.boss;
        pending.item = plan.item;
        pendings.push_back(pending);

        gs.hasBusy = true;
        gs.busyUntil = pending.dueAt + s.groupGapMs;
        return Scheduled::YES;
    }

    void Plain(Plan& plan, Kind kind, uint32 chance)
    {
        plan.kind = kind;
        plan.chance = chance;
    }

    // The first living dungeon boss that fights a member of the group.
    Creature* FindEngagedBoss(Roster const& roster)
    {
        for (Player* member : roster.members)
        {
            if (!member->IsInCombat())
                continue;

            if (Unit* victim = member->GetVictim())
                if (Creature* creature = victim->ToCreature())
                    if (creature->IsAlive() && creature->IsDungeonBoss())
                        return creature;

            for (Unit* attacker : member->getAttackers())
                if (Creature* creature = attacker ? attacker->ToCreature() : nullptr)
                    if (creature->IsAlive() && creature->IsDungeonBoss())
                        return creature;
        }
        return nullptr;
    }

    uint32 ManaPercent(Player* bot)
    {
        uint32 const maxMana = bot->GetMaxPower(POWER_MANA);
        return maxMana ? uint32(uint64(bot->GetPower(POWER_MANA)) * 100 / maxMana) : 100;
    }

    void TrimBotMap(std::unordered_map<uint32, uint32>& map, uint32 now, uint32 maxAgeMs)
    {
        if (map.size() < BOT_TRIM_SIZE)
            return;

        for (auto itr = map.begin(); itr != map.end();)
            itr = Elapsed(itr->second, now) > maxAgeMs ? map.erase(itr) : std::next(itr);
    }

    // The deaths and revivals of the group. A wipe (everyone dead) is left to the bot voice's own wipe line.
    void WatchDeaths(GroupState& gs, Roster const& roster, uint32 now)
    {
        Settings const& s = Cfg();
        Player* newlyDead = nullptr;
        uint32 deadNow = 0;
        for (Player* member : roster.members)
        {
            if (member->IsAlive())
                continue;

            ++deadNow;
            if (std::find(gs.deadSeen.begin(), gs.deadSeen.end(), member->GetGUIDLow()) == gs.deadSeen.end())
            {
                gs.deadSeen.push_back(member->GetGUIDLow());
                newlyDead = member;
            }
        }

        if (roster.members.size() >= 2 && deadNow == roster.members.size())
            gs.wiped = true;

        for (auto itr = gs.deadSeen.begin(); itr != gs.deadSeen.end();)
        {
            Player* member = nullptr;
            for (Player* candidate : roster.members)
                if (candidate->GetGUIDLow() == *itr)
                    member = candidate;

            // Left the map or logged out: not tracked any more. Still dead: keep waiting.
            if (!member)
            {
                itr = gs.deadSeen.erase(itr);
                continue;
            }

            if (!member->IsAlive() || member->IsInCombat())
            {
                ++itr;
                continue;
            }

            if (!gs.wiped && std::find(roster.speakers.begin(), roster.speakers.end(), member) != roster.speakers.end())
            {
                Plan plan;
                Plain(plan, Kind::DEATH_SELF, s.selfDeathChance);
                plan.forced = member;
                plan.delayMin = 2500;
                plan.delayMax = 6000;
                Schedule(gs, roster.speakers, plan, now);
            }
            itr = gs.deadSeen.erase(itr);
        }

        if (gs.deadSeen.empty())
            gs.wiped = false;

        if (newlyDead && !gs.wiped)
        {
            Plan plan;
            Plain(plan, Kind::DEATH_OTHER, s.deathChance);
            plan.name = newlyDead->GetName();
            plan.exclude = newlyDead->GetGUIDLow();
            Schedule(gs, roster.speakers, plan, now);
        }
    }

    // Boss pull and kill, and the length of the fight.
    void WatchFights(GroupState& gs, Roster const& roster, uint32 now)
    {
        Settings const& s = Cfg();

        if (roster.anyCombat)
        {
            if (!gs.inCombat)
            {
                gs.inCombat = true;
                gs.combatSince = now;
                gs.longFightSaid = false;
            }
            gs.lastCombatAt = now;
        }
        else if (gs.inCombat && Elapsed(gs.lastCombatAt, now) >= COMBAT_END_MS)
            gs.inCombat = false;

        if (Creature* boss = FindEngagedBoss(roster))
        {
            gs.bossSeenAt = now;
            if (gs.boss != boss->GetGUID())
            {
                gs.boss = boss->GetGUID();
                gs.bossName = boss->GetName();
                if (Remember(gs.pulled, gs.boss))
                {
                    Plan plan;
                    Plain(plan, Kind::BOSS_PULL, s.bossPullChance);
                    plan.boss = gs.bossName;
                    Schedule(gs, roster.speakers, plan, now);
                }
            }
        }
        else if (gs.boss)
        {
            Creature* boss = roster.map->GetCreature(gs.boss);
            if (boss && !boss->IsAlive())
            {
                if (Remember(gs.killed, gs.boss))
                {
                    Plan plan;
                    Plain(plan, Kind::BOSS_KILL, s.bossKillChance);
                    plan.boss = gs.bossName;
                    plan.ignoreGap = true;
                    plan.delayMin = KILL_MIN_MS;
                    plan.delayMax = KILL_MAX_MS;
                    Schedule(gs, roster.speakers, plan, now);
                }
                gs.boss = 0;
            }
            else if (!boss || (!roster.anyCombat && Elapsed(gs.bossSeenAt, now) > BOSS_RESET_MS))
                gs.boss = 0;
        }

        if (s.longFightMs && gs.inCombat && !gs.longFightSaid && Elapsed(gs.combatSince, now) >= s.longFightMs)
        {
            gs.longFightSaid = true;
            Plan plan;
            Plain(plan, Kind::LONG_FIGHT, s.longFightChance);
            Schedule(gs, roster.speakers, plan, now);
        }
    }

    // A healer low on mana asks for time (and drinks), a caster does the same a little later.
    void WatchMana(GroupState& gs, Roster const& roster, uint32 now)
    {
        Settings const& s = Cfg();
        for (Player* bot : roster.speakers)
        {
            if (!bot->GetMaxPower(POWER_MANA) || bot->IsMounted())
                continue;

            auto const last = botManaAt.find(bot->GetGUIDLow());
            if (last != botManaAt.end() && Elapsed(last->second, now) < MANA_REPEAT_MS)
                continue;

            uint32 const mana = ManaPercent(bot);
            bool const healer = RoleOf(bot) == ROLE_HEALER;

            if (bot->IsInCombat())
            {
                // BT-157: a healer asks for time at the mana where it stops topping up and only answers damage.
                if (!healer || !HealerGuardrails::ShouldAskToDrink(mana) || !gs.inCombat ||
                    Elapsed(gs.combatSince, now) < LOW_MANA_COMBAT_MIN_MS)
                    continue;

                botManaAt[bot->GetGUIDLow()] = now;
                Plan plan;
                Plain(plan, Kind::LOW_MANA, s.lowManaChance);
                plan.forced = bot;
                Schedule(gs, roster.speakers, plan, now);
                continue;
            }

            uint32 const threshold = healer ? s.healerDrinkPct : s.drinkPct;
            if (roster.anyCombat || !threshold || mana > threshold)
                continue;

            botManaAt[bot->GetGUIDLow()] = now;
            TrimBotMap(botManaAt, now, MANA_REPEAT_MS);

            // The drink happens whether or not the bot says so; the line only asks the group to wait.
            Pending pending;
            pending.botLow = bot->GetGUIDLow();
            pending.groupLow = bot->GetGroup() ? bot->GetGroup()->GetLowGUID() : 0;
            pending.kind = Kind::DRINK;
            pending.dueAt = now + urand(REACT_MIN_MS, REACT_MAX_MS);
            pending.speak = !Busy(gs, now) && Roll(s.lowManaChance);
            pendings.push_back(pending);
            if (pending.speak)
            {
                gs.hasBusy = true;
                gs.busyUntil = pending.dueAt + s.groupGapMs;
            }
        }
    }

    // The greeting, the introductions of the tank and the healer, and the jokes between two packs.
    void WatchRun(GroupState& gs, Roster const& roster, uint32 now)
    {
        Settings const& s = Cfg();
        if (roster.anyCombat)
        {
            gs.leaderStill = false;
            return;
        }

        if (!gs.enterDone && Passed(now, gs.enterAt))
        {
            Plan plan;
            Plain(plan, Kind::ENTER, s.enterChance);
            Scheduled const result = Schedule(gs, roster.speakers, plan, now);
            if (result == Scheduled::YES || result == Scheduled::ROLLED_OUT)
                gs.enterDone = true;
            return;
        }

        if (gs.enterDone && gs.roleStage < 2 && Passed(now, gs.roleAt))
        {
            Plan plan;
            Plain(plan, Kind::ROLE, s.roleChance);
            plan.roleOnly = gs.roleStage == 0 ? int(ROLE_TANK) : int(ROLE_HEALER);
            Scheduled const result = Schedule(gs, roster.speakers, plan, now);
            if (result == Scheduled::BUSY)
                return;

            ++gs.roleStage;
            gs.roleAt = now + urand(ROLE_STEP_MIN_MS, ROLE_STEP_MAX_MS);
            return;
        }

        uint32 const quietMs = gs.lastCombatAt ? IDLE_QUIET_MS : FIRST_IDLE_QUIET_MS;
        uint32 const quietSince = gs.lastCombatAt ? gs.lastCombatAt : gs.runStartAt;

        // The leader has stood still for a long time: one bot asks about it, once per pause.
        if (roster.human && roster.human->IsAlive() && !roster.human->isMoving())
        {
            if (!gs.leaderStill)
            {
                gs.leaderStill = true;
                gs.stillSince = now;
                gs.waitingSaid = false;
            }
            else if (s.waitingMs && gs.enterDone && !gs.waitingSaid && Elapsed(gs.stillSince, now) >= s.waitingMs &&
                     Elapsed(quietSince, now) >= quietMs)
            {
                Plan plan;
                Plain(plan, Kind::WAITING, s.waitingChance);
                Scheduled const result = Schedule(gs, roster.speakers, plan, now);
                if (result == Scheduled::YES || result == Scheduled::ROLLED_OUT)
                    gs.waitingSaid = true;
            }
        }
        else
            gs.leaderStill = false;

        if (gs.enterDone && Passed(now, gs.nextIdleAt) && Elapsed(quietSince, now) >= quietMs && roster.human &&
            roster.human->isMoving())
        {
            Plan plan;
            Plain(plan, Kind::IDLE, s.idleChance);
            Scheduled const result = Schedule(gs, roster.speakers, plan, now);
            gs.nextIdleAt = now + (result == Scheduled::BUSY ? IDLE_RETRY_MS : urand(s.idleMinMs, s.idleMaxMs));
        }
    }

    // The player left the dungeon: one of the bots says goodbye.
    void WatchEnd(PlayerbotAI* botAI, Player* bot, GroupState& gs, uint32 now)
    {
        Player* master = botAI->GetMaster();
        Map* masterMap = master ? master->GetMap() : nullptr;
        if (masterMap && masterMap->IsDungeon())
            return;

        gs.active = false;
        gs.endDone = true;
        if (gs.managed || !bot->IsAlive() || Elapsed(gs.runStartAt, now) < MIN_RUN_FOR_GOODBYE_MS)
            return;

        Plan plan;
        Plain(plan, Kind::END, Cfg().endChance);
        plan.forced = bot;
        plan.delayMin = 1500;
        plan.delayMax = 4000;
        plan.ignoreGap = true;
        std::vector<Player*> const alone = { bot };
        Schedule(gs, alone, plan, now);
    }

    void Evaluate(PlayerbotAI* botAI, Player* bot, Group* group, Map* map, bool inDungeon)
    {
        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);

        if (!inDungeon)
        {
            auto const itr = groups.find(group->GetLowGUID());
            if (itr != groups.end() && itr->second.active && !itr->second.endDone)
                WatchEnd(botAI, bot, itr->second, now);
            return;
        }

        GroupState& gs = groups[group->GetLowGUID()];
        gs.touchedAt = now;
        if (gs.evalAt && Elapsed(gs.evalAt, now) < EVAL_GAP_MS)
            return;
        gs.evalAt = now;

        if (groups.size() > TRIM_SIZE)
            for (auto itr = groups.begin(); itr != groups.end();)
                itr = Elapsed(itr->second.touchedAt, now) > GROUP_FORGET_MS ? groups.erase(itr) : std::next(itr);

        Roster roster;
        Collect(group, map, roster);
        if (!roster.human)
            return;

        uint64 const instanceKey = (uint64(map->GetId()) << 32) | map->GetInstanceId();
        if (gs.instanceKey != instanceKey)
        {
            gs = GroupState();
            gs.touchedAt = now;
            gs.evalAt = now;
            gs.instanceKey = instanceKey;
            gs.active = true;
            gs.runStartAt = now;
            gs.managed = group->isLFGGroup() || BotLfm::InsideParty(bot);
            gs.enterAt = now + urand(ENTER_MIN_MS, ENTER_MAX_MS);
            gs.roleAt = gs.enterAt + urand(ROLE_MIN_MS, ROLE_MAX_MS);
            gs.nextIdleAt = now + urand(Cfg().idleMinMs, Cfg().idleMaxMs);
        }

        WatchDeaths(gs, roster, now);
        WatchFights(gs, roster, now);
        WatchMana(gs, roster, now);
        WatchRun(gs, roster, now);
    }

    bool Eligible(PlayerbotAI* botAI, Player* bot, Group*& group)
    {
        if (!Cfg().enabled || !bot->IsInWorld() || bot->InBattleground())
            return false;

        group = bot->GetGroup();
        return group && !group->isBGGroup() && botAI->HasRealPlayerMaster() && sRandomPlayerbotMgr->IsRandomBot(bot);
    }

    // Called with stateLock held. Drops the plans that were never said.
    bool HasDue(uint32 botLow, uint32 now)
    {
        bool due = false;
        for (auto itr = pendings.begin(); itr != pendings.end();)
        {
            if (Elapsed(itr->dueAt, now) > PENDING_GRACE_MS && Passed(now, itr->dueAt))
            {
                itr = pendings.erase(itr);
                continue;
            }

            if (itr->botLow == botLow && Passed(now, itr->dueAt))
                due = true;
            ++itr;
        }
        return due;
    }

    // What a player's message is, or Kind::NONE. The text is lower cased, stripped of everything but letters, digits and
    // spaces, and the closing address ("guys", "all") is cut off.
    std::string Normalize(std::string const& message)
    {
        std::string text;
        for (char c : message)
        {
            if (c >= 'A' && c <= 'Z')
                c = char(c - 'A' + 'a');

            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                text.push_back(c);
            else if (c != '\'' && (text.empty() || text.back() != ' '))
                text.push_back(' ');
        }

        static char const* const ADDRESS[] = { " guys", " all", " everyone", " everybody", " team", " folks", " friends",
                                                " mates", " bots", " again", " please", " pls" };
        bool trimmed = true;
        while (trimmed)
        {
            while (!text.empty() && text.back() == ' ')
                text.pop_back();

            trimmed = false;
            for (char const* address : ADDRESS)
            {
                std::string const tail = address;
                if (text.size() > tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0)
                {
                    text.erase(text.size() - tail.size());
                    trimmed = true;
                }
            }
        }

        while (!text.empty() && text.front() == ' ')
            text.erase(text.begin());
        return text;
    }

    bool IsLaugh(std::string const& text)
    {
        static std::unordered_set<std::string> const WORDS = { "lol", "lmao", "lmfao", "rofl", "heh", "hehe", "hehehe", "kek",
                                                                 "xd", "haha", "hah", "lel", "lul" };
        if (WORDS.count(text))
            return true;

        // "hahaha", "loool", "lolol"
        bool onlyHa = text.size() >= 4, onlyLo = text.size() >= 3 && text.front() == 'l';
        for (char c : text)
        {
            onlyHa = onlyHa && (c == 'h' || c == 'a');
            onlyLo = onlyLo && (c == 'l' || c == 'o');
        }
        return onlyHa || onlyLo;
    }

    Kind Classify(std::string const& rawMessage)
    {
        if (rawMessage.empty() || rawMessage.size() > MAX_MESSAGE_LENGTH)
            return Kind::NONE;

        std::string const text = Normalize(rawMessage);
        if (text.empty())
            return Kind::NONE;

        // The bare word "ready" starts the module's ready check (a chat command): the bots answer that one themselves.
        std::string raw = rawMessage;
        raw.erase(0, raw.find_first_not_of(" \t"));
        raw.erase(raw.find_last_not_of(" \t") + 1);
        std::transform(raw.begin(), raw.end(), raw.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (raw == "ready")
            return Kind::NONE;

        static std::unordered_map<std::string, Kind> const PHRASES = []
        {
            std::unordered_map<std::string, Kind> phrases;
            auto add = [&phrases](Kind kind, std::initializer_list<char const*> list)
            {
                for (char const* phrase : list)
                    phrases[phrase] = kind;
            };

            add(Kind::REPLY_READY, { "ready", "rdy", "all ready", "you ready", "u ready", "are you ready", "are u ready",
                                     "r u ready", "are we ready", "we ready", "ready to go", "ready to pull", "ready for next",
                                     "ready for the next one", "good to go", "all good", "everyone ready" });
            add(Kind::REPLY_WAIT, { "wait", "wait a sec", "wait a second", "wait a moment", "wait for me", "wait up", "hold on",
                                    "hold up", "hang on", "one sec", "one second", "one moment", "just a sec", "just a moment",
                                    "give me a sec", "give me a second", "gimme a sec", "brb", "afk", "bio", "bio break",
                                    "be right back", "one min", "one minute", "sec" });
            add(Kind::REPLY_THANKS, { "thanks", "thank you", "thx", "ty", "tyvm", "thanks a lot", "thank u", "many thanks",
                                      "cheers", "thanks for the help", "thanks for the heal", "thanks for the heals",
                                      "thanks for the buff", "thanks for the group", "thanks for the run", "thanks so much" });
            add(Kind::REPLY_PRAISE, { "good job", "great job", "nice job", "well done", "gj", "gg", "wp", "well played",
                                      "nicely done", "nice", "nice one", "awesome", "excellent", "good work", "great work",
                                      "perfect", "amazing", "nicely played", "good run", "that was great", "good one" });
            add(Kind::REPLY_GREET, { "hi", "hello", "hey", "hiya", "heya", "yo", "sup", "hi there", "hello there", "hey there",
                                     "good morning", "good evening", "greetings" });
            add(Kind::REPLY_SORRY, { "sorry", "my bad", "oops", "oops sorry", "sry", "mb", "whoops", "my fault", "sorry my bad" });
            add(Kind::REPLY_BYE, { "bye", "cya", "see you", "see ya", "gtg", "g2g", "good night", "gn", "goodbye", "have to go",
                                   "got to go", "i have to go", "later", "laters", "take care", "see you later", "cu", "ttyl" });
            return phrases;
        }();

        auto const itr = PHRASES.find(text);
        if (itr != PHRASES.end())
            return itr->second;

        return IsLaugh(text) ? Kind::REPLY_LAUGH : Kind::NONE;
    }

    bool ReadyForTheNextPull(Player* bot)
    {
        if (bot->IsInCombat() || bot->GetHealthPct() < MIN_READY_HEALTH_PCT)
            return false;

        return !bot->GetMaxPower(POWER_MANA) || ManaPercent(bot) >= MIN_READY_MANA_PCT;
    }

    // Somebody of the bot's group fights on the bot's map (the bot's own flag can lag a pull by a moment).
    bool GroupFights(Player* bot, Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->IsInWorld() && member->FindMap() == bot->FindMap() && member->IsInCombat())
                return true;
        }
        return false;
    }

    // Says the planned line through the bot voice and plays the emote; false when nothing was said.
    bool Say(PlayerbotAI* botAI, Player* bot, Pending const& pending, uint32 now)
    {
        Kind const kind = pending.kind;
        KindInfo const& info = KindOf(kind);

        std::string key = info.key;
        if (kind == Kind::ROLE)
            key += pending.role == ROLE_TANK ? "_tank" : (pending.role == ROLE_HEALER ? "_healer" : "_damage");

        std::vector<char const*> const& defaults = info.defaults;
        std::string fallback = defaults[urand(0, uint32(defaults.size() - 1))];
        if (kind == Kind::ROLE)
            fallback = pending.role == ROLE_TANK ? "Tank here. I will pull when everyone is ready" :
                (pending.role == ROLE_HEALER ? "Healer here. I will keep everyone up" : "Damage here, ready when the tank is");

        std::string dungeon;
        if (Map* map = bot->GetMap())
            dungeon = map->GetMapName();

        std::map<std::string, std::string> const placeholders = { { "%name", pending.name }, { "%boss", pending.boss },
                                                                   { "%item", pending.item }, { "%dungeon", dungeon } };

        // The plan was made with the group's gaps in mind, so the voice treats it as a notice: said as planned, but its
        // stamps count for the voice's other chatter.
        if (!BotVoice::Say(botAI, key, fallback, placeholders, BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE, 100,
                           nullptr, true))
            return false;

        {
            std::lock_guard<std::mutex> guard(stateLock);
            botSpokeAt[bot->GetGUIDLow()] = now;
            TrimBotMap(botSpokeAt, now, Cfg().botGapMs);
        }

        if (info.emote && Roll(Cfg().emoteChance))
            PlayBotTextEmote(bot, info.emote);

        return true;
    }
}  // namespace

namespace DungeonChat
{
    bool Enabled() { return Cfg().enabled; }

    bool Due(PlayerbotAI* botAI, Player* bot)
    {
        Group* group = nullptr;
        if (!botAI || !bot || !Eligible(botAI, bot, group))
            return false;

        Map* map = bot->GetMap();
        Evaluate(botAI, bot, group, map, map && map->IsDungeon());

        std::lock_guard<std::mutex> guard(stateLock);
        return !pendings.empty() && HasDue(bot->GetGUIDLow(), getMSTime());
    }

    bool Deliver(PlayerbotAI* botAI)
    {
        Player* bot = botAI->GetBot();
        uint32 const now = getMSTime();
        Pending pending;
        bool found = false;
        {
            std::lock_guard<std::mutex> guard(stateLock);
            for (auto itr = pendings.begin(); itr != pendings.end(); ++itr)
            {
                if (itr->botLow == bot->GetGUIDLow() && Passed(now, itr->dueAt))
                {
                    pending = *itr;
                    pendings.erase(itr);
                    found = true;
                    break;
                }
            }
        }

        Group* group = nullptr;
        if (!found || !Eligible(botAI, bot, group) || !bot->IsAlive() || group->GetLowGUID() != pending.groupLow)
            return false;

        if (pending.kind == Kind::DRINK)
        {
            // Drinking needs no fight in progress, and the line only makes sense for the same reason.
            if (GroupFights(bot, group))
                return false;

            if (pending.speak)
                Say(botAI, bot, pending, now);

            botAI->DoSpecificAction("drink", Event(), true);
            return true;
        }

        // Nobody types a remark about the last pull, the loot or the weather in the middle of a fight (BT-116).
        bool const aboutTheFight = pending.kind == Kind::BOSS_PULL || pending.kind == Kind::LONG_FIGHT ||
            pending.kind == Kind::LOW_MANA || pending.kind == Kind::BOSS_KILL || pending.kind == Kind::DEATH_SELF ||
            pending.kind >= Kind::REPLY_READY;
        if (!aboutTheFight && GroupFights(bot, group))
        {
            if (pending.postponed >= COMBAT_POSTPONES)
                return false;

            ++pending.postponed;
            pending.dueAt = now + urand(POSTPONE_MIN_MS, POSTPONE_MAX_MS);
            std::lock_guard<std::mutex> guard(stateLock);
            pendings.push_back(pending);
            return false;
        }

        if (pending.kind == Kind::REPLY_READY && !ReadyForTheNextPull(bot))
            pending.kind = Kind::REPLY_NOT_READY;

        return Say(botAI, bot, pending, now);
    }

    bool OnGroupChat(Group* group, Player* sender, uint32 chatType, std::string const& message)
    {
        Settings const& s = Cfg();
        if (!s.enabled || !s.replies || !group || !sender || !sender->GetSession() || sender->GetSession()->IsBot())
            return false;

        if (chatType != CHAT_MSG_PARTY && chatType != CHAT_MSG_PARTY_LEADER && chatType != CHAT_MSG_RAID &&
            chatType != CHAT_MSG_RAID_LEADER)
            return false;

        Map* map = sender->GetMap();
        if (!map || !map->IsDungeon() || group->isBGGroup())
            return false;

        Kind const kind = Classify(message);
        if (kind == Kind::NONE)
            return false;

        Roster roster;
        Collect(group, map, roster);
        if (roster.speakers.empty())
            return false;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);
        GroupState& gs = groups[group->GetLowGUID()];
        gs.touchedAt = now;
        // From here the phrase is the rules' own, answered or not: the AI layer never gets a second roll at it.
        if (gs.hasReplyBusy && !Passed(now, gs.replyBusyUntil))
            return true;

        // Answering a player is not held back by the bots' own gaps, only by the reply gap and the roll.
        Plan plan;
        Plain(plan, kind, s.replyChance);
        plan.ignoreGap = true;
        plan.honourBotGap = false;
        plan.delayMin = REPLY_MIN_MS;
        plan.delayMax = REPLY_MAX_MS;
        if (Schedule(gs, roster.speakers, plan, now) != Scheduled::YES)
            return true;

        gs.hasReplyBusy = true;
        gs.replyBusyUntil = now + s.replyGapMs;
        return true;
    }

    bool OnBossKilled(PlayerbotAI* botAI, Creature* boss)
    {
        if (!Cfg().enabled)
            return false;

        Player* bot = botAI->GetBot();
        Group* group = bot->GetGroup();
        Map* map = bot->GetMap();
        if (!group || !boss || !map || !map->IsDungeon())
            return true;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);
        auto const itr = groups.find(group->GetLowGUID());
        if (itr == groups.end() || !itr->second.active || !Remember(itr->second.killed, boss->GetGUID()))
            return true;

        Roster roster;
        Collect(group, map, roster);
        if (!roster.human)
            return true;

        Plan plan;
        Plain(plan, Kind::BOSS_KILL, Cfg().bossKillChance);
        plan.boss = boss->GetName();
        plan.ignoreGap = true;
        plan.delayMin = KILL_MIN_MS;
        plan.delayMax = KILL_MAX_MS;
        Schedule(itr->second, roster.speakers, plan, now);
        itr->second.boss = 0;
        return true;
    }

    void OnLootRoll(PlayerbotAI* botAI, ItemTemplate const* item, uint32 vote)
    {
        Settings const& s = Cfg();
        if (!s.enabled || !item || item->Quality < LOOT_REMARK_QUALITY)
            return;

        Kind kind = Kind::NONE;
        if (vote == uint32(RollType::ROLL_NEED))
            kind = Kind::LOOT_NEED;
        else if (vote == uint32(RollType::ROLL_GREED))
            kind = Kind::LOOT_GREED;
        else
            return;

        Player* bot = botAI->GetBot();
        Group* group = nullptr;
        Map* map = bot->GetMap();
        if (!Eligible(botAI, bot, group) || !map || !map->IsDungeon())
            return;

        uint32 const now = getMSTime();
        std::lock_guard<std::mutex> guard(stateLock);
        auto const itr = groups.find(group->GetLowGUID());
        if (itr == groups.end() || !itr->second.active)
            return;

        // One remark per roll round: the first bot to vote and pass the roll says it, the others stay quiet.
        Plan plan;
        Plain(plan, kind, s.lootChance);
        plan.forced = bot;
        plan.item = item->Name1;
        plan.delayMin = 700;
        plan.delayMax = 2000;
        std::vector<Player*> const alone = { bot };
        Schedule(itr->second, alone, plan, now);
    }
}
