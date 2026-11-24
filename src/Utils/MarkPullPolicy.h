#ifndef AWARE_BOTS_MARK_PULL_POLICY_H
#define AWARE_BOTS_MARK_PULL_POLICY_H

namespace MarkPullPolicy
{
constexpr float TANK_HEALTH_PERCENT = 80.0f;
constexpr float MEMBER_HEALTH_PERCENT = 60.0f;
constexpr float MANA_PERCENT = 50.0f;
constexpr float BOSS_HEALER_MANA_PERCENT = 70.0f;
constexpr float HEALER_RANGE = 40.0f;
constexpr bool HealthReady(float health, bool tank)
{
    return health >= (tank ? TANK_HEALTH_PERCENT : MEMBER_HEALTH_PERCENT);
}
constexpr bool ManaReady(float mana, bool healer, bool boss)
{
    return mana >= (healer && boss ? BOSS_HEALER_MANA_PERCENT : MANA_PERCENT);
}
// BT-170: how long the tank holds a ready mark for a damage dealer that is still low on mana. Live 2026-11-13
// (Deadmines run 3): the warlock sat at 8 to 23 percent at 8 of 12 held marks and recovers about 0.4 percent a second, so
// the wait ran into the 45 s fallback every time and then pulled with the same empty caster. The healer's bar stays
// strict (70 before a boss, 50 before trash): a healer without mana ends the pull for everyone.
constexpr unsigned DAMAGE_MANA_HOLD_SECONDS = 20;
// True when this member's mana still holds the pull. `cappedDamage` is a damage dealer (not healer, not tank).
constexpr bool ManaWaits(float mana, bool healer, bool boss, bool cappedDamage, unsigned heldSeconds)
{
    return !ManaReady(mana, healer, boss) && !(cappedDamage && heldSeconds >= DAMAGE_MANA_HOLD_SECONDS);
}
// BT-171: a mark beyond the group range but in sight is walked to (the pull action reaches spell range); out of sight
// and beyond the range it waits. Live: a mark at 41.4 yd (range 40) held the tank silently for 62 s.
constexpr float MARK_REACH_YARDS = 60.0f;
constexpr bool MarkReachable(float distance, float groupRange, bool inSight)
{
    return distance <= groupRange || (inSight && distance <= MARK_REACH_YARDS);
}
// BT-181: the group has been ready for a mark that is still not fighting, and the tank stands: the pull cannot reach it.
// Deadmines run 4 held three such marks 45 s with no line. A walk to a mark 40 yd away takes about 6 s.
constexpr long READY_WITHOUT_PULL_SECONDS = 10;
constexpr bool PullStuck(long readySince, long now, bool moving)
{
    return readySince != 0 && now - readySince >= READY_WITHOUT_PULL_SECONDS && !moving;
}
// BT-185: the walk to the pull aimed exactly at the edge of the pull range, so the bot often stopped a fraction of a
// yard outside it and the next move (that fraction) was refused: Deadmines run 5 held a mark in sight 45 s at 36.5 yd.
// The walk now ends this far inside the range, so a finished walk always passes the range test.
constexpr float PULL_RANGE_MARGIN_YARDS = 2.0f;
// The distance between the two bodies the walk aims for; pullRange is centre to centre, as the range test reads it.
constexpr float PullReachDistance(float pullRange, float combatDistance)
{
    float const reach = pullRange - combatDistance - PULL_RANGE_MARGIN_YARDS;
    return reach > 0.0f ? reach : 0.0f;
}
// BT-191: a dungeon boss that keeps "immune to players" until it aggroes by itself cannot be attacked or pulled; it
// wakes when someone comes close (Deadmines run 5: Foe Reaper 5000 kept 0x100 after his stun went, the bots refused
// him, nobody walked up, the Foundry Door behind him stayed shut and the leader died beyond it). The tank walks up to
// such a marked boss instead. Unit flag values of the 5.4.8 core (the caller checks them against the core's enum).
constexpr unsigned FLAG_IMMUNE_TO_PC = 0x00000100;
// Any of these still means "not now": not attackable, a scripted stun or pacify phase, not selectable.
constexpr unsigned BODY_PULL_BLOCKING_FLAGS = 0x00000002u /*NON_ATTACKABLE*/ | 0x00000080u /*NOT_ATTACKABLE_1*/ |
    0x00010000u /*UNK_16*/ | 0x00020000u /*PACIFIED*/ | 0x00040000u /*STUNNED*/ | 0x02000000u /*NOT_SELECTABLE*/;
// How close the tank walks, between the two bodies (in run 4 he woke with the group 5 to 11 yd away).
constexpr float BODY_PULL_YARDS = 5.0f;
// A five-player dungeon, a boss, not friendly, immune to players and nothing else blocking.
constexpr bool BodyPull(bool fivePlayerDungeon, bool boss, bool friendly, unsigned unitFlags)
{
    return fivePlayerDungeon && boss && !friendly && (unitFlags & FLAG_IMMUNE_TO_PC) != 0 &&
        (unitFlags & BODY_PULL_BLOCKING_FLAGS) == 0;
}
// BT-200: Shadowfang Keep run 7 held 17 marks "out of reach and out of sight" with the tank 30 to 80 yd from the leader
// (the leader marks what it sees; the tank stood where it was, and its follow read "failed" 156 times in that run). After this long the
// tank walks to the leader's side, so the marked creature comes into reach or sight the way it does for a player who
// steps up beside the one that marks. It never walks toward the mark and never pulls by this walk: the pull trigger
// stays the only thing that starts a fight.
constexpr long LEADER_WALK_AFTER_SECONDS = 10;
// Nearer than this to the leader the follow already keeps the tank by the leader's side: no walk.
constexpr float LEADER_NEAR_YARDS = 20.0f;
// The walk ends this far from the leader (a tank beside the leader, not on top of it).
constexpr float LEADER_SIDE_YARDS = 8.0f;
// `outOfReachSince`: since when the mark is out of reach and out of sight (0 = it is not).
constexpr bool WalksToLeader(long outOfReachSince, long now, float leaderDistance)
{
    return outOfReachSince != 0 && now - outOfReachSince >= LEADER_WALK_AFTER_SECONDS &&
        leaderDistance > LEADER_NEAR_YARDS;
}
// BT-332: the mark is in the group range but the tank has no sight of it and its own walk to it ends short (Deadmines proof
// run, 2026-11-17: a Goblin Overseer 27 yd away behind a rail, "move refused" every 20 s for 3 minutes until the run was
// stopped, while the leader who marked it, 18 yd from it, saw it). A player tank steps to where the one who marks stands.
// The walk waits a little longer than the out-of-reach one (the pull walk may still be on its way round a corner) and
// starts nearer to the leader (the near rule above is for a tank that follows the leader anyway).
constexpr long BLIND_MARK_WALK_AFTER_SECONDS = 15;
constexpr float BLIND_LEADER_NEAR_YARDS = 12.0f;
// `blindSince`: since when the mark is in range and out of sight (0 = it is not).
constexpr bool WalksToLeaderForSight(long blindSince, long now, float leaderDistance)
{
    return blindSince != 0 && now - blindSince >= BLIND_MARK_WALK_AFTER_SECONDS && leaderDistance > BLIND_LEADER_NEAR_YARDS;
}
// BT-204: a healer blocking a marked pull steps into the tank's sight after ten seconds.
constexpr long HEALER_SIGHT_WAIT_SECONDS = 10;
constexpr long HEALER_SIGHT_WALK_SECONDS = 15;
constexpr bool HealerWalksIntoSight(long blockedSince, long now, float distance, bool inSight)
{
    return blockedSince != 0 && now - blockedSince >= HEALER_SIGHT_WAIT_SECONDS &&
        distance <= HEALER_RANGE && !inSight;
}
constexpr bool HealerSightWalkTimedOut(long walkSince, long now, bool inSight)
{
    return walkSince != 0 && now - walkSince >= HEALER_SIGHT_WALK_SECONDS && !inSight;
}
struct HealerSightState
{
    unsigned long long tankGuid = 0;
    unsigned long long markGuid = 0;
    long blockedSince = 0;
    long walkSince = 0;
    bool failureLogged = false;
};
// BT-290: Shadowfang Keep run 8 held one mark 2 min 33 s on "healer is out of sight" until the run was aborted. A player
// tank in that spot does not wait forever for the healer to find him: he steps back to where the healer can see him (the
// spot he then pulls to, so the pack comes round the corner to the group), and if a wall still stands between them he
// pulls anyway and lets the healer catch up in the fight. The reason text is shared with the healer's own walk.
constexpr char const* HEALER_OUT_OF_SIGHT_REASON = "healer is out of sight";
// A bot healer gets its own wait and walk first (BT-204); a real player healer is not walked by anyone, so the tank
// steps after the plain wait.
constexpr long TANK_SIGHT_STEP_AFTER_BOT_HEALER_SECONDS = HEALER_SIGHT_WAIT_SECONDS + HEALER_SIGHT_WALK_SECONDS;
constexpr long TANK_SIGHT_STEP_AFTER_PLAYER_HEALER_SECONDS = HEALER_SIGHT_WAIT_SECONDS;
// The step ends this far from the healer: inside every heal range, out of her melee.
constexpr float TANK_SIGHT_STEP_YARDS = 10.0f;
// After this long, sight no longer holds this mark: both walks had their time and the healer is within HEALER_RANGE, so
// what is left is a vmap edge (a railing, a step) the in-fight reach handles.
constexpr long HEALER_SIGHT_WAIVE_SECONDS = 60;
constexpr bool TankStepsIntoHealerSight(long since, long now, bool botHealer, bool inSight)
{
    return since != 0 && !inSight &&
        now - since >= (botHealer ? TANK_SIGHT_STEP_AFTER_BOT_HEALER_SECONDS : TANK_SIGHT_STEP_AFTER_PLAYER_HEALER_SECONDS);
}
// The tank's clock for one mark: since when a healer has been out of its sight, and whether that no longer holds the pull.
struct TankSightState
{
    unsigned long long markGuid = 0;
    unsigned long long healerGuid = 0;
    long since = 0;
    bool waived = false;
    bool stepLogged = false;
    bool waiveLogged = false;
};
// `healerGuid` is the healer out of the tank's sight, 0 when every healer sees it. The clock runs per mark from the first
// reading out of sight and does not restart when sight comes back for a moment: a follow move that takes the tank or the
// healer round the corner again would otherwise hold the same pull a fresh minute each time. A waive lasts for the mark.
inline void NoteTankSight(TankSightState& state, unsigned long long markGuid, unsigned long long healerGuid, long now)
{
    if (state.markGuid != markGuid)
    {
        state = TankSightState();
        state.markGuid = markGuid;
    }
    if (state.waived)
        return;
    if (healerGuid && (!state.since || state.healerGuid != healerGuid))
    {
        state.since = now;
        state.healerGuid = healerGuid;
    }
    state.waived = state.since != 0 && now - state.since >= HEALER_SIGHT_WAIVE_SECONDS;
}
constexpr bool SightWaivedFor(TankSightState const& state, unsigned long long markGuid)
{
    return state.waived && state.markGuid == markGuid;
}
// A mark that stays out of reach and out of sight for this long, in a group with no player in it, is dropped: nobody will ever
// come to it (a saved bot group whose leader teleported away logged the same wait 74 times in hours).
constexpr long STALE_MARK_SECONDS = 300;
constexpr bool StaleMark(long unreachableSince, long now, bool humanInGroup)
{
    return !humanInGroup && unreachableSince != 0 && now - unreachableSince >= STALE_MARK_SECONDS;
}
constexpr bool EndPullForDefence(bool pullActive, bool memberAttacked)
{
    return pullActive && memberAttacked;
}
}
#endif
