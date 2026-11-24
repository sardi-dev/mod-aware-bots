/* Temple of Kotmogu deterministic policy, independent of game types. */
#ifndef PLAYERBOT_TOK_PLAN_LOGIC_H
#define PLAYERBOT_TOK_PLAN_LOGIC_H
#include <algorithm>
#include <cstdint>
#include <limits>
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
// Rates are measured from public score deltas over one sampling interval.
constexpr std::uint32_t DECISION_MS = 20000;
constexpr std::uint32_t CALL_TTL_MS = 15000;
enum class Stance { BALANCED, HOLD, PUSH, ALL_IN };
struct Projection { double ours, theirs, ourFinish, theirFinish; };
inline Projection Project(unsigned ours, unsigned theirs, unsigned oldOurs, unsigned oldTheirs,
                          std::uint32_t sampleMs, std::uint32_t remainingMs, unsigned win = 1600)
{
    double const a = sampleMs && ours >= oldOurs ? double(ours - oldOurs) / sampleMs : 0;
    double const b = sampleMs && theirs >= oldTheirs ? double(theirs - oldTheirs) / sampleMs : 0;
    double const inf = std::numeric_limits<double>::infinity();
    return {ours + a * remainingMs, theirs + b * remainingMs,
            ours >= win ? 0 : a > 0 ? (win - ours) / a : inf,
            theirs >= win ? 0 : b > 0 ? (win - theirs) / b : inf};
}
inline Stance ChooseStance(Projection p, unsigned ours, unsigned theirs, std::uint32_t remainingMs, Stance previous)
{
    // A margin avoids reacting to score tick noise; a smaller exit margin supplies hysteresis.
    double const margin = previous == Stance::BALANCED ? 80 : 40;
    bool const ahead = ours > theirs && p.ours > p.theirs + margin && p.ourFinish < p.theirFinish;
    bool const behind = theirs > ours && p.theirs > p.ours + margin && p.theirFinish < p.ourFinish;
    if (ahead) return Stance::HOLD;
    if (behind) return remainingMs <= 120000 ? Stance::ALL_IN : Stance::PUSH;
    return Stance::BALANCED;
}
inline Split WithStance(Split s, Stance stance)
{
    if (stance == Stance::HOLD && s.hunt) { --s.hunt; ++s.support; }
    if ((stance == Stance::PUSH || stance == Stance::ALL_IN) && s.support > 1) { --s.support; ++s.hunt; }
    if (stance == Stance::ALL_IN && s.flex) { --s.flex; ++s.hunt; }
    return s;
}
inline bool Fresh(std::uint32_t now, std::uint32_t stamp, std::uint32_t ttl = CALL_TTL_MS)
{ return stamp && std::uint32_t(now - stamp) < ttl; }
inline bool CallAllowed(std::uint32_t now, std::uint32_t lastEquivalent, std::uint32_t lastSender, unsigned tier)
{
    std::uint32_t const gap = tier == 0 ? 20000 : tier == 1 ? 30000 : 45000;
    return (!lastEquivalent || std::uint32_t(now - lastEquivalent) >= 20000) &&
           (!lastSender || std::uint32_t(now - lastSender) >= gap);
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
