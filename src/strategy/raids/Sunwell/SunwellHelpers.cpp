/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SunwellHelpers.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "RaidBotHelpers.h"
#include "Spell.h"
#include <array>
#include <list>

using RaidGeometry::Circle;
using RaidGeometry::Point;

namespace
{
Point ToPoint(WorldObject const* object) { return { object->GetPositionX(), object->GetPositionY() }; }

void AddCreatureHazards(Player* bot, uint32 entry, float radius, std::vector<Circle>& hazards,
    uint32 requiredAura = 0)
{
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, SunwellHelpers::HAZARD_SCAN_RADIUS);
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive() || (requiredAura && !creature->HasAura(requiredAura)))
            continue;

        hazards.push_back({ ToPoint(creature), radius + SunwellHelpers::HAZARD_PADDING });
    }
}

// Raid members carrying a debuff that hurts whoever stands near them: the others keep `radius` away from the
// carrier, and a carrier keeps `radius` away from everyone else.
void AddDebuffSpreadHazards(Player* bot, uint32 spellId, float radius, std::vector<Circle>& hazards)
{
    Group* group = bot->GetGroup();
    if (!group)
        return;

    bool const carrier = bot->HasAura(spellId);
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            bot->GetExactDist2d(member) > SunwellHelpers::HAZARD_SCAN_RADIUS)
        {
            continue;
        }

        if (carrier || member->HasAura(spellId))
            hazards.push_back({ ToPoint(member), radius + SunwellHelpers::HAZARD_PADDING });
    }
}

Creature* FindLowestHealthAdd(Player* bot, uint32 entry);

// Kil'jaeden's Sinister Reflections: the one on a healer first, then on a caster, then the weakest.
Creature* GetReflectionToKill(Player* bot)
{
    constexpr float reflectionScanRadius = 60.0f;
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, SunwellHelpers::NPC_SINISTER_REFLECTION, reflectionScanRadius);

    Creature* best = nullptr;
    int bestRank = 0;
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive() || !bot->IsValidAttackTarget(creature))
            continue;

        Player* victim = creature->GetVictim() ? creature->GetVictim()->ToPlayer() : nullptr;
        int const rank = SunwellLogic::ReflectionRank(victim && PlayerBotSpec::IsHeal(victim),
            victim && PlayerBotSpec::IsRanged(victim));
        if (!best || SunwellLogic::ReflectionBefore(rank, creature->GetHealthPct(), bestRank, best->GetHealthPct()))
        {
            best = creature;
            bestRank = rank;
        }
    }

    return best;
}

Creature* FindLowestHealthAdd(Player* bot, uint32 entry)
{
    constexpr float addScanRadius = 60.0f;
    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, addScanRadius);

    Creature* best = nullptr;
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive() || creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) ||
            !bot->IsValidAttackTarget(creature))
        {
            continue;
        }

        if (!best || creature->GetHealth() < best->GetHealth())
            best = creature;
    }

    return best;
}
}

namespace SunwellHelpers
{

namespace
{
struct EncounterLook
{
    SunwellEncounter encounter = SunwellEncounter::None;
    uint32 lastLookMs = 0;
    bool looked = false;
};

// Kept per bot (by its low GUID): what one bot sees from where it stands is its own view of the fight.
RaidBotHelpers::InstanceStateStore<EncounterLook> encounterLooks;

// What the raid has seen of Felmyst in one instance: where she stood on the ground and the lane she faced before a
// breath. Any bot of the instance may look; all of them stand on the same floor.
struct FelmystWatch
{
    bool looked = false;
    uint32 lastLookMs = 0;
    bool seenGround = false;
    RaidGeometry::Point rally{ 0.0f, 0.0f };
    SunwellLogic::FelmystPhase phase = SunwellLogic::FelmystPhase::Ground;
    bool hasLane = false;
    RaidGeometry::Point laneStart{ 0.0f, 0.0f };
    float laneFacing = 0.0f;
    uint32 laneSeenMs = 0;
};

RaidBotHelpers::InstanceStateStore<FelmystWatch> felmystWatches;
constexpr uint32 FELMYST_LOOK_INTERVAL_MS = 500;

// Many triggers and multipliers ask every tick, so one look stands for a second.
constexpr uint32 ENCOUNTER_LOOK_INTERVAL_MS = 1000;
constexpr float ENCOUNTER_SCAN_RADIUS = 150.0f;

struct EncounterUnits
{
    SunwellEncounter encounter;
    std::array<uint32, 2> entries;
};

// The units that fight in each encounter, in the order of SunwellEncounter after None (0 = none). Kil'jaeden's fight
// begins with the Hands of the Deceiver, before he rises.
constexpr std::array<EncounterUnits, 6> ENCOUNTER_UNITS = {{
    { SunwellEncounter::Kalecgos, { NPC_KALECGOS, NPC_SATHROVARR } },
    { SunwellEncounter::Brutallus, { NPC_BRUTALLUS, 0 } },
    { SunwellEncounter::Felmyst, { NPC_FELMYST, 0 } },
    { SunwellEncounter::EredarTwins, { NPC_LADY_SACROLASH, NPC_GRAND_WARLOCK_ALYTHESS } },
    { SunwellEncounter::Muru, { NPC_MURU, NPC_ENTROPIUS } },
    { SunwellEncounter::Kiljaeden, { NPC_KILJAEDEN, NPC_HAND_OF_THE_DECEIVER } },
}};
}

// The fight going on, as a player sees it: one of its bosses near the raid and in combat. The core's private instance
// data (each boss's IN_PROGRESS state) is not read.
SunwellEncounter GetActiveEncounter(Player* bot)
{
    if (bot->GetMapId() != SWP_MAP_ID)
        return SunwellEncounter::None;

    EncounterLook& look = encounterLooks.Get(bot->GetGUIDLow());
    uint32 const now = getMSTime();
    if (look.looked && getMSTimeDiff(look.lastLookMs, now) < ENCOUNTER_LOOK_INTERVAL_MS)
        return look.encounter;

    look.looked = true;
    look.lastLookMs = now;
    look.encounter = SunwellEncounter::None;
    for (EncounterUnits const& units : ENCOUNTER_UNITS)
    {
        for (uint32 const entry : units.entries)
        {
            if (!entry)
                continue;

            Creature* creature = bot->FindNearestCreature(entry, ENCOUNTER_SCAN_RADIUS, true);
            if (creature && creature->IsInCombat())
            {
                look.encounter = units.encounter;
                return look.encounter;
            }
        }
    }

    return look.encounter;
}

namespace
{
bool AnyVaporOut(Unit* felmyst)
{
    constexpr float vaporScanRadius = 100.0f;
    std::list<Creature*> creatures;
    felmyst->GetCreatureListWithEntryInGrid(creatures, NPC_DEMONIC_VAPOR, vaporScanRadius);
    for (Creature* creature : creatures)
        if (creature && creature->IsAlive() && creature->IsVisible())
            return true;

    creatures.clear();
    felmyst->GetCreatureListWithEntryInGrid(creatures, NPC_DEMONIC_VAPOR_TRAIL, vaporScanRadius);
    for (Creature* creature : creatures)
        if (creature && creature->IsAlive() && creature->IsVisible() && creature->HasAura(SPELL_VAPOR_TRAIL))
            return true;

    return false;
}

// Looks at Felmyst at most twice a second per instance: her height over the bot, whether a vapor is out, whether she
// moves or wears Speed Burst, and where she faces while she hovers.
FelmystWatch& WatchFelmyst(Player* bot)
{
    FelmystWatch& watch = felmystWatches.Get(bot->GetInstanceId());
    uint32 const now = getMSTime();
    if (watch.looked && getMSTimeDiff(watch.lastLookMs, now) < FELMYST_LOOK_INTERVAL_MS)
        return watch;

    watch.looked = true;
    watch.lastLookMs = now;
    Unit* felmyst = FindBoss(bot, NPC_FELMYST);
    if (!felmyst || !felmyst->IsInCombat())
    {
        watch = FelmystWatch{};
        watch.looked = true;
        watch.lastLookMs = now;
        return watch;
    }

    float const height = felmyst->GetPositionZ() - bot->GetPositionZ();
    SunwellLogic::FelmystPhase phase = SunwellLogic::ClassifyFelmyst(height, AnyVaporOut(felmyst),
        felmyst->HasAura(SPELL_SPEED_BURST), felmyst->isMoving());

    // Before she first comes down she hovers where she was summoned: that is no flight phase.
    if (phase == SunwellLogic::FelmystPhase::Ground)
    {
        watch.seenGround = true;
        watch.rally = ToPoint(felmyst);
    }
    else if (!watch.seenGround)
        phase = SunwellLogic::FelmystPhase::Ground;

    if (phase == SunwellLogic::FelmystPhase::AirBreath && !felmyst->isMoving())
    {
        watch.hasLane = true;
        watch.laneStart = ToPoint(felmyst);
        watch.laneFacing = felmyst->GetOrientation();
        watch.laneSeenMs = now;
    }
    else if (phase == SunwellLogic::FelmystPhase::AirBreath)
    {
        // Crossing without a lane seen while she hovered: the rest of the lane lies ahead of her.
        if (!watch.hasLane || getMSTimeDiff(watch.laneSeenMs, now) > SunwellLogic::BREATH_LANE_MEMORY_MS)
        {
            watch.hasLane = true;
            watch.laneStart = ToPoint(felmyst);
            watch.laneFacing = felmyst->GetOrientation();
            watch.laneSeenMs = now;
        }
    }
    else
        watch.hasLane = false;

    watch.phase = phase;
    return watch;
}
}

SunwellLogic::FelmystPhase GetFelmystPhase(Player* bot)
{
    if (GetActiveEncounter(bot) != SunwellEncounter::Felmyst)
        return SunwellLogic::FelmystPhase::Ground;
    return WatchFelmyst(bot).phase;
}

bool GetFelmystRally(Player* bot, Point& rally)
{
    if (GetActiveEncounter(bot) != SunwellEncounter::Felmyst)
        return false;

    FelmystWatch const& watch = WatchFelmyst(bot);
    if (!watch.seenGround)
        return false;

    rally = watch.rally;
    return true;
}

Unit* GetChaser(Player* bot)
{
    uint32 entry = 0;
    switch (GetActiveEncounter(bot))
    {
        case SunwellEncounter::Felmyst:
            entry = NPC_DEMONIC_VAPOR;
            break;
        case SunwellEncounter::Kiljaeden:
            entry = NPC_VOLATILE_FELFIRE_FIEND;
            break;
        default:
            return nullptr;
    }

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, CHASER_SCAN_RADIUS);
    Creature* nearest = nullptr;
    for (Creature* creature : creatures)
    {
        if (!creature || !creature->IsAlive() || creature->GetVictim() != bot)
            continue;
        if (!nearest || bot->GetExactDist2d(creature) < bot->GetExactDist2d(nearest))
            nearest = creature;
    }

    return nearest;
}

Point GetRaidCenter(Player* bot)
{
    Point center{ 0.0f, 0.0f };
    int count = 0;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
                bot->GetExactDist2d(member) > HAZARD_SCAN_RADIUS)
            {
                continue;
            }

            center.x += member->GetPositionX();
            center.y += member->GetPositionY();
            ++count;
        }
    }

    if (!count)
        return ToPoint(bot);

    return { center.x / float(count), center.y / float(count) };
}

namespace
{
int MeteorStacks(Unit* unit)
{
    Aura const* aura = unit->GetAura(SPELL_METEOR_SLASH);
    return aura ? aura->GetStackAmount() : 0;
}

struct SoakTeams
{
    int stacks[2] = { 0, 0 };
    bool available[2] = { false, false };
    int botTeam = -1;
    int botSlot = 0;
    int teamSize[2] = { 0, 0 };
};

// The damage dealers of the group in group order, dealt into two teams in turn; the order is the same for every bot,
// and members who die or catch Burn keep their team.
SoakTeams GetSoakTeams(Player* bot, Group* group)
{
    SoakTeams teams;
    int index = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->GetMap() != bot->GetMap() || PlayerBotSpec::IsTank(member) ||
            PlayerBotSpec::IsHeal(member))
        {
            continue;
        }

        int const team = index++ % 2;
        if (!member->IsAlive())
            continue;

        teams.stacks[team] = std::max(teams.stacks[team], MeteorStacks(member));
        if (member->HasAura(SPELL_BURN))
            continue;

        if (member == bot)
        {
            teams.botTeam = team;
            teams.botSlot = teams.teamSize[team];
        }
        teams.available[team] = true;
        ++teams.teamSize[team];
    }

    return teams;
}

// The first living tank in group order that Brutallus is not hitting: the one who takes over at the swap.
Player* GetSecondTank(Player* bot, Group* group, Unit* victim)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != victim && member->IsAlive() && member->GetMap() == bot->GetMap() &&
            PlayerBotSpec::IsTank(member))
        {
            return member;
        }
    }

    return nullptr;
}
}

MeteorSlashPlan GetMeteorSlashPlan(Player* bot)
{
    MeteorSlashPlan plan;
    if (GetActiveEncounter(bot) != SunwellEncounter::Brutallus || !bot->IsAlive())
        return plan;

    Group* group = bot->GetGroup();
    Unit* boss = FindBoss(bot, NPC_BRUTALLUS);
    if (!group || !boss || !boss->IsInCombat())
        return plan;

    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return plan;

    plan.boss = boss;
    Point const bossPoint = ToPoint(boss);
    float const facing = boss->GetOrientation();
    float const meleeDistance = RaidBotHelpers::MeleeRange(bot, boss) * 0.7f;
    plan.role = MeteorSlashPlan::Role::Avoid;

    if (PlayerBotSpec::IsTank(bot))
    {
        // Only a tank Brutallus hits has a turn to hand over; when someone else holds him, the tanks pick him up
        // through their normal threat play.
        Player* activeTank = victim ? victim->ToPlayer() : nullptr;
        if (!activeTank || !PlayerBotSpec::IsTank(activeTank) || GetSecondTank(bot, group, victim) != bot ||
            bot->HasAura(SPELL_BURN))
        {
            return plan;
        }

        if (SunwellLogic::ChooseSoaker(MeteorStacks(activeTank), MeteorStacks(bot), SunwellLogic::TANK_SWAP_STACKS,
                true, true) == 1)
        {
            plan.role = MeteorSlashPlan::Role::TakeOver;
            plan.spot = SunwellLogic::SoakSpot(bossPoint, facing, 0, 1, meleeDistance);
        }
        return plan;
    }

    if (PlayerBotSpec::IsHeal(bot) || bot->HasAura(SPELL_BURN))
        return plan;

    SoakTeams const teams = GetSoakTeams(bot, group);
    int const soakTeam = SunwellLogic::ChooseSoaker(teams.stacks[0], teams.stacks[1], SunwellLogic::SOAK_SWAP_STACKS,
        teams.available[0], teams.available[1]);
    if (teams.botTeam < 0 || teams.botTeam != soakTeam)
        return plan;

    plan.role = MeteorSlashPlan::Role::Soak;
    float const distance = PlayerBotSpec::IsMelee(bot) ? meleeDistance : SunwellLogic::RANGED_SOAK_DISTANCE;
    plan.spot = SunwellLogic::SoakSpot(bossPoint, facing, teams.botSlot, teams.teamSize[teams.botTeam], distance);
    return plan;
}

bool MeteorPlanNeedsMove(Player* bot, MeteorSlashPlan const& plan)
{
    if (!plan.boss || plan.role == MeteorSlashPlan::Role::None)
        return false;

    // A soaker stands well inside the edge, so the boss turning a little does not push it out.
    constexpr float innerMargin = -RaidGeometry::PI / 18.0f;
    constexpr float soakSpotTolerance = 5.0f;
    Point const bossPoint = ToPoint(plan.boss);
    float const facing = plan.boss->GetOrientation();
    Point const position = ToPoint(bot);
    float const toSpot = std::hypot(position.x - plan.spot.x, position.y - plan.spot.y);

    switch (plan.role)
    {
        case MeteorSlashPlan::Role::Soak:
            return !SunwellLogic::InMeteorCone(bossPoint, facing, position, innerMargin) || toSpot > soakSpotTolerance;
        case MeteorSlashPlan::Role::Avoid:
            return SunwellLogic::InMeteorCone(bossPoint, facing, position, SunwellLogic::METEOR_CONE_MARGIN);
        case MeteorSlashPlan::Role::TakeOver:
            return !SunwellLogic::InMeteorCone(bossPoint, facing, position, innerMargin) ||
                bot->GetExactDist2d(plan.boss) > RaidBotHelpers::MeleeRange(bot, plan.boss);
        default:
            return false;
    }
}

Unit* FindBoss(Player* bot, uint32 entry)
{
    constexpr float bossScanRadius = 150.0f;
    return bot->FindNearestCreature(entry, bossScanRadius, true);
}

bool InSpectralRealm(Player* bot)
{
    return bot->HasAura(SPELL_SPECTRAL_REALM) || bot->GetPositionZ() < SPECTRAL_REALM_MAX_Z;
}

std::vector<Circle> GetHazards(Player* bot)
{
    std::vector<Circle> hazards;
    if (!bot->IsInCombat())
        return hazards;

    switch (GetActiveEncounter(bot))
    {
        case SunwellEncounter::Brutallus:
            AddDebuffSpreadHazards(bot, SPELL_BURN, BURN_SPREAD_DISTANCE, hazards);
            break;
        case SunwellEncounter::Felmyst:
            // Only the others leave the Encapsulate target (it cannot move).
            if (!bot->HasAura(SPELL_ENCAPSULATE))
            {
                if (Group* group = bot->GetGroup())
                {
                    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    {
                        Player* member = ref->GetSource();
                        if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
                            member->HasAura(SPELL_ENCAPSULATE))
                        {
                            hazards.push_back({ ToPoint(member), ENCAPSULATE_RADIUS + HAZARD_PADDING });
                        }
                    }
                }
            }
            AddCreatureHazards(bot, NPC_DEMONIC_VAPOR, DEMONIC_VAPOR_RADIUS, hazards);
            AddCreatureHazards(bot, NPC_DEMONIC_VAPOR_TRAIL, FOG_RADIUS, hazards, SPELL_FOG_OF_CORRUPTION);
            AddCreatureHazards(bot, NPC_DEMONIC_VAPOR_TRAIL, VAPOR_TRAIL_RADIUS, hazards, SPELL_VAPOR_TRAIL);
            // The lane she faces before a breath: every fog cloud she drops on it charms within 20 yd.
            if (FelmystWatch const& watch = WatchFelmyst(bot); watch.phase == SunwellLogic::FelmystPhase::AirBreath &&
                watch.hasLane)
            {
                std::vector<Circle> const lane = SunwellLogic::BreathLaneCircles(watch.laneStart, watch.laneFacing,
                    SunwellLogic::BREATH_LANE_LENGTH, FOG_RADIUS + 2.0f * HAZARD_PADDING,
                    SunwellLogic::BREATH_LANE_SPACING);
                hazards.insert(hazards.end(), lane.begin(), lane.end());
            }
            break;
        case SunwellEncounter::Muru:
            for (Position const& darkness :
                 EncounterHelpers::GetDynamicObjectPositions(bot, HAZARD_SCAN_RADIUS, SPELL_DARKNESS))
            {
                hazards.push_back(
                    { { darkness.GetPositionX(), darkness.GetPositionY() }, DARKNESS_RADIUS + HAZARD_PADDING });
            }
            AddCreatureHazards(bot, NPC_SINGULARITY, SINGULARITY_RADIUS, hazards);
            // Dark Fiends explode on whoever they reach: the tanks hold them, everyone else keeps clear while they die.
            if (!PlayerBotSpec::IsTank(bot))
                AddCreatureHazards(bot, NPC_DARK_FIEND, EXPLODING_ADD_RADIUS, hazards);
            break;
        case SunwellEncounter::Kiljaeden:
            AddDebuffSpreadHazards(bot, SPELL_FIRE_BLOOM, FIRE_BLOOM_RADIUS, hazards);
            AddCreatureHazards(bot, NPC_ARMAGEDDON_TARGET, ARMAGEDDON_RADIUS, hazards);
            AddCreatureHazards(bot, NPC_SPIKE_TARGET, SHADOW_SPIKE_RADIUS, hazards);
            AddCreatureHazards(bot, NPC_SPIKE_TARGET_2, SHADOW_SPIKE_RADIUS, hazards);
            // A fiend's own target kites it instead (GetChaser); the others keep out of its Felfire Fission.
            if (!PlayerBotSpec::IsTank(bot))
            {
                std::list<Creature*> fiends;
                bot->GetCreatureListWithEntryInGrid(fiends, NPC_VOLATILE_FELFIRE_FIEND, HAZARD_SCAN_RADIUS);
                for (Creature* fiend : fiends)
                    if (fiend && fiend->IsAlive() && fiend->GetVictim() != bot)
                        hazards.push_back({ ToPoint(fiend), FELFIRE_FISSION_RADIUS + HAZARD_PADDING });
            }
            break;
        default:
            break;
    }

    return hazards;
}

bool IsInHazard(Player* bot, std::vector<Circle> const& hazards)
{
    // The padding only keeps a bot from stepping back in; it does not move a bot that stands just inside it.
    Point const position = ToPoint(bot);
    for (Circle const& hazard : hazards)
        if (std::hypot(position.x - hazard.center.x, position.y - hazard.center.y) < hazard.radius - HAZARD_PADDING)
            return true;
    return false;
}

Unit* GetKalecgosConeThreat(Player* bot)
{
    if (GetActiveEncounter(bot) != SunwellEncounter::Kalecgos || PlayerBotSpec::IsTank(bot) || InSpectralRealm(bot))
        return nullptr;

    Unit* kalecgos = FindBoss(bot, NPC_KALECGOS);
    if (!kalecgos || !kalecgos->IsInCombat() || kalecgos->GetVictim() == bot ||
        bot->GetExactDist2d(kalecgos) > KALECGOS_CONE_RANGE + kalecgos->GetCombatReach())
    {
        return nullptr;
    }

    Point const boss = ToPoint(kalecgos);
    Point const position = ToPoint(bot);
    float const facing = kalecgos->GetOrientation();
    bool const inCone = RaidGeometry::InFrontCone(boss, facing, position, KALECGOS_CONE_WIDTH) ||
        RaidGeometry::InRearCone(boss, facing, position, KALECGOS_CONE_WIDTH);

    return inCone ? kalecgos : nullptr;
}

Unit* GetPriorityAdd(Player* bot)
{
    std::vector<uint32> order;
    switch (GetActiveEncounter(bot))
    {
        case SunwellEncounter::Felmyst:
            order = { NPC_UNYIELDING_DEAD };
            break;
        case SunwellEncounter::EredarTwins:
            order = { NPC_SHADOW_IMAGE };
            break;
        case SunwellEncounter::Muru:
            order = { NPC_DARK_FIEND, NPC_VOID_SENTINEL, NPC_SHADOWSWORD_FURY_MAGE, NPC_SHADOWSWORD_BERSERKER,
                NPC_VOID_SPAWN };
            break;
        case SunwellEncounter::Kiljaeden:
        {
            // Shield Orbs circle 17 yd above the floor and a fiend explodes on whoever stands by it: both are for the
            // ranged. Felfire Portals cannot be attacked in the core.
            if (PlayerBotSpec::IsRanged(bot))
            {
                if (Creature* orb = FindLowestHealthAdd(bot, NPC_SHIELD_ORB))
                    return orb;
                if (Creature* fiend = FindLowestHealthAdd(bot, NPC_VOLATILE_FELFIRE_FIEND))
                    return fiend;
            }
            return GetReflectionToKill(bot);
        }
        default:
            return nullptr;
    }

    for (uint32 entry : order)
        if (Creature* add = FindLowestHealthAdd(bot, entry))
            return add;

    return nullptr;
}

Unit* GetInterruptTarget(Player* bot)
{
    SunwellEncounter const encounter = GetActiveEncounter(bot);
    if (encounter != SunwellEncounter::Kalecgos && encounter != SunwellEncounter::Muru)
        return nullptr;

    constexpr float interruptScanRadius = 40.0f;
    struct Cast
    {
        uint32 entry;
        uint32 spellId;
    };
    static Cast const casts[] = {
        { NPC_SATHROVARR, SPELL_SATHROVARR_VOLLEY },
        { NPC_SHADOWSWORD_FURY_MAGE, SPELL_FEL_FIREBALL },
        { NPC_VOID_SENTINEL, SPELL_VOID_BLAST },
    };

    for (Cast const& cast : casts)
    {
        std::list<Creature*> creatures;
        bot->GetCreatureListWithEntryInGrid(creatures, cast.entry, interruptScanRadius);
        for (Creature* creature : creatures)
            if (creature && creature->IsAlive() && creature->FindCurrentSpellBySpellId(cast.spellId))
                return creature;
    }

    return nullptr;
}

float GetRangedSpreadDistance(Player* bot)
{
    switch (GetActiveEncounter(bot))
    {
        case SunwellEncounter::EredarTwins:
            return TWINS_RANGED_SPREAD;
        case SunwellEncounter::Kiljaeden:
            return KILJAEDEN_RANGED_SPREAD;
        default:
            return 0.0f;
    }
}

std::vector<Circle> GetSpreadCircles(Player* bot, float distance)
{
    std::vector<Circle> circles;
    Group* group = bot->GetGroup();
    if (!group || distance <= 0.0f)
        return circles;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
            bot->GetExactDist2d(member) < distance + HAZARD_PADDING)
        {
            circles.push_back({ ToPoint(member), distance });
        }
    }

    return circles;
}

Unit* GetAssignedTwin(Player* bot)
{
    if (GetActiveEncounter(bot) != SunwellEncounter::EredarTwins)
        return nullptr;

    if (PlayerBotSpec::IsMainTank(bot))
        return FindBoss(bot, NPC_LADY_SACROLASH);

    if (RaidBotHelpers::IsAssistTankOfIndex(bot, 0, true))
        return FindBoss(bot, NPC_GRAND_WARLOCK_ALYTHESS);

    return nullptr;
}

Unit* GetPyrogenicsTarget(Player* bot)
{
    switch (bot->getClass())
    {
        case CLASS_MAGE:
        case CLASS_SHAMAN:
        case CLASS_PRIEST:
        case CLASS_HUNTER:
            break;
        default:
            return nullptr;
    }

    if (GetActiveEncounter(bot) != SunwellEncounter::EredarTwins)
        return nullptr;

    Unit* alythess = FindBoss(bot, NPC_GRAND_WARLOCK_ALYTHESS);
    return alythess && alythess->HasAura(SPELL_PYROGENICS) ? alythess : nullptr;
}

}
