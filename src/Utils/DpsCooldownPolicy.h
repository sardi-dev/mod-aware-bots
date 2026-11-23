#ifndef DPS_COOLDOWN_POLICY_H
#define DPS_COOLDOWN_POLICY_H

#include <cstdint>
#include <string_view>
namespace DpsCooldownPolicy
{
constexpr std::uint32_t BigCooldownMs = 180000;
constexpr unsigned ElitePackSize = 2;
constexpr float SelfDefenceHealthPct = 50.0f;
// BT-161: save long damage cooldowns for a boss or an elite pack.
inline bool NeedsGate(std::uint32_t cooldownMs, bool bloodlustOrHeroism)
{ return bloodlustOrHeroism || cooldownMs >= BigCooldownMs; }
inline bool Allowed(bool boss, unsigned eliteAttackers)
{ return boss || eliteAttackers >= ElitePackSize; }
// A long cooldown that serves the group's survival or mana, not its damage: it follows the need (a healer out of mana),
// never the weight of the fight. Combat resurrections (Rebirth, Raise Ally) are exempt by their action type.
inline bool ServesTheGroup(std::string_view spell)
{ return spell == "innervate"; }
}
#endif
