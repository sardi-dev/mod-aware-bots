#ifndef AWARE_BOTS_LFG_LEAVE_POLICY_H
#define AWARE_BOTS_LFG_LEAVE_POLICY_H

#include <cstdint>

// When a bot leaves a finished Dungeon Finder group (LfgMannersAction.cpp). BT-184: in Deadmines run 4 (2026-11-13) the
// bots said bye and left 24 to 42 s after the last boss while a 60 s loot roll was still open; a player waits for the
// rolls. An open roll now holds the goodbye, at most ROLL_PATIENCE_MS after the planned leave time.
namespace LfgLeavePolicy
{
// A bot that waits for loot or for the fight to end leaves after this much more at the latest.
constexpr uint32_t LEAVE_PATIENCE_MS = 45000;
// A group loot roll lasts 60 s, so a roll open at the planned leave time is over within this.
constexpr uint32_t ROLL_PATIENCE_MS = 60000;

// A human can pause an unfinished run without inheriting a bot-only run's deadline.
constexpr bool MayTimeOut(bool expired, bool hasHuman)
{
    return expired && !hasHuman;
}

// timeReached: the planned leave time has come; readyToLeave: out of combat with nothing left to loot;
// sinceLeaveAtMs: how long ago the planned leave time was; rollOpen: a loot roll of the group is still open.
constexpr bool MayLeave(bool timeReached, bool readyToLeave, uint32_t sinceLeaveAtMs, bool rollOpen)
{
    return timeReached && (readyToLeave || sinceLeaveAtMs >= LEAVE_PATIENCE_MS) &&
        (!rollOpen || sinceLeaveAtMs >= ROLL_PATIENCE_MS);
}

// BT-206: the leave plan ran only in a bot's non-combat engine. Shadowfang Keep run 7 (2026-11-15): the healer stayed
// flagged in combat by three Officers that never reached her, so she never left the old finder group after the player was
// gone and kept it alive until the maintainer reset her. A bot held in its combat engine now plans the leaves of a group that
// has no player in it (the orphan leave, a wrapped-up or overdue bot run), once its own fight is over in all but the flag.
// A creature with the bot as its victim this close is a fight even if it has missed so far (as GroupRejoinPolicy).
constexpr float ATTACKER_AT_HAND_YARDS = 6.0f;
// No damage taken for this long (the core damage hook, before absorbs), as the stale-combat drop of BT-203.
constexpr uint32_t FIGHT_QUIET_SECONDS = 30;

constexpr bool FightIsOver(bool attackerAtHand, bool playerAttacker, uint32_t secondsSinceDamage)
{
    return !attackerAtHand && !playerAttacker && secondsSinceDamage >= FIGHT_QUIET_SECONDS;
}

// In the combat engine a bot leaves only a group without a player online (nobody to finish the fight with or to say
// goodbye to) and only once its fight is over; thanks, the goodbye after the last boss and greetings wait for the fight to
// end.
constexpr bool MayLeaveFromCombat(bool humanInGroup, bool fightIsOver)
{
    return !humanInGroup && fightIsOver;
}
}

#endif
