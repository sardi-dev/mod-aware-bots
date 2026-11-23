/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "Aq20Actions.h"
#include "Aq20Utils.h"

// AC Ai/Raid/Aq20/Aq20Actions.cpp:1-52. 5.4.8 adaptations (checked against the core's boss_ossirian.cpp):
//  - The crystal is clicked with GameObject::Use, which runs the script's OnGossipHello (go_ossirian_crystal) and that
//    triggers the weakness. AC sends CMSG_GAMEOBJ_USE and then the report-use packet; this core's script needs only the
//    gossip hello, and the report path adds nothing here.
//  - go_ossirian_crystal only acts when Ossirian is within 30 yards of the one who clicks (FindNearestCreature(.., 30))
//    and the boss is in progress, otherwise Use falls through to the generic goober path, which would burn the crystal
//    (animation, "in use" flag) without removing the buff. So the click waits until the bot is that close to him.
// 5.4.8 has no HasGameObjectFlag: HasFlag(GAMEOBJECT_FIELD_FLAGS, ..).

namespace
{
    // The script's own reach (boss_ossirian.cpp go_ossirian_crystal: FindNearestCreature(NPC_OSSIRIAN, 30.0f)) less a margin.
    float const OSSIRIAN_CLICK_RANGE = 28.0f;
}

bool Aq20UseCrystalAction::Execute(Event /*event*/)
{
    if (Unit* boss = AI_VALUE2(Unit*, "find target", "ossirian the unscarred"))
    {
        if (GameObject* crystal = RaidAq20Utils::GetNearestCrystal(boss))
        {
            float botDist = bot->GetDistance(crystal);
            if (botDist > INTERACTION_DISTANCE)
                return MoveTo(bot->GetMapId(),
                    crystal->GetPositionX() + frand(-3.5f, 3.5f),
                    crystal->GetPositionY() + frand(-3.5f, 3.5f),
                    crystal->GetPositionZ());

            // if we're already in range just wait here until it's time to activate crystal
            // (AC's MovementAction::SetNextMovementDelay does not exist here; the whole check is delayed, which is the same wait)
            botAI->SetNextCheckDelay(500);

            // don't activate crystal if boss too far or its already been activated
            if (boss->GetDistance(crystal) > 25.0f || bot->GetDistance(boss) > OSSIRIAN_CLICK_RANGE ||
                crystal->HasFlag(GAMEOBJECT_FIELD_FLAGS, GO_FLAG_IN_USE))
                return false;

            // don't activate crystal if boss doesn't have buff yet AND isn't going to have it soon
            // (though ideally bot should activate it ~5 seconds early due to time it takes for
            // crystal to activate and remove buff)
            if (!RaidAq20Utils::IsOssirianBuffActive(boss) &&
                RaidAq20Utils::GetOssirianDebuffTimeRemaining(boss) > 5000)
                return false;

            // the script removes the buff, puts on one of the five weaknesses and spawns the next crystal
            crystal->Use(bot);
            return true;
        }
    }
    return false;
}
