/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

// AC Ai/Raid/Hyjal/HyjalStrategy.cpp (see the header of the matching .h for what changed on 5.4.8).

#include "HyjalStrategy.h"
#include "HyjalMultipliers.h"

void RaidHyjalStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // General
    triggers.push_back(new TriggerNode("hyjal no encounter in progress",
        NextAction::array(0, new NextAction("hyjal reset encounter states", ACTION_EMERGENCY + 10), nullptr)));

    // Rage Winterchill
    triggers.push_back(new TriggerNode("rage winterchill pulling boss",
        NextAction::array(0, new NextAction("rage winterchill misdirect boss to main tank", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("rage winterchill should be tanked",
        NextAction::array(0, new NextAction("rage winterchill main tank position boss", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("rage winterchill ranged should spread",
        NextAction::array(0, new NextAction("rage winterchill spread ranged in circle", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("rage winterchill melee near death and decay",
        NextAction::array(0, new NextAction("rage winterchill melee maneuver through death and decay", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("rage winterchill ranged in death and decay",
        NextAction::array(0, new NextAction("rage winterchill ranged get out of death and decay", ACTION_EMERGENCY + 1), nullptr)));

    // Anetheron
    triggers.push_back(new TriggerNode("anetheron pulling boss or infernal",
        NextAction::array(0, new NextAction("anetheron misdirect boss and infernals to tanks", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("anetheron should be tanked",
        NextAction::array(0, new NextAction("anetheron main tank position boss", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("anetheron ranged should spread",
        NextAction::array(0, new NextAction("anetheron spread ranged in circle", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("anetheron targeted by infernal",
        NextAction::array(0, new NextAction("anetheron bring infernal to infernal tank", ACTION_EMERGENCY + 7), nullptr)));

    triggers.push_back(new TriggerNode("anetheron near inferno target",
        NextAction::array(0, new NextAction("anetheron move away from inferno target", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("anetheron infernals pulse immolation",
        NextAction::array(0, new NextAction("anetheron get out of immolation", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("anetheron infernals should be tanked away",
        NextAction::array(0, new NextAction("anetheron infernal tank take position", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("anetheron should divide dps",
        NextAction::array(0, new NextAction("anetheron assign dps priority", ACTION_RAID), nullptr)));

    // Kaz'rogal
    triggers.push_back(new TriggerNode("kaz'rogal pulling boss",
        NextAction::array(0, new NextAction("kaz'rogal misdirect boss to main tank", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal should be tanked",
        NextAction::array(0, new NextAction("kaz'rogal main tank position boss", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal can split malevolent cleave damage",
        NextAction::array(0, new NextAction("kaz'rogal assist tanks move in front", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal ranged should avoid war stomp",
        NextAction::array(0, new NextAction("kaz'rogal spread ranged in arc", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal low on mana",
        NextAction::array(0, new NextAction("kaz'rogal move away from group", ACTION_EMERGENCY + 2), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal mark on mage or paladin",
        NextAction::array(0, new NextAction("hyjal remove dangerous dot", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal immunity no longer needed",
        NextAction::array(0, new NextAction("kaz'rogal cancel immunity", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("kaz'rogal warlock should manage mana",
        NextAction::array(0, new NextAction("kaz'rogal warlock manage mana", ACTION_EMERGENCY + 6), nullptr)));

    // Azgalor
    triggers.push_back(new TriggerNode("azgalor pulling boss",
        NextAction::array(0, new NextAction("azgalor misdirect boss to main tank", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("azgalor should be tanked",
        NextAction::array(0, new NextAction("azgalor main tank position boss", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("azgalor ranged should spread",
        NextAction::array(0, new NextAction("azgalor disperse ranged", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("azgalor melee near rain of fire",
        NextAction::array(0, new NextAction("azgalor melee maneuver through fire", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("azgalor ranged in rain of fire",
        NextAction::array(0, new NextAction("azgalor ranged get out of rain of fire", ACTION_EMERGENCY + 1), nullptr)));

    triggers.push_back(new TriggerNode("azgalor bot is doomed",
        NextAction::array(0, new NextAction("azgalor move to doomguard tank", ACTION_EMERGENCY + 2), nullptr)));

    triggers.push_back(new TriggerNode("azgalor should control doomguards",
        NextAction::array(0, new NextAction("azgalor tank position doomguard", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("azgalor should divide dps",
        NextAction::array(0, new NextAction("azgalor determine dps priority", ACTION_RAID), nullptr)));

    // Archimonde
    triggers.push_back(new TriggerNode("archimonde pulling boss",
        NextAction::array(0, new NextAction("archimonde misdirect boss to main tank", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("archimonde should be tanked",
        NextAction::array(0, new NextAction("archimonde move boss to initial position", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("archimonde shaman should protect against fear",
        NextAction::array(0, new NextAction("archimonde set tremor totem", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("archimonde casting air burst",
        NextAction::array(0, new NextAction("archimonde keep air burst away from tank", ACTION_EMERGENCY + 8), nullptr)));

    triggers.push_back(new TriggerNode("archimonde ranged should spread",
        NextAction::array(0, new NextAction("archimonde spread ranged", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("archimonde near doomfire",
        NextAction::array(0, new NextAction("archimonde avoid doomfire", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("archimonde stood in doomfire",
        NextAction::array(0, new NextAction("hyjal remove dangerous dot", ACTION_EMERGENCY + 7), nullptr)));
}

void RaidHyjalStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    // General
    multipliers.push_back(new HyjalDelayDpsCooldownsMultiplier(botAI));
    multipliers.push_back(new HyjalDisableDisperseAndTankFaceMultiplier(botAI));

    // Rage Winterchill
    multipliers.push_back(new RageWinterchillMeleeControlAvoidanceMultiplier(botAI));
    multipliers.push_back(new RageWinterchillRangedControlAvoidanceMultiplier(botAI));

    // Anetheron
    multipliers.push_back(new AnetheronDisableAssistTargetingMultiplier(botAI));
    multipliers.push_back(new AnetheronAvoidAccidentalInfernalAggroMultiplier(botAI));
    multipliers.push_back(new AnetheronInfernalTargetRunToPositionMultiplier(botAI));
    multipliers.push_back(new AnetheronControlMovementMultiplier(botAI));
    multipliers.push_back(new AnetheronControlMisdirectionMultiplier(botAI));

    // Kaz'rogal
    multipliers.push_back(new KazrogalControlLowManaMovementMultiplier(botAI));

    // Azgalor
    multipliers.push_back(new AzgalorDisableAutoTargetingAndPositioningMultiplier(botAI));
    multipliers.push_back(new AzgalorDoomedBotPrioritizePositioningMultiplier(botAI));
    multipliers.push_back(new AzgalorMeleeDpsControlAvoidanceMultiplier(botAI));
    multipliers.push_back(new AzgalorRangedControlAvoidanceMultiplier(botAI));

    // Archimonde
    multipliers.push_back(new ArchimondeControlDoomfireAvoidanceMultiplier(botAI));
    multipliers.push_back(new ArchimondeSetTremorTotemMultiplier(botAI));
}
