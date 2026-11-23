/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_DUNGEONUNSTUCK_H
#define _PLAYERBOT_DUNGEONUNSTUCK_H

#include "LedgeWalkOffPolicy.h"
#include "MovementActions.h"
#include "Trigger.h"

#include <ctime>

class Player;

// BT-222 (owner 2026-11-16): the last-resort shortcuts (this teleport, the Tol Barad graveyard island, the Oculus drake
// flight) run only while no real player can see the bot (the BT-019 rule): a real player within 100 yards with line of
// sight to it.
bool BotSeenByRealPlayer(Player* bot);

// BT-222: one "[LASTRESORT]" info line per use, with what was done and where from and to, so the navmesh spots behind
// them can be fixed later.
void LogLastResort(Player* bot, char const* what, float toX, float toY, float toZ);

// BT-119: inside an instance nothing recovered a bot whose way to its player kept failing: travel to group skips
// instances on purpose and the combat stuck recovery is off for a human master, so the bot stayed behind. Owner choice
// (2026-11-11): walk first, then a side step, and only as a last resort a teleport to the player while no human can
// see the bot leave, with one party line.
class FarFromMasterInInstanceTrigger : public Trigger
{
public:
    FarFromMasterInInstanceTrigger(PlayerbotAI* botAI) : Trigger(botAI, "far from master in instance", 5000) {}

    bool IsActive() override;
};

class DungeonUnstuckAction : public MovementAction
{
public:
    DungeonUnstuckAction(PlayerbotAI* botAI) : MovementAction(botAI, "dungeon unstuck") {}

    bool Execute(Event event) override;
    bool isUseful() override;

private:
    void Reset(float distanceToMaster, time_t now);
    // BT-388: the "I'm stuck" line, once per episode.
    void SayStuck();
    // BT-388 (LedgeWalkOffPolicy.h): a jump to the nearest landing with a whole path to the player; false when none
    // qualifies (the caller keeps trying to walk).
    bool TryWalkOff(Player* master, time_t now);

    // Progress is measured as getting closer to the player (a side step does not count). The state is dropped when
    // the action has not run for a while, so an old timer never jumps straight to a later step.
    float bestDistance = 0.0f;
    time_t lastProgress = 0;
    time_t lastRun = 0;
    uint64 masterGuid = 0;
    time_t lastAnnounce = 0;
    time_t lastLogged = 0;
    // BT-388: what this stuck episode has said and tried; dropped with the rest of the state when the action goes stale.
    LedgeWalkOffPolicy::Episode episode;
};

#endif
