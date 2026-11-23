/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MagStrategy.h"
#include "MagMultipliers.h"

void RaidMagtheridonStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("magtheridon no encounter in progress",
        NextAction::array(0, new NextAction("magtheridon reset encounter states", ACTION_EMERGENCY + 10), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon main tank should tank channelers",
        NextAction::array(0, new NextAction("magtheridon main tank attack first three channelers", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon assist tanks should tank channelers",
        NextAction::array(0, new NextAction("magtheridon assist tanks attack last two channelers", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon pulling west and east channelers",
        NextAction::array(0, new NextAction("magtheridon misdirect hellfire channelers to main tank", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon determining kill order",
        NextAction::array(0, new NextAction("magtheridon assign dps priority", ACTION_RAID), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon burning abyssal spawned",
        NextAction::array(0, new NextAction("magtheridon warlock cc burning abyssal", ACTION_RAID + 2), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon should be tanked",
        NextAction::array(0, new NextAction("magtheridon main tank position boss", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon should spread ranged",
        NextAction::array(0, new NextAction("magtheridon spread ranged", ACTION_RAID + 1), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon standing in debris",
        NextAction::array(0, new NextAction("magtheridon move out of debris", ACTION_EMERGENCY + 7), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon incoming blast nova",
        NextAction::array(0, new NextAction("magtheridon use manticron cube", ACTION_EMERGENCY + 6), nullptr)));

    triggers.push_back(new TriggerNode("magtheridon should manage timers and assignments",
        NextAction::array(0, new NextAction("magtheridon update timers and assignments", ACTION_EMERGENCY + 10), nullptr)));
}

void RaidMagtheridonStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new MagtheridonUseManticronCubeMultiplier(botAI));
    multipliers.push_back(new MagtheridonHoldDpsMultiplier(botAI));
    multipliers.push_back(new MagtheridonControlTankActionsMultiplier(botAI));
    multipliers.push_back(new MagtheridonAvoidDebrisDangerMultiplier(botAI));
}
