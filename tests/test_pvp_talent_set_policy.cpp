#include "PvpTalentSetPolicy.h"
#include "TestHarness.h"

using namespace PvpTalentSetPolicy;

namespace
{
Situation Base()
{
    Situation s;
    s.enabled = true;
    s.hasPvpSet = true;
    s.hasDualSpec = true;
    return s;
}

Situation InMatch(bool preparation, std::uint8_t group = PVE_GROUP)
{
    Situation s = Base();
    s.inPvpMatch = true;
    s.preparation = preparation;
    s.activeGroup = group;
    return s;
}

PvpGroupState Ready()
{
    return PvpGroupState{};
}

DualSpecOffer Offer()
{
    DualSpecOffer o;
    o.enabled = true;
    o.hasPvpSet = true;
    o.playsPvp = true;
    o.level = 30;
    o.money = DUAL_SPEC_COST + 5000;
    o.reserve = 5000;
    return o;
}
}

TEST(pvp_link_parses_columns_and_no_choice)
{
    auto const link = ParseLink("01.2x9123");
    CHECK(link.size() == TALENT_TIERS);
    CHECK(link[0] == 0 && link[1] == 1 && link[2] == NO_CHOICE && link[3] == 2);
    CHECK(link[4] == NO_CHOICE && link[5] == NO_CHOICE);
    CHECK(ParseLink("").empty());
}

TEST(pvp_plan_fills_empty_tiers_and_swaps_others)
{
    std::vector<std::uint8_t> const current = { 0, 1, NO_TALENT, 2 };
    TierPlan const plan = PlanTiers(current, ParseLink("012112"), 4);
    CHECK(plan.learn.size() == 1 && plan.learn[0] == 2);
    CHECK(plan.replace.size() == 1 && plan.replace[0] == 3);
}

TEST(pvp_plan_ignores_locked_and_unconfigured_tiers)
{
    std::vector<std::uint8_t> const current = { 2, 2, 2, 2, 2, 2 };
    CHECK(PlanTiers(current, ParseLink("0....."), 1).replace.size() == 1);
    CHECK(PlanTiers(current, ParseLink(".00000"), 1).Empty());
    CHECK(PlanTiers(current, ParseLink("222222"), 6).Empty());
    CHECK(PlanTiers(current, ParseLink("000000"), 0).Empty());
}

TEST(pvp_glyph_plan_uses_empty_slots_then_replaces_unwanted)
{
    std::vector<GlyphSlot> const slots = {
        { 1, true, 0 }, { 0, true, 500 }, { 1, true, 600 }, { 0, true, 700 }, { 1, false, 0 }, { 0, false, 0 } };
    std::vector<WantedGlyph> const wanted = { { 700, 0 }, { 800, 0 }, { 900, 1 }, { 901, 1 }, { 902, 1 } };
    auto const puts = PlanGlyphs(slots, wanted);
    // 700 is in place; 800 replaces 500; 900 takes the empty minor slot; 901 replaces 600; 902 has no slot left.
    CHECK(puts.size() == 3);
    CHECK(puts[0].slot == 1 && puts[0].glyph == 800);
    CHECK(puts[1].slot == 0 && puts[1].glyph == 900);
    CHECK(puts[2].slot == 2 && puts[2].glyph == 901);
}

TEST(pvp_glyph_plan_never_replaces_a_wanted_glyph)
{
    std::vector<GlyphSlot> const slots = { { 0, true, 10 }, { 0, true, 20 } };
    CHECK(PlanGlyphs(slots, { { 20, 0 }, { 10, 0 }, { 30, 0 } }).empty());
}

TEST(pvp_target_is_pvp_in_matches_and_queues_with_dual_spec_only)
{
    Situation s = InMatch(false);
    CHECK(TargetSet(s) == TalentSet::PVP);
    s.inPvpMatch = false;
    CHECK(TargetSet(s) == TalentSet::PVE);
    s.queuedForPvp = true;
    CHECK(TargetSet(s) == TalentSet::PVP);
    s.hasDualSpec = false;
    CHECK(TargetSet(s) == TalentSet::PVE);
    s.hasDualSpec = true;
    s.hasPvpSet = false;
    CHECK(TargetSet(s) == TalentSet::PVE);
    s.hasPvpSet = true;
    s.enabled = false;
    CHECK(TargetSet(s) == TalentSet::PVE);
}

TEST(pvp_queue_or_battleground_activates_the_secondary_group)
{
    Situation s = Base();
    s.queuedForPvp = true;
    CHECK(Decide(s, Ready()) == Step::ACTIVATE_PVP);
    CHECK(Decide(InMatch(false), Ready()) == Step::ACTIVATE_PVP);
    CHECK(Decide(InMatch(true), Ready()) == Step::ACTIVATE_PVP);
}

TEST(pvp_arena_switches_only_in_preparation)
{
    Situation s = InMatch(false);
    s.inArena = true;
    CHECK(Decide(s, Ready()) == Step::NONE);
    s.preparation = true;
    CHECK(Decide(s, Ready()) == Step::ACTIVATE_PVP);
}

TEST(pvp_no_step_in_combat_or_dead)
{
    Situation s = InMatch(true);
    s.inCombat = true;
    CHECK(Decide(s, Ready()) == Step::NONE);
    s.inCombat = false;
    s.alive = false;
    CHECK(Decide(s, Ready()) == Step::NONE);
}

TEST(pvp_secondary_group_is_set_up_once_active)
{
    PvpGroupState group;
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::NONE);
    group.specMissing = true;
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::SET_UP_PVP);
    group = PvpGroupState{};
    group.specDiffers = true;
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::SET_UP_PVP);
    group = PvpGroupState{};
    group.glyphsDiffer = true;
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::SET_UP_PVP);
    group = PvpGroupState{};
    group.tiers.learn.push_back(3);
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::SET_UP_PVP);
}

TEST(pvp_secondary_group_swaps_a_talent_only_in_preparation)
{
    PvpGroupState group;
    group.tiers.replace.push_back(1);
    CHECK(Decide(InMatch(false, PVP_GROUP), group) == Step::NONE);
    CHECK(Decide(InMatch(true, PVP_GROUP), group) == Step::SET_UP_PVP);
}

TEST(pvp_back_to_pve_at_an_opportunity)
{
    Situation s = Base();
    s.activeGroup = PVP_GROUP;
    CHECK(Decide(s, Ready()) == Step::NONE);
    s.pveOpportunity = true;
    CHECK(Decide(s, Ready()) == Step::ACTIVATE_PVE);
    // Still queued: it stays on the PvP set.
    s.queuedForPvp = true;
    CHECK(Decide(s, Ready()) == Step::NONE);
    // On the primary group already: nothing.
    s.queuedForPvp = false;
    s.activeGroup = PVE_GROUP;
    CHECK(Decide(s, Ready()) == Step::NONE);
}

TEST(pvp_dual_spec_bought_at_level_with_money_above_reserve)
{
    CHECK(WantsDualSpec(Offer()));
    DualSpecOffer o = Offer();
    o.level = 29;
    CHECK(!WantsDualSpec(o));
    o = Offer();
    o.money = DUAL_SPEC_COST + 4999;
    CHECK(!WantsDualSpec(o));
    o = Offer();
    o.specsCount = 2;
    CHECK(!WantsDualSpec(o));
    o = Offer();
    o.playsPvp = false;
    CHECK(!WantsDualSpec(o));
    o = Offer();
    o.hasPvpSet = false;
    CHECK(!WantsDualSpec(o));
    o = Offer();
    o.enabled = false;
    CHECK(!WantsDualSpec(o));
}
