/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_WSGPLAN_H
#define _PLAYERBOT_WSGPLAN_H

// BT-153: the Warsong Gulch team plan (docs/bg-sheets/warsong-gulch.md) between the pure rules (Utils/WsgPlanLogic.h)
// and the game. One record per match holds each team's plan, the role of every player, the flag state, the strike
// group state, the pending call-outs and the human calls. A bot reads it through Decide(); the record is rebuilt from
// the battleground at most every two seconds by whichever bot asks first, on the map thread of the match, so there is
// no extra scan per tick.
//
// BT-470: the plan reads the map through Utils/BgFlagMap.h (sides, middle, clock) and the flag state through
// strategy/bg/BgFlagView.h, so it names no core battleground class. The maps it plays are the ones MapOf() in WsgPlan.cpp
// lists; Enabled() says whether a battleground is one of them.

#include "Define.h"
#include "WsgPlanLogic.h"

#include <string>

class Battleground;
class Player;
class PlayerbotAI;
class Unit;

namespace WsgPlan
{
    // What a bot walks to and why.
    struct Objective
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        WsgLogic::Kind kind = WsgLogic::Kind::HOLD_MID;
        WsgLogic::Role role = WsgLogic::Role::NONE;
        // Escort and cover objectives follow this player (the team flag carrier), 0 for none.
        uint64 followGuid = 0;
    };

    // A battleground whose map the plan plays (the Random Battleground queue keeps its own type id, the map id is
    // reliable).
    bool Enabled(Battleground* bg);

    // The bot's decision: the best option of WsgLogic::Options that has a position. False when the plan cannot say
    // (the caller then runs its older code). strategyAlliance and strategyHorde are the teams' drawn plans
    // (WSBotStrategy numbers) from BGTactics::GetBotStrategyForTeam. carrying is the bot's own flag aura, read live: the
    // record is up to two seconds old.
    bool Decide(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde, bool carrying, Objective& out);

    // The plan state changed since this bot last chose, and the bot's tier lag has passed (core rule: follow the plan
    // state when it changes, late for casual bots).
    bool PlanChanged(Battleground* bg, Player* bot, uint8 strategyAlliance, uint8 strategyHorde);

    // The start spot for the preparation: 0 tunnel side, 1 graveyard side, 2 the spawn (inside the flag room).
    uint32 StartSpot(Battleground* bg, Player* bot);

    // One of the two nearest to the enemy carrier (the bots that focus it, G5).
    bool IsEfcHunter(Battleground* bg, Player* bot);

    // Twin Peaks (BT-705): this bot is on the lane detour the plan ordered after a kill in the locked middle fight (the plan's
    // member flag viaLane), alive, so the detour outranks the fight it stands in. False on every other map.
    bool DetourDue(Battleground* bg, Player* bot);
    bool StrikeRunDue(Battleground* bg, Player* bot);

    // A strike or mid bot may click the enemy flag on its stand: the flag takers, or anybody when no taker or backup is
    // within 50 yd of the stand (casual bots always may: they walk to the stand alone on purpose).
    bool MayTakeEnemyBaseFlag(Battleground* bg, Player* bot);

    // For the escort: it escorts a carrier (strike roles, and the healers that follow it) and stays this close (yards),
    // 0 when it is not an escort.
    float EscortDistance(Battleground* bg, Player* bot);

    // One of the healers that follow our flag carrier (owner rule 2026-11-17): the nearest two alive healer bots, whatever
    // their role.
    bool IsHealerFollower(Battleground* bg, Player* bot);

    // The carrier's escorts peel enemies off it: the unit to engage, or nullptr.
    Unit* PeelTarget(Battleground* bg, Player* bot, Unit* carrier);

    // What stands around a flag carrier, read from the plan's roster (no extra scan; the roster is up to two seconds old):
    // enemy players within WsgLogic::CARRIER_ALONE_RADIUS, bots of the carrier's team within the escort distance, and the
    // healers that follow it with the nearest one's distance (-1 for none). False when the match has no record yet.
    struct Surroundings
    {
        uint32 enemiesNear = 0;
        uint32 escortsNear = 0;
        uint32 healerFollowers = 0;
        float nearestHealer = -1.0f;
    };
    bool SurroundingsOf(Battleground* bg, Player* carrier, Surroundings& out);

    // The info-level decision lines (owner 2026-11-17): true when this bot may write a line now. Only in a match with a human
    // in it, at most one line per 10 s per bot and channel (0 the objective, 1 the carrier's choice), and only when the
    // signature differs from the last line written. Commits the line; playSeconds is the time since the gates opened.
    bool DecisionLogDue(Battleground* bg, Player* bot, uint32 channel, uint32 signature, uint32& playSeconds);

    // A dynamic objective (hunt, escort, cover) has a moving target: the up-to-date position, false when the bot's
    // current objective is not one.
    bool RefreshDynamic(Battleground* bg, Player* bot, Objective& out);

    // The carrier fights with the speed buff in reach for the strike group of a low bracket (extra yards on the buff
    // search), 0 when the bot has no such job.
    float SpeedBuffRange(Battleground* bg, Player* bot);

    // Percent points a healer takes off a target's health when ordering heals: the team's flag carrier first (G6).
    uint32 HealPriority(Player* healer, Player* target);

    // BT-386: the bot stands at its own main graveyard and its objective is far away: `hop` is the same objective
    // moved to the bot's tunnel exit. The caller (BGTactics) uses it only when the objective itself has no path from
    // here and the exit has one; the plan knows nothing of the navmesh.
    bool GraveyardHop(Battleground* bg, Player* bot, Objective const& objective, bool carrying, Objective& hop);

    // The graveyard rule: true when this bot has stayed at the enemy main graveyard too long and its objective must
    // not lead back to it.
    bool MustLeaveGraveyard(Battleground* bg, Player* bot);

    // Said by the bot when its turn comes (after the tier's delay); true when a line went out.
    bool DeliverCalls(PlayerbotAI* botAI);

    // The match is over: drop its record.
    void Forget(uint32 instanceId);
}

#endif
