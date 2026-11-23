/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HunterAiObjectContext.h"

#include "BeastMasteryHunterStrategy.h"
#include "DpsHunterStrategy.h"
#include "MarksmanshipHunterStrategy.h"
#include "SurvivalHunterStrategy.h"
#include "GenericHunterNonCombatStrategy.h"
#include "HunterActions.h"
#include "HunterBuffStrategies.h"
#include "HunterTriggers.h"
#include "NamedObjectContext.h"
#include "Playerbots.h"

class HunterStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    HunterStrategyFactoryInternal()
    {
        creators["bm"] = &HunterStrategyFactoryInternal::bm;
        creators["mm"] = &HunterStrategyFactoryInternal::mm;
        creators["surv"] = &HunterStrategyFactoryInternal::surv;
        creators["dps"] = &HunterStrategyFactoryInternal::dps;
        creators["nc"] = &HunterStrategyFactoryInternal::nc;
        creators["aoe"] = &HunterStrategyFactoryInternal::aoe;
        creators["dps debuff"] = &HunterStrategyFactoryInternal::dps_debuff;
        creators["boost"] = &HunterStrategyFactoryInternal::boost;
        creators["pet"] = &HunterStrategyFactoryInternal::pet;
        creators["cc"] = &HunterStrategyFactoryInternal::cc;
    }

private:
    static Strategy* aoe(PlayerbotAI* botAI) { return new DpsAoeHunterStrategy(botAI); }
    static Strategy* bm(PlayerbotAI* botAI) { return new BeastMasteryHunterStrategy(botAI); }
    static Strategy* mm(PlayerbotAI* botAI) { return new MarksmanshipHunterStrategy(botAI); }
    static Strategy* surv(PlayerbotAI* botAI) { return new SurvivalHunterStrategy(botAI); }
    static Strategy* dps(PlayerbotAI* botAI) { return new DpsHunterStrategy(botAI); }
    static Strategy* nc(PlayerbotAI* botAI) { return new GenericHunterNonCombatStrategy(botAI); }
    static Strategy* dps_debuff(PlayerbotAI* botAI) { return new DpsHunterDebuffStrategy(botAI); }
    static Strategy* boost(PlayerbotAI* botAI) { return new HunterBoostStrategy(botAI); }
    static Strategy* pet(PlayerbotAI* botAI) { return new HunterPetStrategy(botAI); }
    static Strategy* cc(PlayerbotAI* botAI) { return new HunterCcStrategy(botAI); }
};

class HunterBuffStrategyFactoryInternal : public NamedObjectContext<Strategy>
{
public:
    HunterBuffStrategyFactoryInternal() : NamedObjectContext<Strategy>(false, true)
    {
        creators["bspeed"] = &HunterBuffStrategyFactoryInternal::bspeed;
        creators["bdps"] = &HunterBuffStrategyFactoryInternal::bdps;
    }

private:
    static Strategy* bspeed(PlayerbotAI* botAI) { return new HunterBuffSpeedStrategy(botAI); }
    static Strategy* bdps(PlayerbotAI* botAI) { return new HunterBuffDpsStrategy(botAI); }
};

class HunterTriggerFactoryInternal : public NamedObjectContext<Trigger>
{
public:
    HunterTriggerFactoryInternal()
    {
        // #357 slice 3: this context registered 7 of the 33 names AC registers
        // (AC Ai/Class/Hunter/HunterAiObjectContext.cpp:66-98). All 33 are registered below with AC's
        // exact creator names and factory shapes; AC's line numbers are in the trailing comments.
        // The 7 the #149 port already had: "hunters pet dead" (:68), "hunters pet medium health" (:70),
        // "no stings" (:67), "no track" (:79), "concussive shot on snare target" (:82),
        // "tranquilizing shot enrage" (:90), "tranquilizing shot magic" (:91).
        creators["black arrow"] = &HunterTriggerFactoryInternal::black_arrow;                        // AC :66
        creators["no stings"] = &HunterTriggerFactoryInternal::no_stings;                              // AC :67
        creators["hunters pet dead"] = &HunterTriggerFactoryInternal::hunters_pet_dead;              // AC :68
        creators["hunters pet low health"] = &HunterTriggerFactoryInternal::hunters_pet_low_health;  // AC :69
        creators["hunters pet medium health"] = &HunterTriggerFactoryInternal::hunters_pet_medium_health;  // AC :70
        creators["hunter's mark"] = &HunterTriggerFactoryInternal::hunters_mark;                     // AC :71
        creators["freezing trap"] = &HunterTriggerFactoryInternal::freezing_trap;                    // AC :72
        creators["rapid fire"] = &HunterTriggerFactoryInternal::rapid_fire;                          // AC :73
        creators["aspect of the pack"] = &HunterTriggerFactoryInternal::aspect_of_the_pack;          // AC :74
        creators["aspect of the hawk"] = &HunterTriggerFactoryInternal::aspect_of_the_hawk;
        creators["trueshot aura"] = &HunterTriggerFactoryInternal::trueshot_aura;                    // AC :78
        creators["no track"] = &HunterTriggerFactoryInternal::no_track;                              // AC :79
        creators["serpent sting on attacker"] = &HunterTriggerFactoryInternal::serpent_sting_on_attacker;  // AC :80
        creators["pet not happy"] = &HunterTriggerFactoryInternal::pet_not_happy;                   // AC :81
        creators["concussive shot on snare target"] = &HunterTriggerFactoryInternal::concussive_shot_on_snare_target;  // AC :82
        creators["scare beast"] = &HunterTriggerFactoryInternal::scare_beast;                        // AC :83
        creators["switch to melee"] = &HunterTriggerFactoryInternal::switch_to_melee;                // AC :87
        creators["switch to ranged"] = &HunterTriggerFactoryInternal::switch_to_ranged;              // AC :88
        creators["misdirection on main tank"] = &HunterTriggerFactoryInternal::misdirection_on_main_tank;  // AC :89
        creators["tranquilizing shot enrage"] = &HunterTriggerFactoryInternal::remove_enrage;        // AC :90
        creators["tranquilizing shot magic"] = &HunterTriggerFactoryInternal::remove_magic;          // AC :91
        creators["kill command"] = &HunterTriggerFactoryInternal::kill_command;                      // AC :93
        creators["explosive shot"] = &HunterTriggerFactoryInternal::explosive_shot;                  // AC :94
        creators["lock and load"] = &HunterTriggerFactoryInternal::lock_and_load;                    // AC :95
        creators["silencing shot"] = &HunterTriggerFactoryInternal::silencing_shot;                // AC :96
        creators["intimidation"] = &HunterTriggerFactoryInternal::intimidation;                      // AC :97
        // #856 (epic #854) MoP hunter triggers
        creators["counter shot"] = &HunterTriggerFactoryInternal::counter_shot;
        creators["binding shot range"] = &HunterTriggerFactoryInternal::binding_shot_range;
        creators["deterrence health"] = &HunterTriggerFactoryInternal::deterrence_health;
        creators["hunter low health"] = &HunterTriggerFactoryInternal::hunter_low_health;
        creators["hunter medium threat"] = &HunterTriggerFactoryInternal::hunter_medium_threat;
        creators["disengage pvp"] = &HunterTriggerFactoryInternal::disengage_pvp;
        creators["hunter enemy too close"] = &HunterTriggerFactoryInternal::hunter_enemy_too_close;  // task K (#868)
        creators["hunter duel retarget"] = &HunterTriggerFactoryInternal::hunter_duel_retarget;  // task K (#868)
        creators["hunter rapid fire pack"] = &HunterTriggerFactoryInternal::rapid_fire_pack;
        creators["hunter frenzy"] = &HunterTriggerFactoryInternal::frenzy;
        creators["master marksman proc"] = &HunterTriggerFactoryInternal::master_marksman;
        creators["hunter low focus"] = &HunterTriggerFactoryInternal::fervor;
        creators["hunter trap launcher"] = &HunterTriggerFactoryInternal::trap_launcher;
        creators["hunter explosive trap pack"] = &HunterTriggerFactoryInternal::explosive_trap_pack;
        creators["hunter travel aspect"] = &HunterTriggerFactoryInternal::travel_aspect;
        creators["hunter cancel travel aspect"] = &HunterTriggerFactoryInternal::cancel_travel_aspect;
        creators["iron hawk"] = &HunterTriggerFactoryInternal::iron_hawk;
        creators["steady focus low"] = &HunterTriggerFactoryInternal::steady_focus_low;
    }

private:
    static Trigger* travel_aspect(PlayerbotAI* ai) { return new HunterTravelAspectTrigger(ai); }
    static Trigger* cancel_travel_aspect(PlayerbotAI* ai) { return new HunterCancelTravelAspectTrigger(ai); }
    static Trigger* rapid_fire_pack(PlayerbotAI* ai) { return new HunterRapidFirePackTrigger(ai); }
    static Trigger* frenzy(PlayerbotAI* ai) { return new HunterFrenzyTrigger(ai); }
    static Trigger* master_marksman(PlayerbotAI* ai) { return new HunterMasterMarksmanTrigger(ai); }
    static Trigger* fervor(PlayerbotAI* ai) { return new HunterFervorTrigger(ai); }
    static Trigger* trap_launcher(PlayerbotAI* ai) { return new HunterTrapLauncherTrigger(ai); }
    static Trigger* explosive_trap_pack(PlayerbotAI* ai) { return new HunterExplosiveTrapPackTrigger(ai); }
    static Trigger* iron_hawk(PlayerbotAI* ai) { return new HunterIronHawkTrigger(ai); }
    static Trigger* black_arrow(PlayerbotAI* botAI) { return new BlackArrowTrigger(botAI); }
    static Trigger* no_stings(PlayerbotAI* botAI) { return new HunterNoStingsActiveTrigger(botAI); }
    static Trigger* hunters_pet_dead(PlayerbotAI* botAI) { return new HuntersPetDeadTrigger(botAI); }
    static Trigger* hunters_pet_low_health(PlayerbotAI* botAI) { return new HuntersPetLowHealthTrigger(botAI); }
    static Trigger* hunters_pet_medium_health(PlayerbotAI* botAI) { return new HuntersPetMediumHealthTrigger(botAI); }
    static Trigger* hunters_mark(PlayerbotAI* botAI) { return new HuntersMarkTrigger(botAI); }
    static Trigger* freezing_trap(PlayerbotAI* botAI) { return new FreezingTrapTrigger(botAI); }
    static Trigger* rapid_fire(PlayerbotAI* botAI) { return new RapidFireTrigger(botAI); }
    static Trigger* aspect_of_the_pack(PlayerbotAI* botAI) { return new HunterAspectOfThePackTrigger(botAI); }
    static Trigger* aspect_of_the_hawk(PlayerbotAI* botAI) { return new BuffTrigger(botAI, "aspect of the hawk"); }
    static Trigger* trueshot_aura(PlayerbotAI* botAI) { return new TrueshotAuraTrigger(botAI); }
    static Trigger* no_track(PlayerbotAI* botAI) { return new NoTrackTrigger(botAI); }
    static Trigger* serpent_sting_on_attacker(PlayerbotAI* botAI) { return new SerpentStingOnAttackerTrigger(botAI); }
    static Trigger* pet_not_happy(PlayerbotAI* botAI) { return new HunterPetNotHappy(botAI); }
    static Trigger* concussive_shot_on_snare_target(PlayerbotAI* botAI) { return new ConcussiveShotOnSnareTargetTrigger(botAI); }
    static Trigger* scare_beast(PlayerbotAI* botAI) { return new ScareBeastTrigger(botAI); }
    static Trigger* switch_to_melee(PlayerbotAI* botAI) { return new SwitchToMeleeTrigger(botAI); }
    static Trigger* switch_to_ranged(PlayerbotAI* botAI) { return new SwitchToRangedTrigger(botAI); }
    static Trigger* misdirection_on_main_tank(PlayerbotAI* botAI) { return new MisdirectionOnMainTankTrigger(botAI); }
    static Trigger* remove_enrage(PlayerbotAI* botAI) { return new TargetRemoveEnrageTrigger(botAI); }
    static Trigger* remove_magic(PlayerbotAI* botAI) { return new TargetRemoveMagicTrigger(botAI); }
    static Trigger* kill_command(PlayerbotAI* botAI) { return new KillCommandTrigger(botAI); }
    static Trigger* explosive_shot(PlayerbotAI* botAI) { return new ExplosiveShotTrigger(botAI); }
    static Trigger* lock_and_load(PlayerbotAI* botAI) { return new LockAndLoadTrigger(botAI); }
    static Trigger* silencing_shot(PlayerbotAI* botAI) { return new SilencingShotTrigger(botAI); }
    static Trigger* intimidation(PlayerbotAI* botAI) { return new IntimidationTrigger(botAI); }
    static Trigger* counter_shot(PlayerbotAI* botAI) { return new CounterShotTrigger(botAI); }
    static Trigger* binding_shot_range(PlayerbotAI* botAI) { return new HunterBindingShotRangeTrigger(botAI); }
    static Trigger* deterrence_health(PlayerbotAI* botAI) { return new HunterDeterrenceHealthTrigger(botAI); }
    static Trigger* hunter_low_health(PlayerbotAI* botAI) { return new HunterLowHealthNoPvpTrigger(botAI); }
    static Trigger* hunter_medium_threat(PlayerbotAI* botAI) { return new HunterMediumThreatNoPvpTrigger(botAI); }
    static Trigger* disengage_pvp(PlayerbotAI* botAI) { return new HunterDisengagePvpTrigger(botAI); }
    static Trigger* hunter_enemy_too_close(PlayerbotAI* botAI) { return new HunterEnemyTooCloseTrigger(botAI); }
    static Trigger* hunter_duel_retarget(PlayerbotAI* botAI) { return new HunterDuelRetargetTrigger(botAI); }
    static Trigger* steady_focus_low(PlayerbotAI* botAI) { return new HunterSteadyFocusLowTrigger(botAI); }
};

class HunterAiObjectContextInternal : public NamedObjectContext<Action>
{
public:
    HunterAiObjectContextInternal()
    {
        creators["hunter rabid"] = &HunterAiObjectContextInternal::rabid;
        creators["cancel hunter travel aspect"] = &HunterAiObjectContextInternal::cancel_travel_aspect;
        creators["aspect of the iron hawk"] = &HunterAiObjectContextInternal::iron_hawk;
        creators["fervor"] = &HunterAiObjectContextInternal::fervor;
        creators["exhilaration"] = &HunterAiObjectContextInternal::exhilaration;
        creators["focus fire"] = &HunterAiObjectContextInternal::focus_fire;
        creators["auto shot"] = &HunterAiObjectContextInternal::auto_shot;
        creators["aimed shot"] = &HunterAiObjectContextInternal::aimed_shot;
        creators["chimera shot"] = &HunterAiObjectContextInternal::chimera_shot;
        creators["explosive shot"] = &HunterAiObjectContextInternal::explosive_shot;
        creators["explosive shot rank 1"] = &HunterAiObjectContextInternal::explosive_shot_rank_1;
        creators["explosive shot rank 2"] = &HunterAiObjectContextInternal::explosive_shot_rank_2;
        creators["explosive shot rank 3"] = &HunterAiObjectContextInternal::explosive_shot_rank_3;
        creators["explosive shot rank 4"] = &HunterAiObjectContextInternal::explosive_shot_rank_4;
        creators["arcane shot"] = &HunterAiObjectContextInternal::arcane_shot;
        creators["intimidation"] = &HunterAiObjectContextInternal::intimidation;
        creators["concussive shot"] = &HunterAiObjectContextInternal::concussive_shot;
        creators["distracting shot"] = &HunterAiObjectContextInternal::distracting_shot;
        creators["multi-shot"] = &HunterAiObjectContextInternal::multi_shot;
        creators["serpent sting"] = &HunterAiObjectContextInternal::serpent_sting;
        creators["serpent sting on attacker"] = &HunterAiObjectContextInternal::serpent_sting_on_attacker;
        creators["wyvern sting"] = &HunterAiObjectContextInternal::wyvern_sting;
        creators["hunter's mark"] = &HunterAiObjectContextInternal::hunters_mark;
        creators["mend pet"] = &HunterAiObjectContextInternal::mend_pet;
        creators["kill command"] = &HunterAiObjectContextInternal::kill_command;
        creators["revive pet"] = &HunterAiObjectContextInternal::revive_pet;
        creators["call pet 1"] = &HunterAiObjectContextInternal::call_pet;
        creators["black arrow"] = &HunterAiObjectContextInternal::black_arrow;
        creators["freezing trap"] = &HunterAiObjectContextInternal::freezing_trap;
        creators["rapid fire"] = &HunterAiObjectContextInternal::rapid_fire;
        creators["boost"] = &HunterAiObjectContextInternal::rapid_fire;
        creators["deterrence"] = &HunterAiObjectContextInternal::deterrence;
        creators["aspect of the hawk"] = &HunterAiObjectContextInternal::aspect_of_the_hawk;
        creators["aspect of the pack"] = &HunterAiObjectContextInternal::aspect_of_the_pack;
        creators["aspect of the cheetah"] = &HunterAiObjectContextInternal::aspect_of_the_cheetah;
        creators["trueshot aura"] = &HunterAiObjectContextInternal::trueshot_aura;
        creators["track humanoids"] = &HunterAiObjectContextInternal::track_humanoids;
        creators["feign death"] = &HunterAiObjectContextInternal::feign_death;
        creators["bestial wrath"] = &HunterAiObjectContextInternal::bestial_wrath;
        creators["scare beast"] = &HunterAiObjectContextInternal::scare_beast;
        creators["scare beast on cc"] = &HunterAiObjectContextInternal::scare_beast_on_cc;
        creators["tranquilizing shot"] = &HunterAiObjectContextInternal::tranquilizing_shot;
        creators["steady shot"] = &HunterAiObjectContextInternal::steady_shot;
        creators["kill shot"] = &HunterAiObjectContextInternal::kill_shot;
        creators["misdirection on main tank"] = &HunterAiObjectContextInternal::misdirection_on_main_tank;
        creators["silencing shot"] = &HunterAiObjectContextInternal::silencing_shot;
        creators["disengage"] = &HunterAiObjectContextInternal::disengage;          // #521 AC :190
        creators["explosive trap"] = &HunterAiObjectContextInternal::explosive_trap;  // #521 AC :192
        creators["trap launcher"] = &HunterAiObjectContextInternal::trap_launcher;
        // #856 (epic #854) MoP hunter actions
        creators["cobra shot"] = &HunterAiObjectContextInternal::cobra_shot;
        creators["glaive toss"] = &HunterAiObjectContextInternal::glaive_toss;
        creators["binding shot"] = &HunterAiObjectContextInternal::binding_shot;
        creators["dire beast"] = &HunterAiObjectContextInternal::dire_beast;
        creators["stampede"] = &HunterAiObjectContextInternal::stampede;
        creators["a murder of crows"] = &HunterAiObjectContextInternal::murder_of_crows;
        creators["counter shot"] = &HunterAiObjectContextInternal::counter_shot;
        creators["counter shot on enemy caster"] = &HunterAiObjectContextInternal::counter_shot_on_enemy_caster;
    }

private:
    static Action* scare_beast(PlayerbotAI* botAI) { return new CastScareBeastAction(botAI); }
    static Action* scare_beast_on_cc(PlayerbotAI* botAI) { return new CastScareBeastCcAction(botAI); }
    static Action* bestial_wrath(PlayerbotAI* botAI) { return new CastBestialWrathAction(botAI); }
    static Action* feign_death(PlayerbotAI* botAI) { return new CastFeignDeathAction(botAI); }
    static Action* trueshot_aura(PlayerbotAI* botAI) { return new CastTrueshotAuraAction(botAI); }
    static Action* track_humanoids(PlayerbotAI* botAI) { return new CastBuffSpellAction(botAI, "track humanoids"); }
    static Action* auto_shot(PlayerbotAI* botAI) { return new CastAutoShotAction(botAI); }
    static Action* aimed_shot(PlayerbotAI* botAI) { return new CastAimedShotAction(botAI); }
    static Action* chimera_shot(PlayerbotAI* botAI) { return new CastChimeraShotAction(botAI); }
    static Action* explosive_shot(PlayerbotAI* botAI) { return new CastExplosiveShotAction(botAI); }
    static Action* arcane_shot(PlayerbotAI* botAI) { return new CastArcaneShotAction(botAI); }
    static Action* concussive_shot(PlayerbotAI* botAI) { return new CastConcussiveShotAction(botAI); }
    static Action* distracting_shot(PlayerbotAI* botAI) { return new CastDistractingShotAction(botAI); }
    static Action* multi_shot(PlayerbotAI* botAI) { return new CastMultiShotAction(botAI); }
    static Action* serpent_sting(PlayerbotAI* botAI) { return new CastSerpentStingAction(botAI); }
    static Action* serpent_sting_on_attacker(PlayerbotAI* botAI) { return new CastSerpentStingOnAttackerAction(botAI); }
    static Action* wyvern_sting(PlayerbotAI* botAI) { return new CastWyvernStingAction(botAI); }
    static Action* rabid(PlayerbotAI* ai) { return new HunterRabidAction(ai); }
    static Action* cancel_travel_aspect(PlayerbotAI* ai) { return new HunterCancelTravelAspectAction(ai); }
    static Action* iron_hawk(PlayerbotAI* ai) { return new CastIronHawkAction(ai); }
    static Action* fervor(PlayerbotAI* ai) { return new CastFervorAction(ai); }
    static Action* exhilaration(PlayerbotAI* ai) { return new CastExhilarationAction(ai); }
    static Action* focus_fire(PlayerbotAI* ai) { return new CastFocusFireAction(ai); }
    static Action* hunters_mark(PlayerbotAI* botAI) { return new CastHuntersMarkAction(botAI); }
    static Action* mend_pet(PlayerbotAI* botAI) { return new CastMendPetAction(botAI); }
    static Action* kill_command(PlayerbotAI* botAI) { return new CastKillCommandAction(botAI); }
    static Action* revive_pet(PlayerbotAI* botAI) { return new CastRevivePetAction(botAI); }
    static Action* call_pet(PlayerbotAI* botAI) { return new CastCallPetAction(botAI); }
    static Action* black_arrow(PlayerbotAI* botAI) { return new CastBlackArrow(botAI); }
    static Action* freezing_trap(PlayerbotAI* botAI) { return new CastFreezingTrap(botAI); }
    static Action* rapid_fire(PlayerbotAI* botAI) { return new CastRapidFireAction(botAI); }
    static Action* deterrence(PlayerbotAI* botAI) { return new CastDeterrenceAction(botAI); }
    static Action* aspect_of_the_hawk(PlayerbotAI* botAI) { return new CastAspectOfTheHawkAction(botAI); }
    static Action* aspect_of_the_pack(PlayerbotAI* botAI) { return new CastAspectOfThePackAction(botAI); }
    static Action* aspect_of_the_cheetah(PlayerbotAI* botAI) { return new CastAspectOfTheCheetahAction(botAI); }
    static Action* tranquilizing_shot(PlayerbotAI* ai) { return new CastTranquilizingShotAction(ai); }
    static Action* steady_shot(PlayerbotAI* ai) { return new CastSteadyShotAction(ai); }
    static Action* kill_shot(PlayerbotAI* ai) { return new CastKillShotAction(ai); }
    static Action* misdirection_on_main_tank(PlayerbotAI* ai) { return new CastMisdirectionOnMainTankAction(ai); }
    static Action* silencing_shot(PlayerbotAI* ai) { return new CastSilencingShotAction(ai); }
    static Action* intimidation(PlayerbotAI* botAI) { return new CastIntimidationAction(botAI); }
    static Action* disengage(PlayerbotAI* botAI) { return new CastDisengageAction(botAI); }
    static Action* explosive_trap(PlayerbotAI* botAI) { return new CastExplosiveTrapAction(botAI); }
    static Action* trap_launcher(PlayerbotAI* botAI) { return new CastTrapLauncherAction(botAI); }
    static Action* cobra_shot(PlayerbotAI* botAI) { return new CastCobraShotAction(botAI); }
    static Action* glaive_toss(PlayerbotAI* botAI) { return new CastGlaiveTossAction(botAI); }
    static Action* binding_shot(PlayerbotAI* botAI) { return new CastBindingShotAction(botAI); }
    static Action* dire_beast(PlayerbotAI* botAI) { return new CastDireBeastAction(botAI); }
    static Action* stampede(PlayerbotAI* botAI) { return new CastStampedeAction(botAI); }
    static Action* murder_of_crows(PlayerbotAI* botAI) { return new CastMurderOfCrowsAction(botAI); }
    static Action* counter_shot(PlayerbotAI* botAI) { return new CastCounterShotAction(botAI); }
    static Action* counter_shot_on_enemy_caster(PlayerbotAI* botAI) { return new CastCounterShotOnEnemyCasterAction(botAI); }
    static Action* explosive_shot_rank_1(PlayerbotAI* botAI) { return new CastExplosiveShotRank1Action(botAI); }
    static Action* explosive_shot_rank_2(PlayerbotAI* botAI) { return new CastExplosiveShotRank2Action(botAI); }
    static Action* explosive_shot_rank_3(PlayerbotAI* botAI) { return new CastExplosiveShotRank3Action(botAI); }
    static Action* explosive_shot_rank_4(PlayerbotAI* botAI) { return new CastExplosiveShotRank4Action(botAI); }
    
};

// #149: these four registrations were left commented out (presumably a WIP-port leftover), which meant
// hunter bots had ZERO hunter-specific strategies/actions/triggers registered -- not even Auto Shot --
// while every sibling class (Druid, Mage, Paladin, ...) registers its four factories here normally.
// AC's hunter class is fully functional, and HunterStrategyFactoryInternal / HunterBuffStrategyFactoryInternal /
// HunterAiObjectContextInternal below are already fully populated (~45 hunter spell actions), so this is
// required for hunter bots to do anything at all, and for any of the trigger work above to matter.
// Uncommenting this is the single most impactful change in this patch: it takes hunter bots from doing
// literally nothing to fighting normally.
HunterAiObjectContext::HunterAiObjectContext(PlayerbotAI* botAI) : AiObjectContext(botAI)
{
    strategyContexts.Add(new HunterStrategyFactoryInternal());
    strategyContexts.Add(new HunterBuffStrategyFactoryInternal());
    actionContexts.Add(new HunterAiObjectContextInternal());
    triggerContexts.Add(new HunterTriggerFactoryInternal());
}
