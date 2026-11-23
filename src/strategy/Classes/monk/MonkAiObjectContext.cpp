/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Monk AI object context (issue #67). AiFactory::createAiObjectContext hands a CLASS_MONK bot to
 * this context instead of the generic one, so monks get class strategies and can cast abilities.
 */

#include "MonkAiObjectContext.h"

#include "MonkActions.h"
#include "MonkStrategies.h"
#include "MeleeCombatStrategy.h"
#include "NamedObjectContext.h"
#include "HealerActions.h"
#include "HealerTriggers.h"
#include "Playerbots.h"
#include "PriestTriggers.h"

class MonkStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    MonkStrategyFactoryInternal()
    {
        creators["nc"] = &MonkStrategyFactoryInternal::nc;
        creators["monk"] = &MonkStrategyFactoryInternal::monk;
        creators["aoe"] = &MonkStrategyFactoryInternal::aoe;
        // #526: AiFactory.cpp requests "melee aoe" for the windwalker; only "aoe" was registered, so the
        // strategy was silently dropped (AC pattern: ShamanAiObjectContext creators["melee aoe"]).
        creators["melee aoe"] = &MonkStrategyFactoryInternal::melee_aoe;
        creators["cure"] = &MonkStrategyFactoryInternal::cure;
        // #526 (AC pattern: Paladin/Rogue AiObjectContext creators["boost"/"cc"/"pull"]).
        creators["boost"] = &MonkStrategyFactoryInternal::boost;
        creators["cc"] = &MonkStrategyFactoryInternal::cc;
        creators["pull"] = &MonkStrategyFactoryInternal::pull;
        // #593 (AC Ai/Base/StrategyContext.h:105,139): "behind" (SetBehindCombatStrategy) was never registered, so
        // AiFactory's addStrategy("behind") for the windwalker was a silent no-op.
        creators["behind"] = &MonkStrategyFactoryInternal::behind;
        // #526 (AC PaladinAiObjectContext.cpp:32-ish creators["healer dps"]): AiFactory adds it for every healer spec.
        creators["healer dps"] = &MonkStrategyFactoryInternal::healer_dps;
    }

private:
    static Strategy* nc(PlayerbotAI* botAI) { return new GenericMonkNonCombatStrategy(botAI); }
    static Strategy* monk(PlayerbotAI* botAI) { return new MonkStrategy(botAI); }
    static Strategy* aoe(PlayerbotAI* botAI) { return new MonkAoeStrategy(botAI); }
    static Strategy* melee_aoe(PlayerbotAI* botAI) { return new MonkMeleeAoeStrategy(botAI); }
    static Strategy* cure(PlayerbotAI* botAI) { return new MonkCureStrategy(botAI); }
    static Strategy* boost(PlayerbotAI* botAI) { return new MonkBoostStrategy(botAI); }
    static Strategy* cc(PlayerbotAI* botAI) { return new MonkCcStrategy(botAI); }
    static Strategy* pull(PlayerbotAI* botAI) { return new MonkPullStrategy(botAI); }
    static Strategy* behind(PlayerbotAI* botAI) { return new SetBehindCombatStrategy(botAI); }  // #593
    static Strategy* healer_dps(PlayerbotAI* botAI) { return new MonkHealerDpsStrategy(botAI); }
};

/// The three specs. AiFactory adds these by the role names "tank" / "heal" / "melee".
class MonkCombatStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    MonkCombatStrategyFactoryInternal() : NamedObjectContext<Strategy>(false, true)
    {
        creators["tank"] = &MonkCombatStrategyFactoryInternal::tank;
        creators["brewmaster"] = &MonkCombatStrategyFactoryInternal::tank;
        creators["heal"] = &MonkCombatStrategyFactoryInternal::heal;
        creators["mistweaver"] = &MonkCombatStrategyFactoryInternal::heal;
        creators["melee"] = &MonkCombatStrategyFactoryInternal::melee;
        creators["windwalker"] = &MonkCombatStrategyFactoryInternal::melee;
    }

private:
    static Strategy* tank(PlayerbotAI* botAI) { return new BrewmasterMonkStrategy(botAI); }
    static Strategy* heal(PlayerbotAI* botAI) { return new MistweaverMonkStrategy(botAI); }
    static Strategy* melee(PlayerbotAI* botAI) { return new WindwalkerMonkStrategy(botAI); }
};

class MonkTriggerFactoryInternal : public NamedObjectContext<Trigger>
{
public:
    MonkTriggerFactoryInternal()
    {
        // #526 (see MonkActions.h for the AC pattern + 5.4.8 spell source of each).
        creators["spear hand strike"] = &MonkTriggerFactoryInternal::spear_hand_strike;
        creators["spear hand strike on enemy healer"] = &MonkTriggerFactoryInternal::spear_hand_strike_on_enemy_healer;
        creators["provoke on snare target"] = &MonkTriggerFactoryInternal::provoke_on_snare_target;
        creators["paralysis"] = &MonkTriggerFactoryInternal::paralysis;
        creators["legacy of the emperor"] = &MonkTriggerFactoryInternal::legacy_of_the_emperor;
        creators["legacy of the emperor on party"] = &MonkTriggerFactoryInternal::legacy_of_the_emperor_on_party;
        creators["legacy of the white tiger"] = &MonkTriggerFactoryInternal::legacy_of_the_white_tiger;
        creators["legacy of the white tiger on party"] = &MonkTriggerFactoryInternal::legacy_of_the_white_tiger_on_party;
        creators["tigereye brew"] = &MonkTriggerFactoryInternal::tigereye_brew;
        creators["touch of death"] = &MonkTriggerFactoryInternal::touch_of_death;
        // #862 (MoP monk rotations, see MonkActions.h).
        creators["touch of karma"] = &MonkTriggerFactoryInternal::touch_of_karma;
        creators["invoke xuen"] = &MonkTriggerFactoryInternal::invoke_xuen;
        creators["elusive brew"] = &MonkTriggerFactoryInternal::elusive_brew;
        creators["chi brew"] = &MonkTriggerFactoryInternal::chi_brew;
        creators["energizing brew"] = &MonkTriggerFactoryInternal::energizing_brew;
        creators["black ox statue"] = &MonkTriggerFactoryInternal::black_ox_statue;
        creators["jade serpent statue"] = &MonkTriggerFactoryInternal::jade_serpent_statue;
        // Class sheet, brewmaster monk (BT-133).
        creators["brewmaster expel harm"] = &MonkTriggerFactoryInternal::brewmaster_expel_harm;
        // Class sheet, mistweaver monk (BT-135).
        creators["soothing mist channel check"] = &MonkTriggerFactoryInternal::soothing_mist_channel_check;
        creators["renewing mist on main tank"] = &MonkTriggerFactoryInternal::renewing_mist_on_main_tank;
        creators["renewing mist proactive"] = &MonkTriggerFactoryInternal::renewing_mist_proactive;
        creators["enveloping mist on main tank"] = &MonkTriggerFactoryInternal::enveloping_mist_on_main_tank;
        creators["life cocoon on main tank"] = &MonkTriggerFactoryInternal::life_cocoon_on_main_tank;
        creators["mistweaver group heal setting"] = &MonkTriggerFactoryInternal::mistweaver_group_heal_setting;
        creators["mistweaver medium group heal setting"] =
            &MonkTriggerFactoryInternal::mistweaver_medium_group_heal_setting;
        creators["mistweaver low group heal setting"] = &MonkTriggerFactoryInternal::mistweaver_low_group_heal_setting;
        // BT-657 to BT-662 (PvP class rules, Mistweaver package).
        creators["pvp healing sphere"] = &MonkTriggerFactoryInternal::pvp_healing_sphere;
        creators["pvp life cocoon"] = &MonkTriggerFactoryInternal::pvp_life_cocoon;
        creators["pvp thunder focus tea"] = &MonkTriggerFactoryInternal::pvp_thunder_focus_tea;
        creators["pvp instant surging mist"] = &MonkTriggerFactoryInternal::pvp_instant_surging_mist;
        creators["pvp mana tea"] = &MonkTriggerFactoryInternal::pvp_mana_tea;
        creators["pvp tigers lust"] = &MonkTriggerFactoryInternal::pvp_tigers_lust;
        // #526 (AC WarriorAiObjectContext.cpp:68-70 stance triggers).
        creators["stance of the fierce tiger"] = &MonkTriggerFactoryInternal::stance_of_the_fierce_tiger;
        creators["stance of the sturdy ox"] = &MonkTriggerFactoryInternal::stance_of_the_sturdy_ox;
        creators["stance of the wise serpent"] = &MonkTriggerFactoryInternal::stance_of_the_wise_serpent;
        // #388 (AC PaladinAiObjectContext cleanse triggers, MoP PaladinAiObjectContext.cpp:114-122).
        creators["detox cure disease"] = &MonkTriggerFactoryInternal::detox_cure_disease;
        creators["detox cure poison"] = &MonkTriggerFactoryInternal::detox_cure_poison;
        creators["detox cure magic"] = &MonkTriggerFactoryInternal::detox_cure_magic;
        creators["detox party member cure disease"] = &MonkTriggerFactoryInternal::detox_party_member_cure_disease;
        creators["detox party member cure poison"] = &MonkTriggerFactoryInternal::detox_party_member_cure_poison;
        creators["detox party member cure magic"] = &MonkTriggerFactoryInternal::detox_party_member_cure_magic;
    }

private:
    static Trigger* pvp_healing_sphere(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp healing sphere", PvpMistweaverGlue::Rule::SPHERE);
    }
    static Trigger* pvp_life_cocoon(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp life cocoon", PvpMistweaverGlue::Rule::COCOON);
    }
    static Trigger* pvp_thunder_focus_tea(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp thunder focus tea", PvpMistweaverGlue::Rule::TEA);
    }
    static Trigger* pvp_instant_surging_mist(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp instant surging mist", PvpMistweaverGlue::Rule::SURGE);
    }
    static Trigger* pvp_mana_tea(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp mana tea", PvpMistweaverGlue::Rule::MANA_TEA);
    }
    static Trigger* pvp_tigers_lust(PlayerbotAI* botAI)
    {
        return new PvpMistweaverTrigger(botAI, "pvp tigers lust", PvpMistweaverGlue::Rule::LUST);
    }
    static Trigger* spear_hand_strike(PlayerbotAI* botAI) { return new SpearHandStrikeInterruptTrigger(botAI); }
    static Trigger* spear_hand_strike_on_enemy_healer(PlayerbotAI* botAI)
    {
        return new SpearHandStrikeEnemyHealerTrigger(botAI);
    }
    static Trigger* provoke_on_snare_target(PlayerbotAI* botAI) { return new ProvokeSnareTrigger(botAI); }
    static Trigger* paralysis(PlayerbotAI* botAI) { return new ParalysisTrigger(botAI); }
    static Trigger* legacy_of_the_emperor(PlayerbotAI* botAI) { return new LegacyOfTheEmperorTrigger(botAI); }
    static Trigger* legacy_of_the_emperor_on_party(PlayerbotAI* botAI)
    {
        return new LegacyOfTheEmperorOnPartyTrigger(botAI);
    }
    static Trigger* legacy_of_the_white_tiger(PlayerbotAI* botAI) { return new LegacyOfTheWhiteTigerTrigger(botAI); }
    static Trigger* legacy_of_the_white_tiger_on_party(PlayerbotAI* botAI)
    {
        return new LegacyOfTheWhiteTigerOnPartyTrigger(botAI);
    }
    static Trigger* tigereye_brew(PlayerbotAI* botAI) { return new TigereyeBrewTrigger(botAI); }
    static Trigger* touch_of_death(PlayerbotAI* botAI) { return new TouchOfDeathTrigger(botAI); }
    static Trigger* touch_of_karma(PlayerbotAI* botAI) { return new TouchOfKarmaTrigger(botAI); }
    static Trigger* invoke_xuen(PlayerbotAI* botAI) { return new InvokeXuenTrigger(botAI); }
    static Trigger* elusive_brew(PlayerbotAI* botAI) { return new ElusiveBrewTrigger(botAI); }
    static Trigger* chi_brew(PlayerbotAI* botAI) { return new ChiBrewTrigger(botAI); }
    static Trigger* energizing_brew(PlayerbotAI* botAI) { return new EnergizingBrewTrigger(botAI); }
    static Trigger* black_ox_statue(PlayerbotAI* botAI) { return new BlackOxStatueTrigger(botAI); }
    static Trigger* jade_serpent_statue(PlayerbotAI* botAI) { return new JadeSerpentStatueTrigger(botAI); }
    static Trigger* brewmaster_expel_harm(PlayerbotAI* botAI) { return new BrewmasterExpelHarmTrigger(botAI); }
    static Trigger* soothing_mist_channel_check(PlayerbotAI* botAI)
    {
        return new SoothingMistChannelCheckTrigger(botAI);
    }
    static Trigger* renewing_mist_on_main_tank(PlayerbotAI* botAI)
    {
        return new RenewingMistOnMainTankTrigger(botAI);
    }
    static Trigger* enveloping_mist_on_main_tank(PlayerbotAI* botAI)
    {
        return new EnvelopingMistOnMainTankTrigger(botAI);
    }
    static Trigger* life_cocoon_on_main_tank(PlayerbotAI* botAI) { return new LifeCocoonOnMainTankTrigger(botAI); }
    static Trigger* renewing_mist_proactive(PlayerbotAI* botAI)
    {
        return new HealerProactiveHotTrigger(botAI, "renewing mist");
    }
    // The holy priest's small-party group heal trigger (PriestTriggers.h): three hurt members in a party of two to four as
    // well, the shared rule needs five members near. One registration per band.
    static Trigger* mistweaver_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "mistweaver group heal setting", "almost full");
    }
    static Trigger* mistweaver_medium_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "mistweaver medium group heal setting", "medium");
    }
    static Trigger* mistweaver_low_group_heal_setting(PlayerbotAI* botAI)
    {
        return new HolyGroupHealTrigger(botAI, "mistweaver low group heal setting", "low");
    }
    static Trigger* stance_of_the_fierce_tiger(PlayerbotAI* botAI) { return new StanceOfTheFierceTigerTrigger(botAI); }
    static Trigger* stance_of_the_sturdy_ox(PlayerbotAI* botAI) { return new StanceOfTheSturdyOxTrigger(botAI); }
    static Trigger* stance_of_the_wise_serpent(PlayerbotAI* botAI) { return new StanceOfTheWiseSerpentTrigger(botAI); }
    static Trigger* detox_cure_disease(PlayerbotAI* botAI) { return new DetoxCureDiseaseTrigger(botAI); }
    static Trigger* detox_cure_poison(PlayerbotAI* botAI) { return new DetoxCurePoisonTrigger(botAI); }
    static Trigger* detox_cure_magic(PlayerbotAI* botAI) { return new DetoxCureMagicTrigger(botAI); }
    static Trigger* detox_party_member_cure_disease(PlayerbotAI* botAI)
    {
        return new DetoxPartyMemberCureDiseaseTrigger(botAI);
    }
    static Trigger* detox_party_member_cure_poison(PlayerbotAI* botAI)
    {
        return new DetoxPartyMemberCurePoisonTrigger(botAI);
    }
    static Trigger* detox_party_member_cure_magic(PlayerbotAI* botAI)
    {
        return new DetoxPartyMemberCureMagicTrigger(botAI);
    }
};

class MonkActionFactoryInternal : public NamedObjectContext<Action>
{
public:
    MonkActionFactoryInternal()
    {
        // stances / buffs
        // #526: the three stance actions were declared in MonkActions.h but never registered.
        creators["stance of the fierce tiger"] = &MonkActionFactoryInternal::stance_of_the_fierce_tiger;
        creators["stance of the sturdy ox"] = &MonkActionFactoryInternal::stance_of_the_sturdy_ox;
        creators["stance of the wise serpent"] = &MonkActionFactoryInternal::stance_of_the_wise_serpent;
        creators["shuffle"] = &MonkActionFactoryInternal::shuffle;
        creators["legacy of the emperor"] = &MonkActionFactoryInternal::legacy_of_the_emperor;
        creators["fortifying brew"] = &MonkActionFactoryInternal::fortifying_brew;

        // shared melee
        creators["disable pvp"] = &MonkActionFactoryInternal::disable_pvp;
        creators["flying serpent kick pvp"] = &MonkActionFactoryInternal::flying_serpent_kick_pvp;
        creators["nimble brew pvp"] = &MonkActionFactoryInternal::nimble_brew_pvp;
        creators["tiger's lust"] = &MonkActionFactoryInternal::tigers_lust;
        // BT-657 to BT-662 (PvP class rules, Mistweaver package).
        creators["pvp healing sphere"] = &MonkActionFactoryInternal::pvp_healing_sphere;
        creators["pvp life cocoon"] = &MonkActionFactoryInternal::pvp_life_cocoon;
        creators["pvp thunder focus tea"] = &MonkActionFactoryInternal::pvp_thunder_focus_tea;
        creators["pvp instant surging mist"] = &MonkActionFactoryInternal::pvp_instant_surging_mist;
        creators["pvp mana tea"] = &MonkActionFactoryInternal::pvp_mana_tea;
        creators["tiger's lust pvp"] = &MonkActionFactoryInternal::tigers_lust_pvp;
        creators["jab"] = &MonkActionFactoryInternal::jab;
        creators["mistweaver melee"] = &MonkActionFactoryInternal::mistweaver_melee;  // BT-192
        creators["tiger palm"] = &MonkActionFactoryInternal::tiger_palm;
        creators["blackout kick"] = &MonkActionFactoryInternal::blackout_kick;
        creators["rising sun kick"] = &MonkActionFactoryInternal::rising_sun_kick;
        creators["fists of fury"] = &MonkActionFactoryInternal::fists_of_fury;
        creators["spinning crane kick"] = &MonkActionFactoryInternal::spinning_crane_kick;
        creators["expel harm"] = &MonkActionFactoryInternal::expel_harm;
        creators["chi wave"] = &MonkActionFactoryInternal::chi_wave;
        creators["roll"] = &MonkActionFactoryInternal::roll;
        creators["resuscitate"] = &MonkActionFactoryInternal::resuscitate;

        // brewmaster
        creators["keg smash"] = &MonkActionFactoryInternal::keg_smash;
        creators["breath of fire"] = &MonkActionFactoryInternal::breath_of_fire;
        creators["dizzying haze"] = &MonkActionFactoryInternal::dizzying_haze;
        creators["guard"] = &MonkActionFactoryInternal::guard;
        creators["purifying brew"] = &MonkActionFactoryInternal::purifying_brew;
        creators["elusive brew"] = &MonkActionFactoryInternal::elusive_brew;
        // Class sheet, brewmaster monk (BT-133).
        creators["chi wave tank"] = &MonkActionFactoryInternal::chi_wave_tank;
        creators["spinning crane kick tank"] = &MonkActionFactoryInternal::spinning_crane_kick_tank;
        creators["crackling jade lightning"] = &MonkActionFactoryInternal::crackling_jade_lightning;

        // mistweaver
        creators["renewing mist"] = &MonkActionFactoryInternal::renewing_mist;
        creators["enveloping mist"] = &MonkActionFactoryInternal::enveloping_mist;
        creators["surging mist"] = &MonkActionFactoryInternal::surging_mist;
        creators["soothing mist"] = &MonkActionFactoryInternal::soothing_mist;
        creators["uplift"] = &MonkActionFactoryInternal::uplift;
        // #163 S8 (MoP-only): group heals.
        creators["revival"] = &MonkActionFactoryInternal::revival;
        creators["life cocoon on protected"] = &MonkActionFactoryInternal::life_cocoon_on_protected;
        creators["renewing mist on party"] = &MonkActionFactoryInternal::renewing_mist_on_party;
        creators["enveloping mist on party"] = &MonkActionFactoryInternal::enveloping_mist_on_party;
        creators["surging mist on party"] = &MonkActionFactoryInternal::surging_mist_on_party;
        creators["mana tea"] = &MonkActionFactoryInternal::mana_tea;
        creators["detox"] = &MonkActionFactoryInternal::detox;
        // #194 (S8): Thunder Focus Tea + the Chi-gated "uplift on party".
        creators["thunder focus tea"] = &MonkActionFactoryInternal::thunder_focus_tea;
        creators["uplift on party"] = &MonkActionFactoryInternal::uplift_on_party;
        // Class sheet, mistweaver monk (BT-135).
        creators["soothing mist on party"] = &MonkActionFactoryInternal::soothing_mist_on_party;
        creators["renewing mist on main tank"] = &MonkActionFactoryInternal::renewing_mist_on_main_tank;
        creators["renewing mist proactive"] = &MonkActionFactoryInternal::renewing_mist_proactive;
        creators["enveloping mist on main tank"] = &MonkActionFactoryInternal::enveloping_mist_on_main_tank;
        creators["life cocoon on main tank"] = &MonkActionFactoryInternal::life_cocoon_on_main_tank;
        creators["spinning crane kick heal"] = &MonkActionFactoryInternal::spinning_crane_kick_heal;

        // #526 / #388 (see MonkActions.h).
        creators["spear hand strike"] = &MonkActionFactoryInternal::spear_hand_strike;
        creators["spear hand strike on enemy healer"] = &MonkActionFactoryInternal::spear_hand_strike_on_enemy_healer;
        creators["provoke"] = &MonkActionFactoryInternal::provoke;
        creators["provoke on snare target"] = &MonkActionFactoryInternal::provoke_on_snare_target;
        creators["paralysis"] = &MonkActionFactoryInternal::paralysis;
        creators["legacy of the emperor on party"] = &MonkActionFactoryInternal::legacy_of_the_emperor_on_party;
        creators["legacy of the white tiger"] = &MonkActionFactoryInternal::legacy_of_the_white_tiger;
        creators["legacy of the white tiger on party"] = &MonkActionFactoryInternal::legacy_of_the_white_tiger_on_party;
        creators["tigereye brew"] = &MonkActionFactoryInternal::tigereye_brew;
        creators["touch of death"] = &MonkActionFactoryInternal::touch_of_death;
        // #862 (MoP monk rotations, see MonkActions.h).
        creators["touch of karma"] = &MonkActionFactoryInternal::touch_of_karma;
        creators["invoke xuen"] = &MonkActionFactoryInternal::invoke_xuen;
        creators["dampen harm"] = &MonkActionFactoryInternal::dampen_harm;
        creators["summon black ox statue"] = &MonkActionFactoryInternal::summon_black_ox_statue;
        creators["summon jade serpent statue"] = &MonkActionFactoryInternal::summon_jade_serpent_statue;
        creators["chi brew"] = &MonkActionFactoryInternal::chi_brew;
        creators["energizing brew"] = &MonkActionFactoryInternal::energizing_brew;
        creators["chi wave target"] = &MonkActionFactoryInternal::chi_wave_target;
        creators["rushing jade wind"] = &MonkActionFactoryInternal::rushing_jade_wind;
        creators["zen sphere on party"] = &MonkActionFactoryInternal::zen_sphere_on_party;
        creators["chi wave on party"] = &MonkActionFactoryInternal::chi_wave_on_party;
        creators["spear hand strike on enemy caster"] = &MonkActionFactoryInternal::spear_hand_strike_on_enemy_caster;
        creators["roll gap"] = &MonkActionFactoryInternal::roll_gap;
        creators["detox poison on party"] = &MonkActionFactoryInternal::detox_poison_on_party;
        creators["detox disease on party"] = &MonkActionFactoryInternal::detox_disease_on_party;
        creators["detox magic on party"] = &MonkActionFactoryInternal::detox_magic_on_party;
    }

private:
    static Action* stance_of_the_fierce_tiger(PlayerbotAI* botAI) { return new CastStanceOfTheFierceTigerAction(botAI); }
    static Action* stance_of_the_sturdy_ox(PlayerbotAI* botAI) { return new CastStanceOfTheSturdyOxAction(botAI); }
    static Action* stance_of_the_wise_serpent(PlayerbotAI* botAI) { return new CastStanceOfTheWiseSerpentAction(botAI); }
    static Action* shuffle(PlayerbotAI* botAI) { return new CastShuffleAction(botAI); }
    static Action* legacy_of_the_emperor(PlayerbotAI* botAI) { return new CastLegacyOfTheEmperorAction(botAI); }
    static Action* fortifying_brew(PlayerbotAI* botAI) { return new CastFortifyingBrewAction(botAI); }

    static Action* disable_pvp(PlayerbotAI* ai) { return new CastDisablePvpAction(ai); }
    static Action* flying_serpent_kick_pvp(PlayerbotAI* ai) { return new CastFlyingSerpentKickPvpAction(ai); }
    static Action* nimble_brew_pvp(PlayerbotAI* ai) { return new CastNimbleBrewPvpAction(ai); }
    static Action* tigers_lust(PlayerbotAI* botAI) { return new CastTigersLustAction(botAI); }
    static Action* tigers_lust_pvp(PlayerbotAI* botAI) { return new CastTigersLustPvpAction(botAI); }
    static Action* pvp_healing_sphere(PlayerbotAI* botAI)
    {
        return new PvpMistweaverAction(botAI, "pvp healing sphere", PvpMistweaverGlue::Rule::SPHERE);
    }
    static Action* pvp_life_cocoon(PlayerbotAI* botAI)
    {
        return new PvpMistweaverAction(botAI, "pvp life cocoon", PvpMistweaverGlue::Rule::COCOON);
    }
    static Action* pvp_thunder_focus_tea(PlayerbotAI* botAI)
    {
        return new PvpMistweaverAction(botAI, "pvp thunder focus tea", PvpMistweaverGlue::Rule::TEA);
    }
    static Action* pvp_instant_surging_mist(PlayerbotAI* botAI)
    {
        return new PvpMistweaverAction(botAI, "pvp instant surging mist", PvpMistweaverGlue::Rule::SURGE);
    }
    static Action* pvp_mana_tea(PlayerbotAI* botAI)
    {
        return new PvpMistweaverAction(botAI, "pvp mana tea", PvpMistweaverGlue::Rule::MANA_TEA);
    }
    static Action* jab(PlayerbotAI* botAI) { return new CastJabAction(botAI); }
    static Action* mistweaver_melee(PlayerbotAI* botAI) { return new MistweaverMeleeAction(botAI); }
    static Action* tiger_palm(PlayerbotAI* botAI) { return new CastTigerPalmAction(botAI); }
    static Action* blackout_kick(PlayerbotAI* botAI) { return new CastBlackoutKickAction(botAI); }
    static Action* rising_sun_kick(PlayerbotAI* botAI) { return new CastRisingSunKickAction(botAI); }
    static Action* fists_of_fury(PlayerbotAI* botAI) { return new CastFistsOfFuryAction(botAI); }
    static Action* spinning_crane_kick(PlayerbotAI* botAI) { return new CastSpinningCraneKickAction(botAI); }
    static Action* expel_harm(PlayerbotAI* botAI) { return new CastExpelHarmAction(botAI); }
    static Action* chi_wave(PlayerbotAI* botAI) { return new CastChiWaveAction(botAI); }
    static Action* roll(PlayerbotAI* botAI) { return new CastRollAction(botAI); }
    static Action* resuscitate(PlayerbotAI* botAI) { return new CastResuscitateAction(botAI); }

    static Action* keg_smash(PlayerbotAI* botAI) { return new CastKegSmashAction(botAI); }
    static Action* breath_of_fire(PlayerbotAI* botAI) { return new CastBreathOfFireAction(botAI); }
    static Action* dizzying_haze(PlayerbotAI* botAI) { return new CastDizzyingHazeAction(botAI); }
    static Action* guard(PlayerbotAI* botAI) { return new CastGuardAction(botAI); }
    static Action* purifying_brew(PlayerbotAI* botAI) { return new CastPurifyingBrewAction(botAI); }
    static Action* elusive_brew(PlayerbotAI* botAI) { return new CastElusiveBrewAction(botAI); }
    static Action* chi_wave_tank(PlayerbotAI* botAI) { return new CastTankChiWaveAction(botAI); }
    static Action* spinning_crane_kick_tank(PlayerbotAI* botAI) { return new CastTankSpinningCraneKickAction(botAI); }
    static Action* crackling_jade_lightning(PlayerbotAI* botAI)
    {
        return new CastCracklingJadeLightningAction(botAI);
    }

    static Action* renewing_mist(PlayerbotAI* botAI) { return new CastRenewingMistAction(botAI); }
    static Action* enveloping_mist(PlayerbotAI* botAI) { return new CastEnvelopingMistAction(botAI); }
    static Action* surging_mist(PlayerbotAI* botAI) { return new CastSurgingMistAction(botAI); }
    static Action* soothing_mist(PlayerbotAI* botAI) { return new CastSoothingMistAction(botAI); }
    static Action* uplift(PlayerbotAI* botAI) { return new CastUpliftAction(botAI); }
    static Action* revival(PlayerbotAI* botAI) { return new CastRevivalAction(botAI); }
    static Action* life_cocoon_on_protected(PlayerbotAI* botAI) { return new CastLifeCocoonOnProtectedAction(botAI); }
    static Action* renewing_mist_on_party(PlayerbotAI* botAI) { return new CastRenewingMistOnPartyAction(botAI); }
    static Action* enveloping_mist_on_party(PlayerbotAI* botAI) { return new CastEnvelopingMistOnPartyAction(botAI); }
    static Action* surging_mist_on_party(PlayerbotAI* botAI) { return new CastSurgingMistOnPartyAction(botAI); }
    static Action* mana_tea(PlayerbotAI* botAI) { return new CastManaTeaAction(botAI); }
    static Action* detox(PlayerbotAI* botAI) { return new CastDetoxAction(botAI); }
    static Action* thunder_focus_tea(PlayerbotAI* botAI) { return new CastThunderFocusTeaAction(botAI); }
    static Action* uplift_on_party(PlayerbotAI* botAI) { return new CastUpliftOnPartyAction(botAI); }
    static Action* soothing_mist_on_party(PlayerbotAI* botAI) { return new CastSoothingMistOnPartyAction(botAI); }
    static Action* renewing_mist_on_main_tank(PlayerbotAI* botAI)
    {
        return new CastRenewingMistOnMainTankAction(botAI);
    }
    static Action* enveloping_mist_on_main_tank(PlayerbotAI* botAI)
    {
        return new CastEnvelopingMistOnMainTankAction(botAI);
    }
    static Action* life_cocoon_on_main_tank(PlayerbotAI* botAI) { return new CastLifeCocoonOnMainTankAction(botAI); }
    static Action* renewing_mist_proactive(PlayerbotAI* botAI)
    {
        return new CastHealerProactiveHotAction(botAI, "renewing mist");
    }
    static Action* spinning_crane_kick_heal(PlayerbotAI* botAI)
    {
        return new CastSpinningCraneKickHealAction(botAI);
    }

    static Action* spear_hand_strike(PlayerbotAI* botAI) { return new CastSpearHandStrikeAction(botAI); }
    static Action* spear_hand_strike_on_enemy_healer(PlayerbotAI* botAI)
    {
        return new CastSpearHandStrikeOnEnemyHealerAction(botAI);
    }
    static Action* provoke(PlayerbotAI* botAI) { return new CastProvokeAction(botAI); }
    static Action* provoke_on_snare_target(PlayerbotAI* botAI) { return new CastProvokeOnSnareTargetAction(botAI); }
    static Action* paralysis(PlayerbotAI* botAI) { return new CastParalysisAction(botAI); }
    static Action* legacy_of_the_emperor_on_party(PlayerbotAI* botAI)
    {
        return new CastLegacyOfTheEmperorOnPartyAction(botAI);
    }
    static Action* legacy_of_the_white_tiger(PlayerbotAI* botAI) { return new CastLegacyOfTheWhiteTigerAction(botAI); }
    static Action* legacy_of_the_white_tiger_on_party(PlayerbotAI* botAI)
    {
        return new CastLegacyOfTheWhiteTigerOnPartyAction(botAI);
    }
    static Action* tigereye_brew(PlayerbotAI* botAI) { return new CastTigereyeBrewAction(botAI); }
    static Action* touch_of_death(PlayerbotAI* botAI) { return new CastTouchOfDeathAction(botAI); }
    static Action* touch_of_karma(PlayerbotAI* botAI) { return new CastTouchOfKarmaAction(botAI); }
    static Action* invoke_xuen(PlayerbotAI* botAI) { return new CastInvokeXuenAction(botAI); }
    static Action* dampen_harm(PlayerbotAI* botAI) { return new CastDampenHarmAction(botAI); }
    static Action* summon_black_ox_statue(PlayerbotAI* botAI) { return new CastSummonBlackOxStatueAction(botAI); }
    static Action* summon_jade_serpent_statue(PlayerbotAI* botAI)
    {
        return new CastSummonJadeSerpentStatueAction(botAI);
    }
    static Action* chi_brew(PlayerbotAI* botAI) { return new CastChiBrewAction(botAI); }
    static Action* energizing_brew(PlayerbotAI* botAI) { return new CastEnergizingBrewAction(botAI); }
    static Action* chi_wave_target(PlayerbotAI* botAI) { return new CastChiWaveTargetAction(botAI); }
    static Action* rushing_jade_wind(PlayerbotAI* botAI) { return new CastRushingJadeWindAction(botAI); }
    static Action* zen_sphere_on_party(PlayerbotAI* botAI) { return new CastZenSphereOnPartyAction(botAI); }
    static Action* chi_wave_on_party(PlayerbotAI* botAI) { return new CastChiWaveOnPartyAction(botAI); }
    static Action* spear_hand_strike_on_enemy_caster(PlayerbotAI* botAI)
    {
        return new CastSpearHandStrikeOnEnemyCasterAction(botAI);
    }
    static Action* roll_gap(PlayerbotAI* botAI) { return new CastRollGapAction(botAI); }
    static Action* detox_poison_on_party(PlayerbotAI* botAI) { return new CastDetoxPoisonOnPartyAction(botAI); }
    static Action* detox_disease_on_party(PlayerbotAI* botAI) { return new CastDetoxDiseaseOnPartyAction(botAI); }
    static Action* detox_magic_on_party(PlayerbotAI* botAI) { return new CastDetoxMagicOnPartyAction(botAI); }
};

MonkAiObjectContext::MonkAiObjectContext(PlayerbotAI* botAI) : AiObjectContext(botAI)
{
    strategyContexts.Add(new MonkStrategyFactoryInternal());
    strategyContexts.Add(new MonkCombatStrategyFactoryInternal());
    actionContexts.Add(new MonkActionFactoryInternal());
    triggerContexts.Add(new MonkTriggerFactoryInternal());
}
