#ifndef AWARE_BOTS_QUEST_ITEM_SOURCE_POLICY_H
#define AWARE_BOTS_QUEST_ITEM_SOURCE_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// A random bot's trip to a real source of a quest item (BT-211). The DO_QUEST executor only walked to the quest POI and
// left the rest to grinding and looting, so item objectives whose source sits elsewhere (a vendor, a chest, a mob that
// the grind pick passes over) stayed open: "Beer Basted Boar Ribs" had 37 bots stuck, "Exploiting the Situation" 31.
// The sources come from the core's loot and vendor stores; the bot walks to the nearest usable spawn, then kills and
// loots, opens and loots, or buys with its own money. Nothing is ever created for it. A recipe it knows is used when
// the item has no source of its own and the reagents can be fetched the same way. A fetch that finds nothing goes to the
// quest retry book (BT-208).
namespace QuestItemSourcePolicy
{
enum class Kind : uint8_t
{
    Creature,
    GameObject,
    Vendor
};

// Loot rows below this chance (percent) are noise: one boar rib row has 0.0018 %.
constexpr float MIN_DROP_CHANCE = 1.0f;
// A source that is not sure counts as farther: a 25 % drop weighs 300 yd more than a vendor at the same distance.
constexpr float YARDS_PER_MISSING_PERCENT = 4.0f;
// Sources farther than this are in another region the bot is not questing in.
constexpr float MAX_FETCH_DIST = 3000.0f;
// Arrived at a spot: the same distance as a hand-in arrival (QuestHandInPolicy::ARRIVE_DIST).
constexpr float ARRIVE_DIST = 10.0f;
// Out of combat farther than this from the spot, the bot walks back instead of wandering off with the grind.
constexpr float LEASH_DIST = 40.0f;
// A mob, chest or vendor of the source this close to the bot at the spot is "there"; none means the next spot.
constexpr float SOURCE_SIGHT = 50.0f;
// A source mob in sight but farther than this is walked up to (to APPROACH_STOP), so the grind pick sees it.
constexpr float APPROACH_DIST = 25.0f;
constexpr float APPROACH_STOP = 20.0f;
// At a spot this long without a new item: the next spot (mobs respawn elsewhere, chests are opened by others).
constexpr uint32_t SPOT_WAIT_MS = 90 * 1000;
// Spots in a row without a new item before the fetch gives up.
constexpr uint32_t MAX_SPOTS_WITHOUT_ITEM = 5;
// The grind target pick refuses mobs more than this many levels above the bot (GrindTargetValue).
constexpr uint32_t MAX_LEVELS_ABOVE = 4;
// The time budget assumes a slow pace, since fights, eating and detours happen on the way.
constexpr float PLANNED_YARDS_PER_SECOND = 2.5f;
constexpr uint32_t MIN_BUDGET_MS = 10 * 60 * 1000;
constexpr uint32_t MAX_BUDGET_MS = 45 * 60 * 1000;

// What a source asks of the bot, read from the templates by the caller.
struct SourceFacts
{
    Kind kind = Kind::Creature;
    float chance = 100.0f;      // percent per kill or opening; a vendor is 100
    uint32_t minLevel = 0;      // creature
    bool elite = false;         // creature
    bool friendly = false;      // creature: the bot cannot attack it
    bool hostile = false;       // vendor: it will not trade with the bot
    bool skillMissing = false;  // object: a lock skill or key the bot lacks
    bool otherCost = false;     // vendor: extended cost, reputation or stock the bot cannot meet
    uint64_t lotPrice = 0;      // vendor, copper
    uint32_t lotSize = 1;       // vendor: items per purchase
};

struct BotFacts
{
    uint32_t level = 0;
    bool canFightElite = false;
    uint64_t money = 0;
    uint64_t reserve = 0;  // kept for repairs (BotSalePolicy::PurchaseReserve)
};

// Purchases (lots) that bring the bot to `missing` more items without touching the reserve.
inline uint32_t LotsToBuy(uint32_t missing, uint32_t lotSize, uint64_t money, uint64_t reserve, uint64_t lotPrice)
{
    if (!missing)
        return 0;
    uint32_t const size = lotSize ? lotSize : 1;
    uint32_t const lots = (missing + size - 1) / size;
    if (!lotPrice)
        return lots;
    uint64_t const spare = money > reserve ? money - reserve : 0;
    uint64_t const affordable = spare / lotPrice;
    return affordable < lots ? uint32_t(affordable) : lots;
}

// The bot can get the item from this source: it can kill the mob, open the object or pay the vendor.
inline bool Usable(SourceFacts const& source, BotFacts const& bot, uint32_t missing)
{
    if (source.chance < MIN_DROP_CHANCE)
        return false;
    switch (source.kind)
    {
        case Kind::Creature:
            return !source.friendly && source.minLevel <= bot.level + MAX_LEVELS_ABOVE &&
                   (!source.elite || bot.canFightElite);
        case Kind::GameObject:
            return !source.skillMissing;
        case Kind::Vendor:
            return !source.hostile && !source.otherCost &&
                   LotsToBuy(missing, source.lotSize, bot.money, bot.reserve, source.lotPrice) > 0;
    }
    return false;
}

// One spawn of a usable source on the bot's map.
struct Spot
{
    Kind kind = Kind::Creature;
    uint32_t entry = 0;
    uint32_t spawnId = 0;
    uint32_t map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float chance = 100.0f;
};

inline float Dist2d(Spot const& spot, float x, float y)
{
    float const dx = spot.x - x;
    float const dy = spot.y - y;
    return std::sqrt(dx * dx + dy * dy);
}

inline float Score(float dist, float chance)
{
    float const sure = std::min(std::max(chance, 0.0f), 100.0f);
    return dist + (100.0f - sure) * YARDS_PER_MISSING_PERCENT;
}

inline bool Visited(std::vector<uint32_t> const& visited, Spot const& spot)
{
    return std::find(visited.begin(), visited.end(), spot.spawnId) != visited.end();
}

// The best spot on the bot's map within MAX_FETCH_DIST that was not tried yet, or -1. Equal scores go to the lower
// spawn id so the pick is stable.
inline int PickSpot(std::vector<Spot> const& spots, uint32_t map, float x, float y,
                    std::vector<uint32_t> const& visited, float* score = nullptr)
{
    int best = -1;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < spots.size(); ++i)
    {
        Spot const& spot = spots[i];
        if (spot.map != map || Visited(visited, spot))
            continue;
        float const dist = Dist2d(spot, x, y);
        if (dist > MAX_FETCH_DIST)
            continue;
        float const s = Score(dist, spot.chance);
        if (best < 0 || s < bestScore || (s == bestScore && spot.spawnId < spots[best].spawnId))
        {
            best = int(i);
            bestScore = s;
        }
    }
    if (score && best >= 0)
        *score = bestScore;
    return best;
}

// The incomplete item objective to fetch first: the one with the best spot, equal scores to the lower item id.
struct Candidate
{
    uint32_t itemId = 0;
    float score = 0.0f;
};

inline int PickCandidate(std::vector<Candidate> const& candidates)
{
    int best = -1;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        Candidate const& c = candidates[i];
        if (best < 0 || c.score < candidates[best].score ||
            (c.score == candidates[best].score && c.itemId < candidates[best].itemId))
            best = int(i);
    }
    return best;
}

// A recipe for an item that has no source of its own: cast it when the reagents are in the bags, fetch the first missing
// reagent that has a source, skip the recipe when a missing reagent has none (or it takes no reagents: not crafting).
struct Reagent
{
    uint32_t item = 0;
    uint32_t need = 0;
    uint32_t have = 0;
    bool obtainable = false;
};

enum class CraftStep
{
    Cast,
    FetchReagent,
    Skip
};

inline CraftStep PlanCraft(std::vector<Reagent> const& reagents, std::size_t& fetchIdx)
{
    if (reagents.empty())
        return CraftStep::Skip;
    bool missing = false;
    for (std::size_t i = 0; i < reagents.size(); ++i)
    {
        if (reagents[i].have >= reagents[i].need)
            continue;
        if (!reagents[i].obtainable)
            return CraftStep::Skip;
        if (!missing)
            fetchIdx = i;
        missing = true;
    }
    return missing ? CraftStep::FetchReagent : CraftStep::Cast;
}

// Casts of a recipe that makes `perCast` items to cover `missing` items.
inline uint32_t CastsFor(uint32_t missing, uint32_t perCast)
{
    uint32_t const per = perCast ? perCast : 1;
    return (missing + per - 1) / per;
}

struct Fetch
{
    bool active = false;
    uint32_t questId = 0;
    uint32_t itemId = 0;     // fetched now: the quest item, or a reagent while crafting it
    uint32_t forItemId = 0;  // the quest item a reagent is for (0 = the quest item itself)
    uint32_t needed = 0;     // bag count that ends the fetch
    Spot spot;
    std::vector<uint32_t> creatureEntries;  // every usable mob that drops the item, preferred by the grind pick
    std::vector<uint32_t> visited;          // spots tried without an item
    uint32_t startMs = 0;
    uint32_t budgetMs = 0;
    uint32_t spotMs = 0;  // arrival at the current spot (0 = not yet)
    uint32_t lastCount = 0;
    uint32_t spotsWithoutItem = 0;
};

enum class Step
{
    Travel,
    Work,
    NextSpot,
    Done,
    Expired
};

inline uint32_t BudgetMs(float dist)
{
    if (dist < 0.0f)
        dist = 0.0f;
    float const ms = float(MIN_BUDGET_MS) + dist / PLANNED_YARDS_PER_SECOND * 1000.0f;
    return ms >= float(MAX_BUDGET_MS) ? MAX_BUDGET_MS : uint32_t(ms);
}

inline Fetch Start(uint32_t questId, uint32_t itemId, uint32_t forItemId, uint32_t needed, uint32_t count,
                   Spot const& spot, float dist, uint32_t now)
{
    Fetch fetch;
    fetch.active = true;
    fetch.questId = questId;
    fetch.itemId = itemId;
    fetch.forItemId = forItemId;
    fetch.needed = needed;
    fetch.spot = spot;
    fetch.startMs = now;
    fetch.budgetMs = BudgetMs(dist);
    fetch.lastCount = count;
    return fetch;
}

inline bool Expired(Fetch const& fetch, uint32_t now)
{
    return fetch.active && uint32_t(now - fetch.startMs) >= fetch.budgetMs;
}

inline bool IsSourceEntry(Fetch const& fetch, uint32_t entry)
{
    return fetch.active &&
           std::find(fetch.creatureEntries.begin(), fetch.creatureEntries.end(), entry) != fetch.creatureEntries.end();
}

// True when a move to (map, x, y) is this fetch's move to its spot, which must never end in a stuck teleport.
inline bool SameDest(Fetch const& fetch, uint32_t map, float x, float y)
{
    if (!fetch.active || fetch.spot.map != map)
        return false;
    float const dx = fetch.spot.x - x;
    float const dy = fetch.spot.y - y;
    return dx * dx + dy * dy < 1.0f;
}

// One tick of a running fetch: dist is the bot's distance to the spot, count the item in its bags.
inline Step Judge(Fetch& fetch, uint32_t now, float dist, uint32_t count)
{
    if (count >= fetch.needed)
        return Step::Done;
    if (count > fetch.lastCount)
    {
        // A new item: the spot is worth staying at, and the give-up count starts over.
        fetch.spotsWithoutItem = 0;
        if (fetch.spotMs)
            fetch.spotMs = now ? now : 1;
    }
    fetch.lastCount = count;
    if (Expired(fetch, now))
        return Step::Expired;
    if (!fetch.spotMs)
    {
        if (dist > ARRIVE_DIST)
            return Step::Travel;
        // 0 means "not yet", so an arrival at tick 0 is kept as tick 1.
        fetch.spotMs = now ? now : 1;
        return Step::Work;
    }
    return uint32_t(now - fetch.spotMs) >= SPOT_WAIT_MS ? Step::NextSpot : Step::Work;
}

// The current spot gave nothing (waited out, or the mob, object or vendor is not there). True when the fetch gives up.
inline bool LeaveSpot(Fetch& fetch)
{
    fetch.visited.push_back(fetch.spot.spawnId);
    fetch.spotMs = 0;
    return ++fetch.spotsWithoutItem >= MAX_SPOTS_WITHOUT_ITEM;
}

inline void GoToSpot(Fetch& fetch, Spot const& spot)
{
    fetch.spot = spot;
    fetch.spotMs = 0;
}

inline char const* KindName(Kind kind)
{
    switch (kind)
    {
        case Kind::Creature: return "creature loot";
        case Kind::GameObject: return "object loot";
        case Kind::Vendor: return "vendor";
    }
    return "unknown";
}

inline char const* StepName(Step step)
{
    switch (step)
    {
        case Step::Travel: return "travel";
        case Step::Work: return "work";
        case Step::NextSpot: return "nothing at the spot";
        case Step::Done: return "done";
        case Step::Expired: return "out of time";
    }
    return "unknown";
}
}  // namespace QuestItemSourcePolicy

#endif
