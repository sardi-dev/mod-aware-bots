/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "TankPaladinStrategy.h"

#include "Playerbots.h"

class TankPaladinStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    TankPaladinStrategyActionNodeFactory()
    {
        // 5.4.8: Seal of Corruption and Seal of Vengeance do not exist; "seal of command" now falls
        // straight through to "seal of righteousness" (the same threat/damage seal DpsPaladinStrategy
        // uses), instead of the removed corruption/vengeance links.
        creators["seal of command"] = &seal_of_command;
        // #383 (AC TankPaladinStrategy.cpp:15-17,32-50 corruption -> vengeance -> righteousness chain):
        // Seal of Truth (31801) is the 5.x successor of Corruption/Vengeance; chain Truth -> Righteousness.
        creators["seal of truth"] = &seal_of_truth;
        creators["hand of reckoning"] = &hand_of_reckoning;
        creators["taunt spell"] = &hand_of_reckoning;
    }

private:
    static ActionNode* seal_of_command([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("seal of command",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("seal of righteousness"), nullptr),
                              /*C*/ nullptr);
    }
    // #730: below L24 the only 5.4.8 seal is Seal of Command (105361, L3); its node above falls through to Righteousness.
    ACTION_NODE_A(seal_of_truth, "seal of truth", "seal of command");
    // Righteous Defense was removed in 5.0; the 5.x taunt is Reckoning (62124), so no alternative.
    ACTION_NODE(hand_of_reckoning, "hand of reckoning");
};

TankPaladinStrategy::TankPaladinStrategy(PlayerbotAI* botAI) : GenericPaladinStrategy(botAI)
{
    actionNodeFactories.Add(new TankPaladinStrategyActionNodeFactory());
}

NextAction** TankPaladinStrategy::getDefaultActions()
{
    // #735: "shield of righteousness" (Shield of the Righteous 53600, 3 Holy Power) is no longer a default action -
    // it fires from the "holy power 3 available" / "divine purpose" triggers (InitTriggers), at the relevance it had
    // here, as AC fires its finishers from "combo points N available" (AC CombatRogueStrategy.cpp:118-126).
    // rotation task 6 (#861), MoP rows 6600-6607 (prio 0/7..0/1): Shield of the Righteous > Crusader Strike (or Hammer
    // of the Righteous with 3+ enemies, "medium aoe" below) > Avenger's Shield > Hammer of Wrath > Judgment >
    // Consecration > Holy Wrath. Shield of the Righteous fires from the Holy Power trigger (#735).
    return NextAction::array(0, new NextAction("crusader strike", ACTION_DEFAULT + 0.5f),
                             new NextAction("avenger's shield", ACTION_DEFAULT + 0.45f),
                             new NextAction("hammer of wrath", ACTION_DEFAULT + 0.4f),
                             // 5.4.8: Judgement of Wisdom does not exist; Judgement is a single unified
                             // ability in MoP, so this now casts the generic "judgement" action.
                             new NextAction("judgement", ACTION_DEFAULT + 0.35f),
                             new NextAction("consecration", ACTION_DEFAULT + 0.2f),
                             new NextAction("holy wrath", ACTION_DEFAULT + 0.1f),
                             new NextAction("melee", ACTION_DEFAULT), NULL);
}

void TankPaladinStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPaladinStrategy::InitTriggers(triggers);

    // #383 (AC TankPaladinStrategy.cpp:86): seal -> "seal of corruption" -> Seal of Truth (31801, replaces
    // Seal of Command at L24, Wowhead 2026-10-04); factory chain Truth -> Righteousness above. This
    // replaces the old "seal of command" entry, which Seal of Truth makes unlearnable at L24+.
    // Class sheet, protection paladin (BT-127): both guides keep Seal of Insight from level 32 (its melee hits heal), so it
    // comes first; below 32 the engine finds Insight impossible and falls to Seal of Truth (24) and then Seal of Command
    // through the chain above. SealTrigger counts Insight as up for a tank whatever its mana.
    triggers.push_back(new TriggerNode(
        "seal", NextAction::array(0, new NextAction("seal of insight", ACTION_HIGH + 0.1f),
                                  new NextAction("seal of truth", ACTION_HIGH), nullptr)));
    // 5.4.8: Seal of Wisdom does not exist; its successor Seal of Insight restores mana for Holy only
    // (Wowhead 20165 2026-10-04), so there is no low-mana seal swap for a Protection paladin.
    // triggers.push_back(
    //     new TriggerNode("low mana", NextAction::array(0, new NextAction("seal of wisdom", ACTION_HIGH + 9), nullptr)));
    // triggers.push_back(new TriggerNode("devotion aura", NextAction::array(0, new NextAction("devotion aura", 90.0f),
    // NULL)));

    // #735: the Holy Power spender (see getDefaultActions)
    triggers.push_back(new TriggerNode(
        "holy power 3 available", NextAction::array(0, new NextAction("shield of righteousness", ACTION_DEFAULT + 0.6f), nullptr)));
    triggers.push_back(new TriggerNode(
        "divine purpose", NextAction::array(0, new NextAction("shield of righteousness", ACTION_DEFAULT + 0.6f), nullptr)));
    triggers.push_back(new TriggerNode(
        "light aoe", NextAction::array(0, new NextAction("avenger's shield", ACTION_HIGH + 5), nullptr)));
    triggers.push_back(
        new TriggerNode("medium aoe", NextAction::array(0, new NextAction("consecration", ACTION_HIGH + 7),
                                                        new NextAction("avenger's shield", ACTION_HIGH + 6),
                                                        // Hammer of the Righteous with >= 3 enemies in 5 yd
                                                        new NextAction("hammer of the righteous", ACTION_HIGH + 5),
                                                        nullptr)));
    // triggers.push_back(new TriggerNode("avenger's shield", NextAction::array(0, new NextAction("avenger's shield",
    // ACTION_HIGH + 7), nullptr)));
    triggers.push_back(new TriggerNode(
        "lose aggro", NextAction::array(0, new NextAction("hand of reckoning", ACTION_HIGH + 7), nullptr)));
    // Holy Shield was removed in 5.0. Its 5.x replacement, Shield of the Righteous (53600, 3 Holy Power), is
    // already a default action above.
    // 5.4.8: Blessing of Sanctuary does not exist; MoP paladins only have Blessing of Kings/Might, so
    // this dead line stays disabled (the "blessing" self trigger is now registered for real, but Tank
    // has no sanctuary-equivalent blessing to point it at).
    // triggers.push_back(new TriggerNode("blessing", NextAction::array(0, new NextAction("blessing of sanctuary",
    // ACTION_HIGH + 9), nullptr)));
    triggers.push_back(new TriggerNode(
        "avenging wrath", NextAction::array(0, new NextAction("avenging wrath", ACTION_HIGH + 2), nullptr)));
    triggers.push_back(
        new TriggerNode("target critical health",
                        NextAction::array(0, new NextAction("hammer of wrath", ACTION_CRITICAL_HEAL), nullptr)));
    triggers.push_back(new TriggerNode(
        "righteous fury", NextAction::array(0, new NextAction("righteous fury", ACTION_HIGH + 8), nullptr)));
    // MoP defensive set (group 5), all gated on the bot's own health:
    // Class sheet, protection paladin (BT-127): Divine Protection at 65 percent, Ardent Defender at 40, Lay on Hands on
    // itself at 20 only when Ardent Defender cannot be used (the actions hold that rule, see PaladinActions.cpp). The
    // thresholds are TANK_*_HEALTH_PCT in PaladinActions.h. Avenging Wrath is no longer a defensive: it is a 3 minute
    // damage cooldown and keeps its offensive "avenging wrath" edge above.
    triggers.push_back(new TriggerNode("tank divine protection health", NextAction::array(0,
        new NextAction("divine protection", ACTION_HIGH + 6), nullptr)));
    triggers.push_back(new TriggerNode("tank ardent defender health",
        NextAction::array(0, new NextAction("ardent defender", ACTION_EMERGENCY - 1), nullptr)));
    triggers.push_back(new TriggerNode("paladin health 70",
        NextAction::array(0, new NextAction("eternal flame", ACTION_HIGH + 4.5f), nullptr)));      // 6620 health <= 70
    triggers.push_back(new TriggerNode("bastion of glory",
        NextAction::array(0, new NextAction("eternal flame", ACTION_HIGH + 4.4f), nullptr)));      // 6621 >= 3 stacks
    triggers.push_back(new TriggerNode("guardian of ancient kings",
        NextAction::array(0, new NextAction("guardian of ancient kings", ACTION_HIGH + 3), nullptr)));  // 6617
    triggers.push_back(new TriggerNode(
        "enough mana", NextAction::array(0, new NextAction("consecration", ACTION_HIGH + 4), nullptr)));
    triggers.push_back(new TriggerNode("not facing target",
                                       NextAction::array(0, new NextAction("set facing", ACTION_NORMAL + 7), nullptr)));
    triggers.push_back(new TriggerNode(
        "enemy out of melee", NextAction::array(0, new NextAction("reach melee", ACTION_HIGH + 1), nullptr)));
}
