/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// BT-154: the JSON snapshot of the arena ladder. The module writes it to a file under the world server's logs folder (the
// folder), at the rate the scheduler tick allows; an external script can turn it into a local HTML page.
// A file keeps the playerbots database closed to such tools: no grant for a read-only account.

#include "ArenaState.h"

#include "ArenaText.h"
#include "Config.h"
#include "Log.h"
#include "World.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace ArenaLadder
{
namespace
{
constexpr uint32 SNAPSHOT_VERSION = 1;
constexpr char const* WEEKDAY_NAMES[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

std::string Quote(std::string const& text)
{
    std::string out = "\"";
    for (char const c : text)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                    out += ' ';
                else
                    out += c;
        }
    }
    return out + "\"";
}

std::string UtcStamp(int64 epoch)
{
    time_t const t = time_t(epoch);
    tm utc;
    gmtime_r(&t, &utc);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

std::string MemberName(uint32 guid)
{
    if (CharacterNameData const* data = sWorld->GetCharacterNameData(guid))
        return data->m_name;
    return std::string();
}
}  // namespace

std::string BuildSnapshotJson(State const& state, int64 utcNow)
{
    Config const& cfg = state.cfg;
    ArenaMath::LocalTime const local = OwnerNow(cfg, utcNow);

    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": " << SNAPSHOT_VERSION << ",\n";
    out << "  \"generated\": " << Quote(UtcStamp(utcNow)) << ",\n";
    out << "  \"generated_epoch\": " << utcNow << ",\n";
    char clock[96];
    snprintf(clock, sizeof(clock), "%04d-%02u-%02u %02u:%02u %s", local.year, local.month, local.day, local.hour, local.minute,
        WEEKDAY_NAMES[local.weekday % 7]);
    out << "  \"owner_time\": " << Quote(clock) << ",\n";
    out << "  \"schedule\": { \"enabled\": " << (cfg.enable ? "true" : "false")
        << ", \"weight_percent\": " << ArenaMath::ActivityWeightPercent(local)
        << ", \"peak_matches_per_hour\": " << cfg.peakMatchesPerHour
        << ", \"max_concurrent\": " << cfg.maxConcurrentBotMatches
        << ", \"quiet_from_hour\": " << ArenaMath::QUIET_FROM_HOUR
        << ", \"quiet_until_hour\": " << ArenaMath::QUIET_UNTIL_HOUR
        << ", \"min_level\": " << cfg.minLevel
        << ", \"owner_offset_minutes\": " << (cfg.ownerUtcOffsetHours * 60 + (local.daylight ? 60 : 0) + cfg.clockOffsetMinutes)
        << " },\n";

    // Ranks among the active teams of each bracket.
    std::map<uint32, uint32> rankOf;
    {
        std::vector<Team const*> active;
        for (auto const& entry : state.teams)
            if (entry.second.status == STATUS_ACTIVE)
                active.push_back(&entry.second);
        std::sort(active.begin(), active.end(), [](Team const* a, Team const* b)
        {
            return a->bracket != b->bracket ? a->bracket < b->bracket : a->rating != b->rating ? a->rating > b->rating : a->id < b->id;
        });
        uint32 rank = 0;
        uint8 bracket = 0;
        for (Team const* team : active)
        {
            if (team->bracket != bracket)
            {
                bracket = team->bracket;
                rank = 0;
            }
            rankOf[team->id] = ++rank;
        }
    }

    out << "  \"teams\": [";
    bool first = true;
    for (auto const& entry : state.teams)
    {
        Team const& t = entry.second;
        out << (first ? "\n" : ",\n");
        first = false;
        out << "    { \"id\": " << t.id << ", \"name\": " << Quote(t.name) << ", \"bracket\": " << uint32(t.bracket)
            << ", \"faction\": " << Quote(t.faction == 0 ? "Alliance" : "Horde") << ", \"band\": " << uint32(t.band)
            << ", \"level_min\": " << ArenaMath::BandMinLevel(t.band) << ", \"level_max\": " << ArenaMath::BandMaxLevel(t.band)
            << ", \"tier\": " << Quote(ArenaText::TierName(TierOfNumber(t.tier))) << ", \"rating\": " << t.rating
            << ", \"peak\": " << t.peak << ", \"games\": " << t.games << ", \"wins\": " << t.wins
            << ", \"losses\": " << t.losses << ", \"draws\": " << t.draws << ", \"streak\": " << t.streak
            << ", \"best_streak\": " << t.bestStreak << ", \"status\": " << Quote(t.status == STATUS_ACTIVE ? "active" : "retired")
            << ", \"rank\": " << (rankOf.count(t.id) ? rankOf.at(t.id) : 0u) << ", \"created\": " << t.created
            << ", \"last_played\": " << t.lastPlayed << ", \"last_results\": " << Quote(t.recentResults)
            << ", \"members\": [";
        bool firstMember = true;
        for (Member const& m : t.members)
        {
            out << (firstMember ? "" : ", ");
            firstMember = false;
            out << "{ \"name\": " << Quote(MemberName(m.guid)) << ", \"class\": " << Quote(ArenaText::ClassName(m.classId))
                << ", \"spec\": " << Quote(ArenaText::SpecName(m.specId)) << ", \"role\": " << Quote(m.role ? "healer" : "damage")
                << " }";
        }
        out << "] }";
    }
    out << (first ? "" : "\n  ") << "],\n";

    out << "  \"live\": [";
    first = true;
    for (Live const& live : state.live)
    {
        auto const a = state.teams.find(live.teamA);
        auto const b = state.teams.find(live.teamB);
        out << (first ? "\n" : ",\n");
        first = false;
        out << "    { \"team_a\": " << live.teamA << ", \"team_b\": " << live.teamB << ", \"team_a_name\": "
            << Quote(a != state.teams.end() ? a->second.name : "") << ", \"team_b_name\": "
            << Quote(b != state.teams.end() ? b->second.name : "") << ", \"state\": "
            << Quote(live.state == LiveState::QUEUED ? "queued" : live.state == LiveState::RUNNING ? "fighting" : "finished")
            << ", \"since\": " << live.orderedAt << " }";
    }
    out << (first ? "" : "\n  ") << "],\n";

    out << "  \"matches\": [";
    first = true;
    for (MatchRow const& m : state.history)
    {
        out << (first ? "\n" : ",\n");
        first = false;
        out << "    { \"id\": " << m.id << ", \"bracket\": " << uint32(m.bracket) << ", \"ended\": " << m.ended
            << ", \"team_a\": " << m.teamA << ", \"team_b\": " << m.teamB << ", \"winner\": " << uint32(m.winner)
            << ", \"rating_a\": " << m.ratingA << ", \"rating_a_new\": " << m.ratingANew << ", \"rating_b\": " << m.ratingB
            << ", \"rating_b_new\": " << m.ratingBNew << ", \"map\": " << uint32(m.mapId) << ", \"seconds\": " << uint32(m.seconds)
            << " }";
    }
    out << (first ? "" : "\n  ") << "]\n";
    out << "}\n";
    return out.str();
}

namespace
{
// The server's log folder (worldserver.conf LogsDir); the current folder when it is not set.
std::filesystem::path SnapshotTarget(Config const& cfg)
{
    std::filesystem::path const folder = sConfigMgr->GetStringDefault("LogsDir", "");
    return folder / cfg.snapshotFile;
}

bool WriteSnapshotTo(std::filesystem::path const& target, std::string const& json)
{
    namespace fs = std::filesystem;

    fs::path temporary = target;
    temporary += ".tmp";

    std::error_code error;
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            SF_LOG_WARN("playerbots", "[ARENALADDER] cannot write the snapshot %s", temporary.string().c_str());
            return false;
        }
        file << json;
        if (!file.good())
        {
            SF_LOG_WARN("playerbots", "[ARENALADDER] the snapshot %s could not be written completely", temporary.string().c_str());
            return false;
        }
    }

    fs::rename(temporary, target, error);
    if (error)
    {
        SF_LOG_WARN("playerbots", "[ARENALADDER] cannot move the snapshot to %s: %s", target.string().c_str(), error.message().c_str());
        return false;
    }
    return true;
}

// BT-700: at most one writer thread; a text that arrives while it writes waits in `pending` (the newest one wins).
struct SnapshotWriter
{
    std::mutex lock;
    bool running = false;
    bool hasPending = false;
    std::filesystem::path pendingTarget;
    std::string pendingJson;
};

// Never destroyed: the detached thread may still run at shutdown.
SnapshotWriter& Writer()
{
    static SnapshotWriter* writer = new SnapshotWriter();
    return *writer;
}

void RunSnapshotWriter()
{
    SnapshotWriter& writer = Writer();
    for (;;)
    {
        std::filesystem::path target;
        std::string json;
        {
            std::lock_guard<std::mutex> guard(writer.lock);
            if (!writer.hasPending)
            {
                writer.running = false;
                return;
            }
            target = std::move(writer.pendingTarget);
            json = std::move(writer.pendingJson);
            writer.hasPending = false;
        }
        WriteSnapshotTo(target, json);
    }
}
}  // namespace

bool WriteSnapshotFile(Config const& cfg, std::string const& json)
{
    return WriteSnapshotTo(SnapshotTarget(cfg), json);
}

void QueueSnapshotWrite(Config const& cfg, std::string json)
{
    SnapshotWriter& writer = Writer();
    // The target is read here, on the caller's thread: the config store is not read from the worker.
    std::filesystem::path target = SnapshotTarget(cfg);
    bool start = false;
    {
        std::lock_guard<std::mutex> guard(writer.lock);
        writer.pendingTarget = std::move(target);
        writer.pendingJson = std::move(json);
        writer.hasPending = true;
        if (!writer.running)
        {
            writer.running = true;
            start = true;
        }
    }
    if (start)
        std::thread(RunSnapshotWriter).detach();
}
}  // namespace ArenaLadder
