/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_NO_ROUTE_CACHE_H
#define AWARE_BOTS_NO_ROUTE_CACHE_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <tuple>

// BT-697: a shared "no route" memory for the cross-continent route builders (NewRpgRoamTransport.cpp PlanRoute). A failed
// search on the travel graph exhausts it (about 5,600 pops since BT-418), and each of the 300 bots used to prove the
// same No on its own. A No is kept per (map the bot stands on, 256 yd cell of its position, destination map, whether a
// hearth leg was open to it) for ten minutes, the time the trip's own retry gate already keeps a failed trip closed.
// Pure and not thread safe: the caller holds a lock.
namespace NoRouteCache
{
constexpr uint32_t TTL_MS = 10 * 60 * 1000;
constexpr float CELL_YD = 256.0f;
constexpr size_t MAX_ENTRIES = 512;
// One pre-check (a full route build) per bot per minute.
constexpr uint32_t PRECHECK_GAP_MS = 60 * 1000;

struct Key
{
    uint32_t fromMap = 0;
    int32_t cellX = 0;
    int32_t cellY = 0;
    uint32_t destMap = 0;
    bool hearth = false;

    bool operator<(Key const& o) const
    {
        return std::tie(fromMap, cellX, cellY, destMap, hearth) < std::tie(o.fromMap, o.cellX, o.cellY, o.destMap, o.hearth);
    }
};

inline Key MakeKey(uint32_t fromMap, float x, float y, uint32_t destMap, bool hearth)
{
    Key k;
    k.fromMap = fromMap;
    k.cellX = int32_t(std::floor(x / CELL_YD));
    k.cellY = int32_t(std::floor(y / CELL_YD));
    k.destMap = destMap;
    k.hearth = hearth;
    return k;
}

class Cache
{
public:
    bool Known(uint32_t now, Key const& key) const
    {
        auto const itr = m_at.find(key);
        return itr != m_at.end() && uint32_t(now - itr->second) < TTL_MS;
    }

    void Remember(uint32_t now, Key const& key)
    {
        if (m_at.size() >= MAX_ENTRIES && m_at.find(key) == m_at.end())
            MakeRoom(now);
        m_at[key] = now;
    }

    size_t Size() const { return m_at.size(); }

private:
    // Expired entries first; when every entry is live, the oldest one goes.
    void MakeRoom(uint32_t now)
    {
        for (auto itr = m_at.begin(); itr != m_at.end();)
        {
            if (uint32_t(now - itr->second) >= TTL_MS)
                itr = m_at.erase(itr);
            else
                ++itr;
        }
        if (m_at.size() < MAX_ENTRIES)
            return;
        auto oldest = m_at.begin();
        for (auto itr = m_at.begin(); itr != m_at.end(); ++itr)
            if (uint32_t(now - itr->second) > uint32_t(now - oldest->second))
                oldest = itr;
        m_at.erase(oldest);
    }

    std::map<Key, uint32_t> m_at;
};

// lastMs 0 = this bot never pre-checked.
inline bool PrecheckDue(uint32_t lastMs, uint32_t now)
{
    return !lastMs || uint32_t(now - lastMs) >= PRECHECK_GAP_MS;
}
}  // namespace NoRouteCache

#endif
