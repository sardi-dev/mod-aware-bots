#ifndef AWARE_BOTS_MEMBER_REGROUP_POLICY_H
#define AWARE_BOTS_MEMBER_REGROUP_POLICY_H

#include <ctime>

// BT-190: a damage dealer or healer stood by the leader for minutes while a creature it could not see held a member in
// combat (Deadmines run 5: "attackers=1 los=no ... follow not useful" for 4 min while the tank stood near a watcher on
// the floor below). The rescue is the tank's alone. Such a bot now walks to the tank's side after a while, bounded in
// time, and never gets nearer to the attacker than the tank, so it stays out of the way of the tank's pull.
namespace MemberRegroupPolicy
{
// How long the bot has had no target while a member's attacker stays out of its sight before it walks.
constexpr time_t NO_TARGET_SECONDS = 15;
// How long it walks at most in one such spell; then it waits until the attacker is in sight or gone.
constexpr time_t MAX_WALK_SECONDS = 30;
// It stops this close to the tank.
constexpr float TANK_SIDE_YARDS = 6.0f;
// It stays at least this much farther from the attacker than the tank is.
constexpr float BEHIND_TANK_YARDS = 2.0f;

// Since when the bot has no target while a member's attacker is out of its sight (0 = not now).
constexpr time_t NextSince(bool hiddenAttackerWithoutTarget, time_t since, time_t now)
{
    if (!hiddenAttackerWithoutTarget)
        return 0;
    return since ? since : now;
}

constexpr bool ShouldWalk(time_t since, time_t now, float botToTank, float botToAttacker, float tankToAttacker)
{
    return since != 0 && now - since >= NO_TARGET_SECONDS && now - since < NO_TARGET_SECONDS + MAX_WALK_SECONDS &&
        botToTank > TANK_SIDE_YARDS && botToAttacker > tankToAttacker + BEHIND_TANK_YARDS;
}
}

#endif
