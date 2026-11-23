/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "AttackedMemberRescue.h"

#include "Creature.h"
#include "FloorAttackerPolicy.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MemberRegroupPolicy.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PathGenerator.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "TankRescuePolicy.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace
{
constexpr float RESCUE_APPROACH_DISTANCE = 5.0f;
// BT-174: the same attacker is logged again after this long, or at once when the way of taking it changes.
constexpr time_t RESCUE_LOG_GAP_SECONDS = 10;

// BT-118: do not drag a boss away from its position.
bool TankHoldsBoss(Player* tank)
{
    for (Unit* unit : tank->getAttackers())
    {
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (creature && creature->IsAlive() && (creature->IsDungeonBoss() || creature->isWorldBoss()))
            return true;
    }
    return false;
}

bool IsCaster(Unit* unit)
{
    return unit->IsNonMeleeSpellCasted(false) ||
        ((unit->getClass() == CLASS_MAGE || unit->getClass() == CLASS_PALADIN) && unit->getPowerType() == POWER_MANA);
}

// BT-162: CanCastSpell checks the learned spell, cooldown, range, resources and sight.
bool TrySpell(PlayerbotAI* ai, char const* spell, Unit* target)
{
    return spell && ai->CanCastSpell(spell, target) && ai->CastSpell(spell, target);
}

bool TryRanged(PlayerbotAI* ai, Player* tank, Unit* target)
{
    switch (tank->getClass())
    {
        case CLASS_WARRIOR: return TrySpell(ai, "taunt", target) || TrySpell(ai, "heroic throw", target);
        case CLASS_PALADIN: return TrySpell(ai, "reckoning", target) || TrySpell(ai, "avenger's shield", target);
        case CLASS_DEATH_KNIGHT: return TrySpell(ai, "dark command", target) || TrySpell(ai, "death grip", target);
        case CLASS_DRUID: return TrySpell(ai, "growl", target) || TrySpell(ai, "faerie fire", target);
        case CLASS_MONK: return TrySpell(ai, "provoke", target);
        default: return false;
    }
}

bool TryArea(PlayerbotAI* ai, Player* tank, Unit* target)
{
    switch (tank->getClass())
    {
        case CLASS_WARRIOR: return TrySpell(ai, "thunder clap", target);
        case CLASS_PALADIN: return TrySpell(ai, "consecration", target);
        case CLASS_DEATH_KNIGHT: return TrySpell(ai, "blood boil", target);
        case CLASS_DRUID: return TrySpell(ai, "thrash", target) || TrySpell(ai, "swipe", target);
        case CLASS_MONK: return TrySpell(ai, "keg smash", target);
        default: return false;
    }
}
}  // namespace

Unit* AttackedMemberRescue::FindAttacker(Player* tank)
{
    if (!tank || !tank->IsAlive() || !PlayerBotSpec::IsTank(tank) || tank->GetVehicle() || TankHoldsBoss(tank))
        return nullptr;
    Group* group = tank->GetGroup();
    if (!group || !tank->FindMap())
        return nullptr;

    // BT-198: what the tank gave up walking to (TankRescuePolicy.h); without an AI there is nothing to remember.
    PlayerbotAI* const botAI = GET_PLAYERBOT_AI(tank);
    TankRescuePolicy::GiveUpBook const* const book = botAI ? &botAI->rescueGiveUps : nullptr;
    time_t const now = time(nullptr);

    Unit* nearest = nullptr;
    TankRescuePolicy::Priority best = {};
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != tank->FindMap() || PlayerBotSpec::IsTank(member))
            continue;
        for (Unit* unit : member->getAttackers())
        {
            if (!unit || !unit->ToCreature() || !unit->IsAlive() || unit->GetVictim() != member ||
                !member->IsHostileTo(unit) || !tank->IsValidAttackTarget(unit))
                continue;
            // BT-377: the tank is walking away from this mob so that it comes into sight; the rescue must not walk it
            // back under the mob before that (sight ends the order, so a mob that came is taken at once).
            if (botAI && botAI->floorStep.attacker == unit->GetGUID() &&
                FloorAttackerPolicy::Live(botAI->floorStep, now) && !tank->IsWithinLOSInMap(unit))
                continue;
            bool unreached = false;
            if (book && !TankRescuePolicy::Consider(*book, unit->GetGUID(), member->GetGUID(), now,
                                                     [&] { return tank->IsWithinLOSInMap(unit); }, unreached))
                continue;
            TankRescuePolicy::Priority const priority = { PlayerBotSpec::IsHeal(member, true),
                member->GetHealthPct(), IsCaster(unit), tank->GetDistance(unit), unreached };
            if (!nearest || TankRescuePolicy::Better(priority, best))
            {
                nearest = unit;
                best = priority;
            }
        }
    }
    return nearest;
}

Unit* AttackedMemberRescue::FindHiddenAttacker(Player* bot, Player*& victim)
{
    victim = nullptr;
    Group* group = bot->GetGroup();
    if (!group || !bot->FindMap())
        return nullptr;

    Unit* hidden = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->FindMap() != bot->FindMap())
            continue;
        for (Unit* unit : member->getAttackers())
        {
            if (!unit || !unit->ToCreature() || !unit->IsAlive() || unit->GetVictim() != member ||
                !member->IsHostileTo(unit))
                continue;
            // One attacker in sight is enough: the bot's own attacker list fights it.
            if (bot->IsWithinLOSInMap(unit))
                return nullptr;
            if (!hidden)
            {
                hidden = unit;
                victim = member;
            }
        }
    }
    return hidden;
}

Player* AttackedMemberRescue::FindTank(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->FindMap() == bot->FindMap() &&
            PlayerBotSpec::IsTank(member))
            return member;
    }
    return nullptr;
}

namespace
{
// A standing player's eye height, for the sight test from a spot (the pull sight search uses the same).
constexpr float STEP_SPOT_EYE_HEIGHT = 1.5f;

// The length of a path along its points.
float PathLengthOf(PathGenerator const& path)
{
    float length = 0.0f;
    Movement::PointsArray const& points = path.GetPath();
    for (size_t i = 1; i < points.size(); ++i)
        length += (points[i] - points[i - 1]).length();
    return length;
}
}  // namespace

bool AttackedMemberRescue::BeginStepAway(PlayerbotAI* botAI, Unit* attacker)
{
    Player* const bot = botAI ? botAI->GetBot() : nullptr;
    Map* const map = bot ? bot->GetMap() : nullptr;
    if (!map || !attacker)
        return false;

    time_t const now = time(nullptr);
    FloorAttackerPolicy::Order& order = botAI->floorStep;
    uint64 const attackerGuid = attacker->GetGUID();
    if (!FloorAttackerPolicy::MaySearch(order, attackerGuid, now))
        return false;
    // Found or not, the next search waits (a bot that finds nothing does not search every tick).
    FloorAttackerPolicy::NoteSearch(order, now);

    // Awake hostiles that do not fight yet: a spot near one could pull a second pack. The list is the bot's own cached
    // value, read once per search.
    struct Mob
    {
        float x, y;
    };
    std::vector<Mob> others;
    GuidVector const targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get();
    for (BotGuid const& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && unit != attacker && unit->IsAlive() && !unit->IsInCombat())
            others.push_back({ unit->GetPositionX(), unit->GetPositionY() });
    }

    float const botX = bot->GetPositionX();
    float const botY = bot->GetPositionY();
    float const botZ = bot->GetPositionZ();
    float const attackerFlat = std::hypot(attacker->GetPositionX() - botX, attacker->GetPositionY() - botY);
    float const clearance = FloorAttackerPolicy::OTHER_MOB_CLEARANCE_YARDS;

    FloorAttackerPolicy::Spot spots[FloorAttackerPolicy::SPOT_COUNT];
    unsigned count = 0;
    for (unsigned i = 0; i < FloorAttackerPolicy::SPOT_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        FloorAttackerPolicy::Offset(i, dx, dy);
        float const x = botX + dx;
        float const y = botY + dy;
        float const z = map->GetHeight(x, y, botZ + PullSightPolicy::MAX_SPOT_DZ);
        // The spot is on the bot's floor (a ledge or a lower floor is the attacker's, not ours).
        if (z <= INVALID_HEIGHT || !PullSightPolicy::SameFloor(z, botZ))
            continue;

        FloorAttackerPolicy::Spot& spot = spots[count++];
        spot.x = x;
        spot.y = y;
        spot.z = z;
        spot.distance = std::hypot(dx, dy);
        spot.awayGain = std::hypot(attacker->GetPositionX() - x, attacker->GetPositionY() - y) - attackerFlat;
        spot.sees = attacker->IsWithinLOS(x, y, z + STEP_SPOT_EYE_HEIGHT);
        for (Mob const& mob : others)
        {
            if (std::hypot(mob.x - x, mob.y - y) < clearance)
            {
                spot.nearOtherMob = true;
                break;
            }
        }
    }

    float previousX = 0.0f, previousY = 0.0f;
    bool const hasPrevious = FloorAttackerPolicy::PreviousSpot(order, attackerGuid, now, previousX, previousY);
    unsigned shortlist[FloorAttackerPolicy::PATH_SEARCHES];
    unsigned const found = FloorAttackerPolicy::Shortlist(spots, count, hasPrevious, previousX, previousY, shortlist);
    for (unsigned k = 0; k < found; ++k)
    {
        FloorAttackerPolicy::Spot const& spot = spots[shortlist[k]];
        PathGenerator path(bot);
        path.CalculatePath(spot.x, spot.y, spot.z, false);
        uint32 const type = uint32(path.GetPathType());
        bool const complete = (type & PATHFIND_NORMAL) &&
            !(type & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_SHORT));
        if (!FloorAttackerPolicy::PathAccepted(complete, PathLengthOf(path), spot.distance))
            continue;

        FloorAttackerPolicy::Begin(order, attackerGuid, spot.x, spot.y, spot.z, now);
        Unit* const victim = attacker->GetVictim();
        SF_LOG_INFO("playerbots", "[RESCUE] bot=%s how=step-away attacker=%s entry=%u on=%s dz=%.1f walks %.1f yd to a "
                    "spot %.1f yd from the attacker (flat), sees=%u, try %u", bot->GetName().c_str(),
                    attacker->GetName().c_str(), attacker->GetEntry(), victim ? victim->GetName().c_str() : "none",
                    attacker->GetPositionZ() - botZ, PathLengthOf(path),
                    std::hypot(attacker->GetPositionX() - spot.x, attacker->GetPositionY() - spot.y), uint32(spot.sees),
                    order.steps);
        return true;
    }

    SF_LOG_INFO("playerbots", "[RESCUE] bot=%s how=step-away attacker=%s entry=%u finds no spot: %u on its floor, %u "
                "short-listed", bot->GetName().c_str(), attacker->GetName().c_str(), attacker->GetEntry(), count,
                found);
    return false;
}

Unit* AttackedMemberRescue::FindFloorAttackerOnMe(PlayerbotAI* botAI)
{
    Player* const bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot || PlayerBotSpec::IsTank(bot))
        return nullptr;

    time_t const now = time(nullptr);
    for (Unit* unit : bot->getAttackers())
    {
        if (!unit || !unit->ToCreature() || !unit->IsAlive() || unit->GetVictim() != bot || !bot->IsHostileTo(unit))
            continue;
        // In sight it is an ordinary fight; a mob on the bot's own floor is no floor problem.
        if (!FloorAttackerPolicy::NeedsStepAway(unit->GetPositionZ() - bot->GetPositionZ(), bot->GetExactDist2d(unit),
                                                bot->GetDistance(unit)) ||
            bot->IsWithinLOSInMap(unit))
            continue;
        if (!FloorAttackerPolicy::MaySearch(botAI->floorStep, unit->GetGUID(), now))
            continue;
        // Only after the tank gave up on this mob: until then the tank may still reach it.
        Player* const tank = FindTank(bot);
        PlayerbotAI* const tankAI = tank ? GET_PLAYERBOT_AI(tank) : nullptr;
        if (tankAI && TankRescuePolicy::GaveUpLately(tankAI->rescueGiveUps.attackers, unit->GetGUID(), now))
            return unit;
    }
    return nullptr;
}

namespace
{
// The tank and the hidden attacker the regroup walks for, when it should walk now (the policy's distance rules).
bool RegroupTargets(Player* bot, Player*& tank, Unit*& attacker, Player*& victim)
{
    tank = AttackedMemberRescue::FindTank(bot);
    attacker = tank ? AttackedMemberRescue::FindHiddenAttacker(bot, victim) : nullptr;
    return tank && attacker;
}
}  // namespace

bool MemberAttackerOutOfSightTrigger::IsActive()
{
    // Dungeons only, out of combat (a fighting healer heals first), never for a tank (the rescue is its job) and never
    // with a target of its own.
    Map* map = bot->FindMap();
    if (!map || !map->IsDungeon() || bot->InBattleground() || bot->IsInCombat() || PlayerBotSpec::IsTank(bot) ||
        AI_VALUE(Unit*, "current target"))
    {
        since = 0;
        return false;
    }

    Player* tank = nullptr;
    Unit* attacker = nullptr;
    Player* victim = nullptr;
    bool const hidden = RegroupTargets(bot, tank, attacker, victim);
    time_t const now = time(nullptr);
    since = MemberRegroupPolicy::NextSince(hidden, since, now);
    return hidden && MemberRegroupPolicy::ShouldWalk(since, now, bot->GetDistance(tank), bot->GetDistance(attacker),
                                                     tank->GetDistance(attacker));
}

bool MoveToTankForHiddenAttackerAction::isUseful()
{
    return !botAI->HasStrategy("stay", botAI->GetState()) && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
}

bool MoveToTankForHiddenAttackerAction::Execute(Event /*event*/)
{
    Player* tank = nullptr;
    Unit* attacker = nullptr;
    Player* victim = nullptr;
    if (!RegroupTargets(bot, tank, attacker, victim))
        return false;

    if (loggedFor != attacker->GetGUID())
    {
        loggedFor = attacker->GetGUID();
        SF_LOG_INFO("playerbots", "[DEFEND] %s moves to the tank %s at %.1f yd: %s (entry %u) attacks %s out of sight "
                    "at %.1f yd", bot->GetName().c_str(), tank->GetName().c_str(), bot->GetDistance(tank),
                    attacker->GetName().c_str(), attacker->GetEntry(), victim->GetName().c_str(),
                    bot->GetDistance(attacker));
    }
    return MoveTo(tank, MemberRegroupPolicy::TANK_SIDE_YARDS);
}

bool FloorAttackerStepAwayTrigger::IsActive()
{
    // Dungeons and raids only, like the rescue; a bot in a vehicle does not walk on its own.
    Map* map = bot->FindMap();
    if (!bot->IsAlive() || !map || !map->IsDungeon() || bot->InBattleground() || bot->GetVehicle())
        return false;
    if (FloorAttackerPolicy::Live(botAI->floorStep, time(nullptr)))
        return true;
    // The tank orders its walk itself, at its rescue give-up; a member finds its own cause here.
    return AttackedMemberRescue::FindFloorAttackerOnMe(botAI) != nullptr;
}

bool StepAwayFromFloorAttackerAction::isUseful()
{
    return !botAI->HasStrategy("stay", botAI->GetState()) && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);
}

bool StepAwayFromFloorAttackerAction::Execute(Event /*event*/)
{
    time_t const now = time(nullptr);
    FloorAttackerPolicy::Order& order = botAI->floorStep;
    if (!FloorAttackerPolicy::Live(order, now))
    {
        Unit* const cause = AttackedMemberRescue::FindFloorAttackerOnMe(botAI);
        if (!cause || !AttackedMemberRescue::BeginStepAway(botAI, cause))
            return false;
    }

    Unit* const attacker = ObjectAccessor::GetUnit(*bot, order.attacker);
    // The mob came into sight (the rescue takes it from here) or is gone: the walk has done its job.
    if (!attacker || !attacker->IsAlive() || bot->IsWithinLOSInMap(attacker))
    {
        FloorAttackerPolicy::End(order);
        return false;
    }
    // Any other attacker of this bot in sight is an ordinary fight: it comes first.
    for (Unit* unit : bot->getAttackers())
    {
        if (unit && unit->IsAlive() && bot->IsWithinLOSInMap(unit))
        {
            FloorAttackerPolicy::End(order);
            return false;
        }
    }
    // BT-403: for the tank, another mob on a member is an ordinary rescue too, and this action (above the rescue and the
    // rotation) must not hold or walk it away from that. The order stays live, so FindAttacker keeps leaving the mob on
    // the other floor alone; the walk goes on once the other fight is over.
    if (PlayerBotSpec::IsTank(bot) && AttackedMemberRescue::FindAttacker(bot))
        return false;
    if (FloorAttackerPolicy::Arrived(order, bot->GetPositionX(), bot->GetPositionY()))
    {
        // The tank holds the spot until the order runs out, so its default melee does not walk it back under the mob
        // (a member's heals and casts run below this action and must not be starved).
        return PlayerBotSpec::IsTank(bot);
    }
    return MoveTo(bot->GetMapId(), order.x, order.y, order.z, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool PartyMemberAttackedOutOfSightTrigger::IsActive() { return AttackedMemberRescue::FindAttacker(bot) != nullptr; }

bool ReachAttackedMemberAction::isUseful()
{
    Unit* attacker = AttackedMemberRescue::FindAttacker(bot);
    if (!attacker)
    {
        rescueGuid = 0;
        return false;
    }
    // BT-198: an attacker given up on is set aside inside FindAttacker, so a pick is always worth the walk.
    return true;
}

// BT-174: the run of 2026-11-13 could not say whether a rescue fired (only the client's view of a taunt cast). The line
// names the attacker, whom it hangs on, the distance and the way it was taken: area, ranged spell, melee or walking.
// BT-180: `victim` is read before any taunt; after one the attacker's victim is the tank (Deadmines run 4: all 16
// ranged lines read on=<the tank>).
// BT-199: `detail` is appended to the line (the give-up reason); it takes part in no comparison.
void ReachAttackedMemberAction::LogRescue(Unit* attacker, Unit* victim, char const* how, char const* detail)
{
    time_t const now = time(nullptr);
    if (loggedGuid == attacker->GetGUID() && std::strcmp(loggedHow, how) == 0 && now - loggedAt < RESCUE_LOG_GAP_SECONDS)
        return;
    loggedGuid = attacker->GetGUID();
    loggedHow = how;
    loggedAt = now;
    SF_LOG_INFO("playerbots", "[RESCUE] bot=%s how=%s attacker=%s entry=%u on=%s dist=%.1f los=%u%s", bot->GetName().c_str(),
                how, attacker->GetName().c_str(), attacker->GetEntry(), victim ? victim->GetName().c_str() : "none",
                bot->GetDistance(attacker), uint32(bot->IsWithinLOSInMap(attacker)), detail);
}

bool ReachAttackedMemberAction::Execute(Event /*event*/)
{
    Unit* attacker = AttackedMemberRescue::FindAttacker(bot);
    if (!attacker)
        return false;
    Unit* const victim = attacker->GetVictim();
    if (bot->IsWithinLOSInMap(attacker))
    {
        rescueGuid = 0;
        // BT-162: only count known group attackers, never scan the map.
        std::set<Unit*> loose;
        for (GroupReference* ref = bot->GetGroup()->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap())
                continue;
            for (Unit* unit : member->getAttackers())
                if (unit && unit->ToCreature() && unit->IsAlive() && unit->GetVictim() != bot &&
                    bot->IsValidAttackTarget(unit) && bot->GetDistance(unit) <= TankRescuePolicy::AreaRadius)
                    loose.insert(unit);
        }
        time_t const now = time(nullptr);
        if (TankRescuePolicy::UseArea(loose.size(), now >= nextAreaAttempt) &&
            bot->IsWithinMeleeRange(attacker) && TryArea(botAI, bot, attacker))
        {
            nextAreaAttempt = now + TankRescuePolicy::AreaRetrySeconds;
            LogRescue(attacker, victim, "area");
            return true;
        }
        if (TryRanged(botAI, bot, attacker))
        {
            LogRescue(attacker, victim, "ranged");
            return true;
        }
        LogRescue(attacker, victim, "melee");
        return Attack(attacker);
    }

    time_t const now = time(nullptr);
    if (rescueGuid != attacker->GetGUID())
    {
        rescueGuid = attacker->GetGUID();
        rescueSince = now;
        rescueStartZ = bot->GetPositionZ();
    }
    if (now - rescueSince >= TankRescuePolicy::WalkSeconds)
    {
        rescueGuid = 0;
        // BT-198: the next pick skips this attacker (and ranks its victim's other attackers last) instead of the whole
        // action waiting, so a member the tank can reach is taken in the meantime.
        TankRescuePolicy::GiveUpBook& book = botAI->rescueGiveUps;
        bool const ignored = TankRescuePolicy::RecordGiveUp(book.attackers, attacker->GetGUID(), now);
        if (victim)
            TankRescuePolicy::RecordGiveUp(book.victims, victim->GetGUID(), now);

        // One path query per give-up (at most every few seconds), never per tick.
        PathGenerator path(bot);
        path.CalculatePath(attacker->GetPositionX(), attacker->GetPositionY(), attacker->GetPositionZ(), false);
        uint32 const pathType = uint32(path.GetPathType());
        float const heightDifference = attacker->GetPositionZ() - bot->GetPositionZ();
        TankRescuePolicy::GiveUpReason const reason = TankRescuePolicy::ClassifyGiveUp(
            (pathType & uint32(PATHFIND_NOPATH)) != 0, (pathType & uint32(PATHFIND_INCOMPLETE)) != 0,
            bot->GetDistance(attacker), heightDifference);
        // BT-388: why a bot ends on a ledge the mesh does not join. Shadowfang Keep rerun: the tank rose from 90.6 to
        // 98.9 within 4 s of a give-up and nothing logged the cause; the give-up does not stop the walk's spline,
        // which may run on up a route to the mesh point nearest the attacker. The line gives the tank's spot, whether
        // it still moves, how far it rose in the walk, and where the path ended (gap to the attacker, height over
        // the tank).
        Movement::PointsArray const& points = path.GetPath();
        float pathEndGap = -1.0f, pathEndDz = 0.0f;
        if (!points.empty())
        {
            G3D::Vector3 const& end = points.back();
            pathEndGap = std::sqrt((end.x - attacker->GetPositionX()) * (end.x - attacker->GetPositionX()) +
                                   (end.y - attacker->GetPositionY()) * (end.y - attacker->GetPositionY()) +
                                   (end.z - attacker->GetPositionZ()) * (end.z - attacker->GetPositionZ()));
            pathEndDz = end.z - bot->GetPositionZ();
        }
        char detail[220];
        std::snprintf(detail, sizeof(detail),
                      " reason=%s dz=%.1f walked=%ds aside=%ds at=(%.1f, %.1f, %.1f) moving=%u rose=%.1f "
                      "path_end_gap=%.1f path_end_dz=%.1f", TankRescuePolicy::ReasonName(reason), heightDifference,
                      int(TankRescuePolicy::WalkSeconds),
                      int(ignored ? TankRescuePolicy::IgnoreSeconds : TankRescuePolicy::RetrySeconds),
                      bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), uint32(bot->isMoving()),
                      bot->GetPositionZ() - rescueStartZ, pathEndGap, pathEndDz);
        LogRescue(attacker, victim, "gave up", detail);
        // BT-377: an attacker on another floor over its victim, out of reach and out of sight: step away from it so it
        // has to come down (or up) to us, instead of walking toward it again in a minute.
        if (victim && FloorAttackerPolicy::NeedsStepAway(heightDifference, attacker->GetExactDist2d(victim),
                                                         bot->GetDistance(attacker)))
            AttackedMemberRescue::BeginStepAway(botAI, attacker);
        return false;
    }
    LogRescue(attacker, victim, "walking");
    return MoveTo(attacker, RESCUE_APPROACH_DISTANCE);
}
