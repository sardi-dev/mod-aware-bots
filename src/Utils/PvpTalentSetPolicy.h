/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef MOD_AWARE_BOTS_PVPTALENTSETPOLICY_H
#define MOD_AWARE_BOTS_PVPTALENTSETPOLICY_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// PvP talent and glyph sets (BT-320, BT-323): a random bot keeps its PvE set in its primary talent group and its PvP
// set in the secondary one, bought as Dual Talent Specialization at its class trainer with its own gold, and switches
// between them with the Activate Spec cast like a player. Pure rules; BotPvpTalents.cpp reads the bot and applies them.
//
// What 5.4.8 allows (client data and core, build 18414):
// - Dual Talent Specialization: class trainer gossip option 18, BoxMoney 100000 (10 gold) on all 269 world rows,
//   level MinDualSpecLevel (30); it casts 63680 and 63624 (Player::OnGossipSelect).
// - Activating Primary Spec 63645 / Secondary Spec 63644: 5 s cast (SpellCastTimes 6), SPELL_ATTR0 "not in combat",
//   no reagent; Spell::EffectActivateSpec calls Player::ActivateSpec, which swaps talents, spec spells and glyph auras.
// - In the active group: a talent into an empty tier is free (CMSG_LEARN_TALENT); taking one out (Remove Talent 113873)
//   needs a Tome of the Clear Mind except during the preparation phase (SPELL_ATTR5_NO_REAGENT_WHILE_PREP), so a bot
//   only replaces a secondary group talent then; a known glyph replaces another for free out of combat
//   (Spell::EffectApplyGlyph, the glyph spells have no reagent).
// - A specialization is chosen once per group for free (WorldSession::HandeSetTalentSpecialization).
namespace PvpTalentSetPolicy
{
constexpr std::uint32_t TALENT_TIERS = 6;     // Talent.dbc TierID 0-5, unlocked at 15/30/45/60/75/90
constexpr std::uint32_t TALENT_COLUMNS = 3;   // Talent.dbc ColumnIndex 0-2
constexpr std::uint16_t NO_CHOICE = 0xFFFF;   // a configured tier without a preference
constexpr std::uint8_t NO_TALENT = 0xFF;      // a tier the bot has no talent in
constexpr std::uint8_t PVE_GROUP = 0;         // Player::GetActiveSpec() of the primary talent group
constexpr std::uint8_t PVP_GROUP = 1;         // ... and of the secondary one
constexpr std::uint32_t SPELL_ACTIVATE_PRIMARY_SPEC = 63645;
constexpr std::uint32_t SPELL_ACTIVATE_SECONDARY_SPEC = 63644;
constexpr std::uint32_t SPELL_TEACH_SPEC_SWITCHES = 63680;
constexpr std::uint32_t SPELL_LEARN_SECOND_SPEC = 63624;
constexpr std::uint64_t DUAL_SPEC_COST = 100000;   // copper, gossip_menu_option BoxMoney of the trainers' option

enum class TalentSet : std::uint8_t
{
    PVE,
    PVP
};

// One character per tier, as AiPlayerbot.PremadeSpecLink: '0' to '2' is the column, anything else no preference.
// At most TALENT_TIERS entries.
inline std::vector<std::uint16_t> ParseLink(std::string const& link)
{
    std::vector<std::uint16_t> tiers;
    for (char c : link)
    {
        if (tiers.size() >= TALENT_TIERS)
            break;
        tiers.push_back((c >= '0' && c <= '2') ? std::uint16_t(c - '0') : NO_CHOICE);
    }
    return tiers;
}

// The column a set wants in a tier, or NO_CHOICE.
inline std::uint16_t WantedColumn(std::vector<std::uint16_t> const& link, std::uint32_t tier)
{
    return (tier < link.size() && link[tier] < TALENT_COLUMNS) ? link[tier] : NO_CHOICE;
}

// What changes the talents from what the bot has (current[tier] = its column or NO_TALENT) to a set, over the tiers
// it has unlocked: tiers to fill (free) and tiers to swap (a talent comes out first).
struct TierPlan
{
    std::vector<std::uint8_t> learn;
    std::vector<std::uint8_t> replace;

    bool Empty() const { return learn.empty() && replace.empty(); }
};

inline TierPlan PlanTiers(std::vector<std::uint8_t> const& current, std::vector<std::uint16_t> const& link,
                          std::uint32_t unlockedTiers)
{
    TierPlan plan;
    std::uint32_t const tiers = std::min<std::uint32_t>(unlockedTiers, TALENT_TIERS);
    for (std::uint32_t tier = 0; tier < tiers; ++tier)
    {
        std::uint16_t const wanted = WantedColumn(link, tier);
        if (wanted == NO_CHOICE)
            continue;

        std::uint8_t const have = tier < current.size() ? current[tier] : NO_TALENT;
        if (have == NO_TALENT)
            plan.learn.push_back(std::uint8_t(tier));
        else if (have != wanted)
            plan.replace.push_back(std::uint8_t(tier));
    }
    return plan;
}

// A glyph slot as the planner sees it: its type (0 major, 1 minor, GlyphSlot.dbc TypeFlags), whether it is unlocked,
// and the glyph in it (GlyphProperties id, 0 = empty).
struct GlyphSlot
{
    std::uint32_t type = 0;
    bool enabled = false;
    std::uint32_t glyph = 0;
};

// A glyph a set wants: GlyphProperties id and type. The caller leaves out glyphs the bot cannot use yet (level, race).
struct WantedGlyph
{
    std::uint32_t glyph = 0;
    std::uint32_t type = 0;
};

struct GlyphPut
{
    std::uint8_t slot = 0;
    std::uint32_t glyph = 0;
};

// Where each wanted glyph goes, in the set's order: nothing for a glyph already in a slot; else an empty unlocked slot
// of its type, else the first slot of its type whose glyph the set does not want (replaced, which is free). A glyph
// with no such slot is left out.
inline std::vector<GlyphPut> PlanGlyphs(std::vector<GlyphSlot> const& slots, std::vector<WantedGlyph> const& wanted)
{
    std::vector<GlyphSlot> state = slots;
    std::vector<GlyphPut> puts;
    auto isWanted = [&wanted](std::uint32_t glyph)
    {
        return std::any_of(wanted.begin(), wanted.end(), [glyph](WantedGlyph const& w) { return w.glyph == glyph; });
    };

    for (WantedGlyph const& want : wanted)
    {
        if (!want.glyph)
            continue;
        if (std::any_of(state.begin(), state.end(), [&want](GlyphSlot const& s) { return s.glyph == want.glyph; }))
            continue;

        int chosen = -1;
        for (std::size_t i = 0; i < state.size() && chosen < 0; ++i)
            if (state[i].enabled && state[i].type == want.type && !state[i].glyph)
                chosen = int(i);
        for (std::size_t i = 0; i < state.size() && chosen < 0; ++i)
            if (state[i].enabled && state[i].type == want.type && !isWanted(state[i].glyph))
                chosen = int(i);
        if (chosen < 0)
            continue;

        state[std::size_t(chosen)].glyph = want.glyph;
        puts.push_back({ std::uint8_t(chosen), want.glyph });
    }
    return puts;
}

// The bot and its surroundings, read by the caller.
struct Situation
{
    bool enabled = false;          // AiPlayerbot.PvpTalentSets, random bot
    bool hasPvpSet = false;        // its spec has a configured PvP set
    bool hasDualSpec = false;      // Player::GetSpecsCount() > 1
    std::uint8_t activeGroup = PVE_GROUP;
    bool inPvpMatch = false;       // inside a battleground or an arena
    bool inArena = false;
    bool queuedForPvp = false;     // in a battleground or arena queue
    bool preparation = false;      // UNIT_FLAG_PREPARATION (the start of a match)
    bool inCombat = false;
    bool alive = true;
    bool pveOpportunity = false;   // resting (inn or city) or inside a dungeon or raid: where a player sets up for PvE
};

// The set the bot should end up with now: PvP while it waits in a PvP queue and inside a match, PvE everywhere else.
inline TalentSet TargetSet(Situation const& s)
{
    if (!s.enabled || !s.hasPvpSet || !s.hasDualSpec)
        return TalentSet::PVE;
    return (s.inPvpMatch || s.queuedForPvp) ? TalentSet::PVP : TalentSet::PVE;
}

// What the secondary group still lacks for the PvP set, read while it is the active group.
struct PvpGroupState
{
    bool specMissing = false;      // no specialization in the group yet
    bool specDiffers = false;      // another specialization than the primary group (the primary changed role since)
    TierPlan tiers;                // to the PvP link
    bool glyphsDiffer = false;     // PlanGlyphs found something to put
};

enum class Step : std::uint8_t
{
    NONE,
    ACTIVATE_PVP,   // cast Activate Secondary Spec
    ACTIVATE_PVE,   // cast Activate Primary Spec
    SET_UP_PVP      // put the PvP set into the active secondary group
};

// One step at a time, never in combat or dead (both spells refuse it):
// - to PvP: when queued or in a battleground; in an arena only during the preparation phase (the gates are open after
//   it and a player cannot stand 5 s in the open);
// - in the secondary group: set it up (specialization, empty tiers, glyphs; a tier that holds another talent only in
//   the preparation phase, when taking it out is free);
// - back to PvE: at the next PvE opportunity (resting, or inside a dungeon or raid out of combat).
inline Step Decide(Situation const& s, PvpGroupState const& group)
{
    if (s.inCombat || !s.alive)
        return Step::NONE;

    TalentSet const target = TargetSet(s);
    if (target == TalentSet::PVP)
    {
        if (s.activeGroup != PVP_GROUP)
        {
            if (s.inArena && !s.preparation)
                return Step::NONE;
            return Step::ACTIVATE_PVP;
        }
        bool const tiersDue = !group.tiers.learn.empty() || (s.preparation && !group.tiers.replace.empty());
        if (group.specMissing || group.specDiffers || tiersDue || group.glyphsDiffer)
            return Step::SET_UP_PVP;
        return Step::NONE;
    }

    if (s.activeGroup == PVP_GROUP && s.pveOpportunity)
        return Step::ACTIVATE_PVE;
    return Step::NONE;
}

// Whether the bot should buy Dual Talent Specialization at its class trainer now: a random bot with a PvP set that
// plays battlegrounds, at the level the core sets, that can pay the price above its repair reserve.
struct DualSpecOffer
{
    bool enabled = false;
    bool hasPvpSet = false;
    bool playsPvp = false;          // AiPlayerbot.RandomBotJoinBG
    std::uint32_t specsCount = 1;
    std::uint32_t level = 0;
    std::uint32_t minLevel = 30;    // MinDualSpecLevel
    std::uint64_t money = 0;
    std::uint64_t reserve = 0;      // BotSalePolicy::PurchaseReserve
};

inline bool WantsDualSpec(DualSpecOffer const& o)
{
    return o.enabled && o.hasPvpSet && o.playsPvp && o.specsCount < 2 && o.level >= o.minLevel &&
           o.money >= o.reserve + DUAL_SPEC_COST;
}
}

#endif
