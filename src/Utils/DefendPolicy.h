#ifndef AWARE_BOTS_DEFEND_POLICY_H
#define AWARE_BOTS_DEFEND_POLICY_H
namespace DefendPolicy
{
constexpr bool HoldMark(bool markInCombat, bool memberAttacked, bool tankPullsMarks,
                        bool botInCombat, bool botIsTank, bool hasTank)
{
    return !markInCombat && (memberAttacked || (tankPullsMarks && !botInCombat && !botIsTank && hasTank));
}
}
#endif
