/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "RecipePurchasePolicy.h"
#include "TestHarness.h"

#include <limits>

namespace
{
RecipePurchasePolicy::Offer Recipe(uint32_t item, uint32_t spell, uint64_t price)
{
    RecipePurchasePolicy::Offer offer;
    offer.item = item;
    offer.spell = spell;
    offer.skill = 164;
    offer.requiredSkill = 100;
    offer.currentSkill = 100;
    offer.price = price;
    offer.unambiguous = offer.craftingSkill = offer.available = offer.goldOnly = offer.usable = true;
    return offer;
}
}

TEST(recipe_eligibility_requires_a_usable_unknown_crafting_recipe)
{
    using namespace RecipePurchasePolicy;
    Offer r = Recipe(1, 2, 10);
    CHECK(Eligible(r));
    r.currentSkill = 99; CHECK(!Eligible(r)); r.currentSkill = 100;
    r.craftingSkill = false; CHECK(!Eligible(r)); r.craftingSkill = true;
    r.known = true; CHECK(!Eligible(r)); r.known = false;
    r.owned = true; CHECK(!Eligible(r)); r.owned = false;
    r.unambiguous = false; CHECK(!Eligible(r)); r.unambiguous = true;
    r.available = false; CHECK(!Eligible(r)); r.available = true;
    r.goldOnly = false; CHECK(!Eligible(r)); r.goldOnly = true;
    r.usable = false; CHECK(!Eligible(r)); r.usable = true;
    r.price = 0; CHECK(!Eligible(r));
}

TEST(recipe_plan_is_bounded_deterministic_and_spends_only_the_wallet)
{
    using namespace RecipePurchasePolicy;
    auto plans = PlanPurchases({Recipe(3, 3, 5), Recipe(2, 2, 5), Recipe(1, 1, 4)}, 10, 2);
    CHECK(plans.size() == 2);
    CHECK(plans[0].item == 1 && plans[1].item == 2);
    CHECK(PlanPurchases({Recipe(1, 1, 10)}, 10, 1).size() == 1);
    CHECK(PlanPurchases({Recipe(1, 1, 10)}, 9, 1).empty());
    CHECK(PlanPurchases({Recipe(1, 1, 1)}, 10, 0).empty());
    CHECK(PlanPurchases({Recipe(1, 1, 5), Recipe(1, 1, 5)}, 10, 2).size() == 1);
    CHECK(PlanPurchases({Recipe(1, 1, 5), Recipe(2, 1, 5)}, 10, 2).size() == 1);
    auto max = std::numeric_limits<uint64_t>::max();
    CHECK(PlanPurchases({Recipe(1, 1, max), Recipe(2, 2, max)}, max, 2).size() == 1);
}
