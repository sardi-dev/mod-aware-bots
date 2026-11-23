
#include "AssassinationRogueStrategy.h"

#include "Multiplier.h"
#include "Playerbots.h"
#include "PvpSubtlety.h"

class AssassinationRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    AssassinationRogueStrategyActionNodeFactory()
    {
        creators["mutilate"] = &mutilate;
        creators["envenom"] = &envenom;
        // #604/#386 (AC AssassinationRogueStrategy.cpp:16-20): AC's mutilate -> backstab -> sinister strike chain and the
        // rupture -> eviscerate node. Subtlety (no Mutilate, learns Backstab 53 spec 261 L40, cls.py) runs this strategy too.
        creators["backstab"] = &backstab;
        creators["rupture"] = &rupture;
    }

private:
    static ActionNode* mutilate(PlayerbotAI* ai)
    {
        return new ActionNode("mutilate",
                              /*P*/ NULL,
                              /*A*/ NextAction::array(0, new NextAction("backstab"), NULL),
                              /*C*/ NULL);
    }
    static ActionNode* backstab(PlayerbotAI* ai)
    {
        return new ActionNode("backstab",
                              /*P*/ NULL,
                              /*A*/ NextAction::array(0, new NextAction("hemorrhage"), new NextAction("sinister strike"), NULL),
                              /*C*/ NULL);
    }
    static ActionNode* rupture(PlayerbotAI* ai)
    {
        return new ActionNode("rupture",
                              /*P*/ NULL,
                              /*A*/ NULL,
                              /*C*/ NULL);
    }
    static ActionNode* envenom(PlayerbotAI* ai)
    {
        return new ActionNode("envenom",
                              /*P*/ NULL,
                              /*A*/ NextAction::array(0, new NextAction("eviscerate"), NULL),
                              /*C*/ NULL);
    }
};

AssassinationRogueStrategy::AssassinationRogueStrategy(PlayerbotAI* ai) : MeleeCombatStrategy(ai)
{
    actionNodeFactories.Add(new AssassinationRogueStrategyActionNodeFactory());
}

NextAction** AssassinationRogueStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("melee", ACTION_DEFAULT), NULL);
}

void AssassinationRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    // A stealthed rogue switches to the "stealthed" opener; its own "no stealth" edge switches back.
    triggers.push_back(
        new TriggerNode("in stealth", NextAction::array(0, new NextAction("check stealth", ACTION_EMERGENCY), nullptr)));

    triggers.push_back(new TriggerNode("high energy available",
                                       NextAction::array(0, new NextAction("garrote", ACTION_HIGH + 7),
                                                         new NextAction("ambush", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode("rogue energy 55",
                                       NextAction::array(0, new NextAction("mutilate", ACTION_NORMAL + 3), NULL)));

    // #386 (AC AssassinationRogueStrategy.cpp:69-80): AC's "hunger for blood" edge is removed -- Hunger for Blood
    // is not learnable by a rogue in 5.4.8 (cls.py rogue: NOT-LEARNABLE; removed before MoP).

    triggers.push_back(new TriggerNode("slice and dice",
                                       NextAction::array(0, new NextAction("slice and dice", ACTION_HIGH + 7), NULL)));

    // #386 (AC AssassinationRogueStrategy.cpp:117-135): AC fires Envenom at 4 combo points (priority HIGH+5) and
    // on a target that will die soon (HIGH+4). AC's "cold blood" sibling is not learnable in 5.4.8 (cls.py
    // rogue: NOT-LEARNABLE) and is left out. Envenom = 32645, Assassination spec 259, L20 (cls.py).
    // A five-point Rupture enables Venomous Wounds (rogue-assassination sheet).
    triggers.push_back(new TriggerNode("combo points 5 available",
        NextAction::array(0, new NextAction("rupture", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode("combo points 4 available",
                                       NextAction::array(0, new NextAction("envenom", ACTION_HIGH + 5), NULL)));

    triggers.push_back(new TriggerNode("target with combo points almost dead",
                                       NextAction::array(0, new NextAction("envenom", ACTION_HIGH + 4), NULL)));

    triggers.push_back(
        new TriggerNode("expose armor", NextAction::array(0, new NextAction("expose armor", ACTION_HIGH + 3), NULL)));

    // An expert can peel a second attacker (rogue sheets).
    triggers.push_back(new TriggerNode("medium threat", NextAction::array(0, new NextAction("gouge on attacker", ACTION_HIGH + 1), nullptr)));
    triggers.push_back(
        new TriggerNode("medium threat", NextAction::array(0, new NextAction("vanish", ACTION_HIGH), NULL)));

    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("evasion", ACTION_HIGH + 9),
                                                        new NextAction("combat readiness", ACTION_HIGH + 8.5f),
                                                        new NextAction("feint", ACTION_HIGH + 8), nullptr)));

    triggers.push_back(
        new TriggerNode("critical health", NextAction::array(0, new NextAction("cloak of shadows", ACTION_HIGH + 7), nullptr)));

    triggers.push_back(
        new TriggerNode("kick", NextAction::array(0, new NextAction("kick", ACTION_INTERRUPT + 2), NULL)));

    triggers.push_back(
        new TriggerNode("kick on enemy healer",
                        NextAction::array(0, new NextAction("kick on enemy healer", ACTION_INTERRUPT + 1), NULL)));

    triggers.push_back(
        new TriggerNode("medium aoe", NextAction::array(0, new NextAction("fan of knives", ACTION_NORMAL + 5), NULL)));

    // Rotation port (epic #854 / #859), MoP Assassination and Subtlety rows. AC's kit above is kept.
    // Unknown spells are skipped by the action engine.
    // (prio 10/2, 10/1): Kick any casting enemy; BG/arena only (action gate).
    triggers.push_back(new TriggerNode(
        "enemy caster in range", NextAction::array(0, new NextAction("kick on enemy caster", ACTION_INTERRUPT), NULL)));
    // Marked for Death (0/20), Shadowstep (0/10), Vendetta (0/10).
    triggers.push_back(new TriggerNode(
        "marked for death", NextAction::array(0, new NextAction("marked for death", ACTION_NORMAL + 7), NULL)));
    triggers.push_back(new TriggerNode(
        "shadowstep", NextAction::array(0, new NextAction("shadowstep", ACTION_NORMAL + 6), NULL)));
    triggers.push_back(new TriggerNode(
        "vendetta", NextAction::array(0, new NextAction("vendetta", ACTION_NORMAL + 5), NULL)));
    // Dispatch (0/4): above Mutilate (ACTION_NORMAL + 3); the game limits it to targets under 35% hp.
    triggers.push_back(new TriggerNode(
        "rogue energy 30", NextAction::array(0, new NextAction("dispatch", ACTION_NORMAL + 4), NULL)));
    // Group 5: Smoke Bomb (<= 20% hp; BG/arena gate), Recuperate (<= 30% hp, 5 CP),
    // Shadow Blades, Preparation.
    triggers.push_back(new TriggerNode(
        "critical health", NextAction::array(0, new NextAction("smoke bomb", ACTION_HIGH + 6), NULL)));
    triggers.push_back(new TriggerNode(
        "recuperate", NextAction::array(0, new NextAction("recuperate", ACTION_HIGH + 4), NULL)));
    triggers.push_back(new TriggerNode(
        "shadow blades", NextAction::array(0, new NextAction("shadow blades", ACTION_HIGH + 2), NULL)));
    triggers.push_back(new TriggerNode(
        "preparation", NextAction::array(0, new NextAction("preparation", ACTION_HIGH + 1), NULL)));

    triggers.push_back(new TriggerNode(
        "low tank threat",
        NextAction::array(0, new NextAction("tricks of the trade on main tank", ACTION_HIGH + 7), NULL)));

    triggers.push_back(new TriggerNode(
        "enemy out of melee",
        NextAction::array(0, new NextAction("stealth", ACTION_HIGH + 3), new NextAction("sprint", ACTION_HIGH + 2),
                          new NextAction("reach melee", ACTION_HIGH + 1), NULL)));
}

namespace
{
// BT-692: against a player Shadow Dance, Shadow Blades and Marked for Death are the go, held for the burst window (PvP/PvpSubtlety.cpp).
// PvE and casual bots keep the older gates.
class PvpSubtletyMultiplier : public Multiplier
{
public:
    PvpSubtletyMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "pvp subtlety") {}

    float GetValue(Action* action) override { return PvpSubtletyGlue::Holds(botAI, bot, action) ? 0.0f : 1.0f; }
};
}  // namespace

void SubtletyRogueStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    AssassinationRogueStrategy::InitMultipliers(multipliers);
    multipliers.push_back(new PvpSubtletyMultiplier(botAI));
}

void SubtletyRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    // BT-691 to BT-693 (PvP class rules, Subtlety package; every edge answers only against enemy players): Sap on a second enemy
    // from stealth before the opener, and Tricks of the Trade on a damage partner when the team go opens.
    triggers.push_back(new TriggerNode("pvp sap", NextAction::array(0, new NextAction("pvp sap", ACTION_HIGH + 9), nullptr)));
    triggers.push_back(new TriggerNode("pvp tricks", NextAction::array(0, new NextAction("pvp tricks", ACTION_HIGH + 8), nullptr)));

    // A stealthed rogue switches to the "stealthed" opener; its own "no stealth" edge switches back.
    triggers.push_back(
        new TriggerNode("in stealth", NextAction::array(0, new NextAction("check stealth", ACTION_EMERGENCY), nullptr)));

    triggers.push_back(new TriggerNode("high energy available",
                                       NextAction::array(0, new NextAction("garrote", ACTION_HIGH + 7),
                                                         new NextAction("ambush", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode("rogue energy 35",
                                       NextAction::array(0, new NextAction("backstab", ACTION_NORMAL + 3), NULL)));

    triggers.push_back(new TriggerNode("rogue energy 30",
        NextAction::array(0, new NextAction("hemorrhage", ACTION_NORMAL + 2), nullptr)));

    // #386 (AC AssassinationRogueStrategy.cpp:69-80): AC's "hunger for blood" edge is removed -- Hunger for Blood
    // is not learnable by a rogue in 5.4.8 (cls.py rogue: NOT-LEARNABLE; removed before MoP).

    triggers.push_back(new TriggerNode("slice and dice",
                                       NextAction::array(0, new NextAction("slice and dice", ACTION_HIGH + 7), NULL)));

    // Rupture enables Sanguinary Vein (rogue-subtlety sheet).
    triggers.push_back(new TriggerNode("combo points 5 available",
        NextAction::array(0, new NextAction("rupture", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode("combo points 5 available",
                                       NextAction::array(0, new NextAction("eviscerate", ACTION_HIGH + 5), NULL)));

    triggers.push_back(new TriggerNode("target with combo points almost dead",
                                       NextAction::array(0, new NextAction("eviscerate", ACTION_HIGH + 4), NULL)));

    triggers.push_back(
        new TriggerNode("expose armor", NextAction::array(0, new NextAction("expose armor", ACTION_HIGH + 3), NULL)));

    // An expert can peel a second attacker (rogue sheets).
    triggers.push_back(new TriggerNode("medium threat", NextAction::array(0, new NextAction("gouge on attacker", ACTION_HIGH + 1), nullptr)));
    triggers.push_back(
        new TriggerNode("medium threat", NextAction::array(0, new NextAction("vanish", ACTION_HIGH), NULL)));

    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("evasion", ACTION_HIGH + 9),
                                                        new NextAction("combat readiness", ACTION_HIGH + 8.5f),
                                                        new NextAction("feint", ACTION_HIGH + 8), nullptr)));

    triggers.push_back(
        new TriggerNode("critical health", NextAction::array(0, new NextAction("cloak of shadows", ACTION_HIGH + 7), nullptr)));

    triggers.push_back(
        new TriggerNode("kick", NextAction::array(0, new NextAction("kick", ACTION_INTERRUPT + 2), NULL)));

    triggers.push_back(
        new TriggerNode("kick on enemy healer",
                        NextAction::array(0, new NextAction("kick on enemy healer", ACTION_INTERRUPT + 1), NULL)));

    triggers.push_back(
        new TriggerNode("medium aoe", NextAction::array(0, new NextAction("fan of knives", ACTION_NORMAL + 5), NULL)));

    // (prio 10/2, 10/1): Kick any casting enemy; BG/arena only (action gate).
    triggers.push_back(new TriggerNode(
        "enemy caster in range", NextAction::array(0, new NextAction("kick on enemy caster", ACTION_INTERRUPT), NULL)));
    // Shadowstep closes the gap when the talent is known.
    triggers.push_back(new TriggerNode(
        "shadowstep", NextAction::array(0, new NextAction("shadowstep", ACTION_NORMAL + 6), NULL)));
    // Hemorrhage (Subtlety, 0/2): refresh the debuff.
    triggers.push_back(new TriggerNode(
        "hemorrhage", NextAction::array(0, new NextAction("hemorrhage", ACTION_NORMAL + 4), NULL)));
    // Group 5: Smoke Bomb (<= 20% hp; BG/arena gate), Recuperate (<= 30% hp, 5 CP),
    // Shadow Blades, Preparation.
    triggers.push_back(new TriggerNode(
        "critical health", NextAction::array(0, new NextAction("smoke bomb", ACTION_HIGH + 6), NULL)));
    triggers.push_back(new TriggerNode(
        "recuperate", NextAction::array(0, new NextAction("recuperate", ACTION_HIGH + 4), NULL)));
    triggers.push_back(new TriggerNode(
        "shadow blades", NextAction::array(0, new NextAction("shadow blades", ACTION_HIGH + 2), NULL)));
    triggers.push_back(new TriggerNode(
        "preparation", NextAction::array(0, new NextAction("preparation", ACTION_HIGH + 1), NULL)));
    // Subtlety sheet gap item 10: Shadow Dance opens the Ambush window (the "high energy available" Ambush edge above).
    triggers.push_back(new TriggerNode(
        "subtlety shadow dance", NextAction::array(0, new NextAction("shadow dance", ACTION_HIGH + 8), nullptr)));

    triggers.push_back(new TriggerNode(
        "low tank threat",
        NextAction::array(0, new NextAction("tricks of the trade on main tank", ACTION_HIGH + 7), NULL)));

    triggers.push_back(new TriggerNode(
        "enemy out of melee",
        NextAction::array(0, new NextAction("stealth", ACTION_HIGH + 3), new NextAction("sprint", ACTION_HIGH + 2),
                          new NextAction("reach melee", ACTION_HIGH + 1), NULL)));
}
