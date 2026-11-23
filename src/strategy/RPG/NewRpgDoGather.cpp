/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/World/Rpg/Action/NewRpgDoGather.cpp) into this module.
 * Changes: the spawn id is the gameobject's DB table guid, the node index lives in GatherNodeMgr (cells, not zones), and
 * the rpg info is reached through GetRpgInfo().
 */

#include "NewRpgDoGather.h"

#include "GameObject.h"
#include "GatherNodeMgr.h"
#include "LootObjectStack.h"
#include "NewRpgInfo.h"
#include "ObjectDefines.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

// Base-rank gather spells, the same ids OpenLootAction::DoLoot casts.
enum GatherSpells
{
    HERB_GATHERING = 2366,
    MINING = 2575,
};

void NewRpgDoGatherAction::ClearLootTargetForNode(uint32 spawnId)
{
    LootObject lootTarget = AI_VALUE(LootObject, "loot target");
    if (!lootTarget.guid.IsGameObject())
        return;

    // Resolve the object and compare its spawn id; a target that no longer resolves (despawned) is dead weight either
    // way: release it too.
    GameObject* go = botAI->GetGameObject(lootTarget.guid);
    if (go && go->GetDBTableGUIDLow() != spawnId)
        return;

    context->GetValue<LootObject>("loot target")->Set(LootObject());
}

void NewRpgDoGatherAction::AbandonNode(NewRpgInfo::DoGather& data, bool markVisited)
{
    ClearLootTargetForNode(data.nodeSpawnId);
    if (markVisited)
        data.visited.insert(data.nodeSpawnId);
    data.nodeSpawnId = 0;
    data.nodeEntry = 0;
    data.lastReach = 0;
}

bool NewRpgDoGatherAction::Execute(Event /*event*/)
{
    NewRpgInfo& info = botAI->GetRpgInfo();
    auto* dataPtr = std::get_if<NewRpgInfo::DoGather>(&info.data);
    if (!dataPtr)
        return false;
    auto& data = *dataPtr;

    // A dead bot cannot interact with nodes: standing on them just burns the timeout and wrongly writes live nodes into
    // `visited`. Leave the state untouched and resume after the revive.
    if (!bot->IsAlive())
        return false;

    // (Nearly) full bags: the loot pipeline refuses to store new items above this fill level (StoreLootAction), so every
    // harvest would open the node, store nothing and stall until the timeout. Stop and let the sell and repair logic
    // catch up.
    if (AI_VALUE(uint8, "bag space") > 80)
    {
        info.ChangeToIdle();
        return true;
    }

    // Teleported or summoned away mid-route: drop the target and reselect. Checked before anything that uses
    // data.nodePos, which holds coordinates from the old map.
    if (data.nodeSpawnId && bot->GetMapId() != data.nodePos.GetMapId())
    {
        AbandonNode(data);
        return true;
    }

    bool const runPeriodicChecks =
        !data.lastPassiveCheck || GetMSTimeDiffToNow(data.lastPassiveCheck) > passiveCheckInterval;
    if (runPeriodicChecks)
        data.lastPassiveCheck = getMSTime();

    // Periodically drop a target that is verifiably empty (grid loaded, no live object) instead of walking all the way
    // to it. Deliberately not added to `visited`: nodes respawn, and a permanently poisoned spawn point would make the bot
    // run past the herb standing on it later.
    if (runPeriodicChecks && data.nodeSpawnId)
    {
        GatherNodeSpawn probe;
        probe.spawnId = data.nodeSpawnId;
        probe.entry = data.nodeEntry;
        probe.pos = data.nodePos;
        if (GatherNodeMgr::IsVerifiablyDown(bot->GetMap(), probe))
        {
            AbandonNode(data);
            return true;
        }
    }

    // Opportunistic switch (same cadence): take a verifiably live node over the current, possibly unverified target when
    // it is either meaningfully closer (switchMinGain) or right next to the path (nodePickupDistance). Both clauses need
    // the live node to be strictly closer than the current target. Closeness alone does not prevent ping-ponging (bot
    // movement flips which node is closer), so the node last switched away from may not steal the target back until the
    // current target resolves.
    if (runPeriodicChecks && data.nodeSpawnId)
    {
        float const distToTarget = bot->GetExactDist(&data.nodePos);
        GatherNodeSpawn const* live = sGatherNodeMgr.GetNearestLiveNode(bot, data.visited, nodeSwitchDistance);
        if (live && live->spawnId != data.nodeSpawnId && live->spawnId != data.lastSwitchedFrom)
        {
            float const distToLive = bot->GetExactDist(&live->pos);
            bool const muchCloser = distToLive + switchMinGain < distToTarget;
            bool const passingBy = distToLive < nodePickupDistance && distToLive < distToTarget;
            if (muchCloser || passingBy)
            {
                ClearLootTargetForNode(data.nodeSpawnId);
                data.lastSwitchedFrom = data.nodeSpawnId;
                data.nodeSpawnId = live->spawnId;
                data.nodeEntry = live->entry;
                data.nodePos = live->pos;
                data.lastReach = 0;
                return true;
            }
        }
    }

    if (!data.nodeSpawnId)
    {
        // The previous target was resolved (harvested or written off), so the node once switched away from becomes fair
        // game again.
        data.lastSwitchedFrom = 0;

        // Prefer a node already seen to be up over an unverified pick.
        GatherNodeSpawn const* node = sGatherNodeMgr.GetNearestLiveNode(bot, data.visited, nodeSwitchDistance);
        if (!node)
            node = sGatherNodeMgr.GetNextNode(bot, data.visited);
        if (!node)
        {
            // Nothing (left) to gather around here.
            info.ChangeToIdle();
            return true;
        }

        data.nodeSpawnId = node->spawnId;
        data.nodeEntry = node->entry;
        data.nodePos = node->pos;
        data.lastReach = 0;
        return true;
    }

    // 10 yd keeps the bot inside lootDistance (default 15), so the loot pipeline's "loot available" trigger can fire on
    // the node.
    if (bot->GetExactDist(&data.nodePos) > 10.0f)
        return MoveFarTo(data.nodePos);

    if (!data.lastReach)
        data.lastReach = getMSTime();

    GameObject* go = GatherNodeMgr::FindLiveNode(bot->GetMap(), data.nodeEntry, data.nodeSpawnId);
    if (!go)
    {
        // Harvested (by us or someone else) or despawned: move on. Not added to `visited`: the spawn point becomes a valid
        // target again once it respawns.
        AbandonNode(data);
        return true;
    }

    // A harvest cast is already underway: do nothing that could clobber it (movement cancels the cast).
    if (bot->GetCurrentSpell(CURRENT_GENERIC_SPELL) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    // Write-off cases that do go into `visited` (this bot cannot harvest the node, now or later this session): no access
    // to the loot at all (quest-gated chests the index filter misses), and the nodeStayTime timeout, when the loot
    // pipeline failed to harvest a live node in time (without the write-off the bot would reselect it forever).
    LootObject lootObj(bot, go->GetGUID());
    bool const timedOut = GetMSTimeDiffToNow(data.lastReach) > nodeStayTime;
    if (lootObj.IsEmpty() || timedOut)
    {
        AbandonNode(data, /*markVisited*/ true);
        return true;
    }

    // Keep the loot pipeline pointed at our node, not a stale previous one: otherwise its move-to-loot (higher relevance)
    // pulls the bot toward the old target while our approach pulls toward this one. Do not override a valid target the
    // pipeline is already busy with in loot range (a corpse right next to us).
    LootObject currentTarget = AI_VALUE(LootObject, "loot target");
    bool const keepCurrent = !currentTarget.IsEmpty() && currentTarget.IsLootPossible(bot) &&
                             AI_VALUE2(float, "distance", "loot target") <= sPlayerbotAIConfig->lootDistance;
    if (currentTarget.guid != go->GetGUID() && !keepCurrent)
        context->GetValue<LootObject>("loot target")->Set(LootObject(bot, go->GetGUID()));

    // Drive the close approach ourselves when out of interaction range. The loot pipeline's own move-to-loot cannot: it is
    // gated on IsLootPossible (whose Z check fails for nodes well above or below the bot: ledges, underwater kelp), and it
    // approaches via line-of-sight sampled points, which find nothing when the node sits behind an obstacle. A validated
    // path climbs, descends and routes around geometry to the node. This only runs when the pipeline's higher-relevance
    // approach already failed to move this tick; a node with no reachable path is retired by the timeout above.
    if (!lootObj.IsLootPossible(bot) || bot->GetDistance(go) > INTERACTION_DISTANCE - 2.0f)
    {
        if (bot->isMoving())
            return false;
        return MoveTo(bot->GetMapId(), data.nodePos.GetPositionX(), data.nodePos.GetPositionY(),
                      data.nodePos.GetPositionZ(), false, false, false, false);
    }

    // The loot pipeline only dismounts, it never leaves shapeshift form. Drop the form only when it blocks the gather cast
    // (per the spell's own stance rules: Herb Gathering is allowed in flight forms, Travel Form blocks it). The bot is in
    // interaction range here, so it keeps its form speed while approaching. No-op for non-druids.
    if (bot->GetShapeshiftForm() != FORM_NONE)
    {
        uint32 const gatherSpellId = lootObj.skillId == SKILL_MINING ? MINING : HERB_GATHERING;
        SpellInfo const* gatherSpell = sSpellMgr->GetSpellInfo(gatherSpellId);
        if (gatherSpell && gatherSpell->CheckShapeshift(bot->GetShapeshiftForm()) != SPELL_CAST_OK)
        {
            bot->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);
            return true;
        }
    }

    // In range with the pipeline pointed at our node: it casts the gather spell and loots. While it works, Add() returns
    // false and this idles.
    return AI_VALUE(LootObjectStack*, "available loot")->Add(go->GetGUID());
}
