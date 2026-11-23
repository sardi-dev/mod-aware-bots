/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// BT-154: the tables of the arena ladder in the playerbots database.
//
// Plain SQL strings, not prepared statements, on purpose: the module's prepared statements are prepared when the
// playerbots connection opens, and a statement on a table that does not exist yet makes that fail. The module has no SQL
// updater either, so the tables are created here with CREATE TABLE IF NOT EXISTS before the first query (the same text is
// in sql/playerbots/updates/ for fresh installs). Every value is a number or a name from the module's own word lists,
// and a name is reduced to letters, spaces and "&" before it goes into a statement.
//
// Writes go to the asynchronous queue (one worker thread, so they run in the order they were queued) as one transaction
// per change. Nothing here is called from a map thread.

#include "ArenaState.h"

#include "DatabaseEnv.h"
#include "Log.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>

namespace ArenaLadder
{
namespace
{
std::string Fmt(char const* format, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}

// A team name is made of letters, spaces and "&" (ArenaNames::IsValidName). The text is built into the SQL string, so
// anything else, which only a hand-edited row could hold, is dropped and quote characters never get through.
std::string Escaped(std::string const& value)
{
    std::string safe;
    for (char const c : value)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == ' ' || c == '&')
            safe += c;
    return safe;
}

// A stored timestamp is a UTC epoch in RAM; 0 means "not set" and is stored as NULL.
std::string FromUnix(int64 epoch)
{
    return epoch ? Fmt("FROM_UNIXTIME(%lld)", static_cast<long long>(epoch)) : std::string("NULL");
}

constexpr uint32 MATCH_HISTORY_KEPT = 5000;  // rows of the match table; older ones are pruned

char const* const CREATE_TEAM =
    "CREATE TABLE IF NOT EXISTS `playerbots_arena_team` ("
    "`team_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
    "`name` VARCHAR(24) NOT NULL,"
    "`bracket` TINYINT UNSIGNED NOT NULL,"
    "`faction` TINYINT UNSIGNED NOT NULL,"
    "`level_band` TINYINT UNSIGNED NOT NULL,"
    "`skill_tier` TINYINT UNSIGNED NOT NULL,"
    "`rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
    "`peak_rating` SMALLINT UNSIGNED NOT NULL DEFAULT 1500,"
    "`games` INT UNSIGNED NOT NULL DEFAULT 0,"
    "`wins` INT UNSIGNED NOT NULL DEFAULT 0,"
    "`losses` INT UNSIGNED NOT NULL DEFAULT 0,"
    "`draws` INT UNSIGNED NOT NULL DEFAULT 0,"
    "`streak` SMALLINT NOT NULL DEFAULT 0,"
    "`best_streak` SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
    "`status` TINYINT UNSIGNED NOT NULL DEFAULT 0,"
    "`created` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "`last_played` TIMESTAMP NULL DEFAULT NULL,"
    "`retired` TIMESTAMP NULL DEFAULT NULL,"
    "PRIMARY KEY (`team_id`),"
    "UNIQUE KEY `uq_arena_team_name` (`name`),"
    "KEY `idx_arena_ladder` (`bracket`, `status`, `rating`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

char const* const CREATE_MEMBER =
    "CREATE TABLE IF NOT EXISTS `playerbots_arena_team_member` ("
    "`team_id` INT UNSIGNED NOT NULL,"
    "`guid` INT UNSIGNED NOT NULL,"
    "`bracket` TINYINT UNSIGNED NOT NULL,"
    "`class` TINYINT UNSIGNED NOT NULL,"
    "`spec` SMALLINT UNSIGNED NOT NULL,"
    "`role` TINYINT UNSIGNED NOT NULL,"
    "`joined` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "PRIMARY KEY (`team_id`, `guid`),"
    "UNIQUE KEY `uq_arena_member_bracket` (`guid`, `bracket`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

char const* const CREATE_MATCH =
    "CREATE TABLE IF NOT EXISTS `playerbots_arena_match` ("
    "`match_id` INT UNSIGNED NOT NULL AUTO_INCREMENT,"
    "`bracket` TINYINT UNSIGNED NOT NULL,"
    "`team_a` INT UNSIGNED NOT NULL,"
    "`team_b` INT UNSIGNED NOT NULL,"
    "`human_name` VARCHAR(40) NULL,"
    "`winner` TINYINT UNSIGNED NOT NULL,"
    "`rating_a` SMALLINT UNSIGNED NOT NULL,"
    "`rating_a_new` SMALLINT UNSIGNED NOT NULL,"
    "`rating_b` SMALLINT UNSIGNED NOT NULL,"
    "`rating_b_new` SMALLINT UNSIGNED NOT NULL,"
    "`map_id` SMALLINT UNSIGNED NOT NULL,"
    "`seconds` SMALLINT UNSIGNED NOT NULL,"
    "`ended` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "PRIMARY KEY (`match_id`),"
    "KEY `idx_arena_match_ended` (`ended`)"
    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";

std::string TeamRowSql(Team const& t)
{
    return Fmt("REPLACE INTO `playerbots_arena_team` (`team_id`, `name`, `bracket`, `faction`, `level_band`, `skill_tier`, "
               "`rating`, `peak_rating`, `games`, `wins`, `losses`, `draws`, `streak`, `best_streak`, `status`, `created`, "
               "`last_played`, `retired`) VALUES (%u, '%s', %u, %u, %u, %u, %d, %d, %u, %u, %u, %u, %d, %u, %u, %s, %s, %s)",
        t.id, Escaped(t.name).c_str(), uint32(t.bracket), uint32(t.faction), uint32(t.band), uint32(t.tier), t.rating, t.peak,
        t.games, t.wins, t.losses, t.draws, t.streak, t.bestStreak, uint32(t.status),
        FromUnix(t.created ? t.created : int64(time(nullptr))).c_str(), FromUnix(t.lastPlayed).c_str(),
        FromUnix(t.retired).c_str());
}

void AppendTeam(PlayerbotsDatabaseTransaction& trans, Team const& team)
{
    trans->Append(TeamRowSql(team).c_str());
    trans->Append(Fmt("DELETE FROM `playerbots_arena_team_member` WHERE `team_id` = %u", team.id).c_str());
    for (Member const& member : team.members)
        trans->Append(Fmt("INSERT INTO `playerbots_arena_team_member` (`team_id`, `guid`, `bracket`, `class`, `spec`, `role`) "
                          "VALUES (%u, %u, %u, %u, %u, %u)",
            team.id, member.guid, uint32(team.bracket), uint32(member.classId), uint32(member.specId), uint32(member.role))
                           .c_str());
}
}  // namespace

namespace Store
{
void EnsureTables()
{
    for (char const* statement : { CREATE_TEAM, CREATE_MEMBER, CREATE_MATCH })
        PlayerbotsDatabase.DirectExecute(statement);
}

void LoadAll(State& state)
{
    state.teams.clear();
    state.history.clear();

    if (QueryResult result = PlayerbotsDatabase.Query(
            "SELECT `team_id`, `name`, `bracket`, `faction`, `level_band`, `skill_tier`, `rating`, `peak_rating`, `games`, "
            "`wins`, `losses`, `draws`, `streak`, `best_streak`, `status`, IFNULL(UNIX_TIMESTAMP(`created`), 0), "
            "IFNULL(UNIX_TIMESTAMP(`last_played`), 0), IFNULL(UNIX_TIMESTAMP(`retired`), 0) "
            "FROM `playerbots_arena_team` ORDER BY `team_id`"))
    {
        do
        {
            Field* f = result->Fetch();
            Team team;
            team.id = f[0].GetUInt32();
            team.name = f[1].GetString();
            team.bracket = f[2].GetUInt8();
            team.faction = f[3].GetUInt8();
            team.band = f[4].GetUInt8();
            team.tier = f[5].GetUInt8();
            team.rating = int32(f[6].GetUInt16());
            team.peak = int32(f[7].GetUInt16());
            team.games = f[8].GetUInt32();
            team.wins = f[9].GetUInt32();
            team.losses = f[10].GetUInt32();
            team.draws = f[11].GetUInt32();
            team.streak = int32(f[12].GetInt16());
            team.bestStreak = f[13].GetUInt16();
            team.status = f[14].GetUInt8();
            team.created = int64(f[15].GetUInt64());
            team.lastPlayed = int64(f[16].GetUInt64());
            team.retired = int64(f[17].GetUInt64());
            state.nextTeamId = std::max(state.nextTeamId, team.id + 1);
            state.teams[team.id] = std::move(team);
        } while (result->NextRow());
    }

    if (QueryResult result = PlayerbotsDatabase.Query(
            "SELECT `team_id`, `guid`, `class`, `spec`, `role` FROM `playerbots_arena_team_member` ORDER BY `team_id`, `guid`"))
    {
        do
        {
            Field* f = result->Fetch();
            auto const team = state.teams.find(f[0].GetUInt32());
            if (team == state.teams.end())
                continue;

            Member member;
            member.guid = f[1].GetUInt32();
            member.classId = f[2].GetUInt8();
            member.specId = f[3].GetUInt16();
            member.role = f[4].GetUInt8();
            team->second.members.push_back(member);
        } while (result->NextRow());
    }

    // The newest matches, oldest first, so that the RAM-only fields (last results, recent opponents) can be rebuilt.
    if (QueryResult result = PlayerbotsDatabase.PQuery(
            "SELECT `match_id`, `bracket`, `team_a`, `team_b`, `winner`, `rating_a`, `rating_a_new`, `rating_b`, "
            "`rating_b_new`, `map_id`, `seconds`, IFNULL(UNIX_TIMESTAMP(`ended`), 0) FROM `playerbots_arena_match` "
            "ORDER BY `match_id` DESC LIMIT %u",
            HISTORY_IN_RAM))
    {
        do
        {
            Field* f = result->Fetch();
            MatchRow row;
            row.id = f[0].GetUInt32();
            row.bracket = f[1].GetUInt8();
            row.teamA = f[2].GetUInt32();
            row.teamB = f[3].GetUInt32();
            row.winner = f[4].GetUInt8();
            row.ratingA = f[5].GetUInt16();
            row.ratingANew = f[6].GetUInt16();
            row.ratingB = f[7].GetUInt16();
            row.ratingBNew = f[8].GetUInt16();
            row.mapId = f[9].GetUInt16();
            row.seconds = f[10].GetUInt16();
            row.ended = int64(f[11].GetUInt64());
            state.history.push_front(row);
            state.nextMatchId = std::max(state.nextMatchId, row.id + 1);
        } while (result->NextRow());
    }

    if (QueryResult result = PlayerbotsDatabase.Query("SELECT IFNULL(MAX(`match_id`), 0) FROM `playerbots_arena_match`"))
        state.nextMatchId = std::max(state.nextMatchId, result->Fetch()[0].GetUInt32() + 1);

    state.teamCounter = state.nextTeamId - 1;

    for (MatchRow const& row : state.history)
    {
        auto remember = [&](uint32 teamId, uint32 opponentId, char result)
        {
            auto const found = state.teams.find(teamId);
            if (found == state.teams.end())
                return;
            Team& team = found->second;
            if (result != 'V')
            {
                team.recentOpponents.push_back(opponentId);
                if (team.recentOpponents.size() > RECENT_OPPONENTS)
                    team.recentOpponents.pop_front();
                team.recentResults += result;
                if (team.recentResults.size() > RECENT_RESULTS)
                    team.recentResults.erase(0, 1);
            }
        };

        char a = 'V';
        char b = 'V';
        if (row.winner == uint8(ArenaMath::Winner::DRAW))
            a = b = 'D';
        else if (row.winner == uint8(ArenaMath::Winner::TEAM_A))
        {
            a = 'W';
            b = 'L';
        }
        else if (row.winner == uint8(ArenaMath::Winner::TEAM_B))
        {
            a = 'L';
            b = 'W';
        }
        remember(row.teamA, row.teamB, a);
        remember(row.teamB, row.teamA, b);
    }
}

void SaveTeam(Team const& team)
{
    PlayerbotsDatabaseTransaction trans = PlayerbotsDatabase.BeginTransaction();
    AppendTeam(trans, team);
    PlayerbotsDatabase.CommitTransaction(trans);
}

void SaveMatch(MatchRow const& row, Team const* a, Team const* b)
{
    PlayerbotsDatabaseTransaction trans = PlayerbotsDatabase.BeginTransaction();
    if (a)
        AppendTeam(trans, *a);
    if (b)
        AppendTeam(trans, *b);

    trans->Append(Fmt("INSERT INTO `playerbots_arena_match` (`match_id`, `bracket`, `team_a`, `team_b`, `winner`, `rating_a`, "
                      "`rating_a_new`, `rating_b`, `rating_b_new`, `map_id`, `seconds`, `ended`) "
                      "VALUES (%u, %u, %u, %u, %u, %u, %u, %u, %u, %u, %u, FROM_UNIXTIME(%lld))",
        row.id, uint32(row.bracket), row.teamA, row.teamB, uint32(row.winner), row.ratingA, row.ratingANew, row.ratingB,
        row.ratingBNew, uint32(row.mapId), uint32(row.seconds), static_cast<long long>(row.ended))
                       .c_str());
    if (row.id > MATCH_HISTORY_KEPT)
        trans->Append(Fmt("DELETE FROM `playerbots_arena_match` WHERE `match_id` <= %u", row.id - MATCH_HISTORY_KEPT).c_str());
    PlayerbotsDatabase.CommitTransaction(trans);
}
}  // namespace Store
}  // namespace ArenaLadder
