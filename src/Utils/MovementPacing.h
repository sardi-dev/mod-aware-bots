/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_MOVEMENT_PACING_H
#define AWARE_BOTS_MOVEMENT_PACING_H

#include <cstdint>

// BT-250 (owner 2026-11-16, "stop-and-go"): a bot decides its next step once per AI tick, and out of combat the ticks
// are 1 to 3 s apart in the open world and 2 to 20 s apart in a rest area (PlayerbotAI::GetReactDelay). A walk that ends
// between two ticks left the bot standing for the rest of the gap, so every leg of a trip ended in a pause. The next
// decision of a bot walking on its own now comes just before the walk ends.
namespace MovementPacing
{
// MoveFarTo keeps a walk that has more than 10 yd left; 0.7 s before the end a running bot (7 yd/s) is about 5 yd away,
// so it plans the next leg while it is still moving and the two splines join without a stop.
constexpr uint32_t LEAD_MS = 700;
// The soonest next tick when the walk is about to end anyway.
constexpr uint32_t FLOOR_MS = 200;

// delay: the next-check delay the react pacing chose. remainingMs: the time left on the bot's own walk, 0 or less when
// it is not walking on its own. Returns the delay to use: never longer than the one chosen.
constexpr uint32_t CapToWalkEnd(uint32_t delay, int32_t remainingMs)
{
    if (remainingMs <= 0)
        return delay;
    uint32_t const left = uint32_t(remainingMs);
    uint32_t const wake = left > LEAD_MS + FLOOR_MS ? left - LEAD_MS : FLOOR_MS;
    return wake < delay ? wake : delay;
}
}  // namespace MovementPacing

#endif
