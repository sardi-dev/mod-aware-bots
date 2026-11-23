/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_TICK_STAMP_H
#define AWARE_BOTS_TICK_STAMP_H

#include <cstdint>

// BT-699: the rule for facts that are read once per AI tick and shared by every trigger of that tick. Each tick of a bot gets
// a serial that is unique across bots and threads (PlayerbotAI::aiTickSerial). A stamp holds the serial its facts were read
// in; serial 0 means "no tick" (a call outside the AI loop) and never matches, so such a call always reads afresh.
struct TickStamp
{
    std::uint32_t serial = 0;

    // True when the facts must be read again for `tickSerial`; false when they are still those of this tick.
    constexpr bool Renew(std::uint32_t tickSerial)
    {
        if (tickSerial != 0 && tickSerial == serial)
            return false;
        serial = tickSerial;
        return true;
    }
};

#endif
