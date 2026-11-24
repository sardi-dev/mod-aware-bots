#ifndef PLAYERBOTS_BG_IDLE_TRACKER_H
#define PLAYERBOTS_BG_IDLE_TRACKER_H

#include <cstdint>

// Unsigned millisecond differences remain valid across clock wrap for these short intervals.
struct BgIdleTracker
{
    float x = 0, y = 0, z = 0;
    std::uint32_t since = 0, lastLog = 0, instance = 0;
    bool tracking = false, logged = false;

    bool Update(std::uint32_t now, std::uint32_t match, float px, float py, float pz, bool eligible)
    {
        if (instance != match)
        {
            tracking = false;
            instance = match;
        }
        if (!eligible)
        {
            tracking = false;
            return false;
        }
        float const dx = px - x, dy = py - y, dz = pz - z;
        if (!tracking || dx * dx + dy * dy + dz * dz > 100.0f)
        {
            x = px; y = py; z = pz;
            since = now;
            tracking = true;
            return false;
        }
        if (std::uint32_t(now - since) <= 30000 || (logged && std::uint32_t(now - lastLog) < 30000))
            return false;
        lastLog = now;
        logged = true;
        return true;
    }
};
#endif
