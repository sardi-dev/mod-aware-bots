/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "HunterTriggers.h"

#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "GenericTriggers.h"
#include "HunterActions.h"
#include "OneVsOne.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "ServerFacade.h"
#include "SharedDefines.h"
#include "Spell.h"
#include <set>

// Ported from AC mod-playerbots Ai/Class/Hunter/HunterTriggers.cpp (#149)

// Valid targets for "Improved Tracking".
bool NoTrackTrigger::IsActive()
{
    std::vector<std::string> track_list = {
        "track beasts",
        "track demons",
        "track dragonkin",
        "track elementals",
        "track giants",
        "track humanoids",
        "track undead",
    };

    for (auto& track : track_list)
    {
        if (botAI->HasAura(track, bot))
            return false;
    }

    return true;
}

bool HunterNoStingsActiveTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return DebuffTrigger::IsActive() && target &&
           !botAI->HasAura("serpent sting", target, false, true);
}

bool HuntersPetDeadTrigger::IsActive()
{
    return AI_VALUE(bool, "pet dead") && !AI_VALUE2(bool, "mounted", "self target");
}

bool HuntersPetMediumHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < sPlayerbotAIConfig->mediumHealth &&
           !AI_VALUE2(bool, "dead", "pet target") && !AI_VALUE2(bool, "mounted", "self target");
}
// ---------------------------------------------------------------------------
// #357 slice 3: 1:1 port of AC mod-playerbots Ai/Class/Hunter/HunterTriggers.cpp.
// Applied 5.4.8 divergences:
//   * AC's `sPlayerbotAIConfig.mediumHealth` / `.lowMana` are `->` in this fork (PlayerbotAIConfig.h).
//   * AC's `BotState::BOT_STATE_COMBAT` is plain `BOT_STATE_COMBAT` here (SharedDefines.h), same as the
//     rest of the fork's strategy code.
//   * No TC_LOG_* here, so the TC_LOG printf-vs-fmt divergence does not apply to this file.
//   * AC includes "Player.h" for the `bot` member; this fork exposes it via the same headers.
// ---------------------------------------------------------------------------

bool KillCommandTrigger::IsActive() { return !botAI->HasAura("kill command", GetTarget()); }

bool BlackArrowTrigger::IsActive()
{
    if (botAI->HasStrategy("trap weave", BOT_STATE_COMBAT))
        return false;

    return DebuffTrigger::IsActive();
}

bool HunterAspectOfThePackTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("aspect of the cheetah", GetTarget());
}

bool SwitchToRangedTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return botAI->HasStrategy("close", BOT_STATE_COMBAT) && target &&
           (target->GetVictim() != bot &&
            sServerFacade->IsDistanceGreaterThan(AI_VALUE2(float, "distance", "current target"), 8.0f));
}

bool SwitchToMeleeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    return botAI->HasStrategy("ranged", BOT_STATE_COMBAT) && target &&
           (target->GetVictim() == bot &&
            sServerFacade->IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "current target"), 8.0f));
}

bool SerpentStingOnAttackerTrigger::IsActive()
{
    if (!DebuffOnAttackerTrigger::IsActive())
        return false;

    Unit* target = GetTarget();
    if (!target)
        return false;

    return true;
}

bool HuntersPetLowHealthTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    return pet && AI_VALUE2(uint8, "health", "pet target") < 40 && !AI_VALUE2(bool, "dead", "pet target") &&
           !AI_VALUE2(bool, "mounted", "self target");
}

// 5.4.8 divergence, reported not worked around: this fork has NO "pet happy" value - it is absent from
// strategy/ValueContext.h entirely (AC registers it; grep across the whole fork returns 0 hits), so
// AI_VALUE(bool, "pet happy") always yields the default false and this trigger is permanently TRUE
// while unmounted. Harmless today (no fork TriggerNode/NextAction consumes "pet not happy"), and the
// code is kept verbatim so it becomes correct the moment the value lands. value/* is outside this
// slice's ownership, so the value itself is filed as a gap rather than added here.
bool HunterPetNotHappy::IsActive()
{
    return !AI_VALUE(bool, "pet happy") && !AI_VALUE2(bool, "mounted", "self target");
}

// AC HunterTriggers.cpp:inline. Moved out of the header for the 5.4.8 reason given at the class.
bool LockAndLoadTrigger::IsActive()
{
    constexpr uint32 LOCK_AND_LOAD_PROC = 56453;
    return bot->HasAura(LOCK_AND_LOAD_PROC);
}

// ===== #856 rotation task 1 (epic #854) =====
bool HunterBindingShotRangeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !bot->IsInCombat())
        return false;

    float distance = bot->GetDistance(target);
    return distance >= 15.0f && distance <= 30.0f;
}

bool HunterDeterrenceHealthTrigger::IsActive()
{
    return bot->IsInCombat() && bot->IsAlive() && bot->GetHealthPct() <= 30.0f;
}

bool HunterDisengagePvpTrigger::IsActive()
{
    // BT-620: the group test was a solo guard, but every bot of a battleground or an arena is in a group, so Disengage never
    // fired where it is meant to (the trigger needs a pvp instance anyway). An enemy player in melee that attacks the bot is the
    // whole condition, grouped or not.
    if (!bot->IsInCombat() || !AI_VALUE(bool, "in pvp instance"))
        return false;
    // In a 1v1 the 1v1 engine times Disengage by skill tier (OneVsOne.h).
    if (OneVsOneGlue::Owns(botAI, bot))
        return false;

    GuidVector attackers = AI_VALUE(GuidVector, "attackers");
    for (BotGuid const guid : attackers)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (unit && (unit->GetTypeId() == TypeID::TYPEID_PLAYER) && unit->GetVictim() == bot && unit->IsAlive() && bot->GetDistance(unit) <= 5.0f)
            return true;
    }

    return false;
}

bool HunterSteadyFocusLowTrigger::IsActive()
{
    if (!bot->IsInCombat())
        return false;

    // Rotation fix task H: 53224 is the PASSIVE (permanent, duration -1; scripts/Spells/spell_hunter.cpp:1900) that counts two
    // consecutive Steady Shots, so the old test (53224 && duration <= 3000) was true on every tick and Steady Shot (5.84) always
    // out-prioritised Chimera/Aimed Shot (w74: T:steady focus active 20/24 ticks, Chimera/Aimed never cast). The haste BUFF it
    // casts is SPELL_HUNT_STEADY_FOCUS 53220 (spell_hunter.cpp:128/1921): refresh when it is missing or <= 3 s left.
    if (!bot->HasAura(53224))
        return false;  // not a talented/learned MM hunter
    Aura* buff = bot->GetAura(53220);
    return !buff || (buff->GetDuration() >= 0 && buff->GetDuration() <= 3000);
}

bool HunterLowHealthNoPvpTrigger::IsActive()
{
    if (AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("low health");
    return t && t->IsActive();
}

bool HunterMediumThreatNoPvpTrigger::IsActive()
{
    if (AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("medium threat");
    return t && t->IsActive();
}

// Rotation fix task K (#868): see HunterTriggers.h. AC's "enemy too close for auto shot" outside a pvp instance / started duel.
bool HunterEnemyTooCloseTrigger::IsActive()
{
    if (bot->GetGroup() || AI_VALUE(bool, "in pvp instance"))
        return false;

    Trigger* t = botAI->GetAiObjectContext()->GetTrigger("enemy too close for auto shot");
    return t && t->IsActive();
}

// Rotation fix task K (#868): see HunterTriggers.h.
bool HunterDuelRetargetTrigger::IsActive()
{
    if (!bot->duel || !bot->duel->opponent || bot->duel->startTime == 0)
        return false;

    Unit* opponent = bot->duel->opponent;
    return opponent->IsAlive() && !AI_VALUE(Unit*, "current target");
}

// Hunter sheets: use the existing AoE trigger, which reuses the attacker list.
bool HunterRapidFirePackTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    Trigger* pack = context->GetTrigger("medium aoe");
    return target && ((target->ToCreature() && target->ToCreature()->isElite()) || (pack && pack->IsActive()));
}
bool HunterFrenzyTrigger::IsActive()
{
    Unit* pet = AI_VALUE(Unit*, "pet target");
    constexpr uint32 FRENZY = 19615;
    constexpr uint8 FRENZY_STACKS = 5;
    Aura* frenzy = pet ? pet->GetAura(FRENZY) : nullptr;
    return frenzy && frenzy->GetStackAmount() >= FRENZY_STACKS;
}
bool HunterMasterMarksmanTrigger::IsActive() { return bot->HasAura(82926); }
bool HunterFervorTrigger::IsActive() { return bot->IsInCombat() && bot->GetPower(POWER_FOCUS) < 30; }
bool HunterIronHawkTrigger::IsActive()
{
    return bot->HasSpell(109260) && !bot->HasAura(109260) && !bot->HasAura(5118) && !bot->HasAura(13159);
}

// Hunter sheets: travel aspects are only for following a peaceful leader.
static bool HunterCanUseTravelAspect(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    Player* leader = ai->GetGroupMaster();
    return !bot->IsInCombat() && !bot->IsMounted() && leader && leader != bot && !leader->IsInCombat() &&
           ai->HasStrategy("follow", BOT_STATE_NON_COMBAT) &&
           !ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}
bool HunterTravelAspectTrigger::IsActive()
{
    return HunterCanUseTravelAspect(botAI) && !bot->HasAura(5118) && !bot->HasAura(13159);
}
bool HunterCancelTravelAspectTrigger::IsActive()
{
    return (bot->HasAura(5118) || bot->HasAura(13159)) && !HunterCanUseTravelAspect(botAI);
}

bool HunterTrapLauncherTrigger::IsActive()
{
    constexpr uint32 SPELL_TRAP_LAUNCHER = 77769;
    if (!bot->IsInCombat() || !bot->HasSpell(SPELL_TRAP_LAUNCHER))
        return false;
    return DpsRotationMath::Hunter::KeepTrapLauncher(bot->getLevel(), BotSkill::Uses(bot, BotSkillMath::Tier::GOOD),
        botAI->HasAura("trap launcher", bot));
}

bool HunterExplosiveTrapPackTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || !bot->IsInCombat())
        return false;
    // Explosive Trap's blast is 10 yd around the trap; the trap lands on the target.
    constexpr float TRAP_BLAST_YD = 10.0f;
    unsigned pack = 0;
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
        if (Unit* unit = botAI->GetUnit(guid))
            if (unit->IsAlive() && target->IsWithinDistInMap(unit, TRAP_BLAST_YD))
                ++pack;
    return DpsRotationMath::Hunter::ExplosiveTrapAtPack(BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT),
        botAI->HasAura("trap launcher", bot), pack, bot->GetDistance(target));
}
