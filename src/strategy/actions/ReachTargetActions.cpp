/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ReachTargetActions.h"

#include "AttackersValue.h"
#include "CcDutyTargetValue.h"
#include "Event.h"
#include "Group.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "ServerFacade.h"
#include "ResurrectPolicy.h"
#include "ReachPolicy.h"

#include "Log.h"

#include <algorithm>

bool ReachTargetAction::Execute(Event event)
{
    return ReachCombatTo(AI_VALUE(Unit*, GetTargetName()), distance);
}

bool ReachTargetAction::isUseful()
{
    // do not move while staying (AC ReachTargetActions.cpp, #827)
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }

    // do not move while casting
    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
    {
        return false;
    }
    Unit* target = GetTarget();
    if (!target)
        return false;
    // float dis = distance + CONTACT_DISTANCE;
    // sServerFacade->IsDistanceGreaterThan(AI_VALUE2(float, "distance", GetTargetName()), distance);
    if (!bot->IsWithinCombatRange(target, distance))
        return true;

    // BT-187: in range of the stale combat target but out of its sight (a watcher on the floor below the ledge): walk
    // on until the bot sees it. Only that target, so no sight test runs for the usual in-range target.
    return AttackersValue::KeepsStaleTargetWithoutSight(botAI, target) && !bot->IsWithinLOSInMap(target);
}

std::string const ReachTargetAction::GetTargetName() { return "current target"; }

CastReachTargetSpellAction::CastReachTargetSpellAction(PlayerbotAI* botAI, std::string const spell, float distance)
    : CastSpellAction(botAI, spell), distance(distance)
{
}
bool CastReachTargetSpellAction::isUseful()
{
    // do not move while staying (AC ReachTargetActions.cpp, #827)
    if (botAI->HasStrategy("stay", botAI->GetState()))
    {
        return false;
    }

    Unit* target = GetTarget();
    if (!target)
        return false;

    float const dist = AI_VALUE2(float, "distance", "current target");
    if (!sServerFacade->IsDistanceGreaterThan(dist, (distance + sPlayerbotAIConfig->contactDistance)))
        return false;

    // issue #98: there was no upper bound here, and PlayerbotAI::CanCastSpell deliberately lets
    // SPELL_FAILED_OUT_OF_RANGE through (for casts broken by movement). So a bot whose target was
    // out of a gap-closer's range kept re-picking it every tick and never reached melee - measured
    // 35 charge attempts and 35 heroic-leap attempts for one bot in five minutes, with no effect.
    // Bound it by the spell's own maximum range.
    uint32 const spellId = AI_VALUE2(uint32, "spell id", spell);
    SpellInfo const* spellInfo = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    if (!spellInfo)
        return false;

    float const maxRange = spellInfo->GetMaxRange(false, bot);
    return !(maxRange > 0.0f && dist > maxRange);
}

ReachSpellAction::ReachSpellAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach spell", botAI->GetRange("spell"))
{
}

bool ReachCcDutyTargetAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    distance = CcDuty::ApproachDistance(bot, target);
    return ReachTargetAction::Execute(event);
}

bool ReachCcDutyTargetAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !CcDuty::NeedsApproach(botAI))
        return false;

    distance = CcDuty::ApproachDistance(bot, target);
    return ReachTargetAction::isUseful();
}

ReachPartyMemberToHealAction::ReachPartyMemberToHealAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach party member to heal", botAI->GetRange("heal"))
{
}

std::string const ReachPartyMemberToHealAction::GetTargetName() { return "party member to heal"; }

Unit* ReachPartyMemberToHealAction::FindHurtMemberOutOfSight(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    GuidVector focusTargets;
    if (botAI->HasStrategy("focus heal targets", BOT_STATE_COMBAT))
        focusTargets = botAI->GetAiObjectContext()->GetValue<GuidVector>("focus heal targets")->Get();

    Player* worst = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() || member->FindMap() != bot->FindMap())
            continue;

        if (!focusTargets.empty() && std::find(focusTargets.begin(), focusTargets.end(), member->GetGUID()) == focusTargets.end())
            continue;

        if (member->GetHealthPct() >= sPlayerbotAIConfig->mediumHealth ||
            bot->GetDistance2d(member) > sPlayerbotAIConfig->sightDistance)
            continue;

        // In sight and in range: "party member to heal" handles this member.
        if (bot->IsWithinLOSInMap(member) && bot->GetDistance2d(member) < sPlayerbotAIConfig->healDistance * 2)
            continue;

        if (!worst || member->GetHealthPct() < worst->GetHealthPct())
            worst = member;
    }
    return worst;
}

Unit* ReachPartyMemberToHealAction::SelectHealReachTarget(PlayerbotAI* botAI, Unit* visible)
{
    // BT-117: a critical member behind a wall goes before a visible member who is not critical. A critical visible
    // member needs no search (the search traces a line of sight per hurt member).
    if (visible && visible->GetHealthPct() <= sPlayerbotAIConfig->criticalHealth)
        return visible;

    Unit* hidden = FindHurtMemberOutOfSight(botAI);
    if (hidden && (!visible || hidden->GetHealthPct() < sPlayerbotAIConfig->criticalHealth))
        return hidden;

    return visible;
}

bool ReachPartyMemberToHealAction::Execute(Event event)
{
    Unit* visible = AI_VALUE(Unit*, GetTargetName());
    Unit* target = SelectHealReachTarget(botAI, visible);
    if (target == visible)
        return target && ReachTargetAction::Execute(event);

    // BT-155: path toward hidden members; MoveNear rejects destinations outside current sight.
    return target && MoveTo(target, ReachPolicy::HiddenApproachDistance());
}

bool ReachPartyMemberToHealAction::isUseful()
{
    Unit* visible = AI_VALUE(Unit*, GetTargetName());
    Unit* target = SelectHealReachTarget(botAI, visible);
    if (target == visible)
        return target && ReachTargetAction::isUseful();

    if (botAI->HasStrategy("stay", botAI->GetState()) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return false;

    return target != nullptr;
}

ReachPartyMemberToResurrectAction::ReachPartyMemberToResurrectAction(PlayerbotAI* botAI)
    : ReachTargetAction(botAI, "reach party member to resurrect", botAI->GetRange("spell"))
{
}

std::string const ReachPartyMemberToResurrectAction::GetTargetName() { return "party member to resurrect"; }

namespace
{
// The bot is close enough, and does not walk, when it sees the body within this distance.
constexpr float RESURRECT_CLOSE_ENOUGH_DISTANCE = 20.0f;
}  // namespace

WorldObject* ReachPartyMemberToResurrectAction::ResurrectionGoal(Unit* dead) const
{
    Player* player = dead->ToPlayer();
    Corpse* corpse = player ? player->GetCorpse() : nullptr;
    return corpse && corpse->GetMapId() == bot->GetMapId() ? static_cast<WorldObject*>(corpse) : dead;
}

bool ReachPartyMemberToResurrectAction::Execute(Event event)
{
    if (bot->IsInCombat())
        return ReachTargetAction::Execute(event);

    Unit* dead = AI_VALUE(Unit*, GetTargetName());
    if (!dead)
        return false;

    WorldObject* goal = ResurrectionGoal(dead);
    return MoveTo(goal, ResurrectPolicy::ApproachDistance(bot->IsWithinLOSInMap(goal)));
}

bool ReachPartyMemberToResurrectAction::isUseful()
{
    if (bot->IsInCombat())
        return ReachTargetAction::isUseful();

    if (botAI->HasStrategy("stay", botAI->GetState()) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return false;

    Unit* dead = AI_VALUE(Unit*, GetTargetName());
    if (!dead)
        return false;

    WorldObject* goal = ResurrectionGoal(dead);
    return bot->GetDistance(goal) > RESURRECT_CLOSE_ENOUGH_DISTANCE || !bot->IsWithinLOSInMap(goal);
}
