/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#ifndef _PLAYERBOT_ROGUEACTIONS_H
#define _PLAYERBOT_ROGUEACTIONS_H

#include "GenericSpellActions.h"
#include "PvpSubtlety.h"

class PlayerbotAI;

class CastEvasionAction : public CastBuffSpellAction
{
public:
    CastEvasionAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "evasion") {}
};

class CastCloakOfShadowsAction : public CastBuffSpellAction
{
public:
    CastCloakOfShadowsAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "cloak of shadows") {}
};

class CastSprintAction : public CastBuffSpellAction
{
public:
    CastSprintAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "sprint") {}

    std::string const GetTargetName() override { return "self target"; }
};

// Shuriken Toss (114014) is a talent and the rogue's only ranged pull spell; see RoguePullStrategy.
class CastShurikenTossAction : public CastSpellAction
{
public:
    CastShurikenTossAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "shuriken toss") {}
};

class CastStealthAction : public CastBuffSpellAction
{
public:
    CastStealthAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "stealth") {}

    std::string const GetTargetName() override { return "self target"; }

    bool isUseful() override;  // #742 (AC RogueActions.h:49)
    bool isPossible() override;
};

class UnstealthAction : public Action
{
public:
    UnstealthAction(PlayerbotAI* botAI) : Action(botAI, "unstealth") {}

    bool Execute(Event event) override;
};

class RoguePeelAction : public CastDebuffSpellOnMeleeAttackerAction
{
public:
    RoguePeelAction(PlayerbotAI* ai) : CastDebuffSpellOnMeleeAttackerAction(ai, "gouge", false, 0.0f) {}
    bool isUseful() override;
};

class CancelBladeFlurryAction : public Action
{
public:
    CancelBladeFlurryAction(PlayerbotAI* ai) : Action(ai, "cancel blade flurry") {}
    bool Execute(Event event) override;
};

class CheckStealthAction : public Action
{
public:
    CheckStealthAction(PlayerbotAI* botAI) : Action(botAI, "check stealth") {}

    bool isPossible() override { return true; }
    bool Execute(Event event) override;
};

class CastKickAction : public CastSpellAction
{
public:
    CastKickAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "kick") {}
};

class CastFeintAction : public CastBuffSpellAction
{
public:
    CastFeintAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "feint") {}
};

class CastDismantleAction : public CastSpellAction
{
public:
    CastDismantleAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "dismantle") {}
};

class CastDistractAction : public CastSpellAction
{
public:
    CastDistractAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "distract") {}
};

class CastVanishAction : public CastBuffSpellAction
{
public:
    CastVanishAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "vanish") {}

    bool isUseful() override;
};

class CastBlindAction : public CastDebuffSpellAction
{
public:
    CastBlindAction(PlayerbotAI* botAI) : CastDebuffSpellAction(botAI, "blind") {}
};

class CastBladeFlurryAction : public CastBuffSpellAction
{
public:
    CastBladeFlurryAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "blade flurry") {}
};

class CastAdrenalineRushAction : public CastBuffSpellAction
{
public:
    CastAdrenalineRushAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "adrenaline rush") {}
};

// Shadowstep (36554), spec rows, prio group 0/10: gap closer on the target at 8-25 yd.
class CastShadowstepAction : public CastSpellAction
{
public:
    CastShadowstepAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "shadowstep") {}
};

// Smoke Bomb (76577), prio group 5: self cast at low health; BG/arena only ("in pvp instance" gate).
class CastSmokeBombAction : public CastBuffSpellAction
{
public:
    CastSmokeBombAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "smoke bomb") {}

    bool isUseful() override;
};

// Recuperate (73651), prio group 5: finisher heal at <= 30% hp with 5 combo points.
class CastRecuperateAction : public CastBuffSpellAction
{
public:
    CastRecuperateAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "recuperate") {}
};

// Shadow Blades (121471), prio group 5: self buff.
class CastShadowBladesAction : public CastBuffSpellAction
{
public:
    CastShadowBladesAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "shadow blades") {}
    bool isUseful() override;
};

// Subtlety sheet gap item 10: Shadow Dance (51713, level 80) opens an Ambush window.
class CastShadowDanceAction : public CastBuffSpellAction
{
public:
    CastShadowDanceAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "shadow dance") {}
};

// Vendetta (79140), prio group 0/10: debuff on the current target (not while stealthed).
class CastVendettaAction : public CastSpellAction
{
public:
    CastVendettaAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "vendetta") {}
};

// Marked for Death (137619), prio group 0/20: on the target while we have < 2 combo points.
class CastMarkedForDeathAction : public CastSpellAction
{
public:
    CastMarkedForDeathAction(PlayerbotAI* botAI) : CastSpellAction(botAI, "marked for death") {}
};

// Preparation (14185), prio group 5: resets Vanish/Sprint/Shadowstep etc.
class CastPreparationAction : public CastBuffSpellAction
{
public:
    CastPreparationAction(PlayerbotAI* botAI) : CastBuffSpellAction(botAI, "preparation") {}
};

// "Rogue kick victim / random" (Kick 1766, prio group 10): kick any enemy caster in range.
// Uses task 0's CastSpellOnEnemyCasterAction (shared contract). Open world / dungeon behaviour stays AC: BG/arena only.
class CastKickOnEnemyCasterAction : public CastSpellOnEnemyCasterAction
{
public:
    CastKickOnEnemyCasterAction(PlayerbotAI* botAI) : CastSpellOnEnemyCasterAction(botAI, "kick") {}

    std::string const getName() override { return "kick on enemy caster"; }
    bool isUseful() override;
};

class CastKillingSpreeAction : public CastMeleeSpellAction
{
public:
    CastKillingSpreeAction(PlayerbotAI* botAI) : CastMeleeSpellAction(botAI, "killing spree") {}
    bool isUseful() override;
};

class CastKickOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
{
public:
    CastKickOnEnemyHealerAction(PlayerbotAI* botAI) : CastSpellOnEnemyHealerAction(botAI, "kick") {}
};

class EnvenomAction : public CastMeleeSpellAction
{
public:
    EnvenomAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "envenom") {}
};

class CastTricksOfTheTradeOnMainTankAction : public BuffOnMainTankAction
{
public:
    CastTricksOfTheTradeOnMainTankAction(PlayerbotAI* ai) : BuffOnMainTankAction(ai, "tricks of the trade", true) {}
    virtual bool isUseful() override;
};

class FanOfKnivesAction : public CastMeleeSpellAction
{
public:
    FanOfKnivesAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "fan of knives") {}
    ActionThreatType getThreatType() override { return ActionThreatType::Aoe; }
};

// BT-691 to BT-693: the casts of the Subtlety package of the PvP class rules; the glue picks the target and casts.
class PvpSubtletyAction : public Action
{
public:
    PvpSubtletyAction(PlayerbotAI* botAI, char const* name, PvpSubtletyGlue::Rule rule) : Action(botAI, name), rule(rule) {}
    bool isUseful() override;
    bool Execute(Event event) override;

private:
    PvpSubtletyGlue::Rule rule;
};

#endif
