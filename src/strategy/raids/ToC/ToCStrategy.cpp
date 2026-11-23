/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ToCStrategy.h"

#include "Playerbots.h"
#include "ToCMultipliers.h"

// Movement answers sit above the target choices: a bot stepping out of an orb or a charge does not stop to pick a
// target first. Interrupts of Fel Fireball and of the Faction Champions' casts are left to the generic interrupt duty
// ("enemy caster in range"), which already acts on any caster within range.
void RaidToCStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Targets
    triggers.push_back(new TriggerNode(
        "toc mark priority", NextAction::array(0, new NextAction("toc mark priority", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc attack priority", NextAction::array(0, new NextAction("toc attack priority", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc main tank target", NextAction::array(0, new NextAction("toc main tank attack", ACTION_RAID), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc assist tank target", NextAction::array(0, new NextAction("toc assist tank attack", ACTION_RAID), nullptr)));

    // Movement
    triggers.push_back(new TriggerNode(
        "toc ground hazard", NextAction::array(0, new NextAction("toc avoid hazard", ACTION_RAID + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc icehowl charge", NextAction::array(0, new NextAction("toc dodge charge", ACTION_RAID + 3), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc legion flame", NextAction::array(0, new NextAction("toc run from raid", ACTION_RAID + 3), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc twin orbs", NextAction::array(0, new NextAction("toc handle orbs", ACTION_RAID + 2), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc spike pursuit", NextAction::array(0, new NextAction("toc kite spike", ACTION_RAID + 3), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc burrower near boss",
        NextAction::array(0, new NextAction("toc pull burrower away", ACTION_RAID + 1), nullptr)));

    // Other duties
    triggers.push_back(new TriggerNode(
        "toc incinerate flesh", NextAction::array(0, new NextAction("toc heal incinerated", ACTION_RAID + 1), nullptr)));
    triggers.push_back(new TriggerNode(
        "toc need essence", NextAction::array(0, new NextAction("toc take essence", ACTION_RAID + 1), nullptr)));
}

void RaidToCStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ToCMultiplier(botAI));
}
