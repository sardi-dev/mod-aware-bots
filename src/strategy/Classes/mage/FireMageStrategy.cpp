/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "FireMageStrategy.h"

#include "Playerbots.h"

/*
* Rune of power is cast on target not self
*/

class FireMageStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    FireMageStrategyActionNodeFactory()
    {
        creators["fireball"] = &fireball;
        creators["dragon's breath"] = &dragon_breath;
        creators["inferno blast"] = &inferno_blast;
        creators["pyroblast"] = &pyroblast;
        creators["scorch"] = &scorch;
        creators["flamestrike"] = &flamestrike;
        creators["living bomb"] = &living_bomb;
        creators["living bomb on attackers"] = &living_bomb_on_attackers;
    }

private:
    static ActionNode* fireball([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("fireball",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* dragon_breath([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("dragon's breath",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* inferno_blast([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("inferno blast",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("fire blast"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* pyroblast([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("pyroblast",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* scorch([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("scorch",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    // Rotation fix task B (#865): Living Bomb 44457 and Nether Tempest 114923 are two choices of the same level-75 talent
    // row (see FrostMageStrategy.cpp frost_bomb, #790). A fire bot that took Nether Tempest got 35x "living bomb -
    // IMPOSSIBLE" (w73 fire leg) and never applied a bomb (prio 0/10 in the MoP fire list).
    static ActionNode* living_bomb([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("living bomb",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("nether tempest"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* living_bomb_on_attackers([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("living bomb on attackers",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("nether tempest on attackers"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* flamestrike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("flamestrike",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
};

FireMageStrategy::FireMageStrategy(PlayerbotAI* botAI)
    : GenericMageStrategy(botAI)
{
    actionNodeFactories.Add(new FireMageStrategyActionNodeFactory());
}

NextAction** FireMageStrategy::getDefaultActions()
{
    return NextAction::array(0,
        new NextAction("frostjaw", ACTION_DEFAULT + 0.10f),
        new NextAction("combustion", ACTION_DEFAULT + 0.9f),
        new NextAction("living bomb", ACTION_DEFAULT + 0.7f),
        new NextAction("fireball", ACTION_DEFAULT + 0.6f),
        new NextAction("scorch", ACTION_DEFAULT), nullptr);
}

void FireMageStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericMageStrategy::InitTriggers(triggers);
    // Mage sheets: Cone of Cold supplies a close-range player slow.
    triggers.push_back(new TriggerNode("mage player slow", NextAction::array(0, new NextAction("cone of cold", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode("medium health", NextAction::array(0, new NextAction("ice barrier", ACTION_NORMAL), nullptr)));
    // Mage fire sheet: save Inferno Blast for Heating Up unless moving.
    triggers.push_back(new TriggerNode("mage moving", NextAction::array(0, new NextAction("inferno blast", ACTION_NORMAL), nullptr)));
    triggers.push_back(new TriggerNode("heating up", NextAction::array(0, new NextAction("inferno blast", ACTION_INTERRUPT), nullptr)));
    triggers.push_back(new TriggerNode("pyroblast!", NextAction::array(0, new NextAction("pyroblast", ACTION_INTERRUPT + 10), nullptr)));
    // Fire sheet line 5: open a solo fight with a hard-cast Pyroblast, above the Fireball defaults.
    triggers.push_back(new TriggerNode("fire pyroblast opener", NextAction::array(0, new NextAction("pyroblast", ACTION_HIGH + 6), nullptr)));
    // Fire sheet rotation item 5: Presence of Mind (off the global cooldown) on a big fight, then its instant Pyroblast
    // just below the free one; Alter Time while a free Pyroblast is up (level 87 line).
    triggers.push_back(new TriggerNode("fire presence of mind", NextAction::array(0, new NextAction("presence of mind", ACTION_INTERRUPT + 11), nullptr)));
    triggers.push_back(new TriggerNode("presence of mind active", NextAction::array(0, new NextAction("pyroblast", ACTION_INTERRUPT + 9), nullptr)));
    // BT-620: against a player the aura goes to Frostjaw (the freeze setup of the burst), not to the Pyroblast edge above.
    triggers.push_back(new TriggerNode("fire pom control", NextAction::array(0, new NextAction("frostjaw", ACTION_INTERRUPT + 9), nullptr)));
    triggers.push_back(new TriggerNode("fire alter time", NextAction::array(0, new NextAction("alter time", ACTION_INTERRUPT + 11), nullptr)));
    triggers.push_back(new TriggerNode("combustion", NextAction::array(0, new NextAction("combustion", 50.0f), nullptr)));
    triggers.push_back(new TriggerNode("living bomb", NextAction::array(0, new NextAction("living bomb", 19.0f), nullptr)));
    // #385 (AC Ai/Class/Mage/Strategy/FireMageStrategy.cpp:60-67): Dragon's Breath when an enemy is too close for a cast
    triggers.push_back(new TriggerNode("enemy too close for spell",
                                       NextAction::array(0, new NextAction("dragon's breath", ACTION_INTERRUPT + 1), nullptr)));
}

void FireMageAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("medium aoe", NextAction::array(0,
            new NextAction("dragon's breath", 24.0f),
            new NextAction("flamestrike", 23.0f), 
            new NextAction("living bomb on attackers", 21.0f), 
            new NextAction("blizzard", 20.0f), nullptr)));
}

