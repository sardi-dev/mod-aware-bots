/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Base/Actions/PullActions.cpp.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 *
 * Ported from the CMaNGOS playerbots project (https://github.com/cmangos/playerbots), GPL v2,
 * with modifications for AzerothCore.
 * Original authors:
 *   David Parra Ausina (davidonete/Flekz) <davidparraausina@gmail.com> - original author
 *   Sebastiaan Keek (mostlikely4r) <sebastiaan.keek@gmail.com>
 *   Cyberium <cyberium@users.noreply.github.com>
 */

#include "PullActions.h"

#include "AttackersValue.h"
#include "BotVoice.h"
#include "MarkPullPolicy.h"
#include "MarkTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PositionValue.h"
#include "PathGenerator.h"
#include "PullSightPolicy.h"
#include "PullStrategy.h"
#include "RtiTargetValue.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
float GetPullReachDistance(Player* bot, Unit* target, PullStrategy const* strategy)
{
    if (!bot || !target || !strategy)
        return 0.0f;

    float const combatDistance = bot->GetCombatReach() + target->GetCombatReach();
    return MarkPullPolicy::PullReachDistance(strategy->GetRange(), combatDistance);
}

// BT-143: in range is not enough, the pull spell also needs line of sight. Without it every cast was refused
// (SPELL_FAILED_LINE_OF_SIGHT, about 10,000 times in seven minutes in a Shadowfang Keep run) and the tank never moved.
bool IsWithinPullRange(Player* bot, Unit* target, PullStrategy const* strategy)
{
    return bot && target && strategy && bot->GetExactDist(target) <= strategy->GetRange() &&
        bot->IsWithinLOSInMap(target);
}
} // namespace

bool PullRequestAction::Execute(Event event)
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return false;

    // #408: AC's botAI->IsTank(bot) is the static spec helper here on this fork (PlayerBotSpec::IsTank). By spec: a pull
    // starts out of combat, where the strategy check finds no "tank" strategy (it lives in the combat engine).
    if (!PlayerBotSpec::IsTank(bot, true))
        return false;

    Unit* target = GetPullTarget(event);
    if (!target || !target->IsInWorld())
    {
        // #408: PlayerbotTextMgr is not ported on this fork; this is AC's own enUS fallback text.
        botAI->TellError("You have no target");
        return false;
    }

    float const maxPullDistance = sPlayerbotAIConfig->reactDistance * 3.0f;
    if (target->GetMapId() != bot->GetMapId() || bot->GetDistance(target) > maxPullDistance)
    {
        // #408: PlayerbotTextMgr is not ported on this fork; this is AC's own enUS fallback text.
        botAI->TellError("The target is too far away");
        return false;
    }

    if (!AttackersValue::IsPossibleTarget(target, bot))
    {
        // #408: PlayerbotTextMgr is not ported on this fork; this is AC's own enUS fallback text.
        botAI->TellError("The target can't be pulled");
        return false;
    }

    if (!strategy->CanDoPullAction(target))
    {
        // #408: PlayerbotTextMgr is not ported on this fork; AC's "%action_name" fallback, inlined.
        botAI->TellError("Can't perform pull action '" + strategy->GetPullActionName() + "'");
        return false;
    }

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pullPosition = posMap["pull"];
    pullPosition.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
    posMap["pull"] = pullPosition;

    BotVoice::Say(botAI, BotVoice::Moment::BEFORE_PULL);
    strategy->RequestPull(target);
    context->GetValue<Unit*>("current target")->Set(target);
    botAI->ChangeEngine(BOT_STATE_COMBAT);
    botAI->SetNextCheckDelay(sPlayerbotAIConfig->reactDelay);
    return true;
}

Unit* PullMyTargetAction::GetPullTarget(Event event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    if (event.GetSource() == "attack anything")
        return botAI->GetCreature(event.getObject());

    return requester ? requester->GetSelectedUnit() : nullptr;
}

Unit* PullRtiTargetAction::GetPullTarget(Event /*event*/)
{
    Unit* rtiTarget = AI_VALUE(Unit*, "rti target");
    if (rtiTarget)
        return rtiTarget;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    std::string const rti = AI_VALUE(std::string, "rti");
    int32 const index = RtiTargetValue::GetRtiIndex(rti);
    if (index < 0)
        return nullptr;

    BotGuid const guid = group->GetTargetIcon(index);
    return guid.IsEmpty() ? nullptr : botAI->GetUnit(guid);
}

bool PullStartAction::Execute(Event event)
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return false;

    Unit* target = strategy->GetTarget();
    if (!target)
        return false;

    std::string const preActionName = strategy->GetPreActionName();
    if (!preActionName.empty() && !botAI->DoSpecificAction(preActionName, event, true))
        return false;

    if (Pet* pet = bot->GetPet())
    {
        Creature* creature = pet->ToCreature();
        if (creature)
        {
            strategy->SetPetReactState(creature->GetReactState());
            creature->SetReactState(REACT_PASSIVE);
        }
    }

    strategy->OnPullStarted();
    return true;
}

PullAction::PullAction(PlayerbotAI* botAI, std::string const name) : CastSpellAction(botAI, name)
{
    InitPullAction();
}

Unit* PullAction::GetTarget()
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return nullptr;

    return strategy->GetTarget();
}

NextAction** PullAction::getPrerequisites()
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    Unit* target = strategy ? strategy->GetTarget() : nullptr;
    if (!strategy || !target || IsWithinPullRange(bot, target, strategy))
        return nullptr;

    return NextAction::array(0, new NextAction("reach pull", ACTION_MOVE), nullptr);
}

bool PullAction::Execute(Event event)
{
    InitPullAction();

    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return false;

    Unit* target = strategy->GetTarget();
    if (!target || !target->IsInWorld())
        return false;

    if (target->IsInCombat())
        return false;

    if (!IsWithinPullRange(bot, target, strategy))
    {
        strategy->RequestPull(target, false);
        return false;
    }

    if (bot->isMoving())
    {
        bot->StopMoving();
        strategy->RequestPull(target, false);
        return false;
    }

    context->GetValue<Unit*>("current target")->Set(target);
    std::string const actionName = strategy->GetPullActionName();
    bool const success = botAI->DoSpecificAction(actionName, event, true);
    // One line per target and attempt window: every retry of a stuck pull would flood the log.
    constexpr time_t PULL_LOG_SECONDS = 20;
    time_t const now = time(nullptr);
    if (loggedTarget != target->GetGUID() || now - loggedAt >= PULL_LOG_SECONDS)
    {
        loggedTarget = target->GetGUID();
        loggedAt = now;
        // The pull voice line is a 12 percent roll before this action, so the attempt itself is logged: a tank that
        // announced a pull of a mark that never entered combat could not be traced (2026-11-11 Shadowfang Keep run).
        SF_LOG_INFO("playerbots", "[PULL] bot=%s target_entry=%u action=%s result=%s distance=%.1f los=%u",
                    bot->GetName().c_str(), target->GetEntry(), actionName.c_str(),
                    success ? "accepted" : "action-refused", bot->GetDistance(target),
                    uint32(bot->IsWithinLOSInMap(target)));
    }

    return success;
}

bool PullAction::isPossible()
{
    InitPullAction();

    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return false;

    Unit* target = strategy->GetTarget();
    std::string const spellName = strategy->GetSpellName();
    if (!target || !target->IsInWorld() || target->GetMapId() != bot->GetMapId() || spellName.empty())
        return false;

    return true;
}

void PullAction::InitPullAction()
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return;

    std::string const spellName = strategy->GetSpellName();
    if (spellName.empty())
        return;

    spell = spellName;

    bool isShoot = (spellName == "shoot" || spellName == "shoot bow" || spellName == "shoot gun" ||
                    spellName == "shoot crossbow" || spellName == "throw");
    range = botAI->GetRange(isShoot ? "shoot" : "spell");
}

bool PullEndAction::Execute(Event /*event*/)
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return false;

    Unit* pullTarget = strategy->GetTarget();

    if (!strategy->HasPullStarted() && !strategy->IsPullPendingToStart() && !strategy->HasTarget())
        return false;

    if (Pet* pet = bot->GetPet())
    {
        Creature* creature = pet->ToCreature();
        if (creature)
            creature->SetReactState(strategy->GetPetReactState());
    }

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    if (posMap["pull"].isSet())
        posMap.erase("pull");

    if (pullTarget && context->GetValue<Unit*>("current target")->Get() == pullTarget)
        context->GetValue<Unit*>("current target")->Set(nullptr);

    strategy->OnPullEnded();
    return true;
}

bool BodyPullMarkAction::isUseful()
{
    return !botAI->HasStrategy("stay", botAI->GetState()) && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) &&
        PlayerBotSpec::IsTank(bot, true);
}

bool BodyPullMarkAction::Execute(Event /*event*/)
{
    Unit* target = MarkedTargetReadyTrigger::CurrentMark(botAI);
    if (!target || !MarkedTargetReadyTrigger::IsBodyPullMark(bot, target))
        return false;

    if (announcedMark != target->GetGUID())
    {
        announcedMark = target->GetGUID();
        SF_LOG_INFO("playerbots", "[MARKPULL] %s walks up to the boss %s mark=%u: immune to players until it aggroes, "
                    "body pull at %.1f yd", bot->GetName().c_str(), target->GetName().c_str(), target->GetEntry(),
                    MarkPullPolicy::BODY_PULL_YARDS);
        BotVoice::Say(botAI, BotVoice::Moment::BEFORE_PULL);
    }

    bool const moved = ReachCombatTo(target, MarkPullPolicy::BODY_PULL_YARDS);
    // One line per target and window, like [PULL]: the walk, the arrival or why the walk failed.
    constexpr time_t BODY_PULL_LOG_SECONDS = 20;
    time_t const now = time(nullptr);
    if (loggedTarget != target->GetGUID() || now - loggedAt >= BODY_PULL_LOG_SECONDS)
    {
        loggedTarget = target->GetGUID();
        loggedAt = now;
        char const* const reason = ReachFailure();
        bool const arrived = !moved && std::strcmp(reason, "already within reach") == 0;
        SF_LOG_INFO("playerbots", "[PULL] bot=%s target_entry=%u action=body pull result=%s reason=%s distance=%.1f "
                    "los=%u", bot->GetName().c_str(), target->GetEntry(),
                    moved ? "walking" : (arrived ? "arrived" : "reach-failed"), moved ? "none" : reason,
                    bot->GetDistance(target), uint32(bot->IsWithinLOSInMap(target)));
    }
    return moved;
}

bool WalkToLeaderForMarkAction::isUseful()
{
    return !botAI->HasStrategy("stay", botAI->GetState()) && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) &&
        PlayerBotSpec::IsTank(bot, true);
}

bool WalkToLeaderForMarkAction::Execute(Event /*event*/)
{
    Unit* target = MarkedTargetReadyTrigger::CurrentMark(botAI);
    Player* leader = botAI->GetGroupMaster();
    if (!target || !leader || leader == bot)
        return false;

    if (announcedMark != target->GetGUID())
    {
        announcedMark = target->GetGUID();
        SF_LOG_INFO("playerbots", "[MARKPULL] %s walks to the leader %s at %.1f yd: the mark %u is out of reach and out "
                    "of sight (%.1f yd away)", bot->GetName().c_str(), leader->GetName().c_str(),
                    bot->GetDistance(leader), target->GetEntry(), bot->GetDistance(target));
    }

    if (MoveTo(leader, MarkPullPolicy::LEADER_SIDE_YARDS))
        return true;

    // A refused walk (no path, a closed door) is said now and then, so a tank that cannot get to the leader shows it.
    constexpr time_t FAILED_LOG_SECONDS = 20;
    time_t const now = time(nullptr);
    if (now - failedLoggedAt >= FAILED_LOG_SECONDS)
    {
        failedLoggedAt = now;
        SF_LOG_INFO("playerbots", "[MARKPULL] %s cannot walk to the leader %s at %.1f yd (mark %u)", bot->GetName().c_str(),
                    leader->GetName().c_str(), bot->GetDistance(leader), target->GetEntry());
    }
    return false;
}

bool StepIntoHealerSightAction::isUseful()
{
    return !botAI->HasStrategy("stay", botAI->GetState()) && !bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) &&
        PlayerBotSpec::IsTank(bot, true);
}

bool StepIntoHealerSightAction::Execute(Event /*event*/)
{
    Player* healer = MarkHealerOutOfTankSightTrigger::HiddenHealer(botAI);
    if (!healer)
        return false;

    auto& sight = botAI->markTankSight;
    if (!sight.stepLogged)
    {
        sight.stepLogged = true;
        Unit* mark = MarkedTargetReadyTrigger::CurrentMark(botAI);
        SF_LOG_INFO("playerbots", "[MARKPULL] %s steps toward the healer %s at %.1f yd: out of sight for %ld s (mark %u)",
                    bot->GetName().c_str(), healer->GetName().c_str(), bot->GetDistance(healer),
                    long(time(nullptr)) - sight.since, mark ? mark->GetEntry() : 0u);
    }
    // By path, around the corner that hides her; the trigger ends as soon as the two see each other.
    return MoveTo(healer, MarkPullPolicy::TANK_SIGHT_STEP_YARDS);
}

bool ReturnToPullPositionAction::Execute(Event /*event*/)
{
    PositionInfo pullPosition = context->GetValue<PositionMap&>("position")->Get()["pull"];
    if (!pullPosition.isSet() || pullPosition.mapId != bot->GetMapId())
        return false;

    return MoveTo(pullPosition.mapId, pullPosition.x, pullPosition.y, pullPosition.z, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true);
}

bool ReturnToPullPositionAction::isUseful()
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    Unit* target = strategy ? strategy->GetTarget() : nullptr;
    if (!strategy || !target || !target->IsInCombat())
        return false;

    PositionInfo pullPosition = context->GetValue<PositionMap&>("position")->Get()["pull"];
    return pullPosition.isSet() && pullPosition.mapId == bot->GetMapId() &&
           bot->GetDistance(pullPosition.x, pullPosition.y, pullPosition.z) > sPlayerbotAIConfig->followDistance;
}

bool ReachPullAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!target || !strategy)
        return false;

    // BT-143: a target in range but out of sight is walked to along a path until the bot sees it (at worst melee).
    float reachDistance = GetPullReachDistance(bot, target, strategy);
    bool const inSight = bot->IsWithinLOSInMap(target);
    if (!inSight)
        reachDistance = std::min(reachDistance, ATTACK_DISTANCE);
    if (ReachCombatTo(target, reachDistance))
        return true;

    // BT-333: the walk to a mob that stays unseen ended: look for a spot round it that sees it.
    if (!inSight && !bot->isMoving() && WalkToSightSpot(target))
        return true;

    // BT-181: a pull that cannot reach its mark said nothing (Deadmines run 4: three marks held 45 s in silence, on
    // another deck and behind stairs). One line per target and window, like [PULL]; a bot still walking its last move
    // gets "move refused" while it waits, which is no failure.
    constexpr time_t REACH_FAILED_LOG_SECONDS = 20;
    time_t const now = time(nullptr);
    if (!bot->isMoving() && (loggedTarget != target->GetGUID() || now - loggedAt >= REACH_FAILED_LOG_SECONDS))
    {
        loggedTarget = target->GetGUID();
        loggedAt = now;
        SF_LOG_INFO("playerbots", "[PULL] bot=%s target_entry=%u result=reach-failed reason=%s distance=%.1f los=%u "
                    "dz=%.1f reach=%.1f path=%u", bot->GetName().c_str(), target->GetEntry(), ReachFailure(),
                    bot->GetDistance(target), uint32(inSight), target->GetPositionZ() - bot->GetPositionZ(),
                    reachDistance, ReachFailurePathType());
    }
    return false;
}

bool ReachPullAction::WalkToSightSpot(Unit* target)
{
    time_t const now = time(nullptr);
    if (!PullSightPolicy::MaySearch(long(lastSightSearch), long(now)))
        return false;
    lastSightSearch = now;

    Map* map = bot->GetMap();
    if (!map)
        return false;

    struct Spot
    {
        float distance;
        float x, y, z;
    };
    std::vector<Spot> spots;
    for (unsigned i = 0; i < PullSightPolicy::SPOT_COUNT; ++i)
    {
        float dx = 0.0f, dy = 0.0f;
        PullSightPolicy::Offset(i, dx, dy);
        float const x = target->GetPositionX() + dx;
        float const y = target->GetPositionY() + dy;
        float const z = map->GetHeight(x, y, target->GetPositionZ() + PullSightPolicy::MAX_SPOT_DZ);
        if (z <= INVALID_HEIGHT || !PullSightPolicy::SameFloor(z, target->GetPositionZ()))
            continue;
        // Sight from the spot at a standing player's eye height.
        constexpr float EYE_HEIGHT = 1.5f;
        if (!target->IsWithinLOS(x, y, z + EYE_HEIGHT))
            continue;
        spots.push_back({ bot->GetExactDist(x, y, z), x, y, z });
    }
    std::sort(spots.begin(), spots.end(), [](Spot const& a, Spot const& b) { return a.distance < b.distance; });

    unsigned searches = 0;
    for (Spot const& spot : spots)
    {
        if (searches++ >= PullSightPolicy::PATH_SEARCHES)
            break;
        PathGenerator path(bot);
        path.CalculatePath(spot.x, spot.y, spot.z, false);
        if (!(path.GetPathType() & PATHFIND_NORMAL))
            continue;
        float length = 0.0f;
        Movement::PointsArray const& points = path.GetPath();
        for (size_t p = 1; p < points.size(); ++p)
            length += (points[p] - points[p - 1]).length();
        if (!PullSightPolicy::PathIsShort(length, spot.distance))
            continue;

        SF_LOG_INFO("playerbots", "[PULL] bot=%s target_entry=%u result=sight-spot walks %.1f yd to a spot %.1f yd from the "
                    "mark that sees it (no sight from %.1f yd)", bot->GetName().c_str(), target->GetEntry(), length,
                    target->GetExactDist(spot.x, spot.y, spot.z), bot->GetDistance(target));
        return MoveTo(target->GetMapId(), spot.x, spot.y, spot.z, false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT, true);
    }
    return false;
}

bool ReachPullAction::isUseful()
{
    if (botAI->HasStrategy("stay", botAI->GetState()))
        return false;

    if (bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != nullptr)
        return false;

    PullStrategy* strategy = PullStrategy::Get(botAI);
    Unit* target = strategy ? strategy->GetTarget() : nullptr;
    return target && !IsWithinPullRange(bot, target, strategy);
}

Unit* ReachPullAction::GetTarget()
{
    PullStrategy* strategy = PullStrategy::Get(botAI);
    if (!strategy)
        return nullptr;

    return strategy->GetTarget();
}
