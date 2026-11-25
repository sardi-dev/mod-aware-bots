#pragma once

namespace MistweaverHealingPolicy
{
inline bool HoldMelee(bool alive, float healthPercent, float lowHealth)
{
    return alive && healthPercent < lowHealth;
}
}
