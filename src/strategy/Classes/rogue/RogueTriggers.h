/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#ifndef _PLAYERBOT_ROGUETRIGGERS_H
#define _PLAYERBOT_ROGUETRIGGERS_H

#include "GenericTriggers.h"
#include "PvpSubtlety.h"

class PlayerbotAI;

// 1:1 port of AC Ai/Class/Rogue/RogueTriggers.h (WotLK mod-playerbots), registered in
// RogueAiObjectContext.cpp with AC's exact creator names. Every base class used below already exists with
// AC's signature in strategy/triggers/GenericTriggers.h (InterruptSpellTrigger, InterruptEnemyHealerTrigger,
// BuffTrigger, BoostTrigger, SpellCanBeCastTrigger, DebuffTrigger, HasAuraTrigger, HasNoAuraTrigger,
// HasCcTargetTrigger, BuffOnMainTankTrigger), and every spell name is the lower-case full spell name this
// fork already uses in RogueActions.h, so HasAura()/GetSpellId() resolve.
//
// #357 slice 2: the fork registered 1 of AC's 18 Rogue triggers ("no stealth"). These are the other 17.
// Line references are AC's own.
class KickInterruptSpellTrigger : public InterruptSpellTrigger
{
public:
    KickInterruptSpellTrigger(PlayerbotAI* botAI) : InterruptSpellTrigger(botAI, "kick") {}
};

class SliceAndDiceTrigger : public BuffTrigger
{
public:
    SliceAndDiceTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "slice and dice") {}

    // #386 (wave-9: checkcast result=81 NO_COMBO_POINTS every tick): Slice and Dice 5171 is a finisher in
    // 5.4.8 (needs >= 1 combo point), so the buff trigger only fires with a combo point on the target.
    bool IsActive() override;
};

class AdrenalineRushTrigger : public BoostTrigger
{
public:
    AdrenalineRushTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "adrenaline rush") {}

    bool IsActive() override;
};

class BladeFlurryTrigger : public BoostTrigger
{
public:
    BladeFlurryTrigger(PlayerbotAI* botAI) : BoostTrigger(botAI, "blade flurry") {}
};

class RuptureTrigger : public DebuffTrigger
{
public:
    RuptureTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "rupture", 1, true) {}
};

class ExposeArmorTrigger : public DebuffTrigger
{
public:
    ExposeArmorTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "expose armor") {}

    bool IsActive() override;
};

class KickInterruptEnemyHealerSpellTrigger : public InterruptEnemyHealerTrigger
{
public:
    KickInterruptEnemyHealerSpellTrigger(PlayerbotAI* botAI) : InterruptEnemyHealerTrigger(botAI, "kick") {}
};

class InStealthTrigger : public HasAuraTrigger
{
public:
    InStealthTrigger(PlayerbotAI* botAI) : HasAuraTrigger(botAI, "stealth") {}
};

// #149 (AC RogueTriggers.h:77-81): Stealth is unchanged in MoP, ported as-is.
class NoStealthTrigger : public HasNoAuraTrigger
{
public:
    NoStealthTrigger(PlayerbotAI* botAI) : HasNoAuraTrigger(botAI, "stealth") {}
};

class UnstealthTrigger : public BuffTrigger
{
public:
    UnstealthTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "stealth", 3) {}

    bool IsActive() override;
};

class StealthTrigger : public Trigger
{
public:
    StealthTrigger(PlayerbotAI* botAI) : Trigger(botAI, "stealth") {}

    bool IsActive() override;
};

class SapTrigger : public HasCcTargetTrigger
{
public:
    SapTrigger(PlayerbotAI* botAI) : HasCcTargetTrigger(botAI, "sap") {}

    bool IsActive() override;
};

class SprintTrigger : public BuffTrigger
{
public:
    SprintTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "sprint", 3) {}

    bool IsPossible();
    bool IsActive() override;
};

// AC RogueTriggers.h:116-128. Poison in MoP is still applied to the weapon as a temporary enchant, so these
// check the same TEMP_ENCHANTMENT_SLOT item API AC uses (the same correction ShamanTriggers.h:16-31 made for
// weapon imbues). The ActionNodes that consume them stay commented out in
// GenericRogueNonCombatStrategy.cpp, so this is pure registration parity with no runtime effect.
class MainHandWeaponNoEnchantTrigger : public BuffTrigger
{
public:
    MainHandWeaponNoEnchantTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "main hand", 1) {}

    bool IsActive() override;
};

class OffHandWeaponNoEnchantTrigger : public BuffTrigger
{
public:
    OffHandWeaponNoEnchantTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "off hand", 1) {}

    bool IsActive() override;
};

class TricksOfTheTradeOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    TricksOfTheTradeOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "tricks of the trade", true) {}
};

// ---- Rotation port (epic #854, issue #859): MoP rogue rows. All class-local.

// Revealing Strike (84617), Combat, prio 0/4: "target NOT has aura Revealing Strike (cast by me)".
class RevealingStrikeTrigger : public DebuffTrigger
{
public:
    RevealingStrikeTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "revealing strike", 1, true, 0.0f) {}

    bool IsActive() override;  // spell must be known (silent no-op otherwise)
};

// Hemorrhage (16511), Subtlety, prio 0/2: "target NOT has aura Hemorrhage (89775) (cast by me)".
class HemorrhageTrigger : public DebuffTrigger
{
public:
    HemorrhageTrigger(PlayerbotAI* botAI) : DebuffTrigger(botAI, "hemorrhage", 1, true, 0.0f) {}

    bool IsActive() override;
};

// Shadowstep (36554), prio 0/10: not stealthed and target 8-25 yd away.
class ShadowstepTrigger : public Trigger
{
public:
    ShadowstepTrigger(PlayerbotAI* botAI) : Trigger(botAI, "shadowstep") {}

    bool IsActive() override;
};

// Marked for Death (137619), prio 0/20: target has < 2 combo points and we are not stealthed.
class MarkedForDeathTrigger : public Trigger
{
public:
    MarkedForDeathTrigger(PlayerbotAI* botAI) : Trigger(botAI, "marked for death") {}

    bool IsActive() override;
};

// Vendetta (79140), Assassination, prio 0/10: not stealthed, target lacks our Vendetta.
class VendettaTrigger : public Trigger
{
public:
    VendettaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "vendetta") {}

    bool IsActive() override;
};

// Recuperate (73651), Assassination, prio 5/1: health <= 30%, 5 combo points, no Recuperate on me.
class RecuperateTrigger : public Trigger
{
public:
    RecuperateTrigger(PlayerbotAI* botAI) : Trigger(botAI, "recuperate") {}

    bool IsActive() override;
};

// Preparation resets Vanish; save Evasion as well when health is low (rogue sheets).
class PreparationTrigger : public Trigger
{
public:
    PreparationTrigger(PlayerbotAI* botAI) : Trigger(botAI, "preparation") {}

    bool IsActive() override;
};

// Shadow Blades (121471), Assassination, prio 5/1: "always" in combat -> buff when we lack it.
// Subtlety sheet gap item 10: Shadow Dance at 75 energy outside Find Weakness (DpsRotationMath::Rogue).
class SubtletyShadowDanceTrigger : public Trigger
{
public:
    SubtletyShadowDanceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "subtlety shadow dance", 1) {}

    bool IsActive() override;
};

// Whether the bot's own Find Weakness (91021) is on its current target.
bool RogueFindWeaknessUp(PlayerbotAI* botAI);

class ShadowBladesTrigger : public BuffTrigger
{
public:
    ShadowBladesTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "shadow blades", 1) {}

    bool IsActive() override;
};

// Exact energy costs keep builders moving without a fixed 60 energy gate (rogue sheets).
class RogueEnergyTrigger : public Trigger
{
public:
    RogueEnergyTrigger(PlayerbotAI* ai, uint32 cost) : Trigger(ai, "rogue energy"), cost(cost) {}
    bool IsActive() override;
private:
    uint32 cost;
};
class CancelBladeFlurryTrigger : public AoeTrigger
{
public:
    CancelBladeFlurryTrigger(PlayerbotAI* ai) : AoeTrigger(ai, 2, 8.0f) {}
    bool IsActive() override;
};
class TricksPrePullTrigger : public Trigger
{
public:
    TricksPrePullTrigger(PlayerbotAI* ai) : Trigger(ai, "tricks before pull") {}
    bool IsActive() override;
};
class PremeditationTrigger : public Trigger
{
public:
    PremeditationTrigger(PlayerbotAI* ai) : Trigger(ai, "premeditation") {}
    bool IsActive() override;
};

// BT-691 to BT-693: the Subtlety package of the PvP class rules (Utils/PvpSubtletyPolicy.h, PvP/PvpSubtlety.h). One trigger class and one
// action class carry the rule they were registered with; the game glue answers against enemy players only.
class PvpSubtletyTrigger : public Trigger
{
public:
    PvpSubtletyTrigger(PlayerbotAI* botAI, char const* name, PvpSubtletyGlue::Rule rule) : Trigger(botAI, name, 1), rule(rule) {}
    bool IsActive() override;

private:
    PvpSubtletyGlue::Rule rule;
};

#endif
