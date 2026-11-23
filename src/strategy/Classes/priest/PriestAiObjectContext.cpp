/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PriestAiObjectContext.h"

#include "GenericPriestStrategy.h"
#include "HolyPriestStrategy.h"
#include "NamedObjectContext.h"
#include "HealerActions.h"
#include "HealerTriggers.h"
#include "Playerbots.h"
#include "PullStrategy.h"
#include "PriestActions.h"
#include "PriestNonCombatStrategy.h"
#include "PriestTriggers.h"
#include "ShadowPriestStrategy.h"

class PriestStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    PriestStrategyFactoryInternal()
    {
        creators["nc"] = &PriestStrategyFactoryInternal::nc;
        creators["pull"] = &PriestStrategyFactoryInternal::pull;
        creators["aoe"] = &PriestStrategyFactoryInternal::shadow_aoe;
        creators["shadow aoe"] = &PriestStrategyFactoryInternal::shadow_aoe;
        creators["dps debuff"] = &PriestStrategyFactoryInternal::shadow_debuff;
        creators["shadow debuff"] = &PriestStrategyFactoryInternal::shadow_debuff;
        creators["cure"] = &PriestStrategyFactoryInternal::cure;
        creators["buff"] = &PriestStrategyFactoryInternal::buff;
        creators["boost"] = &PriestStrategyFactoryInternal::boost;
        creators["rshadow"] = &PriestStrategyFactoryInternal::rshadow;
        creators["cc"] = &PriestStrategyFactoryInternal::cc;
        creators["healer dps"] = &PriestStrategyFactoryInternal::healer_dps;
    }

private:
    static Strategy* cc(PlayerbotAI* botAI) { return new PriestCcStrategy(botAI); }
    static Strategy* rshadow(PlayerbotAI* botAI) { return new PriestShadowResistanceStrategy(botAI); }
    static Strategy* boost(PlayerbotAI* botAI) { return new PriestBoostStrategy(botAI); }
    static Strategy* buff(PlayerbotAI* botAI) { return new PriestBuffStrategy(botAI); }
    static Strategy* nc(PlayerbotAI* botAI) { return new PriestNonCombatStrategy(botAI); }
    static Strategy* shadow_aoe(PlayerbotAI* botAI) { return new ShadowPriestAoeStrategy(botAI); }
    static Strategy* pull(PlayerbotAI* botAI) { return new PullStrategy(botAI, "smite"); }
    static Strategy* shadow_debuff(PlayerbotAI* botAI) { return new ShadowPriestDebuffStrategy(botAI); }
    static Strategy* cure(PlayerbotAI* botAI) { return new PriestCureStrategy(botAI); }
    static Strategy* healer_dps(PlayerbotAI* botAI) { return new PriestHealerDpsStrategy(botAI); }
};

class PriestCombatStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    PriestCombatStrategyFactoryInternal() : NamedObjectContext<Strategy>(false, true)
    {
        creators["heal"] = &PriestCombatStrategyFactoryInternal::heal;
        creators["shadow"] = &PriestCombatStrategyFactoryInternal::dps;
        creators["dps"] = &PriestCombatStrategyFactoryInternal::dps;
        creators["holy dps"] = &PriestCombatStrategyFactoryInternal::holy_dps;
        creators["holy heal"] = &PriestCombatStrategyFactoryInternal::holy_heal;
    }

private:
    static Strategy* heal(PlayerbotAI* botAI) { return new HealPriestStrategy(botAI); }
    static Strategy* dps(PlayerbotAI* botAI) { return new ShadowPriestStrategy(botAI); }
    static Strategy* holy_dps(PlayerbotAI* botAI) { return new HolyPriestStrategy(botAI); }
    static Strategy* holy_heal(PlayerbotAI* botAI) { return new HolyHealPriestStrategy(botAI); }
};

class PriestTriggerFactoryInternal : public NamedObjectContext<Trigger>
{
public:
    PriestTriggerFactoryInternal()
    {
        // #357 slice 1: full AC parity surface. Names and factory shapes are copied verbatim from
        // AC Ai/Class/Priest/PriestAiObjectContext.cpp:74-106 / 110-148 (WotLK mod-playerbots).
        creators["devouring plague"] = &PriestTriggerFactoryInternal::devouring_plague;
        // #735: the Devouring Plague gate, AC's "combo points N available" pattern (AC TriggerContext.h:120-122)
        creators["shadow orbs 3 available"] = &PriestTriggerFactoryInternal::shadow_orbs_3_available;
        creators["shadow word: pain"] = &PriestTriggerFactoryInternal::shadow_word_pain;
        creators["shadow word: pain on attacker"] = &PriestTriggerFactoryInternal::shadow_word_pain_on_attacker;
        creators["dispel magic"] = &PriestTriggerFactoryInternal::dispel_magic;
        creators["dispel magic on party"] = &PriestTriggerFactoryInternal::dispel_magic_party_member;
        creators["cure disease"] = &PriestTriggerFactoryInternal::cure_disease;
        creators["party member cure disease"] = &PriestTriggerFactoryInternal::party_member_cure_disease;
        creators["power word: fortitude"] = &PriestTriggerFactoryInternal::power_word_fortitude;
        creators["power word: fortitude on party"] = &PriestTriggerFactoryInternal::power_word_fortitude_on_party;
        creators["inner fire"] = &PriestTriggerFactoryInternal::inner_fire;
        creators["vampiric touch"] = &PriestTriggerFactoryInternal::vampiric_touch;
        creators["vampiric touch on attacker"] = &PriestTriggerFactoryInternal::vampiric_touch_on_attacker;
        creators["shadowform"] = &PriestTriggerFactoryInternal::shadowform;
        creators["vampiric embrace"] = &PriestTriggerFactoryInternal::vampiric_embrace;
        // BT-688 to BT-690 (PvP class rules, Shadow package).
        creators["pvp leap of faith"] = &PriestTriggerFactoryInternal::pvp_leap_of_faith;
        creators["pvp mass dispel"] = &PriestTriggerFactoryInternal::pvp_mass_dispel;
        creators["power infusion"] = &PriestTriggerFactoryInternal::power_infusion;
        creators["inner focus"] = &PriestTriggerFactoryInternal::inner_focus;
        creators["shackle undead"] = &PriestTriggerFactoryInternal::shackle_undead;
        creators["holy fire"] = &PriestTriggerFactoryInternal::holy_fire;
        creators["fear ward on main tank"] = &PriestTriggerFactoryInternal::fear_ward_on_main_tank;
        creators["binding heal"] = &PriestTriggerFactoryInternal::binding_heal;
        creators["silence"] = &PriestTriggerFactoryInternal::silence;
        creators["silence on enemy healer"] = &PriestTriggerFactoryInternal::silence_on_enemy_healer;
        creators["shadowfiend"] = &PriestTriggerFactoryInternal::shadowfiend;
        // Class sheet, holy priest (BT-126).
        creators["renew on main tank"] = &PriestTriggerFactoryInternal::renew_on_main_tank;
        creators["renew proactive"] = &PriestTriggerFactoryInternal::renew_proactive;
        creators["prayer of mending on main tank"] = &PriestTriggerFactoryInternal::prayer_of_mending_on_main_tank;
        creators["holy group heal setting"] = &PriestTriggerFactoryInternal::holy_group_heal_setting;
        creators["holy medium group heal setting"] = &PriestTriggerFactoryInternal::holy_medium_group_heal_setting;
        // Class sheet, discipline priest (BT-128): the same small-party rule as the holy priest's, under names of its own.
        creators["power word: shield on main tank"] = &PriestTriggerFactoryInternal::power_word_shield_on_main_tank;
        creators["disc group heal setting"] = &PriestTriggerFactoryInternal::disc_group_heal_setting;
        creators["disc medium group heal setting"] = &PriestTriggerFactoryInternal::disc_medium_group_heal_setting;
        creators["mind sear channel check"] = &PriestTriggerFactoryInternal::mind_sear_channel_check;
        creators["shadow mind flay clip"] = &PriestTriggerFactoryInternal::shadow_mind_flay_clip;
        // #812 MoP-only (no AC counterpart): Holy's Chakra: Serenity stance.
        creators["chakra: serenity"] = &PriestTriggerFactoryInternal::chakra_serenity;
        // Task 8 / #863 (MoP priest rotations)
        creators["chakra: sanctuary"] = &PriestTriggerFactoryInternal::chakra_sanctuary;
        creators["surge of light proc"] = &PriestTriggerFactoryInternal::surge_of_light_proc;
        creators["lightwell can cast"] = &PriestTriggerFactoryInternal::lightwell_can_cast;
        creators["chakra: sanctuary missing"] = &PriestTriggerFactoryInternal::chakra_sanctuary_missing;
        creators["void tendrils target close"] = &PriestTriggerFactoryInternal::void_tendrils_target_close;
        // Shadow priest sheet: reuse cached attackers and the current target's threat list.
        creators["shadow solo flash heal"] = &PriestTriggerFactoryInternal::shadow_solo_flash_heal;
        creators["priest self health 45"] = &PriestTriggerFactoryInternal::self_health_45;
        creators["shadow mind sear"] = &PriestTriggerFactoryInternal::shadow_mind_sear;
        creators["shadowfiend hard fight"] = &PriestTriggerFactoryInternal::shadowfiend_hard_fight;
        creators["priest high threat"] = &PriestTriggerFactoryInternal::priest_high_threat;
        creators["priest self health 30"] = &PriestTriggerFactoryInternal::self_health_30;
        creators["priest self health 40"] = &PriestTriggerFactoryInternal::self_health_40;
        creators["priest self health 60"] = &PriestTriggerFactoryInternal::self_health_60;
        // MoP-only, BEYOND AC PARITY (#357 slice 1): AC has neither spell nor trigger name.
        // #524: "being attacked" comes from the shared TriggerContext (#521, AC TriggerContext.h:127).
        // NOT registered: "boost" - AC registers no Trigger under that name either (AC TriggerContext.h has
        // only "generic boost"; AC PriestAiObjectContext.cpp:31 registers "boost" as a *Strategy*), and
        // trigger "fear ward" - AC registers the trigger "fear ward on main tank" only (that one IS registered
        // above, and #524 registers its action "fear ward on main tank" - AC PriestAiObjectContext.cpp).
    }

private:
    static Trigger* shadow_solo_flash_heal(PlayerbotAI* ai) { return new ShadowPriestConditionTrigger(ai, "shadow solo flash heal"); }
    static Trigger* self_health_45(PlayerbotAI* ai) { return new PriestSelfHealthTrigger(ai, "priest self health 45", 45.0f); }
    static Trigger* shadow_mind_sear(PlayerbotAI* ai) { return new ShadowPriestConditionTrigger(ai, "shadow mind sear"); }
    static Trigger* shadowfiend_hard_fight(PlayerbotAI* ai) { return new ShadowPriestConditionTrigger(ai, "shadowfiend hard fight"); }
    static Trigger* priest_high_threat(PlayerbotAI* ai) { return new ShadowPriestConditionTrigger(ai, "priest high threat"); }
    static Trigger* vampiric_embrace(PlayerbotAI* botAI) { return new VampiricEmbraceTrigger(botAI); }
    static Trigger* shadowform(PlayerbotAI* botAI) { return new ShadowformTrigger(botAI); }
    static Trigger* vampiric_touch(PlayerbotAI* botAI) { return new VampiricTouchTrigger(botAI); }
    static Trigger* vampiric_touch_on_attacker(PlayerbotAI* botAI)
    {
        return new VampiricTouchOnAttackerTrigger(botAI);
    }
    static Trigger* devouring_plague(PlayerbotAI* botAI) { return new DevouringPlagueTrigger(botAI); }
    static Trigger* shadow_orbs_3_available(PlayerbotAI* botAI) { return new ShadowOrbsAvailableTrigger(botAI, 3); }
    static Trigger* shadow_word_pain(PlayerbotAI* botAI) { return new ShadowWordPainTrigger(botAI); }
    static Trigger* shadow_word_pain_on_attacker(PlayerbotAI* botAI)
    {
        return new ShadowWordPainOnAttackerTrigger(botAI);
    }
    static Trigger* dispel_magic(PlayerbotAI* botAI) { return new DispelMagicTrigger(botAI); }
    static Trigger* dispel_magic_party_member(PlayerbotAI* botAI) { return new DispelMagicPartyMemberTrigger(botAI); }
    static Trigger* cure_disease(PlayerbotAI* botAI) { return new CureDiseaseTrigger(botAI); }
    static Trigger* party_member_cure_disease(PlayerbotAI* botAI) { return new PartyMemberCureDiseaseTrigger(botAI); }
    static Trigger* power_word_fortitude(PlayerbotAI* botAI) { return new PowerWordFortitudeTrigger(botAI); }
    static Trigger* power_word_fortitude_on_party(PlayerbotAI* botAI)
    {
        return new PowerWordFortitudeOnPartyTrigger(botAI);
    }
    static Trigger* inner_fire(PlayerbotAI* botAI) { return new InnerFireTrigger(botAI); }
    static Trigger* power_infusion(PlayerbotAI* botAI) { return new PowerInfusionTrigger(botAI); }
    static Trigger* inner_focus(PlayerbotAI* botAI) { return new InnerFocusTrigger(botAI); }
    static Trigger* shackle_undead(PlayerbotAI* botAI) { return new ShackleUndeadTrigger(botAI); }
    static Trigger* fear_ward_on_main_tank(PlayerbotAI* botAI) { return new FearWardOnMainTankTrigger(botAI); }
    static Trigger* holy_fire(PlayerbotAI* botAI) { return new HolyFireTrigger(botAI); }
    static Trigger* shadowfiend(PlayerbotAI* botAI) { return new ShadowfiendTrigger(botAI); }
    static Trigger* silence_on_enemy_healer(PlayerbotAI* botAI) { return new SilenceEnemyHealerTrigger(botAI); }
    static Trigger* silence(PlayerbotAI* botAI) { return new SilenceTrigger(botAI); }
    static Trigger* pvp_leap_of_faith(PlayerbotAI* botAI)
    {
        return new PvpShadowPriestTrigger(botAI, "pvp leap of faith", PvpShadowPriestGlue::Rule::LEAP);
    }
    static Trigger* pvp_mass_dispel(PlayerbotAI* botAI)
    {
        return new PvpShadowPriestTrigger(botAI, "pvp mass dispel", PvpShadowPriestGlue::Rule::DISPEL);
    }
    static Trigger* binding_heal(PlayerbotAI* botAI) { return new BindingHealTrigger(botAI); }
    static Trigger* renew_on_main_tank(PlayerbotAI* botAI) { return new RenewOnMainTankTrigger(botAI); }
    static Trigger* renew_proactive(PlayerbotAI* botAI) { return new HealerProactiveHotTrigger(botAI, "renew"); }
    static Trigger* prayer_of_mending_on_main_tank(PlayerbotAI* botAI)
    {
        return new PrayerOfMendingOnMainTankTrigger(botAI);
    }
    static Trigger* holy_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "holy group heal setting", "almost full");
    }
    static Trigger* holy_medium_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "holy medium group heal setting", "medium");
    }
    static Trigger* power_word_shield_on_main_tank(PlayerbotAI* botAI)
    {
        return new PowerWordShieldOnMainTankTrigger(botAI);
    }
    static Trigger* disc_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "disc group heal setting", "almost full");
    }
    static Trigger* disc_medium_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "disc medium group heal setting", "medium");
    }
    static Trigger* mind_sear_channel_check(PlayerbotAI* botAI) { return new MindSearChannelCheckTrigger(botAI); }
    static Trigger* shadow_mind_flay_clip(PlayerbotAI* botAI) { return new ShadowMindFlayClipTrigger(botAI); }
    static Trigger* chakra_serenity(PlayerbotAI* botAI) { return new ChakraSerenityTrigger(botAI); }
    static Trigger* chakra_sanctuary(PlayerbotAI* botAI) { return new ChakraSanctuaryTrigger(botAI); }
    static Trigger* surge_of_light_proc(PlayerbotAI* botAI) { return new SurgeOfLightProcTrigger(botAI); }
    static Trigger* lightwell_can_cast(PlayerbotAI* botAI) { return new LightwellCanCastTrigger(botAI); }
    static Trigger* chakra_sanctuary_missing(PlayerbotAI* botAI) { return new ChakraSanctuaryMissingTrigger(botAI); }
    static Trigger* void_tendrils_target_close(PlayerbotAI* botAI) { return new VoidTendrilsTargetCloseTrigger(botAI); }
    static Trigger* self_health_30(PlayerbotAI* botAI)
    {
        return new PriestSelfHealthTrigger(botAI, "priest self health 30", 30.0f);
    }
    static Trigger* self_health_40(PlayerbotAI* botAI)
    {
        return new PriestSelfHealthTrigger(botAI, "priest self health 40", 40.0f);
    }
    static Trigger* self_health_60(PlayerbotAI* botAI)
    {
        return new PriestSelfHealthTrigger(botAI, "priest self health 60", 60.0f);
    }
};

class PriestAiObjectContextInternal : public NamedObjectContext<Action>
{
public:
    PriestAiObjectContextInternal()
    {
        creators["power infusion"] = &PriestAiObjectContextInternal::power_infusion;
        creators["inner focus"] = &PriestAiObjectContextInternal::inner_focus;
        creators["shadow word: pain"] = &PriestAiObjectContextInternal::shadow_word_pain;
        creators["shadow word: pain on attacker"] = &PriestAiObjectContextInternal::shadow_word_pain_on_attacker;
        creators["devouring plague"] = &PriestAiObjectContextInternal::devouring_plague;
        creators["mind flay"] = &PriestAiObjectContextInternal::mind_flay;
        creators["holy fire"] = &PriestAiObjectContextInternal::holy_fire;
        creators["smite"] = &PriestAiObjectContextInternal::smite;
        creators["mind blast"] = &PriestAiObjectContextInternal::mind_blast;
        creators["shadowform"] = &PriestAiObjectContextInternal::shadowform;
        creators["remove shadowform"] = &PriestAiObjectContextInternal::remove_shadowform;
        creators["holy nova"] = &PriestAiObjectContextInternal::holy_nova;
        creators["power word: fortitude"] = &PriestAiObjectContextInternal::power_word_fortitude;
        creators["power word: fortitude on party"] = &PriestAiObjectContextInternal::power_word_fortitude_on_party;
        creators["power word: shield"] = &PriestAiObjectContextInternal::power_word_shield;
        creators["power word: shield on party"] = &PriestAiObjectContextInternal::power_word_shield_on_party;
        creators["power word: shield on almost full health below"] =
            &PriestAiObjectContextInternal::power_word_shield_on_almost_full_health_below;
        creators["power word: shield on not full"] =
            &PriestAiObjectContextInternal::power_word_shield_on_not_full;
        creators["renew"] = &PriestAiObjectContextInternal::renew;
        creators["renew on party"] = &PriestAiObjectContextInternal::renew_on_party;
        creators["greater heal"] = &PriestAiObjectContextInternal::greater_heal;
        creators["greater heal on party"] = &PriestAiObjectContextInternal::greater_heal_on_party;
        creators["heal"] = &PriestAiObjectContextInternal::heal;
        creators["heal on party"] = &PriestAiObjectContextInternal::heal_on_party;
        creators["flash heal"] = &PriestAiObjectContextInternal::flash_heal;
        creators["flash heal on party"] = &PriestAiObjectContextInternal::flash_heal_on_party;
        creators["dispel magic"] = &PriestAiObjectContextInternal::dispel_magic;
        creators["dispel magic on party"] = &PriestAiObjectContextInternal::dispel_magic_on_party;
        creators["dispel magic on target"] = &PriestAiObjectContextInternal::dispel_magic_on_target;
        creators["fade"] = &PriestAiObjectContextInternal::fade;
        creators["inner fire"] = &PriestAiObjectContextInternal::inner_fire;
        creators["resurrection"] = &PriestAiObjectContextInternal::resurrection;
        creators["circle of healing on party"] = &PriestAiObjectContextInternal::circle_of_healing;
        creators["psychic scream"] = &PriestAiObjectContextInternal::psychic_scream;
        creators["vampiric touch"] = &PriestAiObjectContextInternal::vampiric_touch;
        creators["vampiric touch on attacker"] = &PriestAiObjectContextInternal::vampiric_touch_on_attacker;
        creators["vampiric embrace"] = &PriestAiObjectContextInternal::vampiric_embrace;
        creators["pvp leap of faith"] = &PriestAiObjectContextInternal::pvp_leap_of_faith;
        creators["pvp mass dispel"] = &PriestAiObjectContextInternal::pvp_mass_dispel;
        creators["dispersion"] = &PriestAiObjectContextInternal::dispersion;
        creators["shackle undead"] = &PriestAiObjectContextInternal::shackle_undead;
        creators["power infusion on party"] = &PriestAiObjectContextInternal::power_infusion_on_party;
        creators["silence"] = &PriestAiObjectContextInternal::silence;
        creators["silence on enemy healer"] = &PriestAiObjectContextInternal::silence_on_enemy_healer;
        creators["levitate"] = &PriestAiObjectContextInternal::levitate;
        creators["prayer of healing on party"] = &PriestAiObjectContextInternal::prayer_of_healing;
        creators["lightwell"] = &PriestAiObjectContextInternal::lightwell;
        creators["desperate prayer"] = &PriestAiObjectContextInternal::desperate_prayer;
        creators["fear ward"] = &PriestAiObjectContextInternal::fear_ward;
        creators["fear ward on party"] = &PriestAiObjectContextInternal::fear_ward_on_party;
        creators["fear ward on main tank"] = &PriestAiObjectContextInternal::fear_ward_on_main_tank;
        creators["purify"] = &PriestAiObjectContextInternal::purify;
        creators["purify on party"] = &PriestAiObjectContextInternal::purify_on_party;
        creators["purify magic on party"] = &PriestAiObjectContextInternal::purify_magic_on_party;  // #524
        creators["shadow word: death"] = &PriestAiObjectContextInternal::shadow_word_death;
        creators["shadowfiend"] = &PriestAiObjectContextInternal::shadowfiend;
        creators["mass dispel"] = &PriestAiObjectContextInternal::mass_dispel;
        creators["pain suppression"] = &PriestAiObjectContextInternal::pain_suppression;
        creators["pain suppression on party"] = &PriestAiObjectContextInternal::pain_suppression_on_party;
        creators["prayer of mending on party"] = &PriestAiObjectContextInternal::prayer_of_mending;
        creators["binding heal"] = &PriestAiObjectContextInternal::binding_heal;
        creators["penance on party"] = &PriestAiObjectContextInternal::penance_on_party;
        creators["hymn of hope"] = &PriestAiObjectContextInternal::hymn_of_hope;
        creators["divine hymn"] = &PriestAiObjectContextInternal::divine_hymn;
        creators["mind sear"] = &PriestAiObjectContextInternal::mind_sear;
        creators["guardian spirit on party"] = &PriestAiObjectContextInternal::guardian_spirit_on_party;
        creators["pain suppression on main tank"] = &PriestAiObjectContextInternal::pain_suppression_on_main_tank;  // BT-293
        creators["guardian spirit on main tank"] = &PriestAiObjectContextInternal::guardian_spirit_on_main_tank;  // BT-293
        // #812 MoP-only (no AC counterpart): Holy's Chakra: Serenity stance and Holy Word: Serenity.
        creators["chakra: serenity"] = &PriestAiObjectContextInternal::chakra_serenity;
        // Task 8 / #863 (MoP priest rotations)
        creators["chakra: sanctuary"] = &PriestAiObjectContextInternal::chakra_sanctuary;
        creators["holy word: sanctuary on party"] = &PriestAiObjectContextInternal::holy_word_sanctuary_on_party;
        creators["halo"] = &PriestAiObjectContextInternal::halo;
        creators["cascade"] = &PriestAiObjectContextInternal::cascade;
        creators["void tendrils"] = &PriestAiObjectContextInternal::void_tendrils;
        creators["penance"] = &PriestAiObjectContextInternal::penance;
        creators["silence on enemy caster"] = &PriestAiObjectContextInternal::silence_on_enemy_caster;
        creators["holy word: serenity on party"] = &PriestAiObjectContextInternal::holy_word_serenity_on_party;
        // Class sheet, holy priest (BT-126).
        creators["renew on main tank"] = &PriestAiObjectContextInternal::renew_on_main_tank;
        creators["renew proactive"] = &PriestAiObjectContextInternal::renew_proactive;
        creators["prayer of mending on main tank"] = &PriestAiObjectContextInternal::prayer_of_mending_on_main_tank;
        // Class sheet, discipline priest (BT-128).
        creators["power word: shield on main tank"] = &PriestAiObjectContextInternal::power_word_shield_on_main_tank;
    }

private:
    static Action* power_infusion(PlayerbotAI* botAI) { return new CastPowerInfusionAction(botAI); }
    static Action* inner_focus(PlayerbotAI* botAI) { return new CastInnerFocusAction(botAI); }
    static Action* dispersion(PlayerbotAI* botAI) { return new CastDispersionAction(botAI); }
    static Action* vampiric_embrace(PlayerbotAI* botAI) { return new CastVampiricEmbraceAction(botAI); }
    static Action* vampiric_touch(PlayerbotAI* botAI) { return new CastVampiricTouchAction(botAI); }
    static Action* vampiric_touch_on_attacker(PlayerbotAI* botAI)
    {
        return new CastVampiricTouchOnAttackerAction(botAI);
    }
    static Action* psychic_scream(PlayerbotAI* botAI) { return new CastPsychicScreamAction(botAI); }
    static Action* circle_of_healing(PlayerbotAI* botAI) { return new CastCircleOfHealingAction(botAI); }
    static Action* resurrection(PlayerbotAI* botAI) { return new CastResurrectionAction(botAI); }
    static Action* shadow_word_pain(PlayerbotAI* botAI) { return new CastPowerWordPainAction(botAI); }
    static Action* shadow_word_pain_on_attacker(PlayerbotAI* botAI)
    {
        return new CastPowerWordPainOnAttackerAction(botAI);
    }
    static Action* devouring_plague(PlayerbotAI* botAI) { return new CastDevouringPlagueAction(botAI); }
    static Action* mind_flay(PlayerbotAI* botAI) { return new CastMindFlayAction(botAI); }
    static Action* holy_fire(PlayerbotAI* botAI) { return new CastHolyFireAction(botAI); }
    static Action* smite(PlayerbotAI* botAI) { return new CastSmiteAction(botAI); }
    static Action* mind_blast(PlayerbotAI* botAI) { return new CastMindBlastAction(botAI); }
    static Action* shadowform(PlayerbotAI* botAI) { return new CastShadowformAction(botAI); }
    static Action* remove_shadowform(PlayerbotAI* botAI) { return new CastRemoveShadowformAction(botAI); }
    static Action* holy_nova(PlayerbotAI* botAI) { return new CastHolyNovaAction(botAI); }
    static Action* power_word_fortitude(PlayerbotAI* botAI) { return new CastPowerWordFortitudeAction(botAI); }
    static Action* power_word_fortitude_on_party(PlayerbotAI* botAI)
    {
        return new CastPowerWordFortitudeOnPartyAction(botAI);
    }
    static Action* power_word_shield(PlayerbotAI* botAI) { return new CastPowerWordShieldAction(botAI); }
    static Action* power_word_shield_on_party(PlayerbotAI* botAI)
    {
        return new CastPowerWordShieldOnPartyAction(botAI);
    }
    static Action* power_word_shield_on_almost_full_health_below(PlayerbotAI* ai)
    {
        return new CastPowerWordShieldOnAlmostFullHealthBelowAction(ai);
    }
    static Action* power_word_shield_on_not_full(PlayerbotAI* ai)
    {
        return new CastPowerWordShieldOnNotFullAction(ai);
    }
    static Action* renew(PlayerbotAI* botAI) { return new CastRenewAction(botAI); }
    static Action* renew_on_party(PlayerbotAI* botAI) { return new CastRenewOnPartyAction(botAI); }
    static Action* greater_heal(PlayerbotAI* botAI) { return new CastGreaterHealAction(botAI); }
    static Action* greater_heal_on_party(PlayerbotAI* botAI) { return new CastGreaterHealOnPartyAction(botAI); }
    static Action* heal(PlayerbotAI* botAI) { return new CastHealAction(botAI); }
    static Action* heal_on_party(PlayerbotAI* botAI) { return new CastHealOnPartyAction(botAI); }
    static Action* flash_heal(PlayerbotAI* botAI) { return new CastFlashHealAction(botAI); }
    static Action* flash_heal_on_party(PlayerbotAI* botAI) { return new CastFlashHealOnPartyAction(botAI); }
    static Action* dispel_magic(PlayerbotAI* botAI) { return new CastDispelMagicAction(botAI); }
    static Action* dispel_magic_on_party(PlayerbotAI* botAI) { return new CastDispelMagicOnPartyAction(botAI); }
    static Action* dispel_magic_on_target(PlayerbotAI* botAI) { return new CastDispelMagicOnTargetAction(botAI); }
    static Action* fade(PlayerbotAI* botAI) { return new CastFadeAction(botAI); }
    static Action* inner_fire(PlayerbotAI* botAI) { return new CastInnerFireAction(botAI); }
    static Action* shackle_undead(PlayerbotAI* botAI) { return new CastShackleUndeadAction(botAI); }
    static Action* fear_ward_on_party(PlayerbotAI* botAI) { return new CastFearWardOnPartyAction(botAI); }
    static Action* fear_ward(PlayerbotAI* botAI) { return new CastFearWardAction(botAI); }
    static Action* fear_ward_on_main_tank(PlayerbotAI* botAI) { return new CastFearWardOnMainTankAction(botAI); }
    static Action* purify(PlayerbotAI* botAI) { return new CastPurifyAction(botAI); }
    static Action* purify_on_party(PlayerbotAI* botAI) { return new CastPurifyOnPartyAction(botAI); }
    static Action* purify_magic_on_party(PlayerbotAI* botAI) { return new CastPurifyMagicOnPartyAction(botAI); }
    static Action* desperate_prayer(PlayerbotAI* botAI) { return new CastDesperatePrayerAction(botAI); }
    static Action* lightwell(PlayerbotAI* botAI) { return new CastLightwellAction(botAI); }
    static Action* prayer_of_healing(PlayerbotAI* botAI) { return new CastPrayerOfHealingAction(botAI); }
    static Action* levitate(PlayerbotAI* botAI) { return new CastLevitateAction(botAI); }
    static Action* silence_on_enemy_healer(PlayerbotAI* botAI) { return new CastSilenceOnEnemyHealerAction(botAI); }
    static Action* silence(PlayerbotAI* botAI) { return new CastSilenceAction(botAI); }
    static Action* power_infusion_on_party(PlayerbotAI* botAI) { return new CastPowerInfusionOnPartyAction(botAI); }
    static Action* binding_heal(PlayerbotAI* botAI) { return new CastBindingHealAction(botAI); }
    static Action* prayer_of_mending(PlayerbotAI* botAI) { return new CastPrayerOfMendingAction(botAI); }
    static Action* pain_suppression_on_party(PlayerbotAI* botAI) { return new CastPainSuppressionProtectAction(botAI); }
    static Action* pain_suppression(PlayerbotAI* botAI) { return new CastPainSuppressionAction(botAI); }
    static Action* mass_dispel(PlayerbotAI* botAI) { return new CastMassDispelAction(botAI); }
    static Action* pvp_leap_of_faith(PlayerbotAI* botAI)
    {
        return new PvpShadowPriestAction(botAI, "pvp leap of faith", PvpShadowPriestGlue::Rule::LEAP);
    }
    static Action* pvp_mass_dispel(PlayerbotAI* botAI)
    {
        return new PvpShadowPriestAction(botAI, "pvp mass dispel", PvpShadowPriestGlue::Rule::DISPEL);
    }
    static Action* shadowfiend(PlayerbotAI* botAI) { return new CastShadowfiendAction(botAI); }
    static Action* shadow_word_death(PlayerbotAI* botAI) { return new CastShadowWordDeathAction(botAI); }
    static Action* penance_on_party(PlayerbotAI* ai) { return new CastPenanceOnPartyAction(ai); }
    static Action* hymn_of_hope(PlayerbotAI* ai) { return new CastHymnOfHopeAction(ai); }
    static Action* divine_hymn(PlayerbotAI* ai) { return new CastDivineHymnAction(ai); }
    static Action* mind_sear(PlayerbotAI* ai) { return new CastMindSearAction(ai); }
    static Action* guardian_spirit_on_party(PlayerbotAI* ai) { return new CastGuardianSpiritOnPartyAction(ai); }
    static Action* pain_suppression_on_main_tank(PlayerbotAI* ai) { return new CastPainSuppressionOnMainTankAction(ai); }
    static Action* guardian_spirit_on_main_tank(PlayerbotAI* ai) { return new CastGuardianSpiritOnMainTankAction(ai); }
    static Action* chakra_serenity(PlayerbotAI* ai) { return new CastChakraSerenityAction(ai); }
    static Action* chakra_sanctuary(PlayerbotAI* ai) { return new CastChakraSanctuaryAction(ai); }
    static Action* holy_word_sanctuary_on_party(PlayerbotAI* ai) { return new CastHolyWordSanctuaryOnPartyAction(ai); }
    static Action* halo(PlayerbotAI* ai) { return new CastHaloAction(ai); }
    static Action* cascade(PlayerbotAI* ai) { return new CastCascadeAction(ai); }
    static Action* void_tendrils(PlayerbotAI* ai) { return new CastVoidTendrilsAction(ai); }
    static Action* penance(PlayerbotAI* ai) { return new CastPenanceAction(ai); }
    static Action* silence_on_enemy_caster(PlayerbotAI* ai) { return new CastSilenceOnEnemyCasterAction(ai); }
    static Action* holy_word_serenity_on_party(PlayerbotAI* ai) { return new CastHolyWordSerenityOnPartyAction(ai); }
    static Action* renew_on_main_tank(PlayerbotAI* ai) { return new CastRenewOnMainTankAction(ai); }
    static Action* renew_proactive(PlayerbotAI* ai) { return new CastHealerProactiveHotAction(ai, "renew"); }
    static Action* prayer_of_mending_on_main_tank(PlayerbotAI* ai)
    {
        return new CastPrayerOfMendingOnMainTankAction(ai);
    }
    static Action* power_word_shield_on_main_tank(PlayerbotAI* ai)
    {
        return new CastPowerWordShieldOnMainTankAction(ai);
    }
};

PriestAiObjectContext::PriestAiObjectContext(PlayerbotAI* botAI) : AiObjectContext(botAI)
{
    strategyContexts.Add(new PriestStrategyFactoryInternal());
    strategyContexts.Add(new PriestCombatStrategyFactoryInternal());
    actionContexts.Add(new PriestAiObjectContextInternal());
    triggerContexts.Add(new PriestTriggerFactoryInternal());
}
