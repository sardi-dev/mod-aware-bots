#ifndef AWARE_BOTS_STALE_GROUP_POLICY_H
#define AWARE_BOTS_STALE_GROUP_POLICY_H

// A group with no player in it that the bots formed (or kept from an earlier run) and that nothing leads any more. Seen in the
// log of 2026-11-23: three bots stayed in one saved group for days, the leader teleported to a random spot (a leader may,
// a member may not, RandomTeleportToLocs), and the members logged "[FOLLOW] dist=3000 to 4700" for hours while the tank
// waited on a mark it could not reach, 74 times. Each member that has had its leader on another map, offline, or very far
// away for ApartSeconds leaves the group; a lone member then ends the group by itself.
namespace StaleGroupPolicy
{
constexpr unsigned APART_SECONDS = 300;
constexpr float APART_YARDS = 1500.0f;

// The leader is not near enough to lead: offline, on another map, or beyond `apartYards` on this one.
constexpr bool Apart(bool leaderOnline, bool sameMap, float distance, float apartYards)
{
    return !leaderOnline || !sameMap || distance > apartYards;
}

// apartSince: when the member was first found apart (0 = not apart); times in seconds.
constexpr bool MayLeave(long apartSince, long now, unsigned apartSeconds)
{
    return apartSince != 0 && now - apartSince >= long(apartSeconds);
}

// The sweep touches a group only when every member is a random bot (none online as a player, none an offline player or an
// alt) and it is no finder, battleground or raid group, no party BotLfm gathers, and no test scenario runs.
constexpr bool SweepsGroup(bool allRandomBots, bool specialGroup, bool botLfmParty, bool scenarioRunning)
{
    return allRandomBots && !specialGroup && !botLfmParty && !scenarioRunning;
}
}

#endif
