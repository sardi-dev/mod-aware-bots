/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ToCTriggers.h"

#include <algorithm>

#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "RaidRoles.h"

using ToCLogic::Point;
using ToCLogic::Twin;

namespace
{
Point PointOf(WorldObject const* object)
{
    return {object->GetPositionX(), object->GetPositionY()};
}

std::vector<Point> PointsOf(std::vector<Unit*> const& units)
{
    std::vector<Point> points;
    points.reserve(units.size());
    for (Unit* unit : units)
        points.push_back(PointOf(unit));
    return points;
}

float HealthPct(Unit* unit)
{
    return static_cast<float>(unit->GetHealthPct());
}

bool IsImmuneChampion(Unit* champion)
{
    return champion->HasAura(ToC::SPELL_DIVINE_SHIELD) || champion->HasAura(ToC::SPELL_ICE_BLOCK) ||
           champion->HasAura(ToC::SPELL_DETERRENCE);
}

bool IsTwinPactCast(Unit* twin)
{
    return twin->HasUnitState(UNIT_STATE_CASTING) && (twin->FindCurrentSpellBySpellId(ToC::SPELL_DARK_TWIN_PACT) ||
                                                      twin->FindCurrentSpellBySpellId(ToC::SPELL_LIGHT_TWIN_PACT));
}

// The first attackable unit of a list, or null
Unit* FirstAttackable(ToC::Scene const& scene, std::vector<Unit*> const& units)
{
    for (Unit* unit : units)
        if (ToC::Attackable(scene.bot, unit))
            return unit;
    return nullptr;
}

Unit* LowestHealth(ToC::Scene const& scene, std::vector<Unit*> const& units)
{
    Unit* best = nullptr;
    for (Unit* unit : units)
        if (ToC::Attackable(scene.bot, unit) && (!best || HealthPct(unit) < HealthPct(best)))
            best = unit;
    return best;
}

// Both worms: they enrage when the other dies, so the damage dealers keep their health close (boss_northrend_beasts.cpp
// SNAKES_SPECIAL). One worm: that one.
Unit* ChooseWorm(ToC::Scene const& scene, Unit* current)
{
    Unit* dread = ToC::Attackable(scene.bot, scene.dreadscale) ? scene.dreadscale : nullptr;
    Unit* acid = ToC::Attackable(scene.bot, scene.acidmaw) ? scene.acidmaw : nullptr;
    if (dread && acid)
    {
        if (current == dread)
            return ToCLogic::SwitchWorm(HealthPct(dread), HealthPct(acid)) ? acid : dread;
        if (current == acid)
            return ToCLogic::SwitchWorm(HealthPct(acid), HealthPct(dread)) ? dread : acid;
        return dread;
    }
    return dread ? dread : acid;
}

Unit* ChooseChampion(ToC::Scene const& scene)
{
    Unit* best = nullptr;
    int bestTier = ToCLogic::CHAMPION_TIER_NONE;
    for (Unit* champion : scene.champions)
    {
        if (!ToC::Attackable(scene.bot, champion) || IsImmuneChampion(champion))
            continue;
        int const tier = ToCLogic::ChampionTier(champion->GetEntry());
        if (!best || ToCLogic::ChampionBetter(tier, HealthPct(champion), bestTier, HealthPct(best)))
        {
            best = champion;
            bestTier = tier;
        }
    }
    return best;
}
}  // namespace

// The scan reads "nearest npcs" (sight distance, 60 yards by default), which covers the coliseum from its middle;
// every list is filtered by entry, so a creature of another encounter is never picked up.
ToC::Scene const& ToC::Scan(PlayerbotAI* botAI)
{
    struct Cache
    {
        Player* bot = nullptr;
        uint32 ms = 0;
        Scene scene;
    };
    static thread_local Cache cache;

    uint32 const now = getMSTime();
    if (cache.bot == botAI->GetBot() && cache.ms == now)
        return cache.scene;

    cache.bot = botAI->GetBot();
    cache.ms = now;
    cache.scene = Scene();
    Scene& scene = cache.scene;
    scene.bot = botAI->GetBot();
    AiObjectContext* context = botAI->GetAiObjectContext();
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (BotGuid const& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit || !unit->IsAlive())
            continue;

        uint32 const entry = unit->GetEntry();
        if (ToCLogic::HazardRadius(entry) > 0.0f)
            scene.hazards.push_back(unit);
        if (ToCLogic::ChampionTier(entry) < ToCLogic::CHAMPION_TIER_NONE)
            scene.champions.push_back(unit);

        switch (entry)
        {
            case NPC_GORMOK:
                scene.gormok = unit;
                break;
            case NPC_DREADSCALE:
                scene.dreadscale = unit;
                break;
            case NPC_ACIDMAW:
                scene.acidmaw = unit;
                break;
            case NPC_ICEHOWL:
                scene.icehowl = unit;
                break;
            case NPC_SNOBOLD_VASSAL:
                scene.snobolds.push_back(unit);
                break;
            case NPC_JARAXXUS:
                scene.jaraxxus = unit;
                break;
            case NPC_NETHER_PORTAL:
                scene.portals.push_back(unit);
                break;
            case NPC_INFERNAL_VOLCANO:
                scene.volcanoes.push_back(unit);
                break;
            case NPC_MISTRESS_OF_PAIN:
                scene.mistresses.push_back(unit);
                break;
            case NPC_FELFLAME_INFERNAL:
                scene.infernals.push_back(unit);
                break;
            case NPC_LIGHTBANE:
                scene.fjola = unit;
                break;
            case NPC_DARKBANE:
                scene.eydis = unit;
                break;
            case NPC_DARK_ESSENCE:
                scene.darkEssences.push_back(unit);
                break;
            case NPC_LIGHT_ESSENCE:
                scene.lightEssences.push_back(unit);
                break;
            case NPC_CONCENTRATED_DARKNESS:
                scene.darkOrbs.push_back(unit);
                break;
            case NPC_CONCENTRATED_LIGHT:
                scene.lightOrbs.push_back(unit);
                break;
            case NPC_ANUBARAK:
                scene.anubarak = unit;
                break;
            case NPC_ANUBARAK_SPIKE:
                scene.spike = unit;
                break;
            case NPC_NERUBIAN_BURROWER:
                scene.burrowers.push_back(unit);
                break;
            case NPC_SWARM_SCARAB:
                scene.scarabs.push_back(unit);
                break;
            case NPC_FROST_SPHERE:
                scene.spheres.push_back(unit);
                break;
            default:
                break;
        }
    }
    return scene;
}

bool ToC::Attackable(Player* bot, Unit* unit)
{
    return unit && unit->IsAlive() && unit->IsHostileTo(bot) &&
           !unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_IMMUNE_TO_PC);
}

// One tank holds each unit the script sends at a different target: Gormok, then the worms apart, Icehowl, Jaraxxus,
// the twins apart and Anub'arak; the Faction Champions pick soft targets by their own threat rule
// (UpdateThreat in boss_faction_champions.cpp), so nobody holds them.
Unit* ToC::MainTankUnit(Scene const& scene)
{
    for (Unit* unit : {scene.gormok, ChooseWorm(scene, nullptr), scene.icehowl, scene.jaraxxus, scene.fjola,
                       scene.eydis, scene.anubarak})
    {
        if (Attackable(scene.bot, unit))
            return unit;
    }
    return nullptr;
}

Unit* ToC::AssistTankUnit(Scene const& scene)
{
    if (Unit* snobold = FirstAttackable(scene, scene.snobolds))
        return snobold;

    // The main tank takes Dreadscale while it can be hit, so the other worm is the assist tank's
    if (Attackable(scene.bot, scene.dreadscale) && Attackable(scene.bot, scene.acidmaw))
        return scene.acidmaw;

    if (scene.jaraxxus)
    {
        if (Unit* mistress = FirstAttackable(scene, scene.mistresses))
            return mistress;
        return FirstAttackable(scene, scene.infernals);
    }

    if (Attackable(scene.bot, scene.fjola) && Attackable(scene.bot, scene.eydis))
        return scene.eydis;

    return FirstAttackable(scene, scene.burrowers);
}

Unit* ToC::UnitOfTwin(Scene const& scene, Twin twin)
{
    Unit* wanted = twin == Twin::Light ? scene.fjola : scene.eydis;
    Unit* other = twin == Twin::Light ? scene.eydis : scene.fjola;
    return Attackable(scene.bot, wanted) ? wanted : (Attackable(scene.bot, other) ? other : nullptr);
}

bool ToC::HoldsEssence(Player* bot, Twin& colour)
{
    if (bot->HasAura(SPELL_LIGHT_ESSENCE))
    {
        colour = Twin::Light;
        return true;
    }
    if (bot->HasAura(SPELL_DARK_ESSENCE))
    {
        colour = Twin::Dark;
        return true;
    }
    return false;
}

Twin ToC::TwinFor(Player* bot)
{
    Twin essence;
    if (HoldsEssence(bot, essence))
        return ToCLogic::Opposite(essence);
    return ToCLogic::AssignedTwin(PlayerBotSpec::IsMainTank(bot), PlayerBotSpec::IsAssistTank(bot),
                                  static_cast<uint32>(bot->GetGUIDLow()));
}

// The twin that casts Twin's Pact is shielded and heals half its health when the cast ends: everyone hits it until the
// shield breaks and the cast with it (boss_twin_valkyr.cpp Stage 2, Shield + Pact). The shield is the same for the
// bot of either essence.
Unit* ToC::PriorityTarget(Scene const& scene, Player* bot, Unit* current)
{
    Player* const self = scene.bot;

    if (scene.IsTwinFight())
    {
        for (Unit* twin : {scene.fjola, scene.eydis})
            if (Attackable(self, twin) && IsTwinPactCast(twin))
                return twin;
        return bot ? UnitOfTwin(scene, TwinFor(bot)) : nullptr;
    }

    // Normal mode keeps the portals and volcanoes untouchable, so they only count once the script lets them be hit
    if (scene.jaraxxus)
    {
        for (std::vector<Unit*> const* list : {&scene.portals, &scene.volcanoes, &scene.mistresses, &scene.infernals})
            if (Unit* add = FirstAttackable(scene, *list))
                return add;
        return Attackable(self, scene.jaraxxus) ? scene.jaraxxus : nullptr;
    }

    if (!scene.champions.empty())
        return ChooseChampion(scene);

    if (scene.anubarak)
    {
        if (Attackable(self, scene.anubarak) && HealthPct(scene.anubarak) <= ToCLogic::ANUBARAK_PHASE3_HP_PCT)
            return scene.anubarak;
        if (Unit* burrower = LowestHealth(scene, scene.burrowers))
            return burrower;
        if (Unit* scarab = FirstAttackable(scene, scene.scarabs))
            return scarab;
        return Attackable(self, scene.anubarak) ? scene.anubarak : nullptr;
    }

    if (Unit* snobold = FirstAttackable(scene, scene.snobolds))
        return snobold;
    if (Attackable(self, scene.gormok))
        return scene.gormok;
    if (Unit* worm = ChooseWorm(scene, current))
        return worm;
    return Attackable(self, scene.icehowl) ? scene.icehowl : nullptr;
}

Unit* ToC::SkullUnit(PlayerbotAI* botAI)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group)
        return nullptr;
    return botAI->GetUnit(group->GetTargetIcon(SKULL_ICON));
}

// The rule of the other raid strategies: a real player main tank leaves the marking to the first bot assist tank
bool ToC::IsMarkerBot(Player* bot, Unit* mainTankUnit)
{
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;
    if (mainTank && !GET_PLAYERBOT_AI(mainTank))
        return RaidRoles::IsAssistTankOfIndex(bot, 0);
    return PlayerBotSpec::IsMainTank(bot);
}

Unit* ToC::MarkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerBotSpec::IsTank(bot) || !bot->GetGroup())
        return nullptr;

    AiObjectContext* context = botAI->GetAiObjectContext();
    if (!IsMarkerBot(bot, AI_VALUE(Unit*, "main tank")))
        return nullptr;

    Scene const& scene = Scan(botAI);
    Unit* skull = SkullUnit(botAI);
    Unit* target = PriorityTarget(scene, nullptr, skull);
    return (target && target != skull) ? target : nullptr;
}

Unit* ToC::DpsTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerBotSpec::IsDps(bot))
        return nullptr;

    AiObjectContext* context = botAI->GetAiObjectContext();
    Unit* current = AI_VALUE(Unit*, "current target");
    Unit* target = PriorityTarget(Scan(botAI), bot, current);
    return (target && target != current) ? target : nullptr;
}

Unit* ToC::TankTarget(PlayerbotAI* botAI, bool main)
{
    Player* bot = botAI->GetBot();
    if (main ? !PlayerBotSpec::IsMainTank(bot) : !PlayerBotSpec::IsAssistTank(bot))
        return nullptr;

    AiObjectContext* context = botAI->GetAiObjectContext();
    Unit* current = AI_VALUE(Unit*, "current target");
    Scene const& scene = Scan(botAI);
    Unit* target = main ? MainTankUnit(scene) : AssistTankUnit(scene);
    return (target && target != current) ? target : nullptr;
}

Unit* ToC::HazardToAvoid(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    for (Unit* hazard : Scan(botAI).hazards)
        if (bot->GetDistance2d(hazard) < ToCLogic::HazardRadius(hazard->GetEntry()))
            return hazard;
    return nullptr;
}

// Icehowl stands still and facing its victim while it is flagged non-attackable and its target is set (stage 3 of the
// script); the hop and the charge that follow clear the target, so there is nothing to read after the 4 seconds.
bool ToC::ChargeEscape(PlayerbotAI* botAI, Point& dest)
{
    Unit* boss = Scan(botAI).icehowl;
    if (!boss || !boss->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
        return false;

    Unit* victim = botAI->GetUnit(boss->GetTarget());
    if (!victim || !victim->ToPlayer())
        return false;

    Player* bot = botAI->GetBot();
    if (victim == bot)
    {
        if (bot->GetDistance2d(boss) >= ToCLogic::TRAMPLE_TARGET_RUN)
            return false;
        dest = ToCLogic::TrampleTargetSpot(PointOf(bot), PointOf(boss));
        return true;
    }
    return ToCLogic::EscapeCharge(PointOf(bot), PointOf(boss), PointOf(victim), dest);
}

Player* ToC::IncineratedMember(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerBotSpec::IsHeal(bot))
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member->IsAlive() && member->HasAura(SPELL_INCINERATE_FLESH))
            return member;
    }
    return nullptr;
}

// A bot walks to the essence of the colour opposite to the twin it attacks and clicks it; while it holds an essence it
// does not take the other one, as the script gives no sign that the two replace each other.
Unit* ToC::EssenceToTake(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Twin held;
    if (HoldsEssence(bot, held))
        return nullptr;

    Scene const& scene = Scan(botAI);
    if (!scene.IsTwinFight())
        return nullptr;

    Twin const wanted = ToCLogic::Opposite(TwinFor(bot));
    Unit* nearest = nullptr;
    for (Unit* essence : (wanted == Twin::Light ? scene.lightEssences : scene.darkEssences))
        if (!nearest || bot->GetDistance2d(essence) < bot->GetDistance2d(nearest))
            nearest = essence;
    return nearest;
}

bool ToC::OrbMove(PlayerbotAI* botAI, Point& dest)
{
    Player* bot = botAI->GetBot();
    Scene const& scene = Scan(botAI);
    if (!scene.IsTwinFight() || (scene.darkOrbs.empty() && scene.lightOrbs.empty()))
        return false;

    Twin essence;
    bool const holds = HoldsEssence(bot, essence);
    std::vector<Point> dangerous;
    for (Unit* orb : scene.darkOrbs)
        if (ToCLogic::OrbDangerous(Twin::Dark, holds, essence))
            dangerous.push_back(PointOf(orb));
    for (Unit* orb : scene.lightOrbs)
        if (ToCLogic::OrbDangerous(Twin::Light, holds, essence))
            dangerous.push_back(PointOf(orb));

    if (ToCLogic::StepOutOfOrbs(PointOf(bot), dangerous, dest))
        return true;

    // A ranged bot that holds an essence soaks an orb of its colour: it stacks Powering Up, which turns into the
    // Empowered buff at 100 stacks. Melee and healers stay where they work.
    if (!holds || !PlayerBotSpec::IsRangedDps(bot))
        return false;

    for (Unit* orb : (essence == Twin::Light ? scene.lightOrbs : scene.darkOrbs))
    {
        if (bot->GetDistance2d(orb) >= ToCLogic::ORB_FETCH_RADIUS)
            continue;
        Point const spot = PointOf(orb);
        bool clear = true;
        for (Point const& danger : dangerous)
            if (ToCLogic::Distance(spot, danger) < ToCLogic::ORB_AVOID_RADIUS)
                clear = false;
        if (clear)
        {
            dest = spot;
            return true;
        }
    }
    return false;
}

bool ToC::SpikeEscape(PlayerbotAI* botAI, Point& dest)
{
    Player* bot = botAI->GetBot();
    if (!bot->HasAura(SPELL_PURSUED_BY_ANUBARAK))
        return false;

    Scene const& scene = Scan(botAI);
    if (!scene.spike)
        return false;

    Point const self = PointOf(bot);
    Point const spike = PointOf(scene.spike);
    std::vector<Point> const spheres = PointsOf(scene.spheres);
    int const sphere = ToCLogic::SpikeSphere(self, spike, spheres);
    dest = sphere >= 0 ? ToCLogic::BehindSphere(spheres[sphere], spike)
                       : ToCLogic::AwayFrom(self, spike, ToCLogic::SPIKE_FLEE_DISTANCE);
    return true;
}

bool ToCChecks::MarkPriority(PlayerbotAI* botAI)
{
    return ToC::MarkTarget(botAI) != nullptr;
}

bool ToCChecks::AttackPriority(PlayerbotAI* botAI)
{
    return ToC::DpsTarget(botAI) != nullptr;
}

bool ToCChecks::MainTankTarget(PlayerbotAI* botAI)
{
    return ToC::TankTarget(botAI, true) != nullptr;
}

bool ToCChecks::AssistTankTarget(PlayerbotAI* botAI)
{
    return ToC::TankTarget(botAI, false) != nullptr;
}

bool ToCChecks::GroundHazard(PlayerbotAI* botAI)
{
    return ToC::HazardToAvoid(botAI) != nullptr;
}

bool ToCChecks::IcehowlCharge(PlayerbotAI* botAI)
{
    Point dest;
    return ToC::ChargeEscape(botAI, dest);
}

bool ToCChecks::LegionFlame(PlayerbotAI* botAI)
{
    return botAI->GetBot()->HasAura(ToC::SPELL_LEGION_FLAME);
}

bool ToCChecks::IncinerateFlesh(PlayerbotAI* botAI)
{
    return ToC::IncineratedMember(botAI) != nullptr;
}

bool ToCChecks::NeedEssence(PlayerbotAI* botAI)
{
    return ToC::EssenceToTake(botAI) != nullptr;
}

bool ToCChecks::TwinOrbs(PlayerbotAI* botAI)
{
    Point dest;
    return ToC::OrbMove(botAI, dest);
}

bool ToCChecks::SpikePursuit(PlayerbotAI* botAI)
{
    Point dest;
    return ToC::SpikeEscape(botAI, dest);
}

// The burrowers are tanked away from the boss so the group can kill them while it keeps Anub'arak (the brief's phase 1)
bool ToCChecks::BurrowerNearBoss(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerBotSpec::IsAssistTank(bot))
        return false;

    AiObjectContext* context = botAI->GetAiObjectContext();
    Unit* current = AI_VALUE(Unit*, "current target");
    if (!current || current->GetEntry() != ToC::NPC_NERUBIAN_BURROWER)
        return false;

    ToC::Scene const& scene = ToC::Scan(botAI);
    return scene.anubarak && bot->GetDistance2d(scene.anubarak) < ToCLogic::BURROWER_TANK_DISTANCE;
}
