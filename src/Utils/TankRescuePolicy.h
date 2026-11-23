#ifndef TANK_RESCUE_POLICY_H
#define TANK_RESCUE_POLICY_H

#include <cstdint>
#include <ctime>

namespace TankRescuePolicy
{
constexpr unsigned AreaMinimum = 2;
constexpr float AreaRadius = 8.0f;
constexpr unsigned AreaRetrySeconds = 5;

// BT-198: Shadowfang Keep run 7 (2026-11-15): three Tormented Officers held the healer on a floor the tank could not
// walk to, and the rescue picked them first every time (healer first), walked 8 s, gave up, waited 10 s and walked
// again, 119 times in 20 minutes, while the leader was hit and died with no rescue on him. The tank now remembers the
// attackers it gave up on: any give-up sets that attacker aside for RetrySeconds, GiveUpsToIgnore give-ups set it aside
// for IgnoreSeconds, and a give-up also ranks the victim's other attackers after everyone else's for IgnoreSeconds.
// Sight always wins: an attacker the tank can see is never set aside, so a healer who runs back is rescued at once.
constexpr time_t WalkSeconds = 8;
constexpr time_t RetrySeconds = 10;
constexpr unsigned GiveUpsToIgnore = 2;
constexpr time_t IgnoreSeconds = 60;
// Attackers and victims remembered at once; the oldest entry makes room (few attackers are out of reach at any time).
constexpr unsigned LedgerSlots = 8;

struct Priority
{
    bool healer;
    float health;
    bool caster;
    float distance;
    // The victim's attackers were given up on lately (out of the tank's sight): they rank after every other attacker.
    bool unreached = false;
};
// BT-162: protect healers, then weaker members, then prefer casters at equal urgency.
// BT-198: but a victim the tank could not reach lately waits behind victims it can.
inline bool Better(Priority const& a, Priority const& b)
{
    if (a.unreached != b.unreached) return !a.unreached;
    if (a.healer != b.healer) return a.healer;
    if (!a.healer && a.health != b.health) return a.health < b.health;
    if (a.caster != b.caster) return a.caster;
    return a.distance < b.distance;
}
// BT-162: make one area attempt before returning to individual pickups.
inline bool UseArea(unsigned nearbyLoose, bool ready)
{
    return ready && nearbyLoose >= AreaMinimum;
}

// One remembered unit (an attacker or a victim): how many give-ups since the last one expired, when the last was, and
// until when the unit is ignored.
struct GiveUpEntry
{
    uint64_t guid = 0;
    unsigned count = 0;
    time_t lastAt = 0;
    time_t ignoreUntil = 0;
};

struct GiveUpLedger
{
    GiveUpEntry slots[LedgerSlots];
};

// Per bot (PlayerbotAI::rescueGiveUps): the attackers given up on, and the victims they hung on.
struct GiveUpBook
{
    GiveUpLedger attackers;
    GiveUpLedger victims;
};

inline GiveUpEntry const* Find(GiveUpLedger const& ledger, uint64_t guid)
{
    for (GiveUpEntry const& entry : ledger.slots)
        if (entry.guid == guid && entry.guid != 0)
            return &entry;
    return nullptr;
}

// Records a give-up on `guid` at `now`. Returns true when this one started an ignore of IgnoreSeconds (the count of
// give-ups then starts over, so an ignore that ran out gives the attacker two more tries).
inline bool RecordGiveUp(GiveUpLedger& ledger, uint64_t guid, time_t now)
{
    GiveUpEntry* entry = nullptr;
    GiveUpEntry* oldest = &ledger.slots[0];
    for (GiveUpEntry& slot : ledger.slots)
    {
        if (slot.guid == guid)
        {
            entry = &slot;
            break;
        }
        if (slot.lastAt < oldest->lastAt)
            oldest = &slot;
    }
    if (!entry)
    {
        entry = oldest;
        *entry = GiveUpEntry();
        entry->guid = guid;
    }
    // A give-up that is a minute old no longer counts toward an ignore.
    if (entry->count > 0 && now - entry->lastAt >= IgnoreSeconds)
        entry->count = 0;
    ++entry->count;
    entry->lastAt = now;
    if (entry->count < GiveUpsToIgnore)
        return false;
    entry->count = 0;
    entry->ignoreUntil = now + IgnoreSeconds;
    return true;
}

// The tank does not pick this attacker now (it is set aside after a give-up, or ignored).
inline bool SetAside(GiveUpLedger const& ledger, uint64_t guid, time_t now)
{
    GiveUpEntry const* entry = Find(ledger, guid);
    return entry && (now < entry->lastAt + RetrySeconds || now < entry->ignoreUntil);
}

// A give-up on this unit is less than IgnoreSeconds old.
inline bool GaveUpLately(GiveUpLedger const& ledger, uint64_t guid, time_t now)
{
    GiveUpEntry const* entry = Find(ledger, guid);
    return entry && now < entry->lastAt + IgnoreSeconds;
}

// BT-199: why a walk toward an attacker ended without sight, for the [RESCUE] "gave up" line. Run 7 had 59 give-ups and
// no way to tell a missing path from a wall or a floor between the tank and the healer.
enum class GiveUpReason
{
    NO_PATH,           // the navmesh has no route to the attacker
    INCOMPLETE_PATH,  // the route stops short of the attacker
    FLOOR_DIFFERENCE,  // another floor: the height difference is at least FloorDifferenceYards
    TOO_FAR,           // a route exists, but the walk limit covers less than the distance left
    NO_SIGHT,          // walked the whole limit on a route and still no line of sight
};
constexpr float FloorDifferenceYards = 5.0f;
// Running speed is about 7 yd/s, so WalkSeconds cover about this many yards: more left than that is a distance problem.
constexpr float TooFarYards = 40.0f;

constexpr GiveUpReason ClassifyGiveUp(bool noPath, bool incompletePath, float distance, float heightDifference)
{
    if (noPath)
        return GiveUpReason::NO_PATH;
    if (incompletePath)
        return GiveUpReason::INCOMPLETE_PATH;
    float const dz = heightDifference < 0.0f ? -heightDifference : heightDifference;
    if (dz >= FloorDifferenceYards)
        return GiveUpReason::FLOOR_DIFFERENCE;
    if (distance > TooFarYards)
        return GiveUpReason::TOO_FAR;
    return GiveUpReason::NO_SIGHT;
}

constexpr char const* ReasonName(GiveUpReason reason)
{
    switch (reason)
    {
        case GiveUpReason::NO_PATH: return "no-path";
        case GiveUpReason::INCOMPLETE_PATH: return "incomplete-path";
        case GiveUpReason::FLOOR_DIFFERENCE: return "floor-difference";
        case GiveUpReason::TOO_FAR: return "distance";
        case GiveUpReason::NO_SIGHT: return "no-sight";
    }
    return "no-sight";
}

// How the pick treats one attacker of `victim`: false = leave it out now; true = a candidate, `unreached` says whether it
// ranks after the others. `inSight` is asked only for a unit the book lists (a sight test is not free), and sight beats
// memory: an attacker the tank sees is always a normal candidate.
template <typename SightFn>
inline bool Consider(GiveUpBook const& book, uint64_t attacker, uint64_t victim, time_t now, SightFn inSight,
                     bool& unreached)
{
    unreached = false;
    bool const setAside = SetAside(book.attackers, attacker, now);
    bool const victimUnreached = GaveUpLately(book.victims, victim, now);
    if (!(setAside || victimUnreached) || inSight())
        return true;
    if (setAside)
        return false;
    unreached = true;
    return true;
}
}
#endif
