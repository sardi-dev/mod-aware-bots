#ifndef AWARE_BOTS_FLOOR_ATTACKER_POLICY_H
#define AWARE_BOTS_FLOOR_ATTACKER_POLICY_H

#include "PullSightPolicy.h"

#include <cmath>
#include <cstdint>
#include <ctime>

// BT-377: a mob that hits a member from another floor. Stockade proof run (2026-11-17): Hogger stood 5 yd above the
// group and hit one member for 7 minutes; the tank walked 8 s toward him, gave up (no sight, no path), waited and did
// it again 7 times while the group stood stacked with no melee swing. Shadowfang Keep: a Frantic Geist hit the healer
// from 9.6 and then 16 yd below. A player tank steps away from the overlap so the mob must come to him (down a ramp,
// round a corner) into sight, and taunts there. After a rescue give-up the tank and the member under attack now walk
// 12 to 16 yd away along a path on their own floor, to a spot away from the attacker, and the rescue takes the mob
// the moment sight returns (TankRescuePolicy::Consider: sight always wins over a give-up).
//
// MemberRegroupPolicy walks a bot TO the tank and does nothing for a bot already at the tank; this walks both away from
// the mob, so the two never act on the same bot: the regroup is out of combat and for a bot with no victim of its own.
namespace FloorAttackerPolicy
{
// The attacker is on another floor when its height differs by this much (a flight of stairs is about 3 yd).
constexpr float MIN_FLOOR_DZ = 3.0f;
// A mob this close (flat) to its victim hangs on the overlap: a ledge or floor over the member, not a caster far away.
constexpr float MAX_ATTACKER_VICTIM_FLAT_YARDS = 6.0f;
// A bot farther from the attacker than this is a distance problem, not a floor problem (TankRescuePolicy TooFarYards).
constexpr float MAX_BOT_ATTACKER_YARDS = 40.0f;

// Rings round the bot, in yards, and spots per ring (the same angles as the pull sight spots).
constexpr float RING_YARDS[] = { 12.0f, 14.0f, 16.0f };
constexpr unsigned RINGS = sizeof(RING_YARDS) / sizeof(RING_YARDS[0]);
constexpr unsigned SPOTS_PER_RING = PullSightPolicy::SPOTS_PER_RING;
constexpr unsigned SPOT_COUNT = RINGS * SPOTS_PER_RING;
// The spot lies at least this much farther (flat) from the attacker than the bot does: a step away, not sideways.
constexpr float MIN_AWAY_YARDS = 6.0f;
// A spot within this many yards of another awake hostile (not an attacker of the group) could wake a second pack.
constexpr float OTHER_MOB_CLEARANCE_YARDS = 20.0f;
// The best few spots get a path search; the rest never do, so one try costs at most this many.
constexpr unsigned PATH_SEARCHES = 4;
// A retry skips the spot of the last try by this much, so a mob that did not follow is tried from another side.
constexpr float SAME_SPOT_YARDS = 5.0f;

// The walk is ordered for this long (a 16 yd walk takes about 3 s; the rest waits for the mob to come into sight).
constexpr time_t ORDER_SECONDS = 15;
// One search (and so one step) per this many seconds per bot, found or not.
constexpr time_t RETRY_SECONDS = 20;
// At most this many steps per attacker inside the window, then the tank and the member stay and the old give-up rules
// (set aside for a minute) apply: a mob that does not come after three tries will not come.
constexpr unsigned MAX_STEPS = 3;
constexpr time_t STEP_WINDOW_SECONDS = 180;
constexpr float ARRIVED_YARDS = 2.5f;

// The attacker hangs on another floor over (or under) its victim, and the bot is near enough that a step could matter.
constexpr bool NeedsStepAway(float attackerDz, float flatAttackerToVictim, float botToAttacker)
{
    float const dz = attackerDz < 0.0f ? -attackerDz : attackerDz;
    return dz >= MIN_FLOOR_DZ && flatAttackerToVictim <= MAX_ATTACKER_VICTIM_FLAT_YARDS &&
        botToAttacker <= MAX_BOT_ATTACKER_YARDS;
}

// Offset of spot `index` (0 to SPOT_COUNT - 1) from the bot.
inline void Offset(unsigned index, float& dx, float& dy)
{
    PullSightPolicy::RingOffset(RING_YARDS[(index / SPOTS_PER_RING) % RINGS], index, dx, dy);
}

// What the caller read of one spot (all cheap reads, no path search): the straight distance from the bot, how much
// farther from the attacker (flat) than the bot it lies, whether the attacker is in sight from there, and whether
// another awake hostile stands within OTHER_MOB_CLEARANCE_YARDS of it. The caller drops spots on another floor.
struct Spot
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float distance = 0.0f;
    float awayGain = 0.0f;
    bool sees = false;
    bool nearOtherMob = false;
};

constexpr bool Eligible(Spot const& spot, bool hasPrevious, float previousX, float previousY)
{
    if (spot.awayGain < MIN_AWAY_YARDS || spot.nearOtherMob)
        return false;
    float const dx = spot.x - previousX;
    float const dy = spot.y - previousY;
    return !hasPrevious || dx * dx + dy * dy >= SAME_SPOT_YARDS * SAME_SPOT_YARDS;
}

// A spot that sees the attacker first (the rescue can taunt from there at once), then the nearest one.
constexpr bool Ranks(Spot const& a, Spot const& b)
{
    if (a.sees != b.sees)
        return a.sees;
    return a.distance < b.distance;
}

// The indices of the best eligible spots, best first, at most PATH_SEARCHES (`out` holds that many). Returns how many.
inline unsigned Shortlist(Spot const* spots, unsigned count, bool hasPrevious, float previousX, float previousY,
                          unsigned* out)
{
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i)
    {
        if (!Eligible(spots[i], hasPrevious, previousX, previousY))
            continue;
        unsigned at = 0;
        while (at < found && !Ranks(spots[i], spots[out[at]]))
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

// A whole path (not a partial one) that does not go round half the floor.
constexpr bool PathAccepted(bool complete, float pathLength, float straightDistance)
{
    return complete && PullSightPolicy::PathIsShort(pathLength, straightDistance);
}

// Per bot (PlayerbotAI::floorStep): the walk in progress and what limits the next one.
struct Order
{
    uint64_t attacker = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // The walk is ordered until this time (0 = no order).
    time_t until = 0;
    // No new search before this time.
    time_t nextSearchAt = 0;
    // Steps taken for `attacker` since `windowStart`.
    unsigned steps = 0;
    time_t windowStart = 0;
    bool hasLast = false;
    float lastX = 0.0f;
    float lastY = 0.0f;
};

constexpr bool Live(Order const& order, time_t now)
{
    return order.until != 0 && now < order.until;
}

// The steps counted in `order` are for this attacker and still inside their window.
constexpr bool InWindow(Order const& order, uint64_t attacker, time_t now)
{
    return order.attacker == attacker && order.windowStart != 0 && now - order.windowStart < STEP_WINDOW_SECONDS;
}

// A new search for `attacker` is allowed: no walk in progress, the retry gap is over and the step budget is not spent.
constexpr bool MaySearch(Order const& order, uint64_t attacker, time_t now)
{
    if (Live(order, now) || now < order.nextSearchAt)
        return false;
    return !InWindow(order, attacker, now) || order.steps < MAX_STEPS;
}

// Call for every search, found or not (a failed search waits RETRY_SECONDS too).
inline void NoteSearch(Order& order, time_t now)
{
    order.nextSearchAt = now + RETRY_SECONDS;
}

// The spot the last step for `attacker` went to, when it is recent: the next search keeps away from it.
inline bool PreviousSpot(Order const& order, uint64_t attacker, time_t now, float& x, float& y)
{
    if (!order.hasLast || !InWindow(order, attacker, now))
        return false;
    x = order.lastX;
    y = order.lastY;
    return true;
}

inline void Begin(Order& order, uint64_t attacker, float x, float y, float z, time_t now)
{
    if (!InWindow(order, attacker, now))
    {
        order.steps = 0;
        order.windowStart = now;
    }
    ++order.steps;
    order.attacker = attacker;
    order.x = x;
    order.y = y;
    order.z = z;
    order.lastX = x;
    order.lastY = y;
    order.hasLast = true;
    order.until = now + ORDER_SECONDS;
}

constexpr void End(Order& order)
{
    order.until = 0;
}

constexpr bool Arrived(Order const& order, float botX, float botY)
{
    float const dx = order.x - botX;
    float const dy = order.y - botY;
    return dx * dx + dy * dy <= ARRIVED_YARDS * ARRIVED_YARDS;
}
}  // namespace FloorAttackerPolicy

#endif
