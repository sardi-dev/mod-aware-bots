/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENALADDER_H
#define _PLAYERBOT_ARENALADDER_H

// BT-154: the bot arena ladder (design decision 0027). Named teams of two random bots (the pilot), a team rating of their own,
// a light schedule of bot-versus-bot matches and a RAM copy of the books for the ladder NPC, the console and the
// snapshot. The 5.4.8 core keeps no arena rating (its arena team code is never reached), so these books are the only
// ladder there is.
//
// Threads. The ladder runs on the world thread, from the world update script, at the point of the update where the map
// threads are idle (World::Update waits for them before the scripts run), so it may touch bots and battlegrounds
// directly. Map threads (bot AI, the ladder NPC) only read: through the functions below, which copy out of the RAM books
// under one lock. Nothing on a map thread waits for the database; writes are queued on the playerbots database's
// asynchronous queue and run in order (PlayerbotsDatabase.WorkerThreads = 1).

#include "BotSkillMath.h"
#include "Define.h"

#include <string>
#include <vector>

class Player;

namespace ArenaLadder
{
// The pilot bracket: players per side. 3v3 and 5v5 come later.
constexpr uint8 BRACKET_2V2 = 2;

constexpr uint8 STATUS_ACTIVE = 0;
constexpr uint8 STATUS_RETIRED = 1;

struct MemberSummary
{
    uint32 guid = 0;
    std::string name;
    uint8 classId = 0;
    uint16 specId = 0;
    bool healer = false;
};

struct TeamSummary
{
    uint32 id = 0;
    std::string name;
    uint8 bracket = BRACKET_2V2;
    uint8 faction = 0;  // 0 Alliance, 1 Horde
    uint8 band = 0;     // the core's PvP bracket index of the members' levels
    BotSkillMath::Tier tier = BotSkillMath::Tier::GOOD;
    uint32 rating = 0;
    uint32 peakRating = 0;
    uint32 games = 0;
    uint32 wins = 0;
    uint32 losses = 0;
    uint32 draws = 0;
    int32 streak = 0;
    uint32 bestStreak = 0;
    uint8 status = STATUS_ACTIVE;
    uint32 rank = 0;  // among the active teams of the bracket, 1 based
    int64 lastPlayed = 0;
    std::string lastResults;  // newest last, "W", "L" and "D"
    std::string rival;        // the opponent met most often lately, with the record against it
    bool playingNow = false;
    std::vector<MemberSummary> members;
};

struct LiveSummary
{
    std::string teamA;
    std::string teamB;
    std::string state;  // "queued" or "fighting"
    uint32 seconds = 0;
};

struct MatchSummary
{
    int64 ended = 0;
    std::string teamA;
    std::string teamB;
    uint8 winner = 0;  // 0 draw, 1 team A, 2 team B, 3 voided
    uint32 ratingA = 0;
    uint32 ratingANew = 0;
    uint32 ratingB = 0;
    uint32 ratingBNew = 0;
    uint32 seconds = 0;
};

// AiPlayerbot.ArenaTeams.Enable (read once at the start; a restart applies a change).
bool Enabled();

// World thread. OnStartup creates the tables when they are missing and loads the books (after the database is open and
// the data stores are loaded); OnUpdate is the world update tick.
void OnStartup();
void OnUpdate(uint32 diff);

// Map threads: the tier a bot plays at inside a ladder match (its team's, the weakest member's). False when the bot is
// not in a ladder match, then the usual rules apply (BotSkill.h).
bool TeamTierOf(uint32 guidLow, BotSkillMath::Tier& tier);

// A bot that belongs to a team in a match that is queued or running: the legacy arena party builder leaves it alone.
bool IsInLadderMatch(uint32 guidLow);

// Copies for the NPC, the console and the tests of the live system. Sorted by rating, best first.
std::vector<TeamSummary> Ladder(uint8 bracket, bool includeRetired = false);
bool FindTeam(uint32 teamId, TeamSummary& out);
std::vector<LiveSummary> LiveMatches();
std::vector<MatchSummary> RecentMatches(uint32 limit);
// The last results of one team, newest first.
std::vector<MatchSummary> RecentMatchesOf(uint32 teamId, uint32 limit);

// The console command ".playerbots ladder <show|matches|form|match|snapshot|help>". Returns the lines to print.
std::vector<std::string> Console(std::string const& args);
}  // namespace ArenaLadder

#endif
