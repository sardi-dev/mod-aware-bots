#ifndef BOT_MOUNT_POLICY_H
#define BOT_MOUNT_POLICY_H

#include <cstddef>
#include <cstdint>
#include <vector>

// BT-235: on this core Unit::Dismount() clears the mount flag but keeps the mount aura and its speed aura, so a bot left
// in that state runs at mount speed with no mount. These are the pure rules for spotting it.
namespace BotMountPolicy
{
// Unmounted, not on a flight path, and still carrying the mount aura: the aura must go so the speed returns to normal.
inline bool IsStaleMount(bool mounted, bool onFlightPath, bool hasMountAura)
{
    return !mounted && !onFlightPath && hasMountAura;
}

// BT-218: a random bot learns riding at a riding trainer with its own gold, then buys a mount like a player.
constexpr std::uint32_t APPRENTICE_RIDING_SKILL = 75;

// The first mount: riding trained and no mount known yet. On 5.4.8 the riding skill sets the speed, so one mount serves
// every ground tier.
inline bool ShouldBuyMount(std::uint32_t ridingSkill, bool knowsMount)
{
    return ridingSkill >= APPRENTICE_RIDING_SKILL && !knowsMount;
}

// The index of the cheapest price the bot can pay while keeping `reserve`, or -1.
inline int CheapestAffordable(std::vector<std::uint64_t> const& prices, std::uint64_t money, std::uint64_t reserve)
{
    int best = -1;
    for (std::size_t i = 0; i < prices.size(); ++i)
    {
        if (money < reserve || prices[i] > money - reserve)
            continue;
        if (best < 0 || prices[i] < prices[std::size_t(best)])
            best = int(i);
    }
    return best;
}
}

#endif
