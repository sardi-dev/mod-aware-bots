/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HolyPriestStrategy.h"

#include "HealerTriggers.h"
#include "Playerbots.h"

class HolyPriestStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    HolyPriestStrategyActionNodeFactory() { creators["smite"] = &smite; }

private:
    static ActionNode* smite([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("smite",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("shoot"), nullptr),
                              /*C*/ nullptr);
    }
};

HolyPriestStrategy::HolyPriestStrategy(PlayerbotAI* botAI) : HealPriestStrategy(botAI)
{
    actionNodeFactories.Add(new HolyPriestStrategyActionNodeFactory());
}

NextAction** HolyPriestStrategy::getDefaultActions()
{
    // #524 (AC HolyPriestStrategy.cpp:22-26): Mana Burn and Starshards are not learnable in 5.4.8 (cls.py priest
    // NOT-LEARNABLE), so only smite is kept.
    return NextAction::array(0, new NextAction("smite", ACTION_DEFAULT + 0.2f), nullptr);
}

void HolyPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    HealPriestStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode("holy fire", NextAction::array(0, new NextAction("holy fire", ACTION_NORMAL + 9), nullptr)));
    triggers.push_back(
        new TriggerNode("shadowfiend", NextAction::array(0, new NextAction("shadowfiend", ACTION_HIGH), nullptr)));
    triggers.push_back(
        new TriggerNode("medium mana", NextAction::array(0, new NextAction("shadowfiend", ACTION_HIGH), nullptr)));
    // #524: AC "low mana -> mana burn" edge removed - Mana Burn is not learnable in 5.4.8 (cls.py).

    // Task E (#863, w74: Chakra: Serenity / Sanctuary and Lightwell 0 casts): a solo or non-healer Holy priest runs this
    // "holy dps" strategy, not "holy heal", so the chakra stance and Lightwell edges below never reached it.
    // MoP rotation row 25724 / 25723 (prio 5/1, in combat): Serenity unless >= 3 group members are below 80 %,
    // then Sanctuary. MoP rotation row 25721 (5/1): Lightwell 126135 in combat, always (3 min cooldown).
    triggers.push_back(new TriggerNode(
        "chakra: serenity", NextAction::array(0, new NextAction("chakra: serenity", ACTION_HIGH), nullptr)));
    triggers.push_back(new TriggerNode(
        "chakra: sanctuary", NextAction::array(0, new NextAction("chakra: sanctuary", ACTION_HIGH), nullptr)));
    triggers.push_back(new TriggerNode(
        "lightwell can cast", NextAction::array(0, new NextAction("lightwell", ACTION_HIGH - 1), nullptr)));
}

HolyHealPriestStrategy::HolyHealPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPriestStrategyActionNodeFactory());
}

NextAction** HolyHealPriestStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("shoot", ACTION_DEFAULT), nullptr);
}

void HolyHealPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    // BT-293: Guardian Spirit on a tank whose danger reading is high, before the critical health edge.
    triggers.push_back(new TriggerNode("main tank in danger",
        NextAction::array(0, new NextAction("guardian spirit on main tank", ACTION_CRITICAL_HEAL + 6), nullptr)));

    // #812 MoP-only (no AC counterpart): keep Chakra: Serenity up, the way AC's shadow strategy keeps Shadowform up
    // (AC ShadowPriestStrategy.cpp:30-36). It replaces, for Holy, AC's "medium mana -> inner focus" edge
    // (AC GenericPriestStrategy.cpp:31-34), dead here: Inner Focus 89485 is Discipline-only in 5.4.8 (spec256).
    // Priority is above every heal so a once-per-fight stance is not starved by healing pressure.
    triggers.push_back(new TriggerNode(
        "chakra: serenity", NextAction::array(0, new NextAction("chakra: serenity", ACTION_CRITICAL_HEAL + 8), nullptr)));
    // Task E (#863, MoP rotation row 25721 / 25717, prio 5/1): Lightwell 126135 in combat (was never wired).
    triggers.push_back(new TriggerNode(
        "lightwell can cast", NextAction::array(0, new NextAction("lightwell", ACTION_CRITICAL_HEAL + 7), nullptr)));

    // Class sheet, holy priest (BT-126): the group heals use "holy group heal setting" and "holy medium group heal setting",
    // which fire on three hurt members in a party of two to four as well; the shared triggers need five near members.
    // Divine Hymn and Holy Word: Sanctuary keep the shared rule (below).
    triggers.push_back(
        new TriggerNode("holy group heal setting",
                        NextAction::array(0, 
                            new NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 9),
                            new NextAction("circle of healing on party", ACTION_MEDIUM_HEAL + 8), nullptr)));

    triggers.push_back(new TriggerNode(
        "holy medium group heal setting",
        NextAction::array(0, new NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 6),
                          new NextAction("circle of healing on party", ACTION_CRITICAL_HEAL + 5),
                          new NextAction("prayer of healing on party", ACTION_CRITICAL_HEAL + 4),
                          new NextAction("halo", ACTION_CRITICAL_HEAL + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium group heal setting",
        NextAction::array(0, new NextAction("divine hymn", ACTION_CRITICAL_HEAL + 7), nullptr)));

    // Task 8 / #863 (MoP rotation row 25715/25723): Chakra: Sanctuary while >= 3 group members are below 80%
    // (ChakraSerenityTrigger stands down meanwhile), and Holy Word: Sanctuary (25716) once it is up.
    triggers.push_back(new TriggerNode(
        "chakra: sanctuary", NextAction::array(0, new NextAction("chakra: sanctuary", ACTION_CRITICAL_HEAL + 8), nullptr)));
    triggers.push_back(new TriggerNode(
        "medium group heal setting",
        NextAction::array(0, new NextAction("holy word: sanctuary on party", ACTION_CRITICAL_HEAL + 8), nullptr)));
    // Task 8 / #863 (MoP rotation row 25603 disc / 25703 holy, group 1/10-1/12): the Surge of Light proc makes
    // Flash Heal free, so spend it on the hurt party member first.
    triggers.push_back(new TriggerNode(
        "surge of light proc", NextAction::array(0, new NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 6), nullptr)));
    // Task 8 / #863 (MoP rotation row 25607 disc / 25707 holy, group 1/9-1/11): Binding Heal when both the bot
    // and the target are hurt (BindingHealTrigger = party member low health + self below medium).
    triggers.push_back(new TriggerNode(
        "binding heal", NextAction::array(0, new NextAction("binding heal", ACTION_MEDIUM_HEAL + 5), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member critical health",
        NextAction::array(0, 
                          new NextAction("guardian spirit on party", ACTION_CRITICAL_HEAL + 6),
                          new NextAction("power word: shield on party", ACTION_CRITICAL_HEAL + 5),
                          // #812 MoP-only: Holy Word: Serenity (instant, 2% base mana) - AC has no counterpart.
                          new NextAction("holy word: serenity on party", ACTION_CRITICAL_HEAL + 4),
                          new NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 3),
                          new NextAction("greater heal on party", ACTION_MEDIUM_HEAL + 2),
                          new NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 1),
                          nullptr)));

    triggers.push_back(
        new TriggerNode("party member low health",
                        NextAction::array(0,
                                          // #812 MoP-only: Holy Word: Serenity - AC has no counterpart.
                                          new NextAction("holy word: serenity on party", ACTION_MEDIUM_HEAL + 5),
                                          new NextAction("circle of healing on party", ACTION_MEDIUM_HEAL + 4),
                                          new NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 3),
                                          // Class sheet, holy priest (BT-126): Greater Heal is the heal of this band
                                          // (below 45 percent); Flash Heal is left to the critical band and to levels
                                          // where Heal is not learned yet (the medium band below).
                                          new NextAction("greater heal on party", ACTION_MEDIUM_HEAL + 2), nullptr)));

    triggers.push_back(
        new TriggerNode("party member medium health",
                        NextAction::array(0,
                                          // #812 MoP-only: Holy Word: Serenity, above AC's greater heal - AC has no counterpart.
                                          new NextAction("holy word: serenity on party", ACTION_MEDIUM_HEAL + 6),
                                          new NextAction("circle of healing on party", ACTION_LIGHT_HEAL + 7),
                                          new NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 6),
                                          // Class sheet, holy priest (BT-126): the cheap Heal (level 28) here, under
                                          // the Greater Heal of the low band (22) so a member below 45 percent still
                                          // gets the bigger heal; Flash Heal stays below it as the heal until level 28.
                                          new NextAction("heal on party", ACTION_MEDIUM_HEAL + 1),
                                          new NextAction("flash heal on party", ACTION_LIGHT_HEAL + 4),
                                          // new NextAction("renew on party", ACTION_LIGHT_HEAL + 8),
                                          nullptr)));

    triggers.push_back(
        new TriggerNode("party member almost full health",
                        NextAction::array(0,
                                          new NextAction("renew on party", ACTION_LIGHT_HEAL + 2),
                                          new NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 1),
                                          // Class sheet, holy priest (BT-126): Heal for the small deficits, after the
                                          // Renew and Prayer of Mending top-ups.
                                          new NextAction("heal on party", ACTION_LIGHT_HEAL),
                                          nullptr)));

    // Class sheet, holy priest (BT-126): upkeep on the main tank, Renew from level 26 and Prayer of Mending from level 68
    // ("good" tier). Both sit under the critical and low health chains (22 and up) and above the medium chain's Heal (21)
    // and the top-ups; the triggers hold the mana rule.
    triggers.push_back(new TriggerNode(
        "renew on main tank",
        NextAction::array(0, new NextAction("renew on main tank", ACTION_MEDIUM_HEAL + 1.9f), nullptr)));
    triggers.push_back(new TriggerNode(
        "prayer of mending on main tank",
        NextAction::array(0, new NextAction("prayer of mending on main tank", ACTION_MEDIUM_HEAL + 1.8f), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member to heal out of spell range",
        NextAction::array(0, new NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 10), nullptr)));

    // BT-157: top up the member the heal target value names while he is only a little hurt - above 60 percent mana any
    // member (good tier), between 30 and 60 percent only one who was just hit (expert tier), never below 30 percent - and
    // walk back to the tank when he is out of sight or out of heal range.
    triggers.push_back(new TriggerNode(
        "renew proactive",
        NextAction::array(0, new NextAction("renew proactive", ACTION_LIGHT_HEAL + HealerRelevance::PROACTIVE_HOT_OFFSET_OVER_LIGHT_HEAL),
                          nullptr)));
    triggers.push_back(new TriggerNode(
        "healer tank out of sight",
        NextAction::array(0, new NextAction("reach main tank to heal", ACTION_NORMAL + HealerRelevance::REACH_TANK_OFFSET_OVER_NORMAL),
                          nullptr)));
}
