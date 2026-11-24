#ifndef AWARE_BOTS_LFG_TELEPORT_POLICY_H
#define AWARE_BOTS_LFG_TELEPORT_POLICY_H

#include <cstdint>

// A finder bot the group's dungeon never took in asks for the teleport again once it is free (LfgActions.cpp,
// LfgTeleportRetryAction). The core refuses a bot that fights, falls or is dead, so a few asks cover the usual case; a bot
// whose teleport can never work (no valid entrance, a locked instance) used to ask for the whole run and then sit outside
// as a member that never arrives. After MAX_ASKS asks the bot leaves the group, so the finder can see the place is free.
namespace LfgTeleportPolicy
{
constexpr uint32_t MAX_ASKS = 3;
constexpr uint32_t ASK_GAP_MS = 15000;

enum class Step : uint8_t
{
    Wait,   // too soon after the last ask
    Ask,    // ask for the teleport
    Leave   // asked enough times: leave the group
};

// asks: how many times the bot asked in this group; msSinceLastAsk: 0 when it never asked (any value counts then).
constexpr Step Next(uint32_t asks, bool everAsked, uint32_t msSinceLastAsk)
{
    if (everAsked && msSinceLastAsk < ASK_GAP_MS)
        return Step::Wait;
    return asks >= MAX_ASKS ? Step::Leave : Step::Ask;
}
}

#endif
