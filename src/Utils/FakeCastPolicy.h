/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_FAKECASTPOLICY_H
#define _PLAYERBOT_FAKECASTPOLICY_H

// BT-523, the fake cast of rule R11 (fundamentals sheet 4.3, design decision 0032 point 5, owner 2026-11-18): an expert caster starts a
// cast the enemy wants to stop, cancels it before the interrupt lands, and casts for real once the interrupt is spent. Only
// EXPERT bots do it, on 60 to 70 percent of their important casts (the default is 65), only while an observed enemy interrupt
// is ready and in range, and never in an emergency.
//
//   - Important: a crowd control cast, a heal on a target at or under 70 percent, a damage cast of 2 s or more; and a cast
//     time of at least 1.5 s (a shorter cast gives no time to fake).
//   - A threat: an enemy player with a way to stop a cast, in range of that stopper, with line of sight to the bot and facing
//     it, not held by control itself, and not seen using its stopper within the cooldown (the awareness book). Unknown counts as
//     ready: the bot does not know the enemy's cooldown, and the fake costs only a global cooldown.
//   - One roll per cast. The cancel point is 45 to 85 percent of the cast time, fixed per cast, so the enemy cannot learn it;
//     never later than 250 ms before the cast ends.
//   - After a fake the next cast within 8 s is real (the sheet: a kicker that is faked twice kicks later, and a bot that fakes
//     for ever never casts).
//   - Emergencies: a heal on a target at or under 25 percent, or the bot itself at or under 20 percent, is cast for real.
//
// Pure logic, free of game types (tests/test_fake_cast.cpp). The glue is strategy/generic/FakeCast.cpp.

#include "BotSkillMath.h"
#include "PvpAwarenessPolicy.h"
#include "PvpInterruptPolicy.h"

#include <algorithm>
#include <cstdint>

namespace FakeCast
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t MIN_CAST_MS = 1500;
constexpr std::uint32_t BIG_DAMAGE_CAST_MS = 2000;
constexpr float HEAL_WORTH_HP = 70.0f;
constexpr float HEAL_EMERGENCY_HP = 25.0f;
constexpr float SELF_EMERGENCY_HP = 20.0f;
constexpr std::uint32_t CANCEL_MIN_PERCENT = 45;
constexpr std::uint32_t CANCEL_MAX_PERCENT = 85;
// The cancel comes this long before the cast would end at the latest, and not before this long after it started.
constexpr std::uint32_t SAFETY_MS = 250;
constexpr std::uint32_t MIN_CANCEL_MS = 400;
// After a fake the next cast within this long is real.
constexpr std::uint32_t REAL_AFTER_FAKE_MS = 8000;
constexpr std::uint32_t NO_FAKE_YET = 0xFFFFFFFFu;

struct Facts
{
    Tier tier = Tier::EXPERT;
    std::uint32_t botGuid = 0;
    std::uint32_t castKey = 0;           // one value per cast
    std::uint32_t chancePercent = 65;    // of the important casts under threat (owner: 60 to 70)

    PvpInterrupt::CastShape shape;       // the bot's own cast
    float healTargetHp = 100.0f;         // the one a heal goes to
    float selfHp = 100.0f;

    bool kickerInRange = false;          // an enemy with a stopper not seen used, in its range, in sight, facing, not held
    std::uint32_t sinceLastFakeMs = NO_FAKE_YET;
};

struct Verdict
{
    bool fake = false;
    std::uint32_t cancelAtMs = 0;        // counted from the start of the cast
    char const* why = "no";
};

inline bool Important(PvpInterrupt::CastShape const& shape, float healTargetHp)
{
    if (shape.channeled || shape.castMs < MIN_CAST_MS)
        return false;
    switch (PvpInterrupt::Classify(shape))
    {
        case PvpInterrupt::CastClass::CROWD_CONTROL: return true;
        case PvpInterrupt::CastClass::HEAL: return healTargetHp <= HEAL_WORTH_HP;
        case PvpInterrupt::CastClass::BIG_DAMAGE: return shape.castMs >= BIG_DAMAGE_CAST_MS;
        case PvpInterrupt::CastClass::FILLER: return false;
    }
    return false;
}

inline bool Emergency(PvpInterrupt::CastShape const& shape, float healTargetHp, float selfHp)
{
    if (selfHp <= SELF_EMERGENCY_HP)
        return true;
    return PvpInterrupt::Classify(shape) == PvpInterrupt::CastClass::HEAL && healTargetHp <= HEAL_EMERGENCY_HP;
}

// The cancel point: 45 to 85 percent of the cast, fixed per cast, inside the safety margins.
inline std::uint32_t CancelAtMs(std::uint32_t botGuid, std::uint32_t castKey, std::uint32_t castMs)
{
    std::uint32_t const span = CANCEL_MAX_PERCENT - CANCEL_MIN_PERCENT + 1;
    std::uint32_t const pct = CANCEL_MIN_PERCENT + PvpAware::Roll(botGuid, castKey, 0xCA2Cu) % span;
    std::uint32_t const wanted = static_cast<std::uint32_t>(std::uint64_t(castMs) * pct / 100);
    std::uint32_t const latest = castMs > SAFETY_MS + MIN_CANCEL_MS ? castMs - SAFETY_MS : MIN_CANCEL_MS;
    return std::min(std::max(wanted, MIN_CANCEL_MS), latest);
}

// Decided once, when the cast starts.
inline Verdict Decide(Facts const& f)
{
    Verdict v;
    if (f.tier != Tier::EXPERT)
    {
        v.why = "not-expert";
        return v;
    }
    if (!Important(f.shape, f.healTargetHp))
    {
        v.why = "not-important";
        return v;
    }
    if (Emergency(f.shape, f.healTargetHp, f.selfHp))
    {
        v.why = "emergency";
        return v;
    }
    if (!f.kickerInRange)
    {
        v.why = "no-kicker";
        return v;
    }
    if (f.sinceLastFakeMs < REAL_AFTER_FAKE_MS)
    {
        v.why = "real-after-fake";
        return v;
    }
    if (PvpAware::Roll(f.botGuid, f.castKey, 0xFA1Eu) >= f.chancePercent)
    {
        v.why = "roll-real";
        return v;
    }
    v.fake = true;
    v.cancelAtMs = CancelAtMs(f.botGuid, f.castKey, f.shape.castMs);
    v.why = "fake";
    return v;
}

// Checked again at the moment of the cancel: the threat is still there and the cast is not an emergency now.
inline bool StillWorthCancelling(bool kickerInRange, PvpInterrupt::CastShape const& shape, float healTargetHp, float selfHp)
{
    return kickerInRange && !Emergency(shape, healTargetHp, selfHp);
}

inline bool CancelDue(Verdict const& v, std::uint32_t sinceStartMs)
{
    return v.fake && sinceStartMs >= v.cancelAtMs;
}
}  // namespace FakeCast

#endif
