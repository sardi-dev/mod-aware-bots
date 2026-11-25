/* Pure decisions for earned equipment. */
#ifndef MOD_AWARE_BOTS_GEAREARNPOLICY_H
#define MOD_AWARE_BOTS_GEAREARNPOLICY_H
#include <cstdint>
namespace GearEarnPolicy
{
struct Reward
{
    float score = 0;
    bool equip = false;
    uint32_t level = 0;
    uint32_t price = 0;
};
inline bool BetterReward(Reward const& a, Reward const& b)
{
    if (a.score != b.score) return a.score > b.score;
    if (a.equip != b.equip) return a.equip;
    if (a.level != b.level) return a.level > b.level;
    return a.price > b.price;
}
inline bool BetterZeroScore(float score, float oldScore, bool usable, float value, float oldValue)
{
    return usable && score == 0 && oldScore == 0 && value > oldValue;
}
inline bool Junk(bool protectedItem, bool poor, bool tradeMaterial, bool ownProfession, bool obsoleteQuest)
{
    return !protectedItem && (poor || (tradeMaterial && !ownProfession) || obsoleteQuest);
}
enum Profession : uint32_t
{
    Cooking = 1u << 0, Herbalism = 1u << 1, Alchemy = 1u << 2, Inscription = 1u << 3,
    Mining = 1u << 4, Blacksmithing = 1u << 5, Engineering = 1u << 6, Jewelcrafting = 1u << 7,
    Skinning = 1u << 8, Leatherworking = 1u << 9, Tailoring = 1u << 10, FirstAid = 1u << 11,
    Enchanting = 1u << 12
};
// Only subclasses with a known exclusive use are classified. Parts and elemental goods have mixed uses.
inline uint32_t MaterialProfessions(uint32_t itemClass, uint32_t subclass)
{
    if (itemClass != 7) return 0;
    switch (subclass)
    {
        case 8: return Cooking;
        case 9: return Herbalism | Alchemy | Inscription;
        case 7: return Mining | Blacksmithing | Engineering | Jewelcrafting;
        case 6: return Skinning | Leatherworking;
        case 5: return Tailoring | FirstAid;
        case 12: return Enchanting;
        default: return 0;
    }
}
inline bool IsJunk(bool protectedItem, bool questItem, bool questKept, uint32_t quality,
                   uint32_t itemClass, uint32_t subclass, uint32_t professions)
{
    if (protectedItem) return false;
    if (questItem) return !questKept;
    uint32_t const uses = MaterialProfessions(itemClass, subclass);
    if (uses && (uses & professions)) return false;
    return quality == 0 || (uses && !(uses & professions));
}
inline bool KeepFutureGear(uint32_t requiredLevel, uint32_t level, bool usable, bool better)
{
    return requiredLevel > level && uint64_t(requiredLevel) <= uint64_t(level) + 5 && usable && better;
}
inline bool BetterBag(uint32_t slots, uint64_t price, uint64_t budget, uint32_t bestSlots, uint64_t bestPrice)
{
    return slots && price <= budget && (slots > bestSlots || (slots == bestSlots && price < bestPrice));
}
}
#endif
