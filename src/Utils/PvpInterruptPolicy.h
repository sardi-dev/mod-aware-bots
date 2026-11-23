/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPINTERRUPTPOLICY_H
#define _PLAYERBOT_PVPINTERRUPTPOLICY_H

// BT-521, rule R1 of the class rules map (fundamentals sheet topic 4): what a bot kicks in a PvP fight, and when. A kick is
// a cooldown of 12 to 24 seconds, so a player spends it on the cast that matters and holds it on filler, and lets the
// kick land late in the cast so that the target cannot react with a second spell of another school.
//
// What to kick, highest first (score):
//   1. crowd control aimed at our side (the healer 1000, the bot itself 950, another friend 900);
//   2. a heal on a target inside our kill window (at or under 40 percent: 800; the same on the healer itself 850);
//   3. a large damage cast or channel on a friend that is hurt (at or under 70 percent: 600);
//   4. anything else is filler: hold the kick.
// A damage spell of a school the enemy healer does not heal with locks the wrong school (an interrupted Lava Burst of a
// restoration shaman closes Fire only): held as well.
//
// When: late in the cast, with the tier's timing (expert at 60 to 90 percent, fixed per cast so that its pattern cannot be
// learned; good at 70; casual as soon as it noticed). Never before the tier's reaction time, never later than 300 ms before
// the cast ends. A second kicker in the group waits 500 ms per rank behind the first (a backup kicks only if the cast still
// runs), so two kicks never go on one cast.
//
// What a tier sees: expert every casting enemy, good its own target, the enemy healer and whoever casts at it, casual its
// own target only (and it misses one turn in three). Mistakes on purpose: a kick on filler (expert 5 percent of the filler
// casts, good 30, casual all it sees).
//
// Pure logic, free of game types (tests/test_pvp_interrupt.cpp). The glue is strategy/generic/PvpInterrupt.cpp.

#include "BotSkillMath.h"
#include "InterruptPolicy.h"
#include "OneVsOnePolicy.h"
#include "PvpAwarenessPolicy.h"
#include "PvpSpellCatalog.h"

#include <algorithm>
#include <cstdint>

namespace PvpInterrupt
{
using Tier = BotSkillMath::Tier;

enum class CastClass : std::uint8_t
{
    FILLER,
    HEAL,
    CROWD_CONTROL,
    BIG_DAMAGE
};

// A heal on a target at or under this health is inside the kill window (sheet IN2).
constexpr float KILL_WINDOW_HP = 40.0f;
// With the bot's own burst running a heal on a target at or under this health is worth the kick as well.
constexpr float BURST_WINDOW_HP = 60.0f;
// A large damage cast is worth a kick when the friend it goes to is at or under this health.
constexpr float BIG_DAMAGE_ALLY_HP = 70.0f;
// A damage cast this long (or any damage channel) counts as large.
constexpr std::uint32_t BIG_CAST_MS = 1500;
// The kick lands this long before the cast ends at the latest (the bot's tick and the latency).
constexpr std::uint32_t SAFETY_MS = 300;
// A second kicker waits this long per rank behind the first.
constexpr std::uint32_t STAGGER_MS = InterruptPolicy::STAGGER_MS;

constexpr std::int32_t SCORE_CC_HEALER = 1000;
constexpr std::int32_t SCORE_CC_SELF = 950;
constexpr std::int32_t SCORE_CC_FRIEND = 900;
constexpr std::int32_t SCORE_HEAL_HEALER = 850;
constexpr std::int32_t SCORE_HEAL = 800;
constexpr std::int32_t SCORE_BURST_HEAL = 750;
constexpr std::int32_t SCORE_BIG_DAMAGE = 600;
constexpr std::int32_t SCORE_FILLER = 100;

// What the glue reads off the spell the enemy is casting.
struct CastShape
{
    std::uint32_t spellId = 0;
    std::uint32_t mechanicMask = 0;
    bool positive = false;     // a beneficial spell
    bool heal = false;         // heals (direct or over time)
    bool damage = false;       // damages (direct or over time)
    std::uint32_t castMs = 0;  // the cast time (0 for a channel)
    bool channeled = false;
};

// The kinds of control worth a kick when they come at our side.
constexpr std::uint32_t CONTROL_KINDS = PvpSpells::CC_STUN | PvpSpells::CC_INCAPACITATE | PvpSpells::CC_DISORIENT |
                                        PvpSpells::CC_FEAR | PvpSpells::CC_HORROR | PvpSpells::CC_CHARM | PvpSpells::CC_SLEEP;

inline CastClass Classify(CastShape const& s)
{
    PvpSpells::DrCategory const category = PvpSpells::DrCategoryFor(s.spellId, s.mechanicMask);
    if (!s.positive && (PvpSpells::CcKindOfCategory(category) & CONTROL_KINDS))
        return CastClass::CROWD_CONTROL;
    if (s.heal && s.positive)
        return CastClass::HEAL;
    if (s.damage && (s.channeled || s.castMs >= BIG_CAST_MS))
        return CastClass::BIG_DAMAGE;
    return CastClass::FILLER;
}

// The schools an enemy healer of this class heals in: a kick of any other school wastes the lock.
inline std::uint32_t HealSchoolsOfClass(std::uint8_t cls)
{
    switch (cls)
    {
        case PvpSpells::CLS_PRIEST:
        case PvpSpells::CLS_PALADIN:
            return PvpSpells::SCHOOL_HOLY;
        case PvpSpells::CLS_DRUID:
        case PvpSpells::CLS_SHAMAN:
        case PvpSpells::CLS_MONK:
            return PvpSpells::SCHOOL_NATURE;
        default:
            return 0;
    }
}

struct CastFacts
{
    Tier tier = Tier::GOOD;
    std::uint32_t botGuid = 0;
    std::uint32_t castKey = 0;       // one value per cast: the rolls of this cast come from it

    CastShape shape;
    std::uint32_t castSchool = 0;    // the school mask of the cast
    std::uint8_t casterClass = 0;

    // Who the cast is about.
    bool onUs = false;               // aimed at the bot (an area control that reaches it counts)
    bool onOurHealer = false;
    bool onOurSide = false;          // aimed at a friend of the bot (the bot itself included)
    bool casterIsHealer = false;     // the caster is an enemy healer
    bool ownTarget = false;          // the caster is the bot's own target
    float healTargetHp = 100.0f;     // health percent of the one a heal goes to
    bool healOnCasterItself = false; // an enemy healing itself
    bool healTargetIsKillTarget = false;
    float allyHp = 100.0f;           // health percent of the friend a damage cast goes to
    bool ourBurst = false;           // the bot's own burst is running

    // Awareness.
    bool schoolAlreadyLocked = false;  // a kick of the group already closed this school on the caster

    // Timing.
    std::uint32_t rank = 0;          // 0 the first kicker, 1 the backup, ...
};

struct Decision
{
    bool wants = false;              // the bot should spend a kick on this cast
    std::int32_t score = 0;
    std::uint32_t kickAtMs = 0;      // when, counted from the first look at the cast
    char const* why = "hold";
    CastClass cls = CastClass::FILLER;
};

// The share of filler casts an unwise kick goes on, per tier.
inline std::uint32_t FillerKickPercent(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT: return 5;
        case Tier::GOOD: return 30;
        case Tier::CASUAL: return 100;
    }
    return 30;
}

// Which casts the tier looks at.
inline bool Sees(CastFacts const& f)
{
    switch (f.tier)
    {
        case Tier::EXPERT: return true;
        case Tier::GOOD: return f.ownTarget || f.casterIsHealer || f.onUs;
        case Tier::CASUAL: return f.ownTarget;
    }
    return false;
}

// Fraction of the cast at which the bot lets the kick go: expert varies per cast (60 to 90), good 70, casual at once.
inline std::uint32_t KickPercent(Tier tier, std::uint32_t botGuid, std::uint32_t castKey)
{
    switch (tier)
    {
        case Tier::EXPERT: return 60 + PvpAware::Roll(botGuid, castKey, 0x6B1Cu) % 31;
        case Tier::GOOD: return 70;
        case Tier::CASUAL: return 0;
    }
    return 70;
}

inline std::uint32_t KickAtMs(Tier tier, std::uint32_t botGuid, std::uint32_t castKey, std::uint32_t castMs, bool channeled,
                              std::uint32_t rank)
{
    std::uint32_t const react = OneVsOne::ReactionMs(tier, botGuid);
    std::uint32_t at = react;
    // A channel has no late point worth waiting for; a cast that ends before a player can act is kicked at once.
    if (!channeled && castMs > react + SAFETY_MS)
    {
        std::uint32_t const wanted = static_cast<std::uint32_t>(std::uint64_t(castMs) * KickPercent(tier, botGuid, castKey) / 100);
        at = std::min(std::max(react, wanted), castMs - SAFETY_MS);
    }
    return at + rank * STAGGER_MS;
}

inline Decision Decide(CastFacts const& f)
{
    Decision d;
    d.cls = Classify(f.shape);
    d.kickAtMs = KickAtMs(f.tier, f.botGuid, f.castKey, f.shape.castMs, f.shape.channeled, f.rank);

    if (!Sees(f))
    {
        d.why = "unseen";
        return d;
    }
    // A casual bot misses one turn in three (the dungeon interrupts do the same).
    if (!InterruptPolicy::TakesTurn(f.tier, f.botGuid, f.castKey))
    {
        d.why = "missed-turn";
        return d;
    }
    if (f.schoolAlreadyLocked)
    {
        d.why = "school-locked";
        return d;
    }

    switch (d.cls)
    {
        case CastClass::CROWD_CONTROL:
            if (f.onOurHealer)
            {
                d.score = SCORE_CC_HEALER;
                d.why = "cc-on-healer";
            }
            else if (f.onUs)
            {
                d.score = SCORE_CC_SELF;
                d.why = "cc-on-self";
            }
            else if (f.onOurSide)
            {
                d.score = SCORE_CC_FRIEND;
                d.why = "cc-on-friend";
            }
            break;
        case CastClass::HEAL:
        {
            bool const window = f.healTargetHp <= KILL_WINDOW_HP || (f.ourBurst && f.healTargetHp <= BURST_WINDOW_HP);
            if (window)
            {
                d.score = f.healOnCasterItself && f.healTargetIsKillTarget ? SCORE_HEAL_HEALER
                          : f.healTargetHp <= KILL_WINDOW_HP                ? SCORE_HEAL
                                                                            : SCORE_BURST_HEAL;
                d.why = d.score == SCORE_HEAL_HEALER ? "heal-on-kill-target" : "heal-in-window";
            }
            break;
        }
        case CastClass::BIG_DAMAGE:
            if (f.onOurSide && f.allyHp <= BIG_DAMAGE_ALLY_HP)
            {
                d.score = SCORE_BIG_DAMAGE;
                d.why = "big-damage";
            }
            break;
        case CastClass::FILLER:
            break;
    }

    // A healer casting a damage spell of a school it does not heal with: the lock would close the wrong school. A control
    // cast of any school is always worth stopping.
    if (d.score > 0 && d.cls != CastClass::CROWD_CONTROL && d.cls != CastClass::HEAL && f.casterIsHealer)
    {
        std::uint32_t const healSchools = HealSchoolsOfClass(f.casterClass);
        if (healSchools != 0 && !(f.castSchool & healSchools))
        {
            d.score = 0;
            d.why = "wrong-school";
        }
    }

    if (d.score > 0)
    {
        d.wants = true;
        return d;
    }

    // Filler: the kick is held, except for the share of casts a mistake or a casual bot spends it on.
    if (PvpAware::Roll(f.botGuid, f.castKey, 0x1D3u) < FillerKickPercent(f.tier))
    {
        d.score = SCORE_FILLER;
        d.wants = true;
        d.why = "filler-kick";
        return d;
    }
    d.why = "hold-filler";
    return d;
}

// Is it time? The first look at the cast is time zero; a cast that is over is not asked about any more.
inline bool Due(Decision const& d, std::uint32_t sinceFirstLookMs)
{
    return d.wants && sinceFirstLookMs >= d.kickAtMs;
}

inline char const* ClassName(CastClass c)
{
    switch (c)
    {
        case CastClass::FILLER: return "filler";
        case CastClass::HEAL: return "heal";
        case CastClass::CROWD_CONTROL: return "control";
        case CastClass::BIG_DAMAGE: return "damage";
    }
    return "?";
}
}  // namespace PvpInterrupt

#endif
