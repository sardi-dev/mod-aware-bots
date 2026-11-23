/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// Generic battleground play - BT-431. A map with no tactics of its own (one a later core adds; Strand of the Ancients
// while its tactics are switched off) used to leave its bots without the "battleground" strategy, or in a reset loop, so
// they stood at the spawn for the whole match. They now take a goal from the three the start positions give (the enemy
// team's start, the midfield, the own start), walk there on the navmesh and fight what they meet, the "pvp" strategy doing
// the fighting. The choice is BgGenericPlay (a scored list of options by the bot's role); the stored goal is read by
// moveToObjective like every other map's.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "BgGenericPlay.h"
#include "Battleground.h"
#include "PositionValue.h"
#include "Playerbots.h"

bool BGTactics::selectObjectiveGeneric(Battleground* bg)
{
    float ownX = 0.0f, ownY = 0.0f, ownZ = 0.0f, ownO = 0.0f;
    float enemyX = 0.0f, enemyY = 0.0f, enemyZ = 0.0f, enemyO = 0.0f;

    // The core's team numbers (ALLIANCE 469, HORDE 67), not TeamIds: see the note at the WAIT_JOIN guard in Execute.
    uint32 const ownTeam = bot->GetBGTeam();
    uint32 const enemyTeam = ownTeam == ALLIANCE ? uint32(HORDE) : uint32(ALLIANCE);
    bg->GetTeamStartLoc(ownTeam, ownX, ownY, ownZ, ownO);
    bg->GetTeamStartLoc(enemyTeam, enemyX, enemyY, enemyZ, enemyO);

    // A template without start locations (0, 0, 0) says nothing about the map: no objective, the bot keeps what it has.
    if (!ownX && !ownY && !enemyX && !enemyY)
        return false;

    float const midX = (ownX + enemyX) * 0.5f;
    float const midY = (ownY + enemyY) * 0.5f;
    float const midZ = (ownZ + enemyZ) * 0.5f;

    uint32 const role = context->GetValue<uint32>("bg role")->Get();
    auto const options = BgGenericPlay::Options(role, bot->GetDistance(enemyX, enemyY, enemyZ),
                                                bot->GetDistance(midX, midY, midZ), bot->GetDistance(ownX, ownY, ownZ));
    BgGenericPlay::Goal const goal = BgGenericPlay::Choose(options);

    float x = midX, y = midY, z = midZ;
    if (goal == BgGenericPlay::Goal::PUSH_ENEMY_START)
    {
        x = enemyX;
        y = enemyY;
        z = enemyZ;
    }
    else if (goal == BgGenericPlay::Goal::GUARD_OWN_START)
    {
        x = ownX;
        y = ownY;
        z = ownZ;
    }

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];

    // Already there: stay where it stands (the same rule the Isle of Conquest guard uses), so the bot is not sent to a new
    // random spot of the goal every time it looks.
    if (BgGenericPlay::Arrived(bot->GetDistance(x, y, z)))
        pos.Set(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetMapId());
    else
        pos.Set(x + frand(-6.0f, 6.0f), y + frand(-6.0f, 6.0f), z, bot->GetMapId());
    posMap["bg objective"] = pos;
    return true;
}
