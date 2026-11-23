/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPSUBTLETYPOLICY_H
#define _PLAYERBOT_PVPSUBTLETYPOLICY_H

// BT-691 to BT-693: the Subtlety Rogue package of the PvP class rules (design decision 0032, owner decision 2026-11-19, rule R14 of docs/pvp).
// The shared layers decide the control chain (Blind, Kidney Shot and Gouge on the enemy healer are rows of the kit), the burst
// window, the interrupts and the defensives (Evasion, Cloak of Shadows and Vanish are in the catalog); this file adds what a
// Subtlety Rogue does inside them:
//   1. Sap on a second enemy in the opener: the healer first, then a caster, from stealth, before the rogue opens.
//   2. Shadow Dance, Shadow Blades and Marked for Death are the go: held for the burst window, not cast on cooldown.
//   3. Tricks of the Trade on a damage partner when the team go opens (the older edge puts it on a bot main tank).
//
// Every decision returns a reason (and a score where candidates compete), deterministic (design decision 0014 and 0026). Tier gating
// (fundamentals sheet 14): an expert plays all of it, a good bot the simple half, a casual bot keeps the older rotation.
//
// Free of game types, so tests/ can run it (tests/test_pvp_subtlety.cpp).

#include "BotSkillMath.h"

#include <cstdint>

namespace PvpSubtlety
{
using Tier = BotSkillMath::Tier;

constexpr std::uint32_t SPELL_SAP = 6770;
constexpr std::uint32_t SPELL_TRICKS_OF_THE_TRADE = 57934;
constexpr std::uint32_t SPELL_STEALTH = 1784;

// ---------------------------------------------------------------------------------------------------------------------
// 1. Sap in the opener
// ---------------------------------------------------------------------------------------------------------------------
enum class SapReason : std::uint8_t
{
    NO,
    NOT_READY,          // not learned, on cooldown or not stealthed
    CASUAL_SKIPS,
    IN_COMBAT,          // the rogue or the candidate is already fighting: Sap needs an unaware target
    KILL_TARGET,        // the rogue opens on this one
    OUT_OF_REACH,
    ALREADY_HELD,       // sapped or in another control
    DR_SPENT,           // the incapacitate diminishing returns are at immune (an expert reads them)
    HEALER,
    CASTER,
    OTHER               // a good bot saps any second enemy; an expert keeps it for a healer or a caster
};

constexpr float SAP_RANGE_YD = 10.0f;
constexpr std::int32_t SCORE_SAP_HEALER = 80;
constexpr std::int32_t SCORE_SAP_CASTER = 60;
constexpr std::int32_t SCORE_SAP_OTHER = 40;

struct SapInputs
{
    bool ready = false;                 // known, off cooldown, the rogue is stealthed
    bool botInCombat = false;
    bool candidateInCombat = false;
    bool candidateIsKillTarget = false;
    float candidateDistance = 0.0f;
    bool candidateHeld = false;         // sapped, or held by any control
    std::uint32_t drStep = 0;           // the rogue's own reading of the incapacitate category on the candidate (an expert)
    bool candidateIsHealer = false;
    bool candidateIsCaster = false;
};

struct SapVerdict
{
    bool cast = false;
    SapReason reason = SapReason::NO;
    std::int32_t score = 0;
};

inline SapVerdict Sap(SapInputs const& in, Tier tier)
{
    if (!in.ready)
        return { false, SapReason::NOT_READY, 0 };
    if (tier == Tier::CASUAL)
        return { false, SapReason::CASUAL_SKIPS, 0 };
    if (in.botInCombat || in.candidateInCombat)
        return { false, SapReason::IN_COMBAT, 0 };
    if (in.candidateIsKillTarget)
        return { false, SapReason::KILL_TARGET, 0 };
    if (in.candidateDistance > SAP_RANGE_YD)
        return { false, SapReason::OUT_OF_REACH, 0 };
    if (in.candidateHeld)
        return { false, SapReason::ALREADY_HELD, 0 };
    if (tier == Tier::EXPERT && in.drStep >= 3)
        return { false, SapReason::DR_SPENT, 0 };
    if (in.candidateIsHealer)
        return { true, SapReason::HEALER, SCORE_SAP_HEALER };
    if (in.candidateIsCaster)
        return { true, SapReason::CASTER, SCORE_SAP_CASTER };
    if (tier == Tier::GOOD)
        return { true, SapReason::OTHER, SCORE_SAP_OTHER };
    return { false, SapReason::OTHER, 0 };
}

// ---------------------------------------------------------------------------------------------------------------------
// 2. The go held for the window
// ---------------------------------------------------------------------------------------------------------------------
enum class GoReason : std::uint8_t
{
    NOT_A_PLAYER,       // the PvE rotation casts them as before
    CASUAL_FREE,
    WINDOW,
    HOLD
};

inline bool GoAllowed(GoReason reason)
{
    return reason != GoReason::HOLD;
}

inline GoReason Go(bool targetIsPlayer, bool windowOpen, Tier tier)
{
    if (!targetIsPlayer)
        return GoReason::NOT_A_PLAYER;
    if (tier == Tier::CASUAL)
        return GoReason::CASUAL_FREE;
    return windowOpen ? GoReason::WINDOW : GoReason::HOLD;
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Tricks of the Trade on the partner at the go
// ---------------------------------------------------------------------------------------------------------------------
enum class TricksReason : std::uint8_t
{
    NO,
    NOT_READY,
    TIER_SKIPS,         // an expert times the partner's go
    NO_GO,              // the team window is not open
    NO_PARTNER,         // nobody to give it to
    PARTNER_BURSTING,   // the partner shows a burst aura already: the threat is his, the damage is ours
    PARTNER_DAMAGE
};

constexpr std::int32_t SCORE_TRICKS_BURSTING = 70;
constexpr std::int32_t SCORE_TRICKS_DAMAGE = 50;

struct TricksInputs
{
    bool ready = false;
    bool teamGoOpen = false;
};

struct TricksCandidate
{
    bool damageDealer = false;     // not a healer, not a tank
    bool bursting = false;
    float healthPercent = 100.0f;
};

inline TricksReason Tricks(TricksInputs const& in, bool havePartner, TricksCandidate const& partner, Tier tier)
{
    if (!in.ready)
        return TricksReason::NOT_READY;
    if (tier != Tier::EXPERT)
        return TricksReason::TIER_SKIPS;
    if (!in.teamGoOpen)
        return TricksReason::NO_GO;
    if (!havePartner || !partner.damageDealer)
        return TricksReason::NO_PARTNER;
    return partner.bursting ? TricksReason::PARTNER_BURSTING : TricksReason::PARTNER_DAMAGE;
}

inline std::int32_t TricksScore(TricksCandidate const& partner)
{
    return (partner.bursting ? SCORE_TRICKS_BURSTING : SCORE_TRICKS_DAMAGE) + std::int32_t(partner.healthPercent / 10.0f);
}

inline bool TricksDue(TricksReason reason)
{
    return reason == TricksReason::PARTNER_BURSTING || reason == TricksReason::PARTNER_DAMAGE;
}
}  // namespace PvpSubtlety

#endif
