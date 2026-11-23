#ifndef DPS_OFFHEAL_POLICY_H
#define DPS_OFFHEAL_POLICY_H

namespace DpsOffhealPolicy
{
constexpr float HealthLimit = 35.0f;
constexpr float StartMana = 15.0f;
constexpr float StopMana = 30.0f;

// BT-161: keep emergency healing active through the mana gap to avoid oscillation.
inline bool Enabled(bool wasEnabled, bool hasHealer, float highestMana, bool anyLowMana)
{
    if (!hasHealer) return true;
    if (highestMana > StopMana) return false;
    if (anyLowMana) return true;
    return wasEnabled;
}
inline bool Heal(bool enabled, float health) { return enabled && health < HealthLimit; }
}
#endif
