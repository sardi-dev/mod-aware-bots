#ifndef AWARE_BOTS_PULL_BOUNCE_POLICY_H
#define AWARE_BOTS_PULL_BOUNCE_POLICY_H

// BT-167: Avenger's Shield jumps to the nearest other enemies around its target. Live 2026-11-13 (Deadmines run 3) it
// bounced from a marked Overseer onto a stationary Mine Bunny seven yards away: the bunny never came, stayed in combat
// at 72 percent and held the tank and the healer flagged in combat for five minutes. A marked pull must open on the
// mark alone, so a tank with another living enemy near the mark pulls with a single target spell.
namespace PullBouncePolicy
{
// Avenger's Shield jumps to enemies up to this far from the first target (spell data: 5.4.8 chain radius is
// 10 yd plus a margin for the size of the two creatures).
constexpr float BOUNCE_RADIUS = 12.0f;

constexpr bool ShieldWouldBounce(unsigned neighboursNearMark) { return neighboursNearMark > 0; }

constexpr bool InBounceRadius(float distanceToMark) { return distanceToMark <= BOUNCE_RADIUS; }
}

#endif
