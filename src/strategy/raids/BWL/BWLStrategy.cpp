/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BWLStrategy.h"
#include "BWLHelpers.h"
#include "BWLMultipliers.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"

void RaidBwlStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // AC gives every bot the Onyxia Scale Cloak aura here, a cheat: a bot without the cloak fights without it. The
    // boss resistance aura triggers are dropped too, as 5.4.8 has no resistance auras.
    triggers.push_back(new TriggerNode("bwl suppression device",
        NextAction::array(0, new NextAction("bwl turn off suppression device", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("bwl razorgore not mind controlled",
        NextAction::array(0, new NextAction("bwl razorgore avoid aoe", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode("bwl razorgore not mind controlled",
        NextAction::array(0, new NextAction("bwl razorgore mark boss", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("bwl vaelastrasz positioning",
        NextAction::array(0, new NextAction("rear flank", ACTION_MOVE + 4), nullptr)));
    triggers.push_back(new TriggerNode("bwl vaelastrasz burning adrenaline",
        NextAction::array(0, new NextAction("bwl vaelastrasz move away", ACTION_RAID + 5), nullptr)));

    triggers.push_back(new TriggerNode("bwl broodlord ranged too close",
        NextAction::array(0, new NextAction("bwl broodlord ranged move away", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("bwl firemaw not victim",
        NextAction::array(0, new NextAction("bwl firemaw avoid breath", ACTION_MOVE + 4), nullptr)));
    triggers.push_back(new TriggerNode("bwl ebonroc not victim",
        NextAction::array(0, new NextAction("bwl ebonroc avoid breath", ACTION_MOVE + 4), nullptr)));
    triggers.push_back(new TriggerNode("bwl flamegor not victim",
        NextAction::array(0, new NextAction("bwl flamegor avoid breath", ACTION_MOVE + 4), nullptr)));

    triggers.push_back(new TriggerNode("bwl affliction bronze",
        NextAction::array(0, new NextAction("bwl use hourglass sand", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("bwl nefarian positioning",
        NextAction::array(0, new NextAction("bwl nefarian rear flank", ACTION_MOVE + 4), nullptr)));
    triggers.push_back(new TriggerNode("bwl nefarian wild magic",
        NextAction::array(0, new NextAction("ice block", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("bwl death talon wyrmguard tank",
        NextAction::array(0, new NextAction("bwl death talon wyrmguard tank move away", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode("bwl death talon wyrmguard ranged",
        NextAction::array(0, new NextAction("bwl death talon wyrmguard ranged move away", ACTION_RAID), nullptr)));
}

void RaidBwlStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new RazorgoreTankMultiplier(botAI));
    multipliers.push_back(new VaelastraszTankMultiplier(botAI));
    multipliers.push_back(new VaelastraszBurningAdrenalineMultiplier(botAI));
}

void RaidBwlStrategy::AppendTargetExclusions(GuidSet& exclusions, TargetValueExclusionType type)
{
    if (type != TargetValueExclusionType::Tank)
        return;

    AiObjectContext* context = botAI->GetAiObjectContext();
    if (Unit* broodlord = AI_VALUE2(Unit*, "find target", "broodlord lashlayer"))
    {
        Unit* bossVictim = broodlord->GetVictim();
        Player* bossVictimPlayer = bossVictim ? bossVictim->ToPlayer() : nullptr;

        Unit* bot = botAI->GetBot();
        if (!bossVictimPlayer || !PlayerBotSpec::IsTank(bossVictimPlayer) || bossVictim == bot)
        {
            // Only the tank actually holding the boss keeps it as its target at all times.
            // All other tanks may switch to non-whelp targets (other branch).
            // This should not happen, if the elites were killed before the encounter, but just in case...
            // If the boss targets a non-tank, all tanks should ignore the adds and attack the boss only.
            for (BotGuid const guid : AI_VALUE(GuidVector, "attackers"))
            {
                Unit* unit = botAI->GetUnit(guid);
                if (unit != broodlord)
                    exclusions.insert(guid);
            }
        }
        else
        {
            // Prevents the tanks from changing targets to a whelp during the encounter.
            // Tanks should build threat on the boss. DPS handles whelps on their own.
            // Change target if it isn't a whelp (boss or respawned elite NPCs).
            for (BotGuid const guid : AI_VALUE(GuidVector, "attackers"))
            {
                Unit* unit = botAI->GetUnit(guid);
                if (BlackwingLairHelpers::IsCorruptedWhelp(unit))
                    exclusions.insert(guid);
            }
        }
    }
}
