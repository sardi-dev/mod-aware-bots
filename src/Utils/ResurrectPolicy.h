/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-157: which dead group member a resurrecting bot goes for, free of game types so that tests/ can run it. The owner's
 * rule (2026-11-12): human players first, out of combat, and never one who already has a resurrect request pending (the core
 * ignores a second request silently while the first is open, so the cast would only cost mana and time).
 */

#ifndef _PLAYERBOT_RESURRECTPOLICY_H
#define _PLAYERBOT_RESURRECTPOLICY_H

namespace ResurrectPolicy
{
// Role rank of a dead member, lower is raised first (the order the value always had: leader and healers, then tanks).
constexpr int ROLE_HEALER = 0;
constexpr int ROLE_TANK = 1;
constexpr int ROLE_OTHER = 2;

// How far from the resurrecting bot a dead member may lie and still be walked to, in yards (the sight distance).
constexpr float MAX_WALK_DISTANCE = 100.0f;

// A released player (a ghost) closer than this to its own corpse is raised through the player himself, as before; farther
// away - or on another map, where the ghost waits at a graveyard - the corpse is the spell's target, as the client does.
constexpr float GHOST_NEAR_CORPSE_DISTANCE = 10.0f;
// The bot casts at a corpse this many yards inside the spell's maximum range: the core checks no range for a corpse target,
// so the margin is the bot's own.
constexpr float CORPSE_CAST_RANGE_MARGIN = 3.0f;
// Used when the spell data names no range.
constexpr float DEFAULT_CORPSE_CAST_RANGE = 20.0f;

// Whether the spell is cast at the corpse instead of the dead player. `ghostToCorpse` is the distance between the two, or
// a negative number when they are not on the same map.
constexpr bool UseCorpseTarget(bool hasCorpse, float ghostToCorpse)
{
    return hasCorpse && (ghostToCorpse < 0.0f || ghostToCorpse > GHOST_NEAR_CORPSE_DISTANCE);
}

// Whether a bot standing `distance` yards from the corpse, with or without line of sight, may cast now.
constexpr bool CorpseInCastRange(float distance, bool inSight, float spellMaxRange)
{
    float const range = (spellMaxRange > 0.0f ? spellMaxRange : DEFAULT_CORPSE_CAST_RANGE + CORPSE_CAST_RANGE_MARGIN) -
        CORPSE_CAST_RANGE_MARGIN;
    return inSight && distance <= range;
}

struct Candidate
{
    bool human;             // a real player, not a bot
    bool pendingRequest;    // a resurrect request is open on him (the core never expires it)
    bool beingResurrected;  // another group member is casting a resurrection on him now
    bool inSight;           // in line of sight and in the cast range of the bot
    int role;               // ROLE_*
    float distance;         // from the bot, in yards
};

constexpr bool Eligible(Candidate const& c) { return !c.pendingRequest && !c.beingResurrected && c.distance <= MAX_WALK_DISTANCE; }

// True when `a` is raised before `b`: a human before a bot, then one in sight before one behind a wall, then by role, then
// the nearer.
constexpr bool Prefer(Candidate const& a, Candidate const& b)
{
    if (a.human != b.human)
        return a.human;
    if (a.inSight != b.inSight)
        return a.inSight;
    if (a.role != b.role)
        return a.role < b.role;
    return a.distance < b.distance;
}
}  // namespace ResurrectPolicy

#endif
