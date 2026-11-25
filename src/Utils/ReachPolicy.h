#ifndef AWARE_BOTS_REACH_POLICY_H
#define AWARE_BOTS_REACH_POLICY_H

// BT-186: the shared combat reach (MovementAction::ReachCombatTo) stopped as soon as the bot stood inside the wanted
// distance, seen or not. A bot on a ledge or a stair landing beside its target never moved to see it: Deadmines run 5
// held the stairs Overseer twice with "[PULL] result=reach-failed reason=already within reach distance=5.0 los=0", and
// the tank stood 4.5 min next to a watcher on the floor below without a hit. Close without sight now walks the path to
// the target's own spot (contact distance); only touching distance without sight still stops.
namespace ReachPolicy
{
// A hidden target needs its position, not a standoff that can move the healer away.
constexpr float HiddenApproachDistance() { return 0.0f; }
constexpr bool UseHealingPath(bool inSight, bool healerInCombat, bool tankInCombat)
{
    return !inSight && (healerInCombat || tankInCombat);
}

enum class Step
{
    ARRIVED,          // inside the wanted distance and in sight, or touching: nothing to walk
    WALK,             // farther than the wanted distance: walk to it
    WALK_TO_CONTACT,  // inside the wanted distance but out of sight: walk the path to the target's own spot
};

// All distances centre to centre; contact is the two combat reaches together.
constexpr Step Decide(float distance, float reach, float contact, bool inSight)
{
    if (distance > reach)
        return Step::WALK;
    if (inSight || distance <= contact)
        return Step::ARRIVED;
    return Step::WALK_TO_CONTACT;
}
}

#endif
