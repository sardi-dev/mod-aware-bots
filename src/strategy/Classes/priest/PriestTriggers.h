/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#ifndef _PLAYERBOT_PRIESTTRIGGERS_H
#define _PLAYERBOT_PRIESTTRIGGERS_H

#include "CureTriggers.h"
#include "HealthTriggers.h"
#include "PvpShadowPriest.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include <set>

class PlayerbotAI;

// 1:1 port of AC Ai/Class/Priest/PriestTriggers.h (WotLK mod-playerbots). Only the divergences this
// core forces were applied: the base trigger classes and every macro used below already exist with AC
// signatures in strategy/AiObject.h + strategy/triggers/{GenericTriggers,CureTriggers,HealthTriggers}.h, and
// spell-name strings are the same lower-case full spell names this fork already uses in PriestActions.h
// ("power word: fortitude", ...), so HasAura()/GetSpellId() resolve.
namespace ShadowPriestSheet
{
// Shadow sheet line 15 (expert): the bot's own DoT on this target is in its refresh window (DpsRotationMath::Priest).
bool DotRefreshDue(PlayerbotAI* botAI, Unit* target, uint32 spellId, float needLifeTime);
}

DEBUFF_CHECKISOWNER_TRIGGER(HolyFireTrigger, "holy fire");
class ShadowWordPainTrigger : public DebuffTrigger
{
public:
    ShadowWordPainTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "shadow word: pain", 1, true) {}
    bool IsActive() override;
};
DEBUFF_ENEMY_TRIGGER_A(ShadowWordPainOnAttackerTrigger, "shadow word: pain");
class VampiricTouchTrigger : public DebuffTrigger
{
public:
    VampiricTouchTrigger(PlayerbotAI* ai) : DebuffTrigger(ai, "vampiric touch", 1, true) {}
    bool IsActive() override;
};
DEBUFF_ENEMY_TRIGGER_A(VampiricTouchOnAttackerTrigger, "vampiric touch");
class VampiricEmbraceTrigger : public Trigger
{
public:
    VampiricEmbraceTrigger(PlayerbotAI* ai) : Trigger(ai, "vampiric embrace") {}
    bool IsActive() override;
};
class ShadowPriestConditionTrigger : public Trigger
{
public:
    ShadowPriestConditionTrigger(PlayerbotAI* ai, std::string const& name) : Trigger(ai, name), condition(name) {}
    bool IsActive() override;
private:
    std::string condition;
};
// Class sheet, holy priest (BT-126, docs/class-sheets/priest-holy.md): the four dispel edges sit above every heal chain,
// so they stay silent while a group member is below the low health threshold (IsActive bodies in PriestTriggers.cpp).
// The party Magic and the two disease triggers only track whether the bot or a party member has something to dispel; the
// spell cast is Purify (see GenericPriestStrategy.cpp), so the "cure disease" name is a label and not a spell lookup.
class DispelMagicTrigger : public NeedCureTrigger
{
public:
    DispelMagicTrigger(PlayerbotAI* botAI) : NeedCureTrigger(botAI, "dispel magic", DISPEL_MAGIC) {}

    bool IsActive() override;
};

class DispelMagicPartyMemberTrigger : public PartyMemberNeedCureTrigger
{
public:
    DispelMagicPartyMemberTrigger(PlayerbotAI* botAI) : PartyMemberNeedCureTrigger(botAI, "dispel magic", DISPEL_MAGIC)
    {
    }

    bool IsActive() override;
};

class CureDiseaseTrigger : public NeedCureTrigger
{
public:
    CureDiseaseTrigger(PlayerbotAI* botAI) : NeedCureTrigger(botAI, "cure disease", DISPEL_DISEASE) {}

    bool IsActive() override;
};

class PartyMemberCureDiseaseTrigger : public PartyMemberNeedCureTrigger
{
public:
    PartyMemberCureDiseaseTrigger(PlayerbotAI* botAI)
        : PartyMemberNeedCureTrigger(botAI, "cure disease", DISPEL_DISEASE)
    {
    }

    bool IsActive() override;
};
BUFF_TRIGGER_A(InnerFireTrigger, "inner fire");
BUFF_TRIGGER_A(ShadowformTrigger, "shadowform");
// #812 MoP-only (no AC counterpart; WotLK has no Chakra): Holy's stance, kept up like Shadow keeps Shadowform
// (AC ShadowPriestStrategy.cpp:30-36). It takes the slot of AC's dead "inner focus" mana edge for Holy
// (AC GenericPriestStrategy.cpp:31-34; Inner Focus 89485 is Discipline-only in 5.4.8, SpecializationSpells spec256).
BUFF_TRIGGER_A(ChakraSerenityTrigger, "chakra: serenity");
// Task 8 / #863 (MoP rotation row 25715/25723/25724, holy): Chakra: Sanctuary while >= 3 group members within
// 30 yd are below 80% HP, Chakra: Serenity otherwise (ChakraSerenityTrigger above stands down for it).
class ChakraSanctuaryTrigger : public Trigger
{
public:
    ChakraSanctuaryTrigger(PlayerbotAI* botAI) : Trigger(botAI, "chakra: sanctuary") {}

    bool IsActive() override;
};

// Task E (#863, MoP rotation row 25721, holy, prio 5/1): Lightwell 126135 whenever the bot is in combat, the
// spell is known and off cooldown.
class LightwellCanCastTrigger : public Trigger
{
public:
    LightwellCanCastTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lightwell can cast") {}

    bool IsActive() override;
};

// Task E (#863, MoP rotation row 25715, holy, prio 2/0, combat + out of combat): Chakra: Sanctuary whenever the
// bot does not carry it, which is how MoP re-enters a stance out of combat (non-combat strategy only).
class ChakraSanctuaryMissingTrigger : public Trigger
{
public:
    ChakraSanctuaryMissingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "chakra: sanctuary missing") {}

    bool IsActive() override;
};

// Task E (#863, MoP rotation row 25802, shadow, prio 0/10): Void Tendrils 108920 on a current target within
// 7 yd. MoP 5.4.8 has no other condition; here it is a player (pvp), or a creature that is attacking the bot.
class VoidTendrilsTargetCloseTrigger : public Trigger
{
public:
    VoidTendrilsTargetCloseTrigger(PlayerbotAI* botAI) : Trigger(botAI, "void tendrils target close") {}

    bool IsActive() override;
};

// Task 8 / #863 (MoP rotation row 25603/25703, disc + holy, group 1/10-1/12): the Surge of Light (114255)
// proc makes Flash Heal free; fires while the bot carries the proc.
class SurgeOfLightProcTrigger : public Trigger
{
public:
    SurgeOfLightProcTrigger(PlayerbotAI* botAI) : Trigger(botAI, "surge of light proc") {}

    bool IsActive() override;
};

// Task 8 / #863: MoP self-defensive health thresholds (shadow rows 25817 PW:S 60%, 25816 Vampiric Embrace 40%,
// 25819 Renew 30%): the bot is in combat and its own health is at or below `pct`. Registered as "priest self health <n>".
class PriestSelfHealthTrigger : public Trigger
{
public:
    PriestSelfHealthTrigger(PlayerbotAI* botAI, std::string const& name, float pct) : Trigger(botAI, name), pct(pct) {}

    bool IsActive() override;

protected:
    float pct;
};

BOOST_TRIGGER(PowerInfusionTrigger, "power infusion");
BUFF_TRIGGER(InnerFocusTrigger, "inner focus");
CC_TRIGGER(ShackleUndeadTrigger, "shackle undead");
INTERRUPT_TRIGGER(SilenceTrigger, "silence");
INTERRUPT_HEALER_TRIGGER(SilenceEnemyHealerTrigger, "silence");
DEBUFF_CHECKISOWNER_TRIGGER(DevouringPlagueTrigger, "devouring plague");

// #735 (owner 2026-10-08): Shadow Orbs (power type 13) gate Devouring Plague the way AC's combo points gate its
// finishers - AC ComboPointsAvailableTrigger (AC Ai/Base/Trigger/GenericTriggers.h:105-114, .cpp:94: StatAvailable
// + an "amount"), registered per amount like "combo points 3 available" (AC Ai/Base/TriggerContext.h:122,369).
// Only Shadow generates orbs, so this is never active for a holy/disc priest with the "shadow debuff" strategy.
class ShadowOrbsAvailableTrigger : public StatAvailable
{
public:
    ShadowOrbsAvailableTrigger(PlayerbotAI* botAI, int32 amount)
        : StatAvailable(botAI, amount, "shadow orbs available")
    {
    }

    bool IsActive() override;
};
BOOST_TRIGGER_A(ShadowfiendTrigger, "shadowfiend");

class PowerWordFortitudeOnPartyTrigger : public BuffOnPartyTrigger
{
public:
    PowerWordFortitudeOnPartyTrigger(PlayerbotAI* botAI)
        : BuffOnPartyTrigger(botAI, "power word: fortitude", 4 * 2000) {}
};

class PowerWordFortitudeTrigger : public BuffTrigger
{
public:
    PowerWordFortitudeTrigger(PlayerbotAI* botAI) : BuffTrigger(botAI, "power word: fortitude", 4 * 2000) {}

    bool IsActive() override;
};

// #524/#521: "being attacked" is the shared BeingAttackedTrigger in strategy/triggers/TriggerContext.h
// (AC TriggerContext.h:127).

class FearWardOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    FearWardOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "fear ward", false, 2000) {}

    bool IsActive() override;
};

// Class sheet, holy priest (BT-126): Renew (level 26) on the main tank, kept up in combat while mana is not being saved.
// Same shape as RejuvenationOnMainTankTrigger of the restoration druid.
class RenewOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    RenewOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "renew", true) {}

    bool IsActive() override;
};

// Class sheet, holy priest (BT-126): Prayer of Mending (level 68, "good" tier) on the main tank whenever it is off
// cooldown and the tank does not carry the bot's charge (the charge jumps away when it heals).
class PrayerOfMendingOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    PrayerOfMendingOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "prayer of mending", true) {}

    bool IsActive() override;
};

// Class sheet, discipline priest (BT-128, docs/class-sheets/priest-discipline.md): Power Word: Shield on the main tank in
// combat when he has neither the bot's shield nor Weakened Soul (the server refuses a shield on a carrier of it), while
// mana is not being saved. A shield per member per 15 seconds is the limit, so the tank is the one who gets it first.
class PowerWordShieldOnMainTankTrigger : public BuffOnMainTankTrigger
{
public:
    PowerWordShieldOnMainTankTrigger(PlayerbotAI* botAI) : BuffOnMainTankTrigger(botAI, "power word: shield", true) {}

    bool IsActive() override;
};

// Class sheet, holy priest (BT-126): the shared AoeInGroupTrigger needs five group members near, so a party of two to
// four never got Circle of Healing, Prayer of Healing or Halo. This one fires on three hurt members in a party of up to
// four as well; with five or more near it keeps the shared rule (Divine Hymn stays on the shared trigger).
class HolyGroupHealTrigger : public AoeInGroupTrigger
{
public:
    HolyGroupHealTrigger(PlayerbotAI* botAI, std::string const name, std::string const type)
        : AoeInGroupTrigger(botAI, name, type)
    {
    }

    bool IsActive() override;
};

class BindingHealTrigger : public PartyMemberLowHealthTrigger
{
public:
    BindingHealTrigger(PlayerbotAI* botAI);

    bool IsActive() override;
};

// Shadow sheet line 14 (expert): stop a Mind Flay after its second tick when Mind Blast is ready.
class ShadowMindFlayClipTrigger : public Trigger
{
public:
    ShadowMindFlayClipTrigger(PlayerbotAI* botAI) : Trigger(botAI, "shadow mind flay clip", 1) {}

    bool IsActive() override;
};

class MindSearChannelCheckTrigger : public Trigger
{
public:
    MindSearChannelCheckTrigger(PlayerbotAI* botAI, uint32 minEnemies = 2)
        : Trigger(botAI, "mind sear channel check"), minEnemies(minEnemies) {}

    bool IsActive() override;

protected:
    uint32 minEnemies;
    static const std::set<uint32> MIND_SEAR_SPELL_IDS;
};

// Deliberately NOT defined/registered here:
//  * "boost" - GenericPriestStrategy.cpp:89 has TriggerNode("boost", ...), but AC registers no trigger under
//    that name either (AC TriggerContext.h only has "generic boost"; AC PriestAiObjectContext.cpp:31
//    registers "boost" as a *Strategy*, not a Trigger). AC has the same dangling node, so inventing a
//    BoostTrigger here would not be AC parity.
//  * "fear ward" - the fork only has it commented out (PriestNonCombatStrategy.cpp:83) and AC registers
//    "fear ward on main tank" (FearWardOnMainTankTrigger, ported above), never "fear ward".

// BT-688 to BT-690: the Shadow package of the PvP class rules (Utils/PvpShadowPriestPolicy.h, PvP/PvpShadowPriest.h). One trigger class
// and one action class carry the rule they were registered with; the game glue answers for a fight against players only.
class PvpShadowPriestTrigger : public Trigger
{
public:
    PvpShadowPriestTrigger(PlayerbotAI* botAI, char const* name, PvpShadowPriestGlue::Rule rule) : Trigger(botAI, name, 1), rule(rule) {}
    bool IsActive() override;

private:
    PvpShadowPriestGlue::Rule rule;
};

#endif
