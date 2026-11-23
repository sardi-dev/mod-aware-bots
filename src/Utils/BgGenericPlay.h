/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BGGENERICPLAY_H
#define _PLAYERBOT_BGGENERICPLAY_H

// BT-431: the generic battleground play, for a map that has no tactics of its own (a map a later core adds, Strand of the
// Ancients while its tactics are switched off): the bots go and fight instead of standing at the spawn. Pure logic, free
// of game types so that tests/ can run it.
//
// The decision is a list of candidate options with a deterministic score (design decision 0014, 0026): the rules stay the fallback
// and a later decision layer can pick among the same options. The role is the "bg role" 0 to 9 every bot rolls on
// entering a match, so the split is the same on every bot of a team and needs no team state.

#include <algorithm>
#include <array>
#include <cstdint>

namespace BgGenericPlay
{
enum class Goal : std::uint8_t
{
    PUSH_ENEMY_START,  // fight at the enemy team's start: their reinforcements arrive there, and a match ends on bases
    HOLD_MIDFIELD,     // the point between the two starts: meets whatever crosses the map
    GUARD_OWN_START    // the own start: stops a raid from walking in
};

struct Option
{
    Goal goal;
    int score;
};

// Roles 0 to 5 push, 6 to 8 hold the midfield, 9 guards: six, three and one in ten bots, so a team attacks with most of its
// bots and keeps a few between the starts and one at home.
constexpr std::uint32_t ROLE_COUNT = 10;
constexpr std::uint32_t LAST_PUSH_ROLE = 5;
constexpr std::uint32_t LAST_MIDFIELD_ROLE = 8;

// A bot within this many yards of its goal has arrived: it stays and fights, the goal is chosen again only when its role
// changes (resetObjective re-rolls it rarely) or it dies.
constexpr float ARRIVED_RANGE = 14.0f;

// Score of a role for a goal: its own goal first, the neighbouring goal second, the far one last.
constexpr int FIT_OWN = 100;
constexpr int FIT_NEXT = 40;
constexpr int FIT_FAR = 10;

inline Goal GoalOfRole(std::uint32_t role)
{
    return role <= LAST_PUSH_ROLE ? Goal::PUSH_ENEMY_START
         : role <= LAST_MIDFIELD_ROLE ? Goal::HOLD_MIDFIELD
         : Goal::GUARD_OWN_START;
}

inline int RoleFit(Goal goal, std::uint32_t role)
{
    Goal const own = GoalOfRole(role);
    if (goal == own)
        return FIT_OWN;
    // push and guard are the two ends of the map, the midfield is next to both
    if (goal == Goal::HOLD_MIDFIELD || own == Goal::HOLD_MIDFIELD)
        return FIT_NEXT;
    return FIT_FAR;
}

// The candidates with their scores, from the bot's distance to each goal.
inline std::array<Option, 3> Options(std::uint32_t role, float distToEnemyStart, float distToMidfield, float distToOwnStart)
{
    // a little pull toward the nearer goal breaks ties between bots of one role (one yard is worth 1/50 of a point)
    auto const nearness = [](float dist) { return static_cast<int>(std::min(dist, 500.0f) / 50.0f); };
    return { {
        { Goal::PUSH_ENEMY_START, RoleFit(Goal::PUSH_ENEMY_START, role) - nearness(distToEnemyStart) },
        { Goal::HOLD_MIDFIELD, RoleFit(Goal::HOLD_MIDFIELD, role) - nearness(distToMidfield) },
        { Goal::GUARD_OWN_START, RoleFit(Goal::GUARD_OWN_START, role) - nearness(distToOwnStart) }
    } };
}

// The best option; the first of equal scores wins (the order above is the order of play).
inline Goal Choose(std::array<Option, 3> const& options)
{
    Option best = options[0];
    for (Option const& option : options)
        if (option.score > best.score)
            best = option;
    return best.goal;
}

inline bool Arrived(float distToGoal)
{
    return distToGoal <= ARRIVED_RANGE;
}
}  // namespace BgGenericPlay

#endif
