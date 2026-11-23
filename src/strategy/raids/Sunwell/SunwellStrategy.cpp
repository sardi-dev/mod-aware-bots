/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "SunwellStrategy.h"

#include "Playerbots.h"
#include "SunwellHelpers.h"
#include "SunwellMultipliers.h"

using namespace SunwellHelpers;

void RaidSunwellStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Every boss: leave void zones, exploding adds and the spread circles of debuffed raid members first.
    triggers.push_back(new TriggerNode("sunwell standing in hazard",
        NextAction::array(0, new NextAction("sunwell leave hazard", ACTION_EMERGENCY + 6), nullptr)));

    // Felmyst's vapors and Kil'jaeden's fiends: the bot they chase leads them away from the raid.
    triggers.push_back(new TriggerNode("sunwell chased",
        NextAction::array(0, new NextAction("sunwell kite chaser", ACTION_EMERGENCY + 5), nullptr)));

    triggers.push_back(new TriggerNode("sunwell interrupt",
        NextAction::array(0, new NextAction("sunwell interrupt", ACTION_INTERRUPT + 5), nullptr)));

    // Kalecgos
    triggers.push_back(new TriggerNode("sunwell in kalecgos cone",
        NextAction::array(0, new NextAction("sunwell leave kalecgos cone", ACTION_EMERGENCY + 3), nullptr)));

    triggers.push_back(new TriggerNode("sunwell in spectral realm",
        NextAction::array(0, new NextAction("sunwell attack sathrovarr", ACTION_RAID + 2), nullptr)));

    // Brutallus: the second tank takes over at four Meteor Slash stacks; soakers to their spots, the rest out of the
    // cone.
    triggers.push_back(new TriggerNode("sunwell brutallus take over",
        NextAction::array(0, new NextAction("sunwell brutallus take over", ACTION_EMERGENCY + 2), nullptr)));

    triggers.push_back(new TriggerNode("sunwell meteor slash position",
        NextAction::array(0, new NextAction("sunwell meteor slash position", ACTION_EMERGENCY + 1), nullptr)));

    // Felmyst in the air: gather where she took off, so she lands among the raid.
    triggers.push_back(new TriggerNode("sunwell felmyst regroup",
        NextAction::array(0, new NextAction("sunwell felmyst regroup", ACTION_RAID + 3), nullptr)));

    // Eredar Twins
    triggers.push_back(new TriggerNode("sunwell twins tank split",
        NextAction::array(0, new NextAction("sunwell twins tank split", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("sunwell pyrogenics",
        NextAction::array(0, new NextAction("sunwell remove pyrogenics", ACTION_RAID + 1), nullptr)));

    // Felmyst, Eredar Twins, M'uru and Kil'jaeden
    triggers.push_back(new TriggerNode("sunwell priority add",
        NextAction::array(0, new NextAction("sunwell attack priority add", ACTION_RAID + 1), nullptr)));

    // Eredar Twins and Kil'jaeden
    triggers.push_back(new TriggerNode("sunwell ranged spread",
        NextAction::array(0, new NextAction("sunwell spread ranged", ACTION_RAID), nullptr)));
}

void RaidSunwellStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new SunwellHoldOutsideHazardMultiplier(botAI));
    multipliers.push_back(new SunwellFelmystAirMultiplier(botAI));
    multipliers.push_back(new SunwellMeteorSoakMultiplier(botAI));
}

// Kalecgos: bots in the Spectral Realm fight Sathrovarr and the others Kalecgos; each side skips the boss it cannot
// reach (the realm lies under the floor).
void RaidSunwellStrategy::AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type)
{
    if (type == TargetValueExclusionType::None)
        return;

    Player* bot = botAI->GetBot();
    if (GetActiveEncounter(bot) != SunwellEncounter::Kalecgos)
        return;

    if (Unit* unreachable = FindBoss(bot, InSpectralRealm(bot) ? NPC_KALECGOS : NPC_SATHROVARR))
        exclusions.insert(unreachable->GetGUID());
}
