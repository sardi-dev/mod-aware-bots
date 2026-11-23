/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "FeralDruidStrategy.h"

#include "Playerbots.h"

// Balance sheet: these heals are allowed in Moonkin Form. Other forms still need to shift.
NextAction** DruidHealingFormPrerequisite(PlayerbotAI* botAI)
{
    return BotGetSpecialization(botAI->GetBot()) == SPEC_DRUID_BALANCE ? nullptr :
        NextAction::array(0, new NextAction("caster form"), nullptr);
}

class FeralDruidStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    FeralDruidStrategyActionNodeFactory()
    {
        creators["survival instincts"] = &survival_instincts;
        creators["omen of clarity"] = &omen_of_clarity;
        creators["cure poison"] = &cure_poison;
        creators["cure poison on party"] = &cure_poison_on_party;
        creators["abolish poison"] = &abolish_poison;
        creators["abolish poison on party"] = &abolish_poison_on_party;
        creators["prowl"] = &prowl;
    }

private:
    static ActionNode* survival_instincts([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("survival instincts",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("barkskin"), nullptr),
                              /*C*/ nullptr);
    }


    static ActionNode* omen_of_clarity([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("omen of clarity",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* cure_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* cure_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("cure poison on party",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* abolish_poison([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* abolish_poison_on_party([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("abolish poison on party",
                              /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* prowl([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("prowl",
                              /*P*/ NextAction::array(0, new NextAction("cat form"), nullptr),
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }
};

FeralDruidStrategy::FeralDruidStrategy(PlayerbotAI* botAI) : GenericDruidStrategy(botAI)
{
    actionNodeFactories.Add(new FeralDruidStrategyActionNodeFactory());
    actionNodeFactories.Add(new ShapeshiftDruidStrategyActionNodeFactory());
}

void FeralDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericDruidStrategy::InitTriggers(triggers);

    // BT-066: a levelling Feral or Guardian druid heals itself like a player (caster form, then back)
    triggers.push_back(new TriggerNode("druid self rejuvenation",
                                       NextAction::array(0, new NextAction("rejuvenation", 34.0f), nullptr)));
    if (botAI->GetBot()->getLevel() >= 26)
    triggers.push_back(new TriggerNode("druid self healing touch",
                                       NextAction::array(0, new NextAction("healing touch", 35.0f), nullptr)));
    // triggers.push_back(new TriggerNode("not facing target", NextAction::array(0, new NextAction("set facing",
    // ACTION_NORMAL + 7), nullptr)));
    triggers.push_back(new TriggerNode(
        "enemy out of melee", NextAction::array(0, new NextAction("reach melee", ACTION_HIGH + 1), nullptr)));
    // triggers.push_back(new TriggerNode("enemy too close for melee", NextAction::array(0, new NextAction("move out of
    // enemy contact", ACTION_NORMAL + 8), nullptr)));
    if (botAI->GetBot()->getLevel() >= 56)
    triggers.push_back(new TriggerNode(
        "critical health", NextAction::array(0, new NextAction("survival instincts", ACTION_EMERGENCY + 1), nullptr)));
    // #384 (AC FeralDruidStrategy.cpp:86-87): AC fires Survival Instincts from "low health" at 91.0 (the fork's
    // "critical health" edge above stays). Survival Instincts (61336) is spec103/104 level 56 in 5.4.8 (cls.py
    // druid); the ActionNode falls back to barkskin when it is on cooldown.
    if (botAI->GetBot()->getLevel() >= 56)
    triggers.push_back(new TriggerNode(
        "low health", NextAction::array(0, new NextAction("survival instincts", ACTION_EMERGENCY + 1), nullptr)));
    // BT-146: Omen of Clarity is passive in 5.4.8 and cannot be cast.
    if (botAI->GetBot()->getLevel() >= 24)
    triggers.push_back(new TriggerNode("player has flag",
                                       NextAction::array(0, new NextAction("dash", ACTION_EMERGENCY + 2), nullptr)));
    if (botAI->GetBot()->getLevel() >= 24)
    triggers.push_back(new TriggerNode("enemy flagcarrier near",
                                       NextAction::array(0, new NextAction("dash", ACTION_EMERGENCY + 2), nullptr)));
    // #595 (AC FeralDruidStrategy.cpp:75-92 has no berserk edge): the fork-only "berserk" edge (ACTION_HIGH+6) that
    // lived here duplicated DruidBoostStrategy's "berserk" -> berserk at 27.5 (AC GenericDruidStrategy.cpp:103) and
    // fired outside the "boost" strategy; removed, Berserk 106952 (spec103/104 L48) now comes only from "boost".
}

// #408 (AC parity, Ai/Class/Druid/Strategy/FeralDruidStrategy.cpp:94-104).
void FeralChargeDruidStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    Player* bot = botAI->GetBot();

    if (BotGetSpecialization(bot) == SPEC_DRUID_FERAL && !bot->HasSpell(102401))
        return;
    if (bot->HasSpell(768) /*cat form*/ && !bot->HasAura(16931) /*thick hide*/)
        triggers.push_back(new TriggerNode(
            "enemy out of melee", NextAction::array(0, new NextAction("feral charge - cat", 29.0f), nullptr)));
    else
        triggers.push_back(new TriggerNode(
            "enemy out of melee", NextAction::array(0, new NextAction("feral charge - bear", 18.0f), nullptr)));
}
