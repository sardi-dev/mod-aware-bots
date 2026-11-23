/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/World/Rpg/Action/NewRpgDoGather.h) into this module.
 */

#ifndef PLAYERBOT_NEWRPGDOGATHER_H
#define PLAYERBOT_NEWRPGDOGATHER_H

#include "NewRpgBaseAction.h"

// The "do gather" status: a bot with Mining or Herbalism walks to the nearest node it can harvest, gathers it with the
// normal gameobject use and loot pipeline (so the skill rises the way a player's does), and goes on to the next. Skinning
// needs no status: the loot pipeline skins the bot's own kills when it has the skill and a knife.
class NewRpgDoGatherAction : public NewRpgBaseAction
{
public:
    NewRpgDoGatherAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg do gather") {}
    bool Execute(Event event) override;

protected:
    // Release the loot pipeline's target if it points at the given spawn. Called whenever the gather action abandons a
    // node, so a half-done harvest cannot leave a stale loot target behind (which would gate off loot target replacement
    // and wedge all later harvesting).
    void ClearLootTargetForNode(uint32 spawnId);

    // Drop the current node target (releasing the loot target with it) and reset the per-node state; markVisited writes
    // the spawn off for this gather session.
    void AbandonNode(NewRpgInfo::DoGather& data, bool markVisited = false);

    // Max time waiting at a reached node for the loot pipeline to harvest it before giving up and moving on.
    const uint32 nodeStayTime = 30 * IN_MILLISECONDS;
    // Cadence for the periodic target liveness and switch checks.
    const uint32 passiveCheckInterval = 2 * IN_MILLISECONDS;
    // A live node passed within this range takes priority over the current (possibly unverified) target.
    const float nodeSwitchDistance = 80.0f;
    // Required distance gain before switching to a live node that is not in immediate pickup range.
    const float switchMinGain = 20.0f;
    // A live node this close is always picked up (if strictly closer than the current target): skipping it reads as
    // "ran past a herb".
    const float nodePickupDistance = 30.0f;
};

#endif
