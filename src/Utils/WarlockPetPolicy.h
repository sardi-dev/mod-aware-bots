#ifndef AWARE_BOTS_WARLOCK_PET_POLICY_H
#define AWARE_BOTS_WARLOCK_PET_POLICY_H
#include <cstdint>
namespace WarlockPetPolicy
{
constexpr bool NeedsSummon(bool sacrificed, bool alive, uint32_t actual, uint32_t desired, uint32_t upgraded)
{
    return !sacrificed && (!alive || (actual != desired && actual != upgraded));
}
}
#endif
