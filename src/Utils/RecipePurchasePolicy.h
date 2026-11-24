/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_RECIPEPURCHASEPOLICY_H
#define MOD_AWARE_BOTS_RECIPEPURCHASEPOLICY_H

#include <algorithm>
#include <cstdint>
#include <vector>

namespace RecipePurchasePolicy
{
struct Offer
{
    uint32_t item = 0;
    uint32_t spell = 0;
    uint32_t skill = 0;
    uint32_t requiredSkill = 0;
    uint32_t currentSkill = 0;
    uint64_t price = 0;
    bool unambiguous = false;
    bool craftingSkill = false;
    bool known = false;
    bool owned = false;
    bool available = false;
    bool goldOnly = false;
    bool usable = false;
};

inline bool Eligible(Offer const& offer)
{
    return offer.item && offer.spell && offer.skill && offer.unambiguous && offer.craftingSkill &&
           offer.currentSkill && offer.currentSkill >= offer.requiredSkill && !offer.known && !offer.owned &&
           offer.available && offer.goldOnly && offer.usable && offer.price;
}

// Plans against one fixed visit wallet. Subtraction avoids overflow when prices are large.
inline std::vector<Offer> PlanPurchases(std::vector<Offer> offers, uint64_t wallet, uint32_t limit)
{
    std::sort(offers.begin(), offers.end(), [](Offer const& a, Offer const& b) {
        return a.price != b.price ? a.price < b.price : a.item < b.item;
    });
    std::vector<Offer> result;
    for (Offer const& offer : offers)
    {
        if (result.size() >= limit)
            break;
        if (!Eligible(offer) || offer.price > wallet)
            continue;
        bool duplicate = false;
        for (Offer const& selected : result)
            if (selected.item == offer.item || selected.spell == offer.spell)
                duplicate = true;
        if (duplicate)
            continue;
        result.push_back(offer);
        wallet -= offer.price;
    }
    return result;
}
} // namespace RecipePurchasePolicy
#endif
