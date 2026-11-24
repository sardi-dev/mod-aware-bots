/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "MarkTriggers.h"

#include "AttackersValue.h"
#include "BotGuid.h"
#include "BotVoice.h"
#include "Creature.h"
#include "Log.h"
#include "Map.h"
#include "MarkPullPolicy.h"
#include "PlayerbotSpec.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "StaleCombat.h"

namespace
{
    // Read once on first use, like the other local settings (BotTraining.cpp), to keep PlayerbotAIConfig.h unchanged.
    bool Enabled()
    {
        static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.TankPullsMarks", true);
        return enabled;
    }

    float Range()
    {
        static float const range = sConfigMgr->GetFloatDefault("AiPlayerbot.MarkPullRange", 40.0f);
        return range;
    }

    constexpr time_t WAIT_NOTICE_SECONDS = 10;
    constexpr time_t NOTICE_GAP_SECONDS = 20;

    // BT-160: give the tank and the healer enough recovery for the next pull.
    // BT-168: a stale combat flag is not a fight. BT-170: a damage dealer's mana holds the pull for a limited time.
    // Alive, out of a real fight and within the group range of the tank: the first test of every member.
    bool CaughtUp(Player* member, Player* tank)
    {
        return member->IsAlive() && (!member->IsInCombat() || StaleCombat::IsStale(member)) &&
            member->GetMapId() == tank->GetMapId() && member->IsWithinDistInMap(tank, Range());
    }

    // BT-290: `sightWaived` is the tank's waive for this mark (MarkPullPolicy::NoteTankSight).
    char const* WaitReason(Player* member, Player* tank, bool boss, unsigned heldSeconds, bool sightWaived)
    {
        if (!CaughtUp(member, tank))
            return "waiting for everyone to catch up";
        bool const healer = PlayerBotSpec::IsHeal(member, true);
        if (healer && !member->IsWithinDistInMap(tank, MarkPullPolicy::HEALER_RANGE))
            return "waiting for everyone to catch up";
        if (healer && !sightWaived && !member->IsWithinLOSInMap(tank))
            return MarkPullPolicy::HEALER_OUT_OF_SIGHT_REASON;
        if (!MarkPullPolicy::HealthReady(member->GetHealthPct(), member == tank))
            return "need a moment to heal up";
        bool const cappedDamage = member != tank && !healer && !PlayerBotSpec::IsTank(member, true);
        if (member->getPowerType() == POWER_MANA && member->GetMaxPower(POWER_MANA) &&
            MarkPullPolicy::ManaWaits(member->GetPower(POWER_MANA) * 100.0f / member->GetMaxPower(POWER_MANA),
                                      healer, boss, cappedDamage, heldSeconds))
            return "waiting on mana";
        return nullptr;
    }

    // True when a player is in the group (a human session, not a bot).
    bool GroupHasHumanOnline(Group* group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && member->GetSession() && !member->GetSession()->IsBot())
                return true;
        }
        return false;
    }

    // BT-169: a blocker is said once per mark and reason, so a mark that nobody pulls shows why in the log.
    constexpr time_t BLOCKER_LOG_GAP_SECONDS = 20;

}

bool MarkedTargetReadyTrigger::IsActive()
{
    if (!Enabled() || !bot->IsAlive() || bot->IsInCombat())
    {
        waitingSince = 0;
        readySince = 0;
        return false;
    }

    Group* group = bot->GetGroup();
    if (!group)
    {
        waitingSince = 0;
        readySince = 0;
        return false;
    }

    Unit* target = CurrentMark(botAI);
    if (!target || !target->IsAlive() || target->IsInCombat() || IsBodyPullMark(bot, target) != bodyPull)
    {
        waitingSince = 0;
        readySince = 0;
        return false;
    }

    if (waitingMark != target->GetGUID())
    {
        waitingMark = target->GetGUID();
        unreachableSince = 0;
        waitingSince = 0;
        announced = false;
        lastBlockerLog = 0;
        readySince = 0;
    }

    // BT-169/BT-171: from here a living mark that is not fighting exists. A mark the bot cannot pull, or one beyond the
    // group range and out of sight, is said once instead of leaving the group standing in silence (live: a mark at
    // 41.4 yd, range 40, held the tank 62 s with nothing in the log).
    if (!bodyPull && !AttackersValue::IsPossibleTarget(target, bot))
    {
        NoteBlocker(target, "the mark cannot be pulled", "", false);
        waitingSince = 0;
        readySince = 0;
        return false;
    }
    float const distance = bot->GetDistance(target);
    if (!MarkPullPolicy::MarkReachable(distance, Range(), bot->IsWithinLOSInMap(target)))
    {
        NoteBlocker(target, "the mark is out of reach and out of sight", "", true);
        waitingSince = 0;
        readySince = 0;
        // A group with no player in it never comes to a mark this far away: the mark is dropped after a while.
        time_t const unreachableNow = time(nullptr);
        if (!unreachableSince)
            unreachableSince = unreachableNow;
        if (MarkPullPolicy::StaleMark(long(unreachableSince), long(unreachableNow), GroupHasHumanOnline(group)))
        {
            for (uint8 icon = 0; icon < TARGETICONCOUNT; ++icon)
                if (group->GetTargetIcon(icon) == target->GetGUID())
                    group->SetTargetIcon(icon, bot->GetGUID(), ObjectGuid(uint64(0)), 0);
            SF_LOG_INFO("playerbots", "[MARKPULL] %s drops the mark %u: out of reach for %ld s and no player is in the group",
                        bot->GetName().c_str(), target->GetEntry(), long(unreachableNow - unreachableSince));
            unreachableSince = 0;
        }
        return false;
    }
    unreachableSince = 0;

    time_t const now = time(nullptr);
    // BT-290: the healer's sight is clocked on its own, not only while it is the first blocker, so the healer's walk and
    // the tank's step run while someone else still drinks.
    auto& sight = botAI->markTankSight;
    Player* hiddenHealer = HealerOutOfSight(group, bot);
    MarkPullPolicy::NoteTankSight(sight, target->GetGUID(), hiddenHealer ? hiddenHealer->GetGUID() : 0, long(now));
    if (sight.waived && !sight.waiveLogged)
    {
        sight.waiveLogged = true;
        SF_LOG_INFO("playerbots", "[MARKPULL] %s no longer waits for the healer's sight after %ld s: the healer is within "
                    "%.0f yd (mark %u)", bot->GetName().c_str(), long(now) - sight.since, MarkPullPolicy::HEALER_RANGE,
                    target->GetEntry());
    }
    unsigned const held = waitingSince ? unsigned(now - waitingSince) : 0;
    Player* blocker = nullptr;
    char const* reason = GroupWaitReason(group, bot, target, held, &blocker);
    if (!reason)
    {
        botAI->markWaitGuid = 0;
        botAI->markWaitSince = 0;
        waitingSince = 0;
        // BT-181: ready, yet the mark is still not fighting after a while and the tank stands: the pull cannot reach it
        // (the [PULL] result=reach-failed line says why). Deadmines run 4 held three such marks 45 s with no line.
        if (!readySince)
            readySince = now;
        else if (MarkPullPolicy::PullStuck(long(readySince), long(now), bot->isMoving()))
            NoteBlocker(target, bodyPull ? "the boss does not wake when I come close" : "the mark cannot be reached",
                        "", true);
        return true;
    }
    readySince = 0;

    if (!waitingSince)
        waitingSince = now;
    botAI->markWaitGuid = target->GetGUID();
    botAI->markWaitSince = waitingSince;
    // BT-160: one useful notice per mark, so the leader knows why the tank stands still, never a stream of them.
    // BT-169: the log line names who holds the pull.
    if (!announced && now - waitingSince >= WAIT_NOTICE_SECONDS && (!lastNotice || now - lastNotice >= NOTICE_GAP_SECONDS))
    {
        SF_LOG_INFO("playerbots", "[MARKPULL] %s waits: %s blocker=%s mark=%u held=%us", bot->GetName().c_str(), reason,
                    blocker ? blocker->GetName().c_str() : "?", target->GetEntry(), unsigned(now - waitingSince));
        lastNotice = now;
        announced = BotVoice::Say(botAI, "mark_pull_wait", reason, {}, BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
    }
    return false;
}

bool MarkedTargetOutOfReachTrigger::IsActive()
{
    // The same marks the pull trigger works on: a plain pullable mark of a bot out of combat in a group. A body pull
    // mark (a boss that wakes when approached) has its own walk.
    Unit* target = Enabled() && bot->IsAlive() && !bot->IsInCombat() && bot->GetGroup()
        ? MarkedTargetReadyTrigger::CurrentMark(botAI) : nullptr;
    if (!target || !target->IsAlive() || target->IsInCombat() || MarkedTargetReadyTrigger::IsBodyPullMark(bot, target) ||
        !AttackersValue::IsPossibleTarget(target, bot))
    {
        outOfReachSince = 0;
        return false;
    }

    // BT-332: out of sight but inside the range is a walk of its own (a longer wait, a nearer leader); in sight the pull
    // walk does it all.
    bool const inSight = bot->IsWithinLOSInMap(target);
    bool const reachable = MarkPullPolicy::MarkReachable(bot->GetDistance(target), Range(), inSight);
    if (reachable && inSight)
    {
        outOfReachSince = 0;
        return false;
    }

    // The clock starts at the first reading for this mark and restarts with a new one.
    time_t const now = time(nullptr);
    if (!outOfReachSince || markGuid != target->GetGUID())
    {
        markGuid = target->GetGUID();
        outOfReachSince = now;
    }

    Player* leader = botAI->GetGroupMaster();
    if (!leader || leader == bot || !leader->IsAlive() || leader->FindMap() != bot->FindMap())
        return false;
    float const leaderDistance = bot->GetDistance(leader);
    return reachable ? MarkPullPolicy::WalksToLeaderForSight(long(outOfReachSince), long(now), leaderDistance)
                     : MarkPullPolicy::WalksToLeader(long(outOfReachSince), long(now), leaderDistance);
}

void MarkedTargetReadyTrigger::NoteBlocker(Unit* target, char const* reason, char const* who, bool say)
{
    time_t const now = time(nullptr);
    if (lastBlockerLog && now - lastBlockerLog < BLOCKER_LOG_GAP_SECONDS)
        return;
    lastBlockerLog = now;
    SF_LOG_INFO("playerbots", "[MARKPULL] %s waits: %s blocker=%s mark=%u", bot->GetName().c_str(), reason,
                who[0] ? who : "mark", target->GetEntry());
    if (say && !announced)
        announced = BotVoice::Say(botAI, "mark_pull_wait", reason, {}, BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
}

bool MarkedTargetReadyTrigger::IsBossMark(Unit* target)
{
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature)
        return false;
    CreatureTemplate const* info = creature->GetCreatureTemplate();
    return creature->IsDungeonBoss() || (info && info->rank == CREATURE_ELITE_WORLDBOSS);
}

Unit* MarkedTargetReadyTrigger::CurrentMark(PlayerbotAI* botAI)
{
    // The leader may mark a target the tank cannot see yet (around a corner): the "rti target" value wants line of
    // sight, so fall back to the group's mark itself, as PullRtiTargetAction does.
    AiObjectContext* context = botAI->GetAiObjectContext();
    if (Unit* target = context->GetValue<Unit*>("rti target")->Get())
        return target;
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return nullptr;
    int32 const index = RtiTargetValue::GetRtiIndex(context->GetValue<std::string>("rti")->Get());
    uint64 const guid = index >= 0 ? group->GetTargetIcon(uint8(index)) : 0;
    return guid ? botAI->GetUnit(BotGuid(guid)) : nullptr;
}

static_assert(MarkPullPolicy::FLAG_IMMUNE_TO_PC == UNIT_FLAG_IMMUNE_TO_PC, "unit flag values changed");
static_assert(MarkPullPolicy::BODY_PULL_BLOCKING_FLAGS ==
                  (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_ATTACKABLE_1 | UNIT_FLAG_UNK_16 | UNIT_FLAG_PACIFIED |
                   UNIT_FLAG_STUNNED | UNIT_FLAG_NOT_SELECTABLE),
              "unit flag values changed");

bool MarkedTargetReadyTrigger::IsBodyPullMark(Player* bot, Unit* target)
{
    Creature* creature = target ? target->ToCreature() : nullptr;
    Map* map = bot->FindMap();
    if (!creature || !map || !creature->IsInWorld() || !creature->IsAlive() || creature->IsInCombat() ||
        creature->GetMapId() != bot->GetMapId() ||
        !MarkPullPolicy::BodyPull(map->IsDungeon() && !map->IsRaid(), IsBossMark(creature), creature->IsFriendlyTo(bot),
                                  creature->GetUInt32Value(UNIT_FIELD_FLAGS)))
        return false;
    // The same visibility tests as AttackersValue::IsPossibleTarget, after the cheap flag test.
    return creature->IsVisible() && !creature->IsInEvadeMode() && bot->CanSeeOrDetect(creature);
}

char const* MarkedTargetReadyTrigger::GroupWaitReason(Group* group, Player* tank, Unit* target, unsigned heldSeconds,
                                                      Player** blocker)
{
    bool const boss = IsBossMark(target);
    PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
    bool const sightWaived = tankAI && target && MarkPullPolicy::SightWaivedFor(tankAI->markTankSight, target->GetGUID());
    if (char const* reason = WaitReason(tank, tank, boss, heldSeconds, sightWaived))
    {
        if (blocker)
            *blocker = tank;
        return reason;
    }
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == tank)
            continue;
        if (char const* reason = WaitReason(member, tank, boss, heldSeconds, sightWaived))
        {
            if (blocker)
                *blocker = member;
            return reason;
        }
    }
    return nullptr;
}

Player* MarkedTargetReadyTrigger::HealerOutOfSight(Group* group, Player* tank)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != tank && PlayerBotSpec::IsHeal(member, true) && CaughtUp(member, tank) &&
            member->IsWithinDistInMap(tank, MarkPullPolicy::HEALER_RANGE) && !member->IsWithinLOSInMap(tank))
            return member;
    }
    return nullptr;
}

bool MarkedTargetReadyTrigger::GroupReady(Group* group, Player* tank, Unit* target)
{
    return !GroupWaitReason(group, tank, target);
}

Player* MarkHealerOutOfSightTrigger::BlockingTank(PlayerbotAI* ai, Unit** mark)
{
    Player* healer = ai->GetBot();
    Group* group = healer->GetGroup();
    if (!Enabled() || !group || !healer->IsAlive() || healer->IsInCombat() ||
        !PlayerBotSpec::IsHeal(healer, true) || ai->HasStrategy("stay", ai->GetState()))
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* tank = ref->GetSource();
        if (!tank || tank == healer || !tank->IsAlive() || tank->IsInCombat() ||
            tank->FindMap() != healer->FindMap() || !PlayerBotSpec::IsTank(tank, true))
            continue;
        PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
        if (!tankAI || !tankAI->HasStrategy("mark pull", BOT_STATE_NON_COMBAT))
            continue;
        Unit* target = MarkedTargetReadyTrigger::CurrentMark(tankAI);
        if (!target || !target->IsAlive() || target->IsInCombat())
            continue;
        bool const bodyPull = MarkedTargetReadyTrigger::IsBodyPullMark(tank, target);
        if ((!bodyPull && !AttackersValue::IsPossibleTarget(target, tank)) ||
            !MarkPullPolicy::MarkReachable(tank->GetDistance(target), Range(), tank->IsWithinLOSInMap(target)))
            continue;
        // The tank holds this mark (any reason). BT-290: the healer walks when she is out of its sight, also while
        // another member is the first blocker (a damage dealer drinking), and not once the tank waived sight.
        if (tankAI->markWaitGuid != target->GetGUID() || !tankAI->markWaitSince ||
            MarkPullPolicy::SightWaivedFor(tankAI->markTankSight, target->GetGUID()))
            continue;
        if (MarkedTargetReadyTrigger::HealerOutOfSight(group, tank) == healer)
        {
            if (mark)
                *mark = target;
            return tank;
        }
    }
    return nullptr;
}

bool MarkHealerOutOfSightTrigger::IsActive()
{
    auto& state = botAI->markHealerSight;
    Unit* mark = nullptr;
    Player* tank = BlockingTank(botAI, &mark);
    if (!tank)
    {
        // BT-291: the walk aims at the tank's own spot; once the tank is in sight it ends where it is, so the healer does
        // not stand on the tank (in reach of the mob's cleave) and the group does not stack.
        if (state.walkSince)
            if (Unit* walkedTo = botAI->GetUnit(BotGuid(state.tankGuid)))
                if (bot->isMoving() && walkedTo->GetMapId() == bot->GetMapId() && bot->IsWithinLOSInMap(walkedTo))
                    bot->StopMoving();
        state = MarkPullPolicy::HealerSightState();
        return false;
    }
    long const now = long(time(nullptr));
    if (!state.blockedSince || state.tankGuid != tank->GetGUID() || state.markGuid != mark->GetGUID())
    {
        state = MarkPullPolicy::HealerSightState();
        state.tankGuid = tank->GetGUID();
        state.markGuid = mark->GetGUID();
        state.blockedSince = now;
    }
    bool const inSight = bot->IsWithinLOSInMap(tank);
    if (!state.failureLogged && MarkPullPolicy::HealerSightWalkTimedOut(state.walkSince, now, inSight))
    {
        state.failureLogged = true;
        SF_LOG_INFO("playerbots", "[MARKPULL] %s cannot reach sight of %s after %ld s (mark %u)",
                    bot->GetName().c_str(), tank->GetName().c_str(), now - state.walkSince, mark->GetEntry());
    }
    return MarkPullPolicy::HealerWalksIntoSight(state.blockedSince, now, bot->GetDistance(tank), inSight);
}

Player* MarkHealerOutOfTankSightTrigger::HiddenHealer(PlayerbotAI* botAI)
{
    Player* tank = botAI->GetBot();
    auto const& sight = botAI->markTankSight;
    if (!Enabled() || !sight.since || sight.waived || !tank->IsAlive() || tank->IsInCombat() || !tank->GetGroup())
        return nullptr;
    // The clock belongs to the mark the tank still holds; a pull, a fight or a new mark ends it.
    Unit* mark = MarkedTargetReadyTrigger::CurrentMark(botAI);
    if (!mark || mark->GetGUID() != sight.markGuid || !mark->IsAlive() || mark->IsInCombat() ||
        botAI->markWaitGuid != sight.markGuid || !botAI->markWaitSince)
        return nullptr;
    Unit* unit = botAI->GetUnit(BotGuid(sight.healerGuid));
    Player* healer = unit ? unit->ToPlayer() : nullptr;
    if (!healer || !healer->IsAlive() || healer->GetMapId() != tank->GetMapId())
        return nullptr;
    PlayerbotAI* healerAI = GET_PLAYERBOT_AI(healer);
    bool const botHealer = healerAI && !healerAI->IsRealPlayer();
    return MarkPullPolicy::TankStepsIntoHealerSight(sight.since, long(time(nullptr)), botHealer,
                                                    tank->IsWithinLOSInMap(healer)) ? healer : nullptr;
}

bool MarkHealerOutOfTankSightTrigger::IsActive()
{
    return HiddenHealer(botAI) != nullptr;
}
