/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * #201 (#612 #614 #615): duel spots. New for the Legends of Azeroth Pandaria fork (AC has no equivalent; the shape
 * follows AC's NewRpgOutdoorPvP.* - a status, a NewRpgBaseAction subclass and a status trigger).
 *
 * Contains the "new rpg duel spot" action and the DuelSpotMgr singleton (spot selection, occupancy registry,
 * seeding hand-off, real-player cooldown, harness overrides, and the seeder driven from RandomPlayerbotMgr).
 */

#ifndef _PLAYERBOT_NEWRPGDUELSPOT_H
#define _PLAYERBOT_NEWRPGDUELSPOT_H

#include "NewRpgBaseAction.h"
#include "NewRpgInfo.h"
#include "BotGuid.h"
#include "PlayerbotAIConfig.h"
#include "SharedDefines.h"

#include <mutex>
#include <string>
#include <unordered_map>

class Player;
class WorldObject;

/// Thread-safe (one std::mutex around every map). The status action runs on map-update threads, Update() on the
/// world thread.
class DuelSpotMgr
{
public:
    static DuelSpotMgr* instance()
    {
        static DuelSpotMgr instance;
        return &instance;
    }

    /// Nearest enabled spot for this bot (same map, within DuelSpotSelectDistance, team and level match, not in an
    /// instance / BG / arena / queue / group with a real player). -1 if none.
    int32 SelectSpotFor(Player* bot);
    /// Same map and within radius + extra of the spot's anchor.
    bool InSpot(WorldObject const* o, uint32 idx, float extra = 0.0f);

    // Occupancy registry. Entries not refreshed for 30 s are pruned.
    void Heartbeat(BotGuid bot, uint32 idx, TeamId team, bool seeded, bool dead);
    void Leave(BotGuid bot);
    uint32 Count(uint32 idx, TeamId team);
    bool IsRegistered(BotGuid bot);
    /// BT-044: a bot that left a spot less than an hour ago is not seeded again, so the spots rotate their bots and
    /// every bot goes back to levelling between visits.
    bool LeftRecently(BotGuid bot, uint32 nowMs);

    // #201 (#612, wave-13 t1): the no-rematch window (MoP 25-35 s). It is recorded when a duel ENDS, from the
    // PlayerScript OnDuelEnd hook (mod_playerbots.cpp): a bot inside a duel is in combat, so its non-combat engine -
    // the one that runs the status action - does not tick, and the in-action record (Execute step 4) missed most duels
    // (wave 13: MoP min re-request gap 3.4 s, AC 19.9 s, against 30 s). Both directions are stored per ordered pair;
    // a harness rematch override of the bot wins over the config delay. Only duels inside a spot count (IsDuelInSpot).
    void NoteDuelEnd(Player* a, Player* b);
    bool IsRematchBlocked(BotGuid bot, BotGuid other, uint32 nowMs);

    // Seeding hand-off. SetPending always registers a placeholder entry (seeded), so Count sees the bot at once; the
    // team falls back to the online player's team (SPEC amendment A8, same as AC).
    void SetPending(BotGuid bot, uint32 idx, TeamId team = TEAM_NEUTRAL);
    bool TakePending(BotGuid bot, uint32& idx);

    // Real-player cooldown (nowMs / untilMs are getMSTime() values).
    bool PlayerOnCooldown(BotGuid player, uint32 nowMs);
    void SetPlayerCooldown(BotGuid player, uint32 untilMs);

    /// Seeder (#615). Called from RandomPlayerbotMgr::UpdateAIInternal on the world thread.
    void Update(uint32 diff);

    /// True when DuelSpotEnable and either player is within radius + 20 yd of an enabled spot (duel reset, #614).
    bool IsDuelInSpot(Player* a, Player* b);

    // Harness overrides keyed by bot GUID (0 clears). The action uses an override when set, else the config.
    void SetScanOverride(BotGuid bot, uint32 ms);
    void SetRematchOverride(BotGuid bot, uint32 seconds);
    uint32 GetScanOverride(BotGuid bot);
    uint32 GetRematchOverride(BotGuid bot);

    /// Spot index from a name (case-insensitive) or a decimal index; -1 if unknown.
    int32 FindSpot(std::string const& nameOrIndex);

private:
    DuelSpotMgr() = default;

    struct Entry
    {
        uint32 idx = 0;
        TeamId team = TEAM_NEUTRAL;
        bool seeded = false;
        uint32 lastBeatMs = 0;
        uint32 deadSinceMs = 0;  // 0 = alive
        uint32 reviveAtMs = 0;
        // #201 (#615, wave 32 t8): the last beat from the bot itself (status action Heartbeat or the seeding hand-off);
        // lastBeatMs is also refreshed by the seeder's hold (Update step 1), this one only by the bot.
        uint32 actionBeatMs = 0;
    };
    struct Pending
    {
        uint32 idx = 0;
        uint32 sinceMs = 0;
    };

    void PruneLocked(uint32 nowMs);

    std::mutex _mutex;
    std::unordered_map<BotGuid, Entry> _registry;
    std::unordered_map<BotGuid, Pending> _pending;
    std::unordered_map<BotGuid, uint32> _leftAt;  // BT-044: getMSTime() when the bot last left a spot
    std::unordered_map<BotGuid, uint32> _playerCooldown;
    std::unordered_map<BotGuid, uint32> _scanOverride;
    std::unordered_map<BotGuid, uint32> _rematchOverride;
    std::unordered_map<BotGuid, std::unordered_map<BotGuid, uint32>> _rematchUntil;  // bot -> other -> stamp
    uint32 _lastSeedMs = 0;
};

#define sDuelSpotMgr DuelSpotMgr::instance()

/// Stand at a duel spot, patrol it, and every scan challenge an eligible player through AC's own (dormant until now)
/// "rpg duel" trigger and action. Pattern: AC NewRpgOutdoorPvP.cpp.
class NewRpgDuelSpotAction : public NewRpgBaseAction
{
public:
    NewRpgDuelSpotAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "new rpg duel spot") {}

    bool Execute(Event event) override;

protected:
    bool Patrol(DuelSpotInfo const& spot);
    bool ScanAndChallenge(NewRpgInfo::DuelSpot& data, DuelSpotInfo const& spot);
};

#endif
