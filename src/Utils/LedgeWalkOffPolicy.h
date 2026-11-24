#ifndef AWARE_BOTS_LEDGE_WALK_OFF_POLICY_H
#define AWARE_BOTS_LEDGE_WALK_OFF_POLICY_H

#include <cmath>
#include <ctime>

// BT-388: a bot stuck where a real player can see it. Shadowfang Keep rerun (2026-11-17): the tank and the healer stood
// on the wall walk over the Ashbury courtyard for 9 to 11 minutes, 62 yd and 16 yd above their leader. The navmesh has
// no path from the ledge to the floor, and the last-resort teleport stays held while a player has the bot in sight
// (design decision 0016 and 0017: a bot never vanishes in a player's view). A player in that spot says so in chat and jumps down.
// After STUCK_SAY_SECONDS the bot says one short line in party chat, and from WALK_OFF_AFTER_SECONDS it jumps to the
// nearest spot (by flat distance, with a small charge per yard of drop) that has a whole path to the player: a lower
// floor or the mesh itself. At most MAX_ATTEMPTS tries per stuck episode, RETRY_SECONDS apart, each from another spot.
// The teleport rule is untouched. A bot with no path to a leader far below it also searches rings out to 30 yd and prefers a
// landing on the leader's floor.
namespace LedgeWalkOffPolicy
{
constexpr time_t STUCK_SAY_SECONDS = 60;
constexpr time_t WALK_OFF_AFTER_SECONDS = 60;
constexpr time_t RETRY_SECONDS = 20;
// Shadowfang Keep proof run 2026-11-23 (build 44): three tries, each "3 read short-listed, no landing", then nothing for 5
// minutes while the group waited for the healer. The shortlist was the same cheapest spots each time (see
// LEVEL_LANDING_COST and the searched list below), so the retries repeated the first search. Each try now goes on to new
// spots; five tries cover the whole ring set.
constexpr unsigned MAX_ATTEMPTS = 5;
// After a jump the bot gets this long to land before the unstuck clock starts afresh from where it is.
constexpr time_t SETTLE_SECONDS = 5;

// Candidate landings on rings round the bot (flat yards) and directions per ring. The first NEAR_RINGS are the nearest edge;
// the outer rings are read only when the leader stands well below a bot with no path (see WantsLeaderFloor).
constexpr float RING_YARDS[] = { 3.0f, 5.0f, 7.0f, 9.0f, 12.0f, 16.0f, 20.0f, 25.0f, 30.0f };
constexpr unsigned RINGS = sizeof(RING_YARDS) / sizeof(RING_YARDS[0]);
constexpr unsigned NEAR_RINGS = 5;
constexpr unsigned DIRECTIONS = 16;
constexpr unsigned CANDIDATE_COUNT = RINGS * DIRECTIONS;
constexpr unsigned NEAR_CANDIDATE_COUNT = NEAR_RINGS * DIRECTIONS;
// How far below the bot the landing may be: the Ashbury island is 16 yd above the courtyard. A jump here is a server
// side spline, so there is no fall damage, but a bounded drop keeps it out of a pit or the next map level.
constexpr float MAX_DROP_YARDS = 20.0f;
// When the leader is well below, the floor it stands on may be a little further down than the usual bound.
constexpr float WIDE_MAX_DROP_YARDS = 30.0f;
// Shadowfang Keep, 2026-11-23: two bots stood 40 to 63 yd from their leader on the Ashbury wall walk, the leader 16 yd below,
// "no path" from the ledge, and the 12 yd search found no landing for 356 s. A bot with no path to a leader this far below
// searches the wide rings, and a landing on the leader's own floor counts as cheaper than any other.
constexpr float LEADER_BELOW_YARDS = 10.0f;
// A landing within this many yards of the leader's height is on the leader's floor.
constexpr float LEADER_FLOOR_BAND_YARDS = 4.0f;
// Taken off the cost of a landing on the leader's floor: more than the longest ring plus its drop, so such a landing is
// searched before any other.
constexpr float LEADER_FLOOR_BONUS = 40.0f;
// A landing may lie this much above the bot (a step up onto the mesh).
constexpr float MAX_RISE_YARDS = 1.5f;
// Each yard of drop counts like this many yards of flat distance when the nearest landing is chosen.
constexpr float DROP_WEIGHT = 0.5f;
// A landing within this many yards of the last try is skipped: the next try goes elsewhere.
constexpr float SAME_SPOT_YARDS = 4.0f;
// The best few landings get a path search; the rest never do.
constexpr unsigned PATH_SEARCHES = 3;
// A landing less than this far below the bot is on the bot's own floor (the wall walk itself, a step beside it). A bot with
// no path to its player stands on a part of the navmesh the player's part does not join, so such a landing has no whole
// path either, unless the bot stands off the mesh. On the Ashbury wall walk those spots were the cheapest by flat
// distance and filled all three path searches of every try, so the floor below was never searched.
constexpr float LEVEL_LANDING_DROP_YARDS = 1.5f;
// Added to the cost of a landing on the bot's own floor: it is searched only after the real drops, never dropped from the
// list (a bot standing on a crate beside the mesh still finds its step).
constexpr float LEVEL_LANDING_COST = 30.0f;
// Landings already given a path search in this episode (PATH_SEARCHES per try, MAX_ATTEMPTS tries).
constexpr unsigned SEARCHED_CAPACITY = MAX_ATTEMPTS * PATH_SEARCHES;
// A bot below this health does not jump (a drop is never a risk to a healthy bot, but a dying one stays put).
constexpr float MIN_HEALTH_PERCENT = 50.0f;

// Offset of candidate `index` (0 to CANDIDATE_COUNT - 1) from the bot.
inline void Offset(unsigned index, float& dx, float& dy)
{
    constexpr float TWO_PI = 6.28318530718f;
    float const angle = TWO_PI * float(index % DIRECTIONS) / float(DIRECTIONS);
    float const ring = RING_YARDS[(index / DIRECTIONS) % RINGS];
    dx = std::cos(angle) * ring;
    dy = std::sin(angle) * ring;
}

// One landing as the caller read it (cheap reads only): where, how far flat from the bot, how far below it (negative
// above), and whether the line from the bot to just above the landing is clear of walls.
struct Candidate
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float flat = 0.0f;
    float drop = 0.0f;
    bool clear = false;
    // The landing lies within LEADER_FLOOR_BAND_YARDS of the leader's height (only read in a wide search).
    bool onLeaderFloor = false;
};

// What one stuck episode remembers (an episode ends when the bot stops qualifying for the unstuck action).
struct Episode
{
    bool said = false;
    // The leader stands well below a bot with no path: this episode's tries search the wide rings (WantsLeaderFloor).
    bool wide = false;
    unsigned attempts = 0;
    time_t nextAttemptAt = 0;
    bool hasLast = false;
    float lastX = 0.0f;
    float lastY = 0.0f;
    // The bot jumped and lands before this time (0 = no jump in flight).
    time_t settleUntil = 0;
    // Every landing a path search was spent on, so a retry goes on to the next spots instead of repeating the search.
    unsigned searchedCount = 0;
    float searchedX[SEARCHED_CAPACITY] = {};
    float searchedY[SEARCHED_CAPACITY] = {};
};

constexpr void NoteSearched(Episode& episode, float x, float y)
{
    if (episode.searchedCount >= SEARCHED_CAPACITY)
        return;
    episode.searchedX[episode.searchedCount] = x;
    episode.searchedY[episode.searchedCount] = y;
    ++episode.searchedCount;
}

constexpr bool NearSearched(Episode const& episode, float x, float y)
{
    for (unsigned i = 0; i < episode.searchedCount; ++i)
    {
        float const dx = x - episode.searchedX[i];
        float const dy = y - episode.searchedY[i];
        if (dx * dx + dy * dy < SAME_SPOT_YARDS * SAME_SPOT_YARDS)
            return true;
    }
    return false;
}

// The bot has no path to its leader and the leader stands more than LEADER_BELOW_YARDS lower (leaderBelowYards: the leader's
// height under the bot's): the wide rings are read and the leader's floor is preferred.
constexpr bool WantsLeaderFloor(bool noPath, float leaderBelowYards)
{
    return noPath && leaderBelowYards > LEADER_BELOW_YARDS;
}

constexpr unsigned CandidateCount(bool wide)
{
    return wide ? CANDIDATE_COUNT : NEAR_CANDIDATE_COUNT;
}

constexpr float MaxDrop(bool wide)
{
    return wide ? WIDE_MAX_DROP_YARDS : MAX_DROP_YARDS;
}

constexpr bool OnLeaderFloor(float landingZ, float leaderZ)
{
    return landingZ - leaderZ <= LEADER_FLOOR_BAND_YARDS && leaderZ - landingZ <= LEADER_FLOOR_BAND_YARDS;
}

// Not eligible: a landing outside the drop bounds or out of sight, one near the spot of the last jump, or one near a spot
// whose path search already failed in this episode.
constexpr bool Eligible(Candidate const& candidate, Episode const& episode)
{
    if (!candidate.clear || candidate.drop > MaxDrop(episode.wide) || candidate.drop < -MAX_RISE_YARDS)
        return false;
    if (episode.hasLast)
    {
        float const dx = candidate.x - episode.lastX;
        float const dy = candidate.y - episode.lastY;
        if (dx * dx + dy * dy < SAME_SPOT_YARDS * SAME_SPOT_YARDS)
            return false;
    }
    return !NearSearched(episode, candidate.x, candidate.y);
}

constexpr float Cost(Candidate const& candidate)
{
    return candidate.flat + DROP_WEIGHT * (candidate.drop > 0.0f ? candidate.drop : 0.0f) +
        (candidate.drop < LEVEL_LANDING_DROP_YARDS ? LEVEL_LANDING_COST : 0.0f) -
        (candidate.onLeaderFloor ? LEADER_FLOOR_BONUS : 0.0f);
}

// The indices of the cheapest eligible landings, cheapest first, at most PATH_SEARCHES (`out` holds that many).
inline unsigned Shortlist(Candidate const* candidates, unsigned count, Episode const& episode, unsigned* out)
{
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        if (!Eligible(candidates[i], episode))
            continue;
        unsigned at = 0;
        while (at < found && Cost(candidates[out[at]]) <= Cost(candidates[i]))
            ++at;
        if (at >= PATH_SEARCHES)
            continue;
        unsigned const last = found < PATH_SEARCHES ? found : PATH_SEARCHES - 1;
        for (unsigned k = last; k > at; --k)
            out[k] = out[k - 1];
        out[at] = i;
        if (found < PATH_SEARCHES)
            ++found;
    }
    return found;
}

constexpr bool HealthOk(float healthPercent)
{
    return healthPercent >= MIN_HEALTH_PERCENT;
}

constexpr bool ShouldSay(time_t stuckFor, Episode const& episode)
{
    return !episode.said && stuckFor >= STUCK_SAY_SECONDS;
}

constexpr bool MayWalkOff(time_t stuckFor, Episode const& episode, time_t now)
{
    return stuckFor >= WALK_OFF_AFTER_SECONDS && episode.attempts < MAX_ATTEMPTS && now >= episode.nextAttemptAt &&
        episode.settleUntil == 0;
}

// Call for every try, found or not.
constexpr void NoteAttempt(Episode& episode, time_t now)
{
    ++episode.attempts;
    episode.nextAttemptAt = now + RETRY_SECONDS;
}

constexpr void NoteJump(Episode& episode, float x, float y, time_t now)
{
    episode.hasLast = true;
    episode.lastX = x;
    episode.lastY = y;
    episode.settleUntil = now + SETTLE_SECONDS;
}

// True while the jump is in flight; false (and the mark cleared) the first time it is over: `landed` is then true once.
constexpr bool Settling(Episode& episode, time_t now, bool& landed)
{
    landed = false;
    if (episode.settleUntil == 0)
        return false;
    if (now < episode.settleUntil)
        return true;
    episode.settleUntil = 0;
    landed = true;
    return false;
}
}  // namespace LedgeWalkOffPolicy

#endif
