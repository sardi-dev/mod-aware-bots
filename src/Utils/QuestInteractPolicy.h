#ifndef AWARE_BOTS_QUEST_INTERACT_POLICY_H
#define AWARE_BOTS_QUEST_INTERACT_POLICY_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

// A random bot's trip to the creature or object that really gives a quest's creature credit (BT-212). Many creature
// objectives name a credit marker that no player kills: "Dearest Natalia" credits two invisible triggers when the bot
// questions Rutgar and Frankal, "Aid for the Wounded" credits a bunny when the first aid kit is used on a wounded
// mountaineer, "Evacuate the Merchant Square" credits a facing marker when a door is opened. The grind at the quest POI
// never earns those, so 24 to 27 bot logs kept each of them open. The core's own data says how the credit is given
// (the SmartAI row that gives it and its event, the spell click, the quest item's spell and its target condition, the
// object's spell); the bot travels to that target and does what a player does: talks, clicks, uses the item or opens
// the object. The core awards the credit. Vehicle and escort quests are left out.
namespace QuestInteractPolicy
{
enum class Way : uint8_t
{
    Gossip,
    Click,
    UseItem,
    OpenObject
};

// Why an interaction stays out of the index.
enum class Skip : uint8_t
{
    None,
    Vehicle,
    Escort
};

// SmartAI event and action ids read by the index (SmartScriptMgr.h).
constexpr uint32_t EVENT_SPELLHIT = 8;
constexpr uint32_t EVENT_LINK = 61;
constexpr uint32_t EVENT_GOSSIP_SELECT = 62;
constexpr uint32_t EVENT_GOSSIP_HELLO = 64;
constexpr uint32_t EVENT_GO_STATE_CHANGED = 70;
constexpr uint32_t EVENT_ON_SPELLCLICK = 73;
constexpr uint32_t ACTION_CAST = 11;
constexpr uint32_t ACTION_KILLEDMONSTER = 33;
constexpr uint32_t ACTION_WP_START = 53;
constexpr uint32_t ACTION_TIMED_ACTIONLIST = 80;
constexpr uint32_t ACTION_INVOKER_CAST = 85;
constexpr uint32_t ACTION_CROSS_CAST = 86;
constexpr uint32_t ACTION_RANDOM_TIMED_ACTIONLIST = 87;

// A linked chain is short; a loop in bad data must not hang the index build.
constexpr std::size_t MAX_LINK_DEPTH = 8;
// Gossip menus a route may cross before the option that gives the credit (Rutgar's chain has seven pages).
constexpr std::size_t MAX_ROUTE_MENUS = 4;
constexpr uint32_t MAX_GOSSIP_PAGES = 10;
// C++ gossip scripts give their options an action from 1000 on (GOSSIP_ACTION_INFO_DEF).
constexpr uint32_t SCRIPT_ACTION_BASE = 1000;
constexpr uint32_t GOSSIP_OPTION_GOSSIP = 1;

// Targets farther than this are in another region the bot is not questing in (as the item fetch).
constexpr float MAX_TRIP_DIST = 3000.0f;
// A phased spawn shows only to a bot in that phase (the quest usually gives it); an unphased one goes first.
constexpr float PHASED_PENALTY = 300.0f;
constexpr float ARRIVE_DIST = 10.0f;
// A target of the interaction this close to the bot at the spot is "there"; none means the next spot.
constexpr float TARGET_SIGHT = 50.0f;
// The core needs a moment after an act (a cast, a script timer) before the counter moves.
constexpr uint32_t ACT_GAP_MS = 4 * 1000;
// Acts at one spot without a new credit before the bot tries the next spot.
constexpr uint32_t MAX_ACTS_WITHOUT_CREDIT = 3;
// At a spot this long without a new credit: the next spot.
constexpr uint32_t SPOT_WAIT_MS = 60 * 1000;
// Spots in a row without a new credit before the trip gives up.
constexpr uint32_t MAX_SPOTS_WITHOUT_CREDIT = 4;
constexpr float PLANNED_YARDS_PER_SECOND = 2.5f;
constexpr uint32_t MIN_BUDGET_MS = 5 * 60 * 1000;
constexpr uint32_t MAX_BUDGET_MS = 40 * 60 * 1000;

// What the bot knows about a credit entry: whether the kill pick can earn it the normal way.
struct CreditFacts
{
    bool spawned = false;     // an open-world spawn of the entry exists
    bool attackable = false;  // the bot may attack it (faction, unit flags)
    bool trigger = false;     // a trigger creature or one no player can select
};

// The grind earns a credit only from a spawned creature the bot can attack and see.
inline bool KillEarnsCredit(CreditFacts const& facts)
{
    return facts.spawned && facts.attackable && !facts.trigger;
}

// The interaction planner takes over only where the index knows how the credit is given and killing cannot give it.
inline bool NeedsInteraction(CreditFacts const& facts, bool known)
{
    return known && !KillEarnsCredit(facts);
}

// Vehicle quests (the click seats the bot, or the target is a vehicle) and escorts (the target starts a waypoint walk)
// need a driver the bots do not have.
inline Skip Classify(bool targetIsVehicle, bool rideSpell, bool startsEscort)
{
    if (targetIsVehicle || rideSpell)
        return Skip::Vehicle;
    if (startsEscort)
        return Skip::Escort;
    return Skip::None;
}

// One SmartAI row of a script, as far as the link chase needs it.
struct SmartRow
{
    uint32_t id = 0;
    uint32_t link = 0;
    uint32_t eventType = 0;
};

// The row whose event starts the chain that reaches rows[idx]: a linked row (event 61) climbs to the row that links to
// it. -1 when the chain is broken or loops.
inline int RootRow(std::vector<SmartRow> const& rows, std::size_t idx)
{
    if (idx >= rows.size())
        return -1;
    std::size_t current = idx;
    for (std::size_t depth = 0; depth < MAX_LINK_DEPTH; ++depth)
    {
        if (rows[current].eventType != EVENT_LINK)
            return int(current);
        bool found = false;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (i != current && rows[i].link && rows[i].link == rows[current].id)
            {
                current = i;
                found = true;
                break;
            }
        if (!found)
            return -1;
    }
    return -1;
}

// The player action that fires a SmartAI event, or false for events a player does not start by hand (timers, deaths,
// line of sight). A spell hit needs the item or click that casts the spell, which the caller looks up.
inline bool WayForEvent(uint32_t eventType, bool onObject, Way& way)
{
    switch (eventType)
    {
        case EVENT_GOSSIP_SELECT:
        case EVENT_GOSSIP_HELLO:
            way = onObject ? Way::OpenObject : Way::Gossip;
            return true;
        case EVENT_ON_SPELLCLICK:
            if (onObject)
                return false;
            way = Way::Click;
            return true;
        case EVENT_GO_STATE_CHANGED:
            if (!onObject)
                return false;
            way = Way::OpenObject;
            return true;
        default:
            return false;
    }
}

// One gossip option of a database menu and the menu it opens.
struct MenuEdge
{
    uint32_t menu = 0;
    uint32_t option = 0;
    uint32_t next = 0;
};

struct GossipStep
{
    uint32_t menu = 0;
    uint32_t option = 0;
};

// The options to pick from the creature's first menu to the option that gives the credit, the last step included. Empty
// when the target menu cannot be reached within MAX_ROUTE_MENUS (a script opens it, or the data is broken).
inline std::vector<GossipStep> GossipRoute(uint32_t start, uint32_t targetMenu, uint32_t targetOption,
                                           std::vector<MenuEdge> const& edges)
{
    std::vector<GossipStep> route;
    if (!start || !targetMenu)
        return route;
    // Breadth first, so the shortest walk wins; parent edge per reached menu.
    std::unordered_map<uint32_t, int> parent;  // menu -> index of the edge that reached it (-1 for the start)
    parent[start] = -1;
    std::deque<std::pair<uint32_t, std::size_t>> open;
    open.emplace_back(start, 0);
    while (!open.empty() && !parent.count(targetMenu))
    {
        auto const [menu, depth] = open.front();
        open.pop_front();
        if (depth + 1 >= MAX_ROUTE_MENUS)
            continue;
        for (std::size_t i = 0; i < edges.size(); ++i)
            if (edges[i].menu == menu && edges[i].next && !parent.count(edges[i].next))
            {
                parent[edges[i].next] = int(i);
                open.emplace_back(edges[i].next, depth + 1);
            }
    }
    auto const reached = parent.find(targetMenu);
    if (reached == parent.end())
        return route;
    route.push_back({targetMenu, targetOption});
    for (int edge = reached->second; edge >= 0; edge = parent[edges[edge].menu])
        route.push_back({edges[edge].menu, edges[edge].option});
    std::reverse(route.begin(), route.end());
    return route;
}

// One option the core shows in the bot's gossip window.
struct ShownOption
{
    uint32_t key = 0;
    uint32_t optionType = 0;
    uint32_t boxMoney = 0;
    bool coded = false;
};

inline bool Shown(std::vector<ShownOption> const& shown, uint32_t key)
{
    for (ShownOption const& option : shown)
        if (option.key == key)
            return true;
    return false;
}

// The option to pick on this page of a known route, or -1 (the page is not on the route or the option is not offered).
inline int64_t PickRouteOption(std::vector<GossipStep> const& route, uint32_t currentMenu,
                               std::vector<ShownOption> const& shown)
{
    for (GossipStep const& step : route)
        if (step.menu == currentMenu && Shown(shown, step.option))
            return int64_t(step.option);
    return -1;
}

// A C++ gossip chain the bot talks through: the first plain chat option of each page, never one that costs money, asks
// for a code or opens a vendor, trainer or other service. -1 when the page has none.
inline int64_t PickChatOption(std::vector<ShownOption> const& shown)
{
    for (ShownOption const& option : shown)
        if (!option.boxMoney && !option.coded &&
            (option.optionType == GOSSIP_OPTION_GOSSIP || option.optionType >= SCRIPT_ACTION_BASE))
            return int64_t(option.key);
    return -1;
}

// One spawn of an interaction target on the bot's map.
struct Spot
{
    uint32_t interaction = 0;  // index in the interaction index
    uint32_t entry = 0;
    uint32_t spawnId = 0;
    uint32_t map = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool phased = false;
};

inline float Dist2d(Spot const& spot, float x, float y)
{
    float const dx = spot.x - x;
    float const dy = spot.y - y;
    return std::sqrt(dx * dx + dy * dy);
}

inline float Score(Spot const& spot, float x, float y)
{
    return Dist2d(spot, x, y) + (spot.phased ? PHASED_PENALTY : 0.0f);
}

// The best spot on the bot's map within MAX_TRIP_DIST that was not tried yet, or -1. Equal scores go to the lower spawn
// id so the pick is stable.
inline int PickSpot(std::vector<Spot> const& spots, uint32_t map, float x, float y,
                    std::vector<uint32_t> const& visited, float* score = nullptr)
{
    int best = -1;
    float bestScore = 0.0f;
    for (std::size_t i = 0; i < spots.size(); ++i)
    {
        Spot const& spot = spots[i];
        if (spot.map != map || std::find(visited.begin(), visited.end(), spot.spawnId) != visited.end() ||
            Dist2d(spot, x, y) > MAX_TRIP_DIST)
            continue;
        float const s = Score(spot, x, y);
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

// A talk can be repeated with the same creature (a chain moves on); a click, an item use or an opened object is done
// once per target, as the scripts expect (a healed mountaineer runs off, each door counts once).
inline bool ReusesTarget(Way way)
{
    return way == Way::Gossip;
}

struct Trip
{
    bool active = false;
    uint32_t questId = 0;
    uint32_t objectiveId = 0;
    uint32_t credit = 0;
    uint32_t needed = 0;  // counter that ends the trip
    Spot spot;
    std::vector<uint32_t> visited;      // spots tried without a credit
    std::vector<uint64_t> usedTargets;  // creatures or objects already acted on
    std::vector<uint32_t> deferred;     // credits whose target offered nothing yet (an earlier step comes first)
    uint32_t startMs = 0;
    uint32_t budgetMs = 0;
    uint32_t spotMs = 0;  // arrival at the current spot (0 = not yet)
    uint32_t lastActMs = 0;
    uint32_t lastCount = 0;
    uint32_t actsWithoutCredit = 0;
    uint32_t spotsWithoutCredit = 0;
};

enum class Step
{
    Travel,
    Act,
    Wait,
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

inline Trip Start(uint32_t questId, uint32_t objectiveId, uint32_t credit, uint32_t needed, uint32_t count,
                  Spot const& spot, float dist, uint32_t now)
{
    Trip trip;
    trip.active = true;
    trip.questId = questId;
    trip.objectiveId = objectiveId;
    trip.credit = credit;
    trip.needed = needed;
    trip.spot = spot;
    trip.startMs = now;
    trip.budgetMs = BudgetMs(dist);
    trip.lastCount = count;
    return trip;
}

inline bool Expired(Trip const& trip, uint32_t now)
{
    return trip.active && uint32_t(now - trip.startMs) >= trip.budgetMs;
}

// True when a move to (map, x, y) is this trip's move to its spot, which must never end in a stuck teleport.
inline bool SameDest(Trip const& trip, uint32_t map, float x, float y)
{
    if (!trip.active || trip.spot.map != map)
        return false;
    float const dx = trip.spot.x - x;
    float const dy = trip.spot.y - y;
    return dx * dx + dy * dy < 1.0f;
}

// One tick of a running trip: dist is the bot's distance to the spot, count the objective's counter.
inline Step Judge(Trip& trip, uint32_t now, float dist, uint32_t count)
{
    if (count >= trip.needed)
        return Step::Done;
    if (count > trip.lastCount)
    {
        // A new credit: the spot works, and the give-up counts start over.
        trip.actsWithoutCredit = 0;
        trip.spotsWithoutCredit = 0;
        if (trip.spotMs)
            trip.spotMs = now ? now : 1;
    }
    trip.lastCount = count;
    if (Expired(trip, now))
        return Step::Expired;
    if (!trip.spotMs)
    {
        if (dist > ARRIVE_DIST)
            return Step::Travel;
        // 0 means "not yet", so an arrival at tick 0 is kept as tick 1.
        trip.spotMs = now ? now : 1;
        return Step::Act;
    }
    if (trip.lastActMs && uint32_t(now - trip.lastActMs) < ACT_GAP_MS)
        return Step::Wait;
    if (trip.actsWithoutCredit >= MAX_ACTS_WITHOUT_CREDIT || uint32_t(now - trip.spotMs) >= SPOT_WAIT_MS)
        return Step::NextSpot;
    return Step::Act;
}

// The bot acted on a target: wait for the core, and count the act until a credit arrives.
inline void NoteAct(Trip& trip, uint32_t now, uint64_t target, Way way)
{
    trip.lastActMs = now ? now : 1;
    ++trip.actsWithoutCredit;
    if (!ReusesTarget(way))
        trip.usedTargets.push_back(target);
}

inline bool Used(Trip const& trip, uint64_t target)
{
    return std::find(trip.usedTargets.begin(), trip.usedTargets.end(), target) != trip.usedTargets.end();
}

// The current spot gave nothing. True when the trip gives up.
inline bool LeaveSpot(Trip& trip)
{
    trip.visited.push_back(trip.spot.spawnId);
    trip.spotMs = 0;
    trip.lastActMs = 0;
    trip.actsWithoutCredit = 0;
    return ++trip.spotsWithoutCredit >= MAX_SPOTS_WITHOUT_CREDIT;
}

// The target offered nothing for this credit: the quest's other credits go first (Frankal talks only after Rutgar). The
// list goes with the quest's next trip, so the bot does not bounce back; it ends with the trip that earns a credit.
inline std::vector<uint32_t> Defer(Trip const& trip)
{
    std::vector<uint32_t> deferred = trip.deferred;
    if (std::find(deferred.begin(), deferred.end(), trip.credit) == deferred.end())
        deferred.push_back(trip.credit);
    return deferred;
}

inline bool Deferred(std::vector<uint32_t> const& deferred, uint32_t credit)
{
    return std::find(deferred.begin(), deferred.end(), credit) != deferred.end();
}

inline void GoToSpot(Trip& trip, Spot const& spot)
{
    trip.spot = spot;
    trip.spotMs = 0;
    trip.lastActMs = 0;
}

// The verb of the log line `[QUEST] <bot> <verb> <target> for <quest>`.
inline char const* Verb(Way way)
{
    switch (way)
    {
        case Way::Gossip: return "gossips";
        case Way::Click: return "clicks";
        case Way::UseItem: return "uses item on";
        case Way::OpenObject: return "opens";
    }
    return "interacts with";
}

inline char const* StepName(Step step)
{
    switch (step)
    {
        case Step::Travel: return "travel";
        case Step::Act: return "act";
        case Step::Wait: return "wait";
        case Step::NextSpot: return "no credit at the spot";
        case Step::Done: return "done";
        case Step::Expired: return "out of time";
    }
    return "unknown";
}
}  // namespace QuestInteractPolicy

#endif
