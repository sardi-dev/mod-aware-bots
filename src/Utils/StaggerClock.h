/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The clock behind a staggered raid step (a short human reaction before a cube release or a spine throw), kept inside
 * the action instead of a delayed callback that could outlive it. Header-only and free of core types, so tests/ can
 * check it; RaidBotHelpers::StaggerTimer feeds it the game time and a random delay.
 */

#ifndef PLAYERBOTS_STAGGERCLOCK_H
#define PLAYERBOTS_STAGGERCLOCK_H

#include <cstdint>

class StaggerClock
{
public:
    // How long after its delay an armed clock still counts: a few AI ticks; later the step was abandoned.
    static constexpr uint32_t STALE_AFTER_MS = 3000;

    // True once the delay armed for this key has passed (and the clock disarms); otherwise false, arming the clock
    // with delayIfArmingMs when it is not armed for the key or its delay went stale. Times wrap like the game's
    // millisecond clock.
    bool Ready(uint64_t key, uint32_t nowMs, uint32_t delayIfArmingMs)
    {
        if (_armed && _key == key)
        {
            uint32_t const elapsed = nowMs - _armedAtMs;
            if (elapsed < _delayMs)
                return false;

            if (elapsed <= _delayMs + STALE_AFTER_MS)
            {
                _armed = false;
                return true;
            }
        }

        _key = key;
        _armedAtMs = nowMs;
        _delayMs = delayIfArmingMs;
        _armed = true;
        return false;
    }

    uint32_t RemainingMs(uint32_t nowMs) const
    {
        if (!_armed)
            return 0;

        uint32_t const elapsed = nowMs - _armedAtMs;
        return elapsed < _delayMs ? _delayMs - elapsed : 0;
    }

private:
    uint64_t _key = 0;
    uint32_t _armedAtMs = 0;
    uint32_t _delayMs = 0;
    bool _armed = false;
};

#endif
