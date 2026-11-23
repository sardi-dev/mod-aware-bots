#pragma once

#include <cstdint>
#include "BotSkillMath.h"

// BT-165: each control mark has one stable owner, independent of iteration order
// and transient cast readiness. The caller supplies spell and target eligibility.
namespace CrowdControlDutyPolicy
{
constexpr unsigned TriangleIcon = 3;
constexpr unsigned MoonIcon = 4;
constexpr unsigned SquareIcon = 5;
constexpr std::uint64_t CasualReapplyDelayMs = 3000;
constexpr std::uint64_t GoodReapplyDelayMs = 1000;

enum class Spell { Polymorph, Hex, Repentance, Hibernate, Cyclone, FreezingTrap, Sap, ShackleUndead, Other };

constexpr bool MatchesMark(unsigned icon, Spell spell)
{
    switch (icon)
    {
        case MoonIcon:
            return spell == Spell::Polymorph || spell == Spell::Hex || spell == Spell::Repentance ||
                   spell == Spell::Hibernate || spell == Spell::Cyclone;
        case SquareIcon: return spell == Spell::FreezingTrap;
        case TriangleIcon: return spell == Spell::Sap || spell == Spell::ShackleUndead;
        default: return false;
    }
}

// BT-165: do not contest the tank or a human who has chosen to attack the mob.
constexpr bool SafeTarget(bool alive, bool tankTarget, bool humanAttacking)
{
    return alive && !tankTarget && !humanAttacking;
}

constexpr std::uint64_t ReapplyDelayMs(BotSkillMath::Tier tier)
{
    return tier == BotSkillMath::Tier::CASUAL ? CasualReapplyDelayMs :
           tier == BotSkillMath::Tier::GOOD ? GoodReapplyDelayMs : 0;
}

// BT-165: only a break after observed control is delayed; a fresh mark can be
// controlled before the pull. Each controller keeps this state across updates.
class ReapplyGate
{
public:
    bool Ready(std::uint64_t target, bool controlled, std::uint64_t nowMs, BotSkillMath::Tier tier)
    {
        if (target != targetGuid)
        {
            targetGuid = target;
            wasControlled = false;
            waiting = false;
        }
        if (!target)
            return false;
        if (controlled)
        {
            wasControlled = true;
            waiting = false;
            return false;
        }
        if (wasControlled)
        {
            wasControlled = false;
            waiting = true;
            brokenAtMs = nowMs;
        }
        return !waiting || (nowMs >= brokenAtMs && nowMs - brokenAtMs >= ReapplyDelayMs(tier));
    }

private:
    std::uint64_t targetGuid = 0;
    std::uint64_t brokenAtMs = 0;
    bool wasControlled = false;
    bool waiting = false;
};

struct Candidate
{
    std::uint64_t guid;
    bool eligible; // Bot knows a matching spell that can control this target type.
    bool tank;
    bool healer;
};

class Selection
{
public:
    void Add(Candidate candidate)
    {
        if (!candidate.guid || !candidate.eligible || candidate.tank)
            return;
        if (!found || (best.healer && !candidate.healer) ||
            (best.healer == candidate.healer && candidate.guid < best.guid))
        {
            best = candidate;
            found = true;
        }
    }

    std::uint64_t Guid() const { return found ? best.guid : 0; }

private:
    Candidate best{};
    bool found = false;
};
}
