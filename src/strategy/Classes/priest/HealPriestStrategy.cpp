/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HealPriestStrategy.h"

#include "GenericPriestStrategyActionNodeFactory.h"
#include "HealerTriggers.h"
#include "Playerbots.h"

HealPriestStrategy::HealPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPriestStrategyActionNodeFactory());
}

NextAction** HealPriestStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("shoot", ACTION_DEFAULT), nullptr);
}

void HealPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    // triggers.push_back(new TriggerNode(
    //     "enemy out of spell",
    //     NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    // triggers.push_back(new TriggerNode(
    // 	"medium aoe heal",
    // 	NextAction::array(0,
    //         new NextAction("circle of healing on party", ACTION_MEDIUM_HEAL + 8),
    //         // new NextAction("power word: shield on almost full health below", ACTION_MEDIUM_HEAL + 7),
    //         NULL)));

    // Class sheet, discipline priest (BT-128, docs/class-sheets/priest-discipline.md): the group heals use "disc group
    // heal setting" and "disc medium group heal setting", which fire on three hurt members in a party of two to four as
    // well (the shared triggers need five near members). Power Word: Shield is no longer part of these chains: the
    // group shield went to whoever was not full, mostly a damage dealer, at 6.1 percent of the base mana each; it stays
    // at critical and low health and on the main tank (below). Divine Hymn is not a discipline spell and keeps the
    // shared rule, for the holy priest that runs this chain as a damage dealer.
    triggers.push_back(new TriggerNode(
        "disc group heal setting",
        NextAction::array(0, new NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 8), nullptr)));

    triggers.push_back(new TriggerNode(
        "disc medium group heal setting",
        NextAction::array(0, new NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 6),
                          new NextAction("prayer of healing on party", ACTION_CRITICAL_HEAL + 4),
                          new NextAction("halo", ACTION_CRITICAL_HEAL + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium group heal setting",
        NextAction::array(0, new NextAction("divine hymn", ACTION_CRITICAL_HEAL + 7), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member critical health",
        NextAction::array(0, new NextAction("power word: shield on party", ACTION_CRITICAL_HEAL + 5),
                          new NextAction("penance on party", ACTION_CRITICAL_HEAL + 4),
                          new NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 3),
                          new NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 2),
                          nullptr)));

    // Class sheet, discipline priest (BT-128): Greater Heal is the heal of this band (below 45 percent), in place of
    // Flash Heal, which stays with the critical band and with the medium band below until Greater Heal is learned (34).
    triggers.push_back(
        new TriggerNode("party member low health",
                        NextAction::array(0, new NextAction("power word: shield on party", ACTION_MEDIUM_HEAL + 4),
                                          new NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 3),
                                          new NextAction("penance on party", ACTION_MEDIUM_HEAL + 2),
                                          new NextAction("greater heal on party", ACTION_MEDIUM_HEAL + 0), nullptr)));

    // Class sheet, discipline priest (BT-128): no shield here any more (non-tank members below 65 percent get Penance and
    // the cheap Heal, level 28; Flash Heal stays below it as the heal until Heal is learned).
    triggers.push_back(
        new TriggerNode("party member medium health",
                        NextAction::array(0, new NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 7),
                                          new NextAction("penance on party", ACTION_LIGHT_HEAL + 6),
                                          new NextAction("heal on party", ACTION_LIGHT_HEAL + 5),
                                          new NextAction("flash heal on party", ACTION_LIGHT_HEAL + 4),
                                          // new NextAction("renew on party", ACTION_LIGHT_HEAL + 8),
                                          nullptr)));

    // Class sheet, discipline priest (BT-128): the almost full band lost its shield too (#524 had put it first, after
    // AC) and gets Heal for the small deficits, after the Prayer of Mending and Renew top-ups.
    triggers.push_back(
        new TriggerNode("party member almost full health",
                        NextAction::array(0,
                                          // new NextAction("penance on party", ACTION_LIGHT_HEAL + 3),
                                          new NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 2),
                                          new NextAction("renew on party", ACTION_LIGHT_HEAL + 1),
                                          new NextAction("heal on party", ACTION_LIGHT_HEAL),
                                          nullptr)));

    // Class sheet, discipline priest (BT-128): upkeep on the main tank in combat, Power Word: Shield first (it needs no
    // Weakened Soul), then Renew from level 26 and Prayer of Mending from level 68 ("good" tier), the last two with the
    // triggers and actions of the holy priest's tank upkeep. All three sit under the critical and low health chains
    // (22 and up) and above the medium band's Penance and Heal; the triggers hold the mana rule.
    triggers.push_back(new TriggerNode(
        "power word: shield on main tank",
        NextAction::array(0, new NextAction("power word: shield on main tank", ACTION_MEDIUM_HEAL + 1.95f), nullptr)));
    triggers.push_back(new TriggerNode(
        "renew on main tank",
        NextAction::array(0, new NextAction("renew on main tank", ACTION_MEDIUM_HEAL + 1.9f), nullptr)));
    triggers.push_back(new TriggerNode(
        "prayer of mending on main tank",
        NextAction::array(0, new NextAction("prayer of mending on main tank", ACTION_MEDIUM_HEAL + 1.8f), nullptr)));

    // triggers.push_back(new TriggerNode("almost full health", NextAction::array(0, new NextAction("renew", 43.f),
    // nullptr))); triggers.push_back(new TriggerNode("party member almost full health", NextAction::array(0, new
    // NextAction("heal on party", 41.0f), new NextAction("renew on party", 40.0f), nullptr))); triggers.push_back(new
    // TriggerNode("party member medium health", NextAction::array(0, new NextAction("greater heal on party", 47.0f),
    // nullptr))); triggers.push_back(new TriggerNode("party member low health", NextAction::array(0, new
    // NextAction("power word: shield on party", 51.0f), new NextAction("greater heal on party", 50.0f), nullptr)));
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
    // triggers.push_back(new TriggerNode("medium aoe heal", NextAction::array(0, new NextAction("prayer of
    // mending", 49.0f), nullptr))); triggers.push_back(new TriggerNode("medium aoe heal", NextAction::array(0, new
    // NextAction("circle of healing on party", 48.0f), nullptr))); triggers.push_back(new TriggerNode("binding heal",
    // NextAction::array(0, new NextAction("binding heal", 52.0f), nullptr))); triggers.push_back(new TriggerNode("low
    // mana", NextAction::array(0, new NextAction("shadowfiend", ACTION_HIGH), nullptr)));

    // Task 8 / #863 (MoP rotation row 25603 disc / 25703 holy, group 1/10-1/12): the Surge of Light proc makes
    // Flash Heal free, so spend it on the hurt party member first.
    triggers.push_back(new TriggerNode(
        "surge of light proc", NextAction::array(0, new NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 6), nullptr)));
    // Task 8 / #863 (MoP rotation row 25607 disc / 25707 holy, group 1/9-1/11): Binding Heal when both the bot
    // and the target are hurt (BindingHealTrigger = party member low health + self below medium).
    triggers.push_back(new TriggerNode(
        "binding heal", NextAction::array(0, new NextAction("binding heal", ACTION_MEDIUM_HEAL + 5), nullptr)));

    triggers.push_back(new TriggerNode(
        "critical health", NextAction::array(0, new NextAction("pain suppression", ACTION_EMERGENCY + 1), nullptr)));
    triggers.push_back(
        new TriggerNode("protect party member",
                        NextAction::array(0, new NextAction("pain suppression on party", ACTION_EMERGENCY), nullptr)));
    // BT-293: before the tank's health reaches the critical band when its danger reading is high (TankDangerPolicy).
    triggers.push_back(new TriggerNode("main tank in danger",
        NextAction::array(0, new NextAction("pain suppression on main tank", ACTION_EMERGENCY), nullptr)));
}
