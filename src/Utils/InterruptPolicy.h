/* BT-164: deterministic, game-independent interrupt duty rules. */
#ifndef PLAYERBOTS_INTERRUPT_POLICY_H
#define PLAYERBOTS_INTERRUPT_POLICY_H

#include "BotSkillMath.h"
#include <cstdint>

namespace InterruptPolicy
{
constexpr std::uint32_t STAGGER_MS = 500;
constexpr std::uint32_t GOOD_DELAY_MS = 150;
constexpr std::uint32_t CASUAL_MISS_DIVISOR = 3;

enum class Role : std::uint8_t { TANK, MELEE, RANGED };

// A GUID breaks ties independently of group iteration order.
inline bool Before(Role leftRole, std::uint32_t leftGuid, Role rightRole, std::uint32_t rightGuid)
{
    return leftRole != rightRole ? leftRole < rightRole : leftGuid < rightGuid;
}

// castId must identify the individual cast, not merely the spell's database ID.
inline bool TakesTurn(BotSkillMath::Tier tier, std::uint32_t guid, std::uint32_t castId)
{
    return tier != BotSkillMath::Tier::CASUAL ||
        BotSkillMath::Mix(guid ^ BotSkillMath::Mix(castId)) % CASUAL_MISS_DIVISOR != 0;
}

inline std::uint32_t DelayMs(std::uint32_t rank, BotSkillMath::Tier tier)
{
    return rank * STAGGER_MS + (tier == BotSkillMath::Tier::GOOD ? GOOD_DELAY_MS : 0);
}
} // namespace InterruptPolicy
#endif
