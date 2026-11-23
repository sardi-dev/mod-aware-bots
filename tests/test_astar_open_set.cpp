#include "TestHarness.h"
#include "AStarOpenSet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <type_traits>
#include <unordered_map>
#include <vector>

// BT-696: the heap-backed open list pops the same nodes in the same order as the per-pop sort it replaced.

namespace
{
struct Stub
{
    int id = 0;
    float f = 0.0f, g = 0.0f, h = 0.0f;
    bool open = false, close = false;
    Stub* parent = nullptr;
};

struct Edge
{
    int to;
    float cost;
};

struct Graph
{
    std::vector<std::pair<float, float>> pos;
    std::vector<std::vector<Edge>> links;
};

Graph RandomGraph(std::mt19937& rng, int nodes, int degree)
{
    std::uniform_real_distribution<float> coord(0.0f, 1000.0f), jitter(0.9f, 1.6f);
    Graph graph;
    graph.pos.resize(nodes);
    graph.links.resize(nodes);
    for (auto& p : graph.pos)
        p = {coord(rng), coord(rng)};
    for (int a = 0; a < nodes; ++a)
        for (int k = 0; k < degree; ++k)
        {
            int const b = int(rng() % nodes);
            if (b == a)
                continue;
            float const dist = std::hypot(graph.pos[a].first - graph.pos[b].first, graph.pos[a].second - graph.pos[b].second);
            graph.links[a].push_back({b, dist * jitter(rng) + 0.001f});
        }
    return graph;
}

// The search loop of TravelNodeMap::getRoute, with the open list as a policy. `Open` provides Push(stub), PopMin().
struct OldSortOpen
{
    std::vector<Stub*> v;
    void Push(Stub* s) { v.push_back(s); }
    Stub* PopMin()
    {
        if (v.empty())
            return nullptr;
        auto less = [](Stub* i, Stub* j) { return i->f < j->f; };
        std::sort(v.begin(), v.end(), less);
        Stub* current = v.front();
        std::pop_heap(v.begin(), v.end(), less);
        v.pop_back();
        return current;
    }
};

struct HeapOpen
{
    AStarOpenSet<Stub> set;
    void Push(Stub* s) { set.Push(s, s->f); }
    Stub* PopMin()
    {
        return set.PopMin([](Stub* s, float key) { return s->open && !(s->f < key) && !(key < s->f); });
    }
};

template <class Open>
std::vector<int> Search(Graph const& graph, int start, int goal, float heuristicScale, std::vector<int>& path)
{
    std::unordered_map<int, Stub> stubs;
    auto stubFor = [&](int id) -> Stub*
    {
        auto itr = stubs.find(id);
        if (itr == stubs.end())
        {
            Stub s;
            s.id = id;
            itr = stubs.emplace(id, s).first;
        }
        return &itr->second;
    };
    auto heuristic = [&](int id)
    {
        return heuristicScale * std::hypot(graph.pos[id].first - graph.pos[goal].first,
                                           graph.pos[id].second - graph.pos[goal].second);
    };

    Open open;
    std::vector<int> popped;
    Stub* startStub = stubFor(start);
    open.Push(startStub);
    startStub->open = true;
    while (Stub* current = open.PopMin())
    {
        current->open = false;
        current->close = true;
        popped.push_back(current->id);
        if (current->id == goal)
        {
            for (Stub* p = current; p; p = p->parent)
                path.push_back(p->id);
            std::reverse(path.begin(), path.end());
            return popped;
        }
        for (Edge const& link : graph.links[current->id])
        {
            Stub* child = stubFor(link.to);
            float const g = current->g + link.cost;
            if ((child->open || child->close) && child->g <= g)
                continue;
            child->h = heuristic(link.to);
            child->g = g;
            child->f = g + child->h;
            child->parent = current;
            if (child->close)
                child->close = false;
            if (!child->open)
            {
                open.Push(child);
                child->open = true;
            }
            else if constexpr (std::is_same_v<Open, HeapOpen>)
                open.Push(child);  // a lowered key of an open node is pushed again (getRoute does the same)
        }
    }
    return popped;
}
}  // namespace

TEST(astar_open_set_pops_the_same_nodes_in_the_same_order_as_the_old_sort)
{
    std::mt19937 rng(20261119);
    for (int round = 0; round < 300; ++round)
    {
        int const nodes = 20 + int(rng() % 400);
        Graph const graph = RandomGraph(rng, nodes, 3 + int(rng() % 4));
        int const start = int(rng() % nodes), goal = int(rng() % nodes);
        // 1.0 is admissible; 1.8 overestimates, so closed nodes are reopened and open ones get lower keys.
        float const scale = round % 2 ? 1.8f : 1.0f;
        std::vector<int> pathOld, pathNew;
        std::vector<int> const old = Search<OldSortOpen>(graph, start, goal, scale, pathOld);
        std::vector<int> const heap = Search<HeapOpen>(graph, start, goal, scale, pathNew);
        CHECK(old == heap);
        CHECK(pathOld == pathNew);
    }
}

TEST(astar_open_set_exhausts_like_the_old_sort_when_the_goal_is_unreachable)
{
    Graph graph;
    graph.pos = {{0, 0}, {10, 0}, {20, 0}, {900, 900}};
    graph.links = {{{1, 10.0f}}, {{2, 10.0f}, {0, 10.0f}}, {{1, 10.0f}}, {}};
    std::vector<int> pathOld, pathNew;
    std::vector<int> const old = Search<OldSortOpen>(graph, 0, 3, 1.0f, pathOld);
    std::vector<int> const heap = Search<HeapOpen>(graph, 0, 3, 1.0f, pathNew);
    CHECK(old == heap);
    CHECK(pathNew.empty());
    CHECK(heap.size() == 3);
}

TEST(astar_open_set_ties_come_out_in_push_order_and_stale_entries_are_skipped)
{
    Stub a, b, c;
    a.id = 1; b.id = 2; c.id = 3;
    a.f = b.f = c.f = 5.0f;
    a.open = b.open = c.open = true;
    AStarOpenSet<Stub> set;
    set.Push(&a, a.f);
    set.Push(&b, b.f);
    set.Push(&c, c.f);
    auto current = [](Stub* s, float key) { return s->open && s->f == key; };
    // b gets a lower key: its old entry (5) goes stale, the new one (3) leads.
    b.f = 3.0f;
    set.Push(&b, b.f);
    Stub* first = set.PopMin(current);
    CHECK(first == &b);
    first->open = false;
    CHECK(set.PopMin(current) == &a);
    a.open = false;
    CHECK(set.PopMin(current) == &c);
    c.open = false;
    // b's stale entry is the only one left and b is closed: nothing comes out.
    CHECK(set.PopMin(current) == nullptr);
    CHECK(set.Empty());
}
