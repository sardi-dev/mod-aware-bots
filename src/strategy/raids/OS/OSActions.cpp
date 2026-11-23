/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "OSTriggers.h"
#include "OsTsunamiLogic.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

namespace
{
    // The 5.4.8 script's wave lanes (boss_sartharion.cpp FlameRight1Spawn..FlameLeft2Spawn): the three "right" waves
    // start at x 3200 and run towards +X, the two "left" ones start at x 3289 and run towards -X.
    float constexpr TSUNAMI_LANES_TOWARDS_PLUS_X[] = {573.211f, 532.211f, 491.211f};
    float constexpr TSUNAMI_LANES_TOWARDS_MINUS_X[] = {552.711f, 511.711f};
    float constexpr LANE_MATCH = 2.0f;

    bool InAnyLane(float y, float const* lanes, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            if (std::fabs(y - lanes[i]) < LANE_MATCH)
                return true;
        return false;
    }

    bool RunsTowardsPlusX(Unit* wave, Player* bot)
    {
        if (InAnyLane(wave->GetPositionY(), TSUNAMI_LANES_TOWARDS_PLUS_X, std::size(TSUNAMI_LANES_TOWARDS_PLUS_X)))
            return true;
        if (InAnyLane(wave->GetPositionY(), TSUNAMI_LANES_TOWARDS_MINUS_X, std::size(TSUNAMI_LANES_TOWARDS_MINUS_X)))
            return false;
        // An unknown lane: treat the wave as coming at the bot
        return wave->GetPositionX() < bot->GetPositionX();
    }
}

// AC Ai/Raid/OS/OSActions.cpp:9. 5.4.8: AC PlayerbotAI::IsMainTank -> PlayerBotSpec::IsMainTank; ObjectGuid
// compares -> the core's uint64 guids.
bool SartharionTankPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss)
        return false;

    Unit* shadron = nullptr;
    Unit* tenebron = nullptr;
    Unit* vesperon = nullptr;

    // Detect incoming drakes before they are on aggro table
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (BotGuid const& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit)
            continue;

        switch (unit->GetEntry())
        {
            case NPC_SHADRON:
                shadron = unit;
                break;
            case NPC_TENEBRON:
                tenebron = unit;
                break;
            case NPC_VESPERON:
                vesperon = unit;
                break;
            default:
                break;
        }
    }

    // Adjustable, this is the acceptable distance to stack point that will be accepted as "safe"
    float constexpr looseDistance = 12.0f;

    if (PlayerBotSpec::IsMainTank(bot))
    {
        if (bot->GetExactDist2d(SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second) > looseDistance)
            return MoveTo(OS_MAP_ID, SARTHARION_MAINTANK_POSITION.first, SARTHARION_MAINTANK_POSITION.second,
                          bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    // Offtank grab drakes
    else if (shadron || tenebron || vesperon)
    {
        float constexpr triggerDistance = 100.0f;
        Unit* currentTarget = AI_VALUE(Unit*, "current target");
        // Prioritise threat before positioning
        for (Unit* drake : {tenebron, shadron, vesperon})
        {
            if (drake && bot->GetExactDist2d(drake) < triggerDistance && drake->GetTarget() != bot->GetGUID() &&
                currentTarget != drake)
                return Attack(drake);
        }

        bool const drakeInCombat = (tenebron && bot->GetExactDist2d(tenebron) < triggerDistance) ||
                                   (shadron && bot->GetExactDist2d(shadron) < triggerDistance) ||
                                   (vesperon && bot->GetExactDist2d(vesperon) < triggerDistance);
        // Offtank has threat on drakes, check positioning
        if (drakeInCombat &&
            bot->GetExactDist2d(SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second) > looseDistance)
            return MoveTo(OS_MAP_ID, SARTHARION_OFFTANK_POSITION.first, SARTHARION_OFFTANK_POSITION.second,
                          bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

// AC Ai/Raid/OS/OSActions.cpp:90
bool AvoidTwilightFissureAction::Execute(Event /*event*/)
{
    float constexpr radius = 5.0f;

    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (BotGuid const& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == NPC_TWILIGHT_FISSURE)
        {
            float const currentDistance = bot->GetDistance2d(unit);
            if (currentDistance < radius)
                return MoveAway(unit, radius - currentDistance);
        }
    }
    return false;
}

// AC Ai/Raid/OS/OSActions.cpp:107, rewritten for the 5.4.8 script's waves: AC tells its left and right waves apart by
// their Y and walks to fixed safe lines between them; here every wave that has not passed the bot is a 7 yard lane,
// and a bot inside one steps along Y to the nearer free side (OsTsunamiLogic.h).
bool AvoidFlameTsunamiAction::Execute(Event /*event*/)
{
    std::vector<float> lanes;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (BotGuid const& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit || unit->GetEntry() != NPC_FLAME_TSUNAMI)
            continue;

        if (OsTsunami::WavePassed(unit->GetPositionX(), bot->GetPositionX(), RunsTowardsPlusX(unit, bot)))
            continue;

        lanes.push_back(unit->GetPositionY());
    }

    float safeY = 0.0f;
    if (!OsTsunami::DodgeY(bot->GetPositionY(), lanes, safeY))
        return false;

    return MoveTo(OS_MAP_ID, bot->GetPositionX(), safeY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

// AC Ai/Raid/OS/OSActions.cpp:160
bool SartharionAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* sartharion = AI_VALUE2(Unit*, "find target", "sartharion");
    Unit* shadron = AI_VALUE2(Unit*, "find target", "shadron");
    Unit* tenebron = AI_VALUE2(Unit*, "find target", "tenebron");
    Unit* vesperon = AI_VALUE2(Unit*, "find target", "vesperon");
    Unit* acolyte = AI_VALUE2(Unit*, "find target", "acolyte of shadron");

    Unit* target = nullptr;

    if (acolyte)
        target = acolyte;
    else if (vesperon)
        target = vesperon;
    else if (tenebron)
        target = tenebron;
    else if (shadron)
        target = shadron;
    else if (sartharion)
        target = sartharion;

    if (target && AI_VALUE(Unit*, "current target") != target)
        return Attack(target);

    return false;
}

// AC Ai/Raid/OS/OSActions.cpp:187. 5.4.8: AC builds a CMSG_GAMEOBJ_USE packet for the session handler; the bot clicks
// the portal like UseAction::UseGameObject does (strategy/actions/UseAction.cpp), only once it stands within the
// core's interaction distance (Player::GetGameObjectIfCanInteractWith).
bool EnterTwilightPortalAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sartharion");
    if (!boss || !boss->HasAura(SPELL_GIFT_OF_TWILIGHT_FIRE))
        return false;

    GameObject* portal = bot->FindNearestGameObject(GO_TWILIGHT_PORTAL, 100.0f);
    if (!portal || !portal->isSpawned())
        return false;

    if (!portal->IsWithinDistInMap(bot, INTERACTION_DISTANCE))
        return MoveTo(portal, std::max(INTERACTION_DISTANCE - 1.0f, 0.0f));

    portal->Use(bot);
    return true;
}
