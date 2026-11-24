/* Temple of Kotmogu deterministic policy, independent of game types. */
#ifndef PLAYERBOT_TOK_PLAN_LOGIC_H
#define PLAYERBOT_TOK_PLAN_LOGIC_H
#include <algorithm>
#include <cstdint>
namespace TokPlan
{
constexpr float RETREAT_HEALTH_PCT = 40.0f;
constexpr float RETURN_HEALTH_PCT = 65.0f;
constexpr float SUPPORT_RADIUS = 40.0f;
constexpr std::uint32_t ZONE_DWELL_MS = 10000;
enum class Role { PICKUP, SUPPORT, HUNT, FLEX };
struct Split { unsigned pickup, support, hunt, flex; };
inline Split Opening(unsigned size)
{
    size = std::min(size, 10u);
    if (!size) return {0, 0, 0, 0};
    // Proportional allocations preserve the approved five and ten player examples.
    unsigned pickup = std::max(1u, std::min(4u, (size * 4 + 5) / 10));
    unsigned support = (size >= 3) ? std::max(1u, (size * 3) / 10) : 0;
    unsigned hunt = (size >= 4) ? std::max(1u, (size * 2) / 10) : 0;
    while (pickup + support + hunt > size) --pickup;
    return {pickup, support, hunt, size - pickup - support - hunt};
}
inline Role RoleAt(Split s, unsigned index)
{
    if (index < s.pickup) return Role::PICKUP;
    index -= s.pickup;
    if (index < s.support) return Role::SUPPORT;
    index -= s.support;
    return index < s.hunt ? Role::HUNT : Role::FLEX;
}
enum class Zone { CENTRE, RING, OUTER };
inline Zone CarrierZone(Zone previous, float health, unsigned support, unsigned seenEnemies,
                        bool ringSafe, std::uint32_t elapsedMs)
{
    bool const immediate = health < RETREAT_HEALTH_PCT && seenEnemies > 0;
    bool const supported = support >= 2 && support >= seenEnemies;
    Zone wanted = immediate && !ringSafe ? Zone::OUTER :
                  (health < RETREAT_HEALTH_PCT || !supported || seenEnemies > support) ? Zone::RING : Zone::CENTRE;
    if (previous != Zone::CENTRE && health <= RETURN_HEALTH_PCT && wanted == Zone::CENTRE)
        wanted = previous;
    if (wanted != previous && !immediate && elapsedMs < ZONE_DWELL_MS)
        return previous;
    return wanted;
}
template <typename Range, typename Predicate>
unsigned CountSeen(Range const& enemies, Predicate seen)
{
    unsigned count = 0;
    for (auto const& enemy : enemies) if (seen(enemy)) ++count;
    return count;
}
}
#endif
