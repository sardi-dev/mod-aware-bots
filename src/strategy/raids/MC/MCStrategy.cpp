/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MCStrategy.h"
#include "MCHelpers.h"
#include "MCMultipliers.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "PlayerbotSpec.h"

using namespace MoltenCoreHelpers;

void RaidMcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Lucifron, Magmadar, Gehennas, Garr, Sulfuron Harbinger, Majordomo Executus and Ragnaros have only AC's
    // resistance aura triggers, and 5.4.8 has no resistance auras.

    // Baron Geddon
    triggers.push_back(
        new TriggerNode("mc living bomb debuff",
        NextAction::array(0, new NextAction("mc move from group", ACTION_RAID), nullptr)));
    triggers.push_back(
        new TriggerNode("mc baron geddon inferno",
        NextAction::array(0, new NextAction("mc move from baron geddon", ACTION_RAID), nullptr)));

    // Shazzrah
    triggers.push_back(
        new TriggerNode("mc shazzrah ranged",
        NextAction::array(0, new NextAction("mc shazzrah move away", ACTION_RAID), nullptr)));

    // Golemagg the Incinerator
    triggers.push_back(
        new TriggerNode("mc golemagg mark boss",
        NextAction::array(0, new NextAction("mc golemagg mark boss", ACTION_RAID), nullptr)));
    triggers.push_back(
        new TriggerNode("mc golemagg is main tank",
        NextAction::array(0, new NextAction("mc golemagg main tank attack golemagg", ACTION_RAID), nullptr)));
    triggers.push_back(
        new TriggerNode("mc golemagg is assist tank",
        NextAction::array(0, new NextAction("mc golemagg assist tank attack core rager", ACTION_RAID), nullptr)));
    triggers.push_back(
        new TriggerNode("mc golemagg magma splash",
        NextAction::array(0, new NextAction("mc golemagg back off", ACTION_RAID + 1), nullptr)));
    triggers.push_back(
        new TriggerNode("mc golemagg is healer",
        NextAction::array(0, new NextAction("mc golemagg healer position", ACTION_RAID), nullptr)));

    // Trash
    triggers.push_back(
        new TriggerNode("mc core hound mark",
        NextAction::array(0, new NextAction("mc core hound mark", ACTION_RAID), nullptr)));

    // Anywhere in MC: escaping a lava pool outranks everything else.
    triggers.push_back(
        new TriggerNode("mc in lava",
        NextAction::array(0, new NextAction("mc move from lava", ACTION_RAID + 1), nullptr)));
}

void RaidMcStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new GarrDisableDpsAoeMultiplier(botAI));
    multipliers.push_back(new BaronGeddonAbilityMultiplier(botAI));
    multipliers.push_back(new GolemaggMultiplier(botAI));
}

void RaidMcStrategy::AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type)
{
    // AC also skips its area-of-effect target for tanks; this module has no such exclusion type.
    if (type == TargetValueExclusionType::Tank || type == TargetValueExclusionType::None)
        return;

    // Damage into these is wasted: Core Ragers are unkillable while Golemagg
    // lives (full heal at 50%), and Majordomo reflects and cannot die; his
    // encounter ends when the eight adds are dead. Tanks still pick them up
    // through their own target selection.
    AiObjectContext* context = botAI->GetAiObjectContext();
    bool const golemaggAlive = AI_VALUE2(Unit*, "find target", "golemagg the incinerator") != nullptr;
    for (BotGuid const guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        if ((golemaggAlive && unit->GetEntry() == NPC_CORE_RAGER) || unit->GetEntry() == NPC_MAJORDOMO_EXECUTUS)
            exclusions.insert(guid);
    }
}
