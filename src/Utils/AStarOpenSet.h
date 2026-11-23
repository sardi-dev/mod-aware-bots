/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_ASTAR_OPEN_SET_H
#define AWARE_BOTS_ASTAR_OPEN_SET_H

#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

// BT-696: the open list of the travel graph's A* (TravelNodeMap::getRoute) as a binary min-heap. The search used to sort
// the whole open list on every pop (O(n log n) per pop, a failing search on the 5,574-node graph pops about 5,600
// nodes). A node whose key falls while it is open is pushed again with the new key; the older entry is skipped on pop
// (lazy deletion), so the node that comes out is always the open node with the smallest current key. Ties on the key
// come out in the order they were pushed, which makes the search deterministic (the old sort left them to the
// implementation's partitioning).
template <class Node>
class AStarOpenSet
{
public:
    void Push(Node* node, float key)
    {
        m_heap.push(Entry{key, m_seq++, node});
    }

    // The open node with the smallest key; `isCurrent(node, key)` says whether an entry still stands for its node (the
    // node is open and its key is still that one). nullptr when no entry is left.
    template <class IsCurrent>
    Node* PopMin(IsCurrent isCurrent)
    {
        while (!m_heap.empty())
        {
            Entry const top = m_heap.top();
            m_heap.pop();
            if (isCurrent(top.node, top.key))
                return top.node;
        }
        return nullptr;
    }

    // Entries, stale ones included: for the log line only.
    size_t Entries() const { return m_heap.size(); }
    bool Empty() const { return m_heap.empty(); }

private:
    struct Entry
    {
        float key;
        uint64_t seq;
        Node* node;
    };

    struct Later
    {
        bool operator()(Entry const& a, Entry const& b) const
        {
            return a.key != b.key ? a.key > b.key : a.seq > b.seq;
        }
    };

    std::priority_queue<Entry, std::vector<Entry>, Later> m_heap;
    uint64_t m_seq = 0;
};

#endif
