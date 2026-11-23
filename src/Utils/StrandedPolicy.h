/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_STRANDED_POLICY_H
#define AWARE_BOTS_STRANDED_POLICY_H

#include <cmath>
#include <cstdint>

// A bot stranded on a piece of walkable navmesh that joins nothing: the deck of the flooded Thousand Needles barge
// (941 polygons, the innkeeper, the flight masters and the Gnome Bar patrons, 6 yd above water whose bed carries the
// real mesh). The server's own mmaps give the deck no path to the water or the shore, so every route from it fails
// (INCOMPLETE ends at the rail), a hand-in trip "gives up: stuck on the way" and the next pick fails the same way: a
// random bot stood there for two days and the owner watched one try for seven minutes. The unseen-only teleports
// (BT-019, BT-222) never fire for a bot a player can see, and a grouped bot that follows a bot leader has no stuck
// recovery out of instances at all.
//
// Owner rule 2026-11-21 (replaces the strict part of BT-019 for this case): a bot stuck for two minutes, seen by a real
// player or not, first gets itself out the way a player would (walks to the nearest rail, jumps or swims to a spot
// that belongs to the open mesh, or side steps), and when that fails uses its Hearthstone (a normal cast, with its cast
// time and cooldown). A bot whose Hearthstone is on cooldown is left to the existing unseen-only teleport rules.
//
// "Stuck" is read from the mesh, not from a goal: the bot's own polygon is flooded with Dijkstra over the walkable
// graph; a search that ends complete with fewer polygons than the buffer holds is a closed pocket (the bot can never
// walk out), anything else is the open mesh. The decision logic is here; the reads and the moves are
// strategy/generic/StrandedRecovery.cpp.
namespace StrandedPolicy
{
// Owner 2026-11-21: two minutes. Counted on the bot's quest clock (active play time), as the other stuck rules do.
constexpr uint32_t STUCK_MS = 120000;
// The mesh read runs at most this often per bot.
constexpr uint32_t READ_GAP_MS = 10000;
// A longer gap between two reads means the bot did not run its AI (or the rule was off): the count starts over.
constexpr uint32_t READ_RESET_MS = 40000;

// ---- the mesh read ------------------------------------------------------------------------------------------------
// The flood returns at most this many polygons. The server's query holds 1024 nodes, and the barge is 941 polygons, so
// the buffer is the largest closed pocket the rule can prove; anything bigger reads as the open mesh (a limit, not a
// guess: a pocket the read cannot see is left alone).
constexpr unsigned PROBE_MAX_POLYS = 1000;
// A bot or a landing counts as standing on a polygon when its closest point is this near (a bot swimming above a bed 50 yd
// down must not be matched to a hull polygon that happens to lie inside the search box).
constexpr float STANDING_YARDS = 3.0f;
// The flood's radius: far beyond any pocket, so only the node and buffer limits end a search in the open mesh.
constexpr float PROBE_RADIUS_YARDS = 2000.0f;

enum class Reading
{
    Skip,    // not judged: dead, fighting, swimming, flying, in transit, no polygon under the bot
    Open,    // joined to the open mesh
    Closed   // a closed pocket: no walk leads out
};

// `foundPolygon`: the bot stands on a polygon. `complete`: the flood ended on its own (no buffer or node overflow).
constexpr Reading Classify(bool foundPolygon, bool complete, unsigned polygons)
{
    if (!foundPolygon)
        return Reading::Skip;
    return complete && polygons < PROBE_MAX_POLYS ? Reading::Closed : Reading::Open;
}

// ---- the landings -------------------------------------------------------------------------------------------------
// Candidates on rings round the bot (flat yards) and directions per ring. The rings reach past the rail of a deck.
constexpr float RING_YARDS[] = { 4.0f, 7.0f, 10.0f, 14.0f, 19.0f, 25.0f, 32.0f, 40.0f };
constexpr unsigned RINGS = sizeof(RING_YARDS) / sizeof(RING_YARDS[0]);
constexpr unsigned DIRECTIONS = 16;
constexpr unsigned CANDIDATE_COUNT = RINGS * DIRECTIONS;
// A jump is a server side spline (no fall damage); the drop is bounded to keep it out of a pit.
constexpr float MAX_DROP_YARDS = 25.0f;
constexpr float MAX_RISE_YARDS = 1.5f;
// Water counts as a swim landing from this depth; shallower is wading ground, read as land.
constexpr float MIN_WATER_DEPTH_YARDS = 1.5f;
// Each yard of drop counts like this many yards of flat distance when the nearest landing is chosen.
constexpr float DROP_WEIGHT = 0.5f;
// A landing within this many yards of the last jump is skipped: the next try goes elsewhere.
constexpr float SAME_SPOT_YARDS = 4.0f;
// The cheapest few landings get a read of the mesh under them; the rest never do.
constexpr unsigned PROBES_PER_TRY = 4;
// Up to this far (flat) the bot jumps from where it stands; farther it walks toward the landing first (the path ends at
// the rail), and the next try jumps.
constexpr float JUMP_REACH_YARDS = 10.0f;

// One landing as the caller read it: where, how far flat from the bot, how far below it (negative above), whether the
// line from the bot to just above the landing is clear of walls, and whether it is a swim landing (water surface).
struct Candidate
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float flat = 0.0f;
    float drop = 0.0f;
    bool clear = false;
    bool water = false;
};

// Offset of candidate `index` (0 to CANDIDATE_COUNT - 1) from the bot.
inline void Offset(unsigned index, float& dx, float& dy)
{
    constexpr float TWO_PI = 6.28318530718f;
    float const angle = TWO_PI * float(index % DIRECTIONS) / float(DIRECTIONS);
    float const ring = RING_YARDS[(index / DIRECTIONS) % RINGS];
    dx = std::cos(angle) * ring;
    dy = std::sin(angle) * ring;
}

constexpr bool Eligible(Candidate const& candidate, bool hasPrevious, float previousX, float previousY)
{
    if (!candidate.clear || candidate.drop > MAX_DROP_YARDS || candidate.drop < -MAX_RISE_YARDS)
        return false;
    float const dx = candidate.x - previousX;
    float const dy = candidate.y - previousY;
    return !hasPrevious || dx * dx + dy * dy >= SAME_SPOT_YARDS * SAME_SPOT_YARDS;
}

constexpr float Cost(Candidate const& candidate)
{
    return candidate.flat + DROP_WEIGHT * (candidate.drop > 0.0f ? candidate.drop : 0.0f);
}

// The indices of the cheapest eligible landings, cheapest first, at most PROBES_PER_TRY (`out` holds that many).
inline unsigned Shortlist(Candidate const* candidates, unsigned count, bool hasPrevious, float previousX,
                          float previousY, unsigned* out)
{
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        if (!Eligible(candidates[i], hasPrevious, previousX, previousY))
            continue;
        unsigned at = 0;
        while (at < found && Cost(candidates[out[at]]) <= Cost(candidates[i]))
            ++at;
        if (at >= PROBES_PER_TRY)
            continue;
        unsigned const last = found < PROBES_PER_TRY ? found : PROBES_PER_TRY - 1;
        for (unsigned k = last; k > at; --k)
            out[k] = out[k - 1];
        out[at] = i;
        if (found < PROBES_PER_TRY)
            ++found;
    }
    return found;
}

constexpr bool InJumpReach(float flat)
{
    return flat <= JUMP_REACH_YARDS;
}

// ---- the episode --------------------------------------------------------------------------------------------------
// Tries to get out on foot: a walk to the rail, the jump, a side step. The Hearthstone comes after them.
constexpr unsigned MAX_GET_OUT_TRIES = 4;
// Between two steps (a walk to the rail takes about this long; a jump lands within SETTLE).
constexpr uint32_t STEP_GAP_MS = 12000;
// After a jump the bot gets this long to land and swim before the next judgement (a read that finds the open mesh ends
// the episode sooner).
constexpr uint32_t JUMP_SETTLE_MS = 8000;
// The Hearthstone is a 10 s cast; the wait covers it and the teleport (real time: the cast runs in world time).
constexpr uint32_t HEARTH_WAIT_MS = 16000;
constexpr unsigned HEARTH_MAX_TRIES = 2;
// A bot that cannot use its Hearthstone looks again at this gap, and tries the steps on foot again after the cycle.
constexpr uint32_t NO_HEARTH_RETRY_MS = 60000;
constexpr uint32_t GET_OUT_CYCLE_MS = 300000;

struct State
{
    // Since when the bot's reads say closed (0 = not now), and the clock of the last read.
    uint32_t closedSinceMs = 0;
    uint32_t lastReadMs = 0;
    unsigned polygons = 0;
    // Steps on foot.
    unsigned getOutTries = 0;
    bool noLanding = false;
    uint32_t nextStepMs = 0;
    uint32_t settleUntilMs = 0;
    bool hasLast = false;
    float lastX = 0.0f;
    float lastY = 0.0f;
    uint32_t exhaustedAtMs = 0;
    // The Hearthstone (hearthSentRealMs is world time, 0 = none sent).
    unsigned hearthTries = 0;
    uint32_t hearthSentRealMs = 0;
    bool noHearthLogged = false;
};

enum class Step
{
    None,
    GetOut,    // walk to the rail, jump or side step
    Hearth,    // cast the Hearthstone
    NoHearth   // nothing left to do now: the stone is not ready (the unseen-only teleport rules stay)
};

inline void ResetEpisode(State& state)
{
    state = State();
}

// Takes one read of the mesh (call at most every READ_GAP_MS, with the bot's quest clock). Anything but a closed read, or
// a gap too long to trust, ends the episode.
inline void Observe(State& state, Reading reading, unsigned polygons, uint32_t clockMs)
{
    uint32_t const previous = state.lastReadMs;
    bool const gap = previous && clockMs - previous > READ_RESET_MS;
    if (reading != Reading::Closed)
    {
        ResetEpisode(state);
        state.lastReadMs = clockMs;
        return;
    }
    if (gap || !state.closedSinceMs)
    {
        ResetEpisode(state);
        state.closedSinceMs = clockMs;
    }
    state.lastReadMs = clockMs;
    state.polygons = polygons;
}

constexpr uint32_t StuckFor(State const& state, uint32_t clockMs)
{
    return state.closedSinceMs ? clockMs - state.closedSinceMs : 0;
}

constexpr bool Stuck(State const& state, uint32_t clockMs, uint32_t stuckMs)
{
    return state.closedSinceMs && StuckFor(state, clockMs) >= stuckMs;
}

constexpr bool GetOutExhausted(State const& state)
{
    return state.noLanding || state.getOutTries >= MAX_GET_OUT_TRIES;
}

// What the bot does now. `clockMs` is the quest clock, `realMs` world time, `hearthReady` whether the stone is in the
// bags, off cooldown and castable (alive, not in combat).
inline Step Decide(State const& state, uint32_t clockMs, uint32_t realMs, bool hearthReady, uint32_t stuckMs)
{
    if (!Stuck(state, clockMs, stuckMs))
        return Step::None;
    if (state.settleUntilMs && clockMs < state.settleUntilMs)
        return Step::None;
    if (state.hearthSentRealMs && realMs - state.hearthSentRealMs < HEARTH_WAIT_MS)
        return Step::None;
    if (clockMs < state.nextStepMs)
        return Step::None;
    if (!GetOutExhausted(state))
        return Step::GetOut;
    if (hearthReady && state.hearthTries < HEARTH_MAX_TRIES)
        return Step::Hearth;
    return Step::NoHearth;
}

enum class Outcome
{
    Jumped,     // the jump order was accepted
    Walking,    // the walk to the rail was ordered
    NoLanding,  // no landing on the open mesh was found: the bot side steps
    Failed      // an order was refused
};

// Call for every try on foot, found or not.
inline void NoteGetOut(State& state, Outcome outcome, float landingX, float landingY, uint32_t clockMs)
{
    ++state.getOutTries;
    state.nextStepMs = clockMs + STEP_GAP_MS;
    if (outcome == Outcome::Jumped)
    {
        state.hasLast = true;
        state.lastX = landingX;
        state.lastY = landingY;
        state.settleUntilMs = clockMs + JUMP_SETTLE_MS;
    }
    else if (outcome == Outcome::NoLanding)
        state.noLanding = true;
    if (GetOutExhausted(state) && !state.exhaustedAtMs)
        state.exhaustedAtMs = clockMs ? clockMs : 1;
}

inline void NoteHearth(State& state, uint32_t realMs)
{
    ++state.hearthTries;
    state.hearthSentRealMs = realMs ? realMs : 1;
}

// The stone is not ready: look again later, and after the cycle try the steps on foot afresh. Returns true the first time
// of an episode, when the caller writes the line.
inline bool NoteNoHearth(State& state, uint32_t clockMs)
{
    state.nextStepMs = clockMs + NO_HEARTH_RETRY_MS;
    if (state.exhaustedAtMs && clockMs - state.exhaustedAtMs >= GET_OUT_CYCLE_MS)
    {
        state.getOutTries = 0;
        state.noLanding = false;
        state.exhaustedAtMs = 0;
        state.nextStepMs = clockMs;
    }
    bool const first = !state.noHearthLogged;
    state.noHearthLogged = true;
    return first;
}
}  // namespace StrandedPolicy

#endif
