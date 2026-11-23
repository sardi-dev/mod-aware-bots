/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENASTATE_H
#define _PLAYERBOT_ARENASTATE_H

// BT-154: the RAM books of the arena ladder and the types its files share (ArenaLadder.cpp, ArenaStore.cpp,
// ArenaSnapshot.cpp, ArenaChat.cpp). Internal: the rest of the module uses ArenaLadder.h.

#include "ArenaChatMath.h"
#include "ArenaLadder.h"
#include "ArenaMath.h"
#include "Define.h"

#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ArenaLadder
{
// Settings, read from the configuration (AiPlayerbot.ArenaTeams.* and AiPlayerbot.ArenaChat.*; conf/playerbots.conf.dist).
struct Config
{
    bool enable = true;
    uint32 minLevel = 60;                 // arena teams only from this level (owner, 2026-11-12)
    uint32 teamsPerFactionAndBand = 4;
    int32 minPairScore = 60;
    uint32 mixExpert = 1;
    uint32 mixGood = 3;
    uint32 mixCasual = 2;
    uint32 startupDelaySeconds = 600;
    uint32 formationIntervalSeconds = 600;
    uint32 peakMatchesPerHour = 4;
    uint32 maxConcurrentBotMatches = 1;
    uint32 maxUpdateMs = 80;
    uint32 botMatchCutSeconds = ArenaMath::BOT_MATCH_CUT_SECONDS;
    int32 ownerUtcOffsetHours = 0;
    bool ownerUsDaylight = false;
    int32 clockOffsetMinutes = 0;         // test hook: moves the clock to 20:00 or 03:00
    uint32 retireAfterChecks = 12;        // failed checks in a row before a team is retired
    uint32 replacePartnerEveryDays = 7;
    uint32 orderTimeoutSeconds = 240;     // a queued pair that has not entered by then is released
    uint32 releaseAfterSeconds = 300;     // a finished match frees its teams at the latest after this
    std::string snapshotFile = "arena-ladder.json";
    uint32 snapshotMinSeconds = 30;
    uint32 snapshotMaxSeconds = 300;
    bool chatEnable = true;
    ArenaChatMath::ChatLimits chatLimits;
};

struct Member
{
    uint32 guid = 0;
    uint8 classId = 0;
    uint16 specId = 0;
    uint8 role = 0;  // 0 damage, 1 healer (the table column)
};

constexpr uint32 RECENT_OPPONENTS = 3;  // the pairing looks at a team's last three opponents
constexpr uint32 RECENT_RESULTS = 5;
constexpr uint32 HISTORY_IN_RAM = 300;  // matches kept for the NPC, the snapshot and the rivalry check

struct Team
{
    uint32 id = 0;
    std::string name;
    uint8 bracket = BRACKET_2V2;
    uint8 faction = 0;
    uint8 band = 0;
    uint8 tier = 1;  // BotSkillMath::Tier as a number (0 expert, 1 good, 2 casual)
    int32 rating = 1500;
    int32 peak = 1500;
    uint32 games = 0;
    uint32 wins = 0;
    uint32 losses = 0;
    uint32 draws = 0;
    int32 streak = 0;
    uint32 bestStreak = 0;
    uint8 status = STATUS_ACTIVE;
    int64 created = 0;
    int64 lastPlayed = 0;  // 0: never
    int64 retired = 0;
    std::vector<Member> members;

    // Not stored:
    std::deque<uint32> recentOpponents;  // newest last
    std::string recentResults;           // newest last
    uint32 failedChecks = 0;
    uint32 failedPasses = 0;             // consecutive scheduler passes in which the team found nobody
    int64 lastPartnerReplace = 0;
    int64 backoffUntil = 0;              // a team whose queueing failed rests until then
    bool dormant = false;                // the composition no longer scores: not scheduled until it does
};

// A row of the match table, and the RAM copy of the newest ones.
struct MatchRow
{
    uint32 id = 0;
    uint8 bracket = BRACKET_2V2;
    uint32 teamA = 0;
    uint32 teamB = 0;
    uint8 winner = 3;  // 0 draw, 1 team A, 2 team B, 3 voided
    uint32 ratingA = 0;
    uint32 ratingANew = 0;
    uint32 ratingB = 0;
    uint32 ratingBNew = 0;
    uint16 mapId = 0;
    uint16 seconds = 0;
    int64 ended = 0;
};

enum class LiveState : uint8
{
    QUEUED,    // both parties are in the arena queue, the core has not made the match yet
    RUNNING,   // the bots are in the arena
    FINISHED   // the result is booked; the teams are released when the members have left
};

struct Live
{
    uint32 teamA = 0;
    uint32 teamB = 0;
    LiveState state = LiveState::QUEUED;
    int64 orderedAt = 0;
    int64 finishedAt = 0;
    uint32 instanceId = 0;
    uint32 bgTypeId = 0;
    uint32 mapId = 0;
    uint32 sideA = 0;       // the core's team (ALLIANCE or HORDE) of team A inside the arena
    uint32 sideB = 0;
    uint32 runningSinceMs = 0;   // getMSTime() when the arena was first seen in progress
    bool seenInProgress = false;
    bool cutAsDraw = false;
    bool voided = false;
    bool bookGuard = false;      // the result has been booked
};

struct State
{
    std::mutex lock;  // guards everything below except tierByGuid

    Config cfg;
    bool started = false;
    std::map<uint32, Team> teams;
    std::deque<MatchRow> history;  // oldest first
    std::vector<Live> live;
    uint32 nextTeamId = 1;
    uint32 nextMatchId = 1;
    uint32 teamCounter = 0;  // the counter that feeds the name generator

    // World thread timers, milliseconds.
    uint32 sinceStartMs = 0;
    uint32 untilFormationMs = 0;
    uint32 untilSchedulerMs = 0;
    uint32 sincePollMs = 0;
    uint32 sinceSnapshotMs = 0;
    bool snapshotDirty = true;
    bool forceFormation = false;
    bool forceMatch = false;
    bool forceSnapshot = false;
    bool loaded = false;

    // The world update time (the diff between two world ticks), smoothed over a few seconds, for the scheduler's load check.
    double avgUpdateMs = 0.0;
    bool avgUpdateSeen = false;

    ArenaChatMath::ChatLimiter chat;

    // The tier of every bot of a live match, read by BotSkill on map threads.
    std::shared_mutex tierLock;
    std::unordered_map<uint32, uint8> tierByGuid;
};

State& S();

BotSkillMath::Tier TierOfNumber(uint8 tier);

// The clock of the owner for the config (the test hook included).
ArenaMath::LocalTime OwnerNow(Config const& cfg, int64 utcNow);

// Reads the settings from the configuration.
Config ReadConfig();

// ---- the store (ArenaStore.cpp) -------------------------------------------------------------------------------
namespace Store
{
// CREATE TABLE IF NOT EXISTS for the three tables (the module has no SQL updater).
void EnsureTables();
// Loads teams, members and the newest matches into `state` (synchronous; start-up only).
void LoadAll(State& state);
// Queued on the asynchronous queue, in order.
void SaveTeam(Team const& team);
void SaveMatch(MatchRow const& row, Team const* a, Team const* b);
}

// ---- the snapshot (ArenaSnapshot.cpp) -------------------------------------------------------------------------
// Builds the JSON text of the books (caller holds the lock).
std::string BuildSnapshotJson(State const& state, int64 utcNow);
// Writes it next to the world server's logs: a temporary file, then a rename. False on failure.
bool WriteSnapshotFile(Config const& cfg, std::string const& json);
// BT-700: the same write on a worker thread, so the world thread never waits for the disk. The text is a copy made under the
// lock; while one write runs, a newer text replaces any text still waiting (only the latest snapshot matters).
void QueueSnapshotWrite(Config const& cfg, std::string json);
}  // namespace ArenaLadder

#endif
