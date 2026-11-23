#ifndef DPS_TARGET_POLICY_H
#define DPS_TARGET_POLICY_H
#include <cstdint>
namespace DpsTargetPolicy
{
constexpr std::uint32_t StickinessMs = 3000;
constexpr unsigned SkullIcon = 7;
constexpr unsigned CrossIcon = 6;
enum Priority { HealerAttacker, MemberAttacker, Skull, Cross, TankTarget, Existing };
// BT-161: rescue threatened group members before following marks or the tank.
inline Priority Rank(bool healerVictim, bool otherNonTankVictim, bool skull, bool cross, bool tankTarget)
{
    if (healerVictim) return HealerAttacker;
    if (otherNonTankVictim) return MemberAttacker;
    if (skull) return Skull;
    if (cross) return Cross;
    if (tankTarget) return TankTarget;
    return Existing;
}
inline bool Keep(bool valid, std::uint32_t now, std::uint32_t since)
{ return valid && std::uint32_t(now - since) < StickinessMs; }
}
#endif
