/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_BOTAUCTIONPOLICY_H
#define MOD_AWARE_BOTS_BOTAUCTIONPOLICY_H

#include "BotSalePolicy.h"
#include "PricePolicy.h"
#include "ReservePolicy.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

// Pure rules of what a random bot puts on the auction house (design decision 0033, design decision 0034, docs/economy/pricing-audit.md).
// It reuses the vendor trip's protections (BotSalePolicy::ShouldSell and its keep caps), so a bot never lists what it
// would not sell to a vendor: its equipment, bags, quest items, tools, reagents, the profession materials and food it
// keeps, useful gear. It lists whole stacks only, never a part of one, and only what is left over after the keep caps.
// The caller (a future BotAuction.cpp) reads the bot and the items into StackFacts; nothing here touches the game core.
namespace BotAuctionPolicy
{
// ---------------------------------------------------------------------------------------------------------------
// Profession materials in three levels by use (owner decision 2026-11-23). The auction keeps for the bot:
//   Crafting  (60): one of the bot's own crafting professions works the item, at the skill the bot has;
//   Secondary (20): only a secondary skill (Cooking, First Aid, Fishing, Archaeology) uses it, or the bot is a gatherer
//                   (Mining, Herbalism, Skinning) of that kind of material and no crafting profession of its own uses it;
//   None      (0):  no profession of the bot uses it, so every stack is listed.
// The vendor trip keeps its own rule (BotSalePolicy::PROFESSION_MATERIAL_KEEP_MAX for every profession the bot has).
// "Used by" comes from the recipes of the client data and the trainers (BotSale.cpp reads them into RecipeUse rows), not
// from the item class: a recipe counts for a bot that knows it or that a trainer would teach at its skill value.
// ---------------------------------------------------------------------------------------------------------------

// Skill line ids of the client data (SkillLine.dbc); the caller static_asserts them against the core's SKILL_* values.
namespace Skill
{
constexpr uint32_t FIRST_AID = 129;
constexpr uint32_t BLACKSMITHING = 164;
constexpr uint32_t LEATHERWORKING = 165;
constexpr uint32_t ALCHEMY = 171;
constexpr uint32_t HERBALISM = 182;
constexpr uint32_t COOKING = 185;
constexpr uint32_t MINING = 186;
constexpr uint32_t TAILORING = 197;
constexpr uint32_t ENGINEERING = 202;
constexpr uint32_t ENCHANTING = 333;
constexpr uint32_t FISHING = 356;
constexpr uint32_t SKINNING = 393;
constexpr uint32_t JEWELCRAFTING = 755;
constexpr uint32_t INSCRIPTION = 773;
constexpr uint32_t ARCHAEOLOGY = 794;
}  // namespace Skill

enum class MaterialUse : uint8_t
{
    None,       // no profession of the bot uses it
    Gathered,   // the bot gathers this kind of material (or smelts it with Mining) and nothing else uses it
    Secondary,  // a secondary skill of the bot uses it
    Crafting    // a crafting profession of the bot uses it
};

constexpr uint32_t MATERIAL_KEEP_SECONDARY = 20;
constexpr uint32_t MATERIAL_KEEP_CRAFTING = BotSalePolicy::PROFESSION_MATERIAL_KEEP_MAX;

// How many of one material entry the bot holds back before the rest goes to the auction house.
constexpr uint32_t MaterialKeepCap(MaterialUse use)
{
    switch (use)
    {
        case MaterialUse::Crafting:
            return MATERIAL_KEEP_CRAFTING;
        case MaterialUse::Secondary:
        case MaterialUse::Gathered:
            return MATERIAL_KEEP_SECONDARY;
        case MaterialUse::None:
            break;
    }
    return 0;
}

constexpr bool IsCraftingSkill(uint32_t skill)
{
    return skill == Skill::ALCHEMY || skill == Skill::BLACKSMITHING || skill == Skill::ENCHANTING ||
           skill == Skill::ENGINEERING || skill == Skill::INSCRIPTION || skill == Skill::JEWELCRAFTING ||
           skill == Skill::LEATHERWORKING || skill == Skill::TAILORING;
}

constexpr bool IsSecondarySkill(uint32_t skill)
{
    return skill == Skill::COOKING || skill == Skill::FIRST_AID || skill == Skill::FISHING || skill == Skill::ARCHAEOLOGY;
}

// The gathering skill that yields a kind of material (Mining is also a recipe skill: smelting), or 0.
constexpr uint32_t GatheringSkillOf(BotSalePolicy::MaterialKind kind)
{
    switch (kind)
    {
        case BotSalePolicy::MaterialKind::Herb:
            return Skill::HERBALISM;
        case BotSalePolicy::MaterialKind::Metal:
            return Skill::MINING;
        case BotSalePolicy::MaterialKind::Leather:
            return Skill::SKINNING;
        default:
            break;
    }
    return 0;
}

// One recipe of the client data that takes the item as a reagent. rank is the skill value from which a trainer teaches the
// recipe (the lowest reqskillvalue of the trainers that teach it; for a recipe a profession gives on learning, its
// minimum rank), RECIPE_UNTAUGHT when no trainer teaches it (a recipe from an item, a quest or a drop): the bot then
// counts it only when it knows the spell. For ore smelted into a bar that a crafting profession works, the smelting
// spell and its rank are carried in also*: the bot needs both the smelting and the bar's recipe.
constexpr uint32_t RECIPE_UNTAUGHT = UINT32_MAX;

struct RecipeUse
{
    uint32_t skill = 0;
    uint32_t spell = 0;
    uint32_t rank = RECIPE_UNTAUGHT;
    uint32_t alsoSkill = 0;   // 0 for none
    uint32_t alsoSpell = 0;
    uint32_t alsoRank = RECIPE_UNTAUGHT;
};

// A recipe the bot can make: it has the skill, and it either knows the spell or a trainer teaches it at its skill value.
template <typename SkillValueOf, typename KnowsSpell>
inline bool RecipeAvailable(uint32_t skill, uint32_t spell, uint32_t rank, SkillValueOf const& skillValueOf, KnowsSpell const& knowsSpell)
{
    uint32_t const value = skillValueOf(skill);
    return value && (knowsSpell(spell) || (rank != RECIPE_UNTAUGHT && value >= rank));
}

// The level of use of one item for one bot. skillValueOf(skill) is the bot's skill value, 0 when it does not have it;
// knowsSpell(spell) says whether the bot has learned the recipe.
template <typename SkillValueOf, typename KnowsSpell>
inline MaterialUse MaterialUseOf(std::vector<RecipeUse> const& uses, BotSalePolicy::MaterialKind kind,
                                 SkillValueOf skillValueOf, KnowsSpell knowsSpell)
{
    MaterialUse best = MaterialUse::None;
    for (RecipeUse const& use : uses)
    {
        if (!RecipeAvailable(use.skill, use.spell, use.rank, skillValueOf, knowsSpell))
            continue;

        if (use.alsoSkill && !RecipeAvailable(use.alsoSkill, use.alsoSpell, use.alsoRank, skillValueOf, knowsSpell))
            continue;

        if (IsCraftingSkill(use.skill))
            return MaterialUse::Crafting;

        MaterialUse const level = IsSecondarySkill(use.skill) ? MaterialUse::Secondary : MaterialUse::Gathered;
        if (level > best)
            best = level;
    }

    if (best == MaterialUse::None)
        if (uint32_t const gathering = GatheringSkillOf(kind); gathering && skillValueOf(gathering))
            best = MaterialUse::Gathered;

    return best;
}

// One stack in the bot's bags.
struct StackFacts
{
    uint32_t entry = 0;
    uint32_t count = 0;

    // The protections as BotSale::SelectItemsToSell fills them. usableFoodOrDrink means "the bot keeps this kind" before
    // the keep cap: this policy applies the cap across all stacks of the entry, in bag order. facts.professionMaterial is
    // not read here: the auction's own levels below replace the vendor trip's one-size flag.
    BotSalePolicy::ItemFacts facts;

    // The bot's level of use of this item (the keep cap of its three levels), applied across the stacks of the entry.
    MaterialUse material = MaterialUse::None;

    // A trade good (item class 7), of any kind: profession materials are listed when the bot keeps none of them, also the
    // ones the vendor trip does not sell (elemental, parts, hides, sacs).
    bool tradeGood = false;

    // Made by the bot itself with a profession: listed like loot it has no use for.
    bool crafted = false;

    // The core would take it: tradable (Item::CanBeTraded), not conjured, no expiry, not in a mail or auction already.
    bool tradable = false;

    // The item usage value wants it (an upgrade to equip, a quest, a recipe the bot can learn): kept.
    bool usageKeeps = false;

    uint32_t vendorSellPrice = 0;   // copper a vendor pays per item (ItemTemplate::SellPrice)

    bool hasRange = false;          // the price table has the item
    PricePolicy::PriceRange range;
};

// What the visit allows.
struct SellPlan
{
    PricePolicy::HouseRates rates;   // of the house the bot stands at
    uint32_t durationSeconds = PricePolicy::LISTING_SECONDS;   // the base duration of a listing
    // A stack whose price is above this many copper is listed for 24 hours instead (PricePolicy::ListingDuration); 0 keeps
    // every listing at the base duration. The plan judges a stack by its range minimum, the lowest price it can get; the
    // caller settles the final duration and deposit with PricePolicy::ResolveTerms once the price is rolled.
    uint64_t longListingAboveCopper = 0;
    uint32_t maxListings = 0;        // new listings this visit (0 lists nothing)
    uint32_t activeListings = 0;     // listings of this bot already on the house
    uint32_t maxActiveListings = 0;  // the cap on a bot's listings in all
    uint64_t depositBudget = 0;      // what the visit's wallet still allows for deposits (PricePolicy::BudgetLeft)
};

struct Listing
{
    uint32_t stack = 0;        // index into the stacks given
    uint32_t entry = 0;
    uint32_t count = 0;
    uint32_t durationSeconds = 0;  // planned from the range minimum
    uint64_t deposit = 0;      // copper the bot pays now for that duration
    uint64_t minTotal = 0;     // the range minimum times the count
    uint64_t vendorTotal = 0;  // what a vendor would pay for the same stack
};

// Whether a stack may be listed at all: the vendor trip's protections, plus the auction's own conditions. caps applied.
inline bool IsListable(StackFacts const& stack, uint32_t keptBefore, uint32_t& keptAfter)
{
    keptAfter = keptBefore;
    BotSalePolicy::ItemFacts facts = stack.facts;
    facts.professionMaterial = BotSalePolicy::KeepStack(keptBefore, MaterialKeepCap(stack.material));
    facts.usableFoodOrDrink = facts.usableFoodOrDrink && BotSalePolicy::KeepStack(keptBefore, BotSalePolicy::FOOD_DRINK_KEEP_MAX);
    if (stack.tradeGood)
        facts.gatheredMaterial = true;
    if (facts.professionMaterial || facts.usableFoodOrDrink)
        keptAfter = keptBefore + stack.count;

    // The vendor rule asks for a sell price; here the price table stands in for it, and bot-made goods count as items
    // the bot has no use for.
    facts.hasSellPrice = stack.hasRange;
    if (stack.crafted)
        facts.unwantedLoot = true;

    return BotSalePolicy::ShouldSell(facts) && stack.tradable && !stack.usageKeeps && stack.count > 0 &&
           stack.count <= PricePolicy::MAX_LISTING_COUNT && PricePolicy::IsTradable(stack.range);
}

namespace detail
{
// The ways one stack can be listed, the preferred first.
struct Candidate
{
    std::vector<Listing> options;
};
}  // namespace detail

// The stacks to list on this visit, best first: the ones whose range minimum beats the vendor by the most. A stack that
// would not beat the vendor after deposit and cut is left to the vendor trip. Listings stop at the visit's caps and at
// the deposit budget (each deposit counts as spending). Items are matched to stacks by index, so the caller lists
// exactly these stacks, whole.
inline std::vector<Listing> SelectListings(std::vector<StackFacts> const& stacks, SellPlan const& plan)
{
    std::vector<Listing> chosen;
    if (!plan.maxListings || plan.activeListings >= plan.maxActiveListings)
        return chosen;

    std::unordered_map<uint32_t, uint32_t> keptByEntry;
    using detail::Candidate;
    std::vector<Candidate> candidates;
    for (uint32_t i = 0; i < stacks.size(); ++i)
    {
        StackFacts const& stack = stacks[i];
        uint32_t& kept = keptByEntry[stack.entry];
        uint32_t keptAfter = kept;
        bool const listable = IsListable(stack, kept, keptAfter);
        kept = keptAfter;
        if (!listable)
            continue;

        Listing listing;
        listing.stack = i;
        listing.entry = stack.entry;
        listing.count = stack.count;
        if (!PricePolicy::TotalPrice(stack.range.minCopper, stack.count, listing.minTotal) ||
            !PricePolicy::TotalPrice(stack.vendorSellPrice, stack.count, listing.vendorTotal))
            continue;

        // The long listing when the stack is worth it, then the base duration: a doubled deposit can make the long one lose
        // to the vendor or miss the deposit budget, where the base one still works.
        uint32_t const wanted = std::max(plan.durationSeconds, PricePolicy::ListingDuration(listing.minTotal, plan.longListingAboveCopper));
        Candidate candidate;
        for (uint32_t const duration : { wanted, plan.durationSeconds })
        {
            if (!candidate.options.empty() && duration == candidate.options.back().durationSeconds)
                continue;

            Listing option = listing;
            option.durationSeconds = duration;
            if (PricePolicy::ListingBeatsVendor(stack.range, stack.count, stack.vendorSellPrice, plan.rates, duration) &&
                PricePolicy::AuctionDeposit(stack.vendorSellPrice, stack.count, duration, plan.rates, option.deposit))
                candidate.options.push_back(option);
        }
        if (!candidate.options.empty())
            candidates.push_back(candidate);
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](Candidate const& a, Candidate const& b)
    {
        // minTotal beats vendorTotal for every candidate, so the difference cannot go below zero.
        return a.options[0].minTotal - a.options[0].vendorTotal > b.options[0].minTotal - b.options[0].vendorTotal;
    });

    uint64_t depositsLeft = plan.depositBudget;
    uint32_t room = plan.maxActiveListings - plan.activeListings;
    for (Candidate const& candidate : candidates)
    {
        if (chosen.size() >= plan.maxListings || chosen.size() >= room)
            break;

        // A deposit too big for the budget does not stop a cheaper one behind it; a long listing too dear falls back to the short.
        for (Listing const& option : candidate.options)
        {
            if (option.deposit > depositsLeft)
                continue;

            depositsLeft -= option.deposit;
            chosen.push_back(option);
            break;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------------------------------------------
// Pricing one stack at the counter, once the visit has read the house (docs/economy/auction-wiring.md section 4.3).
// ---------------------------------------------------------------------------------------------------------------

// Owner answer 2026-11-22: a listing starts at 95 percent of its buyout, so a human may bid under the buyout and a
// bot only ever pays the buyout (phase 1 does not bid).
constexpr uint32_t START_BID_PERCENT = 95;

// The core refuses a buyout above MAX_MONEY_AMOUNT (Player.h).
constexpr uint64_t MAX_BUYOUT_COPPER = 9999999999ull;

// ceil(95 percent of the buyout), at least 1 copper and never above the buyout.
inline uint64_t StartBidFor(uint64_t buyout)
{
    if (!buyout)
        return 0;

    uint64_t const bid = buyout / 100 * START_BID_PERCENT + (buyout % 100 * START_BID_PERCENT + 99) / 100;
    return std::min(std::max<uint64_t>(bid, 1), buyout);
}

// One auction of the house as the visit's snapshot holds it (copied scalars, never a pointer into the manager).
struct SnapshotListing
{
    uint32_t id = 0;
    uint32_t entry = 0;
    uint32_t count = 0;
    uint64_t buyout = 0;     // 0 means a bid-only auction
    uint64_t bid = 0;
    bool own = false;        // listed by the bot itself
};

// The market of one item: the cheapest unit price among the other sellers' buyouts, and how many listings of the item the
// house shows (the bot's own included: its own stacks crowd the market too).
inline PricePolicy::Market MarketOf(std::vector<SnapshotListing> const& listings, uint32_t entry)
{
    PricePolicy::Market market;
    uint64_t cheapest = 0;
    for (SnapshotListing const& listing : listings)
    {
        if (listing.entry != entry)
            continue;

        ++market.listings;
        if (listing.own || !listing.buyout || !listing.count)
            continue;

        uint64_t const unit = PricePolicy::UnitPriceCeil(listing.buyout, listing.count);
        if (!cheapest || unit < cheapest)
            cheapest = unit;
    }
    market.cheapestUnit = uint32_t(std::min<uint64_t>(cheapest, UINT32_MAX));
    return market;
}

// What a listing will cost and ask: the rolled unit price, the buyout of the whole stack, the start bid, and the terms.
struct Quote
{
    uint32_t unit = 0;
    uint64_t buyout = 0;
    uint64_t startBid = 0;
    PricePolicy::ListingTerms terms;
};

// false when the stack must not be listed now: no usable range, a total that overflows or is above the core's cap, a
// price that does not beat the vendor after the deposit and the cut, or a deposit above what the visit may still pay.
inline bool QuoteListing(StackFacts const& stack, PricePolicy::Market const& market, double roll, double undercutRoll,
                         double mood, SellPlan const& plan, uint64_t depositLeft, Quote& out)
{
    if (!stack.hasRange || !stack.count)
        return false;

    uint32_t const unit = PricePolicy::ListingPrice(stack.range, market, roll, undercutRoll, mood);
    uint64_t total = 0;
    if (!unit || !PricePolicy::TotalPrice(unit, stack.count, total) || total > MAX_BUYOUT_COPPER)
        return false;

    Quote quote;
    if (!PricePolicy::ResolveTerms(total, stack.count, stack.vendorSellPrice, plan.rates, plan.longListingAboveCopper, depositLeft,
                                   quote.terms))
        return false;

    quote.unit = unit;
    quote.buyout = total;
    quote.startBid = StartBidFor(total);
    out = quote;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Buying (docs/economy/auction-wiring.md section 4.4). A bot buys what an existing system already says it needs, at a
// price under the cap of that need, with the money its reserve leaves. The same rule for a bot's and a human's listing.
// ---------------------------------------------------------------------------------------------------------------

// A profession material is bought only for a profession the bot has: up to 60 for one of its crafting professions, up to
// 20 for a secondary skill, and never for one it lacks or for a material it only gathers. The bot holds no more than it
// keeps (MaterialKeepCap), so what it buys is never listed again at once.
inline bool MaterialWanted(MaterialUse use, uint32_t held, uint32_t count)
{
    return (use == MaterialUse::Crafting || use == MaterialUse::Secondary) && count &&
           uint64_t(held) + count <= MaterialKeepCap(use);
}

// Food or drink the bot can use, while it holds fewer than the low water, and never more than the keep cap.
inline bool ConsumableWanted(bool usableFoodOrDrink, uint32_t held, uint32_t count)
{
    return usableFoodOrDrink && count && BotSalePolicy::SupplyLow(held) &&
           uint64_t(held) + count <= BotSalePolicy::FOOD_DRINK_KEEP_MAX;
}

// The need of an item, the strongest first: an upgrade, a bigger bag, a material, a consumable.
constexpr PricePolicy::BuyNeed NeedOf(bool gearUpgrade, bool biggerBag, bool material, bool consumable)
{
    if (gearUpgrade)
        return PricePolicy::BuyNeed::GearUpgrade;
    if (biggerBag)
        return PricePolicy::BuyNeed::Bag;
    if (material)
        return PricePolicy::BuyNeed::ProfessionMaterial;
    if (consumable)
        return PricePolicy::BuyNeed::Consumable;
    return PricePolicy::BuyNeed::None;
}

struct PurchaseCandidate
{
    uint32_t id = 0;          // auction id
    uint32_t entry = 0;
    uint32_t count = 0;
    uint64_t buyout = 0;
    bool own = false;
    PricePolicy::BuyNeed need = PricePolicy::BuyNeed::None;
    bool hasRange = false;
    PricePolicy::PriceRange range;
};

struct Purchase
{
    uint32_t candidate = 0;   // index into the candidates given
    uint32_t id = 0;
    uint32_t entry = 0;
    uint32_t count = 0;
    uint64_t buyout = 0;
    uint32_t capUnit = 0;
    PricePolicy::BuyNeed need = PricePolicy::BuyNeed::None;
};

namespace detail
{
// How far a buyout sits under the cap of its need, in thousandths of the cap (0 at the cap).
inline uint64_t SavingPermille(uint64_t buyout, uint64_t capTotal)
{
    return capTotal > buyout ? (capTotal - buyout) * 1000 / capTotal : 0;
}
}  // namespace detail

// The purchases of this visit, the cheapest against their cap first, as many as maxPurchases and the visit's wallet allow
// (each one is counted against the budget and against the money still held, so the reserve holds for the whole list).
// A bot never buys its own listing, a bid-only auction, an item without a range or one it does not need.
inline std::vector<Purchase> SelectPurchases(std::vector<PurchaseCandidate> const& candidates, ReservePolicy::VisitMoney visit,
                                             uint64_t moneyNow, uint32_t maxPurchases)
{
    struct Ranked
    {
        Purchase purchase;
        uint64_t saving;
    };
    std::vector<Ranked> ranked;
    for (uint32_t i = 0; i < candidates.size(); ++i)
    {
        PurchaseCandidate const& c = candidates[i];
        if (c.own || !c.hasRange || !c.buyout || !c.count || c.need == PricePolicy::BuyNeed::None)
            continue;

        if (!PricePolicy::WillBuy(c.buyout, c.count, c.range, c.need, visit.wallet, moneyNow, visit.reserve))
            continue;

        uint32_t const cap = PricePolicy::BuyCap(c.range, c.need);
        uint64_t capTotal = 0;
        if (!PricePolicy::TotalPrice(cap, c.count, capTotal))
            continue;

        Purchase purchase;
        purchase.candidate = i;
        purchase.id = c.id;
        purchase.entry = c.entry;
        purchase.count = c.count;
        purchase.buyout = c.buyout;
        purchase.capUnit = cap;
        purchase.need = c.need;
        ranked.push_back({ purchase, detail::SavingPermille(c.buyout, capTotal) });
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](Ranked const& a, Ranked const& b) { return a.saving > b.saving; });

    std::vector<Purchase> chosen;
    for (Ranked const& item : ranked)
    {
        if (chosen.size() >= maxPurchases)
            break;

        if (!ReservePolicy::SpendPurchase(visit, moneyNow, item.purchase.buyout))
            continue;

        moneyNow -= item.purchase.buyout;
        chosen.push_back(item.purchase);
    }
    return chosen;
}
}  // namespace BotAuctionPolicy

#endif
