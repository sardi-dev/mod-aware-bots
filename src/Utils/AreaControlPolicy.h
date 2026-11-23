#pragma once

// BT-165: a single breakable control in the impact area makes the area unsafe.
namespace AreaControlPolicy
{
constexpr unsigned RaidIconCount = 8;

struct Count
{
    int targets = 0;
    bool controlled = false;

    void Add(bool alive, float distance, float radius, bool breakable)
    {
        if (!alive || distance > radius)
            return;
        if (breakable)
            controlled = true;
        else
            ++targets;
    }

    bool CanCast(int minimum) const { return !controlled && targets >= minimum; }
};
}
