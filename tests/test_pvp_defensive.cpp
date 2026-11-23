#include "TestHarness.h"
#include "PvpDefensivePolicy.h"

// BT-522: defensives early and one per go, immunity last; the cheapest break before the medallion; the medallion by role.

using namespace PvpDefense;
using namespace PvpSpells;

namespace
{
Candidate Tempo(std::uint32_t id, std::uint32_t cd, Fit fit = Fit::ALL, bool blocked = false, bool ready = true)
{
    Candidate c;
    c.spellId = id; c.kind = Kind::DEFENSIVE_TEMPO; c.fit = fit; c.cooldownMs = cd; c.silenceBlocked = blocked; c.ready = ready;
    return c;
}
Candidate Immunity(std::uint32_t id, std::uint32_t cd = 300000, bool ready = true)
{
    Candidate c;
    c.spellId = id; c.kind = Kind::DEFENSIVE_IMMUNITY; c.cooldownMs = cd; c.ready = ready;
    return c;
}

// A paladin: Divine Protection (1 min, silence blocked) and Divine Shield (5 min).
std::vector<Candidate> Paladin() { return { Tempo(498, 60000, Fit::ALL, true), Immunity(642) }; }

DefFacts Go(Tier tier = Tier::EXPERT, float hp = 70.0f)
{
    DefFacts f;
    f.tier = tier;
    f.guid = 99;
    f.goKey = 1;
    f.hpPct = hp;
    f.lossPct3s = 25.0f;
    f.attackers = 2;
    f.meleeThreat = true;
    return f;
}

// A goKey for which the tier does not press late (no mistake).
std::uint32_t CleanKey(Tier tier, std::uint32_t guid)
{
    for (std::uint32_t k = 1; k < 5000; ++k)
        if (!PvpAware::MakesMistake(tier, guid, k))
            return k;
    return 0;
}

BreakCandidate Brk(std::uint32_t id, std::uint32_t breaks, std::uint8_t cost, bool ready = true)
{
    BreakCandidate c;
    c.spellId = id; c.breaks = breaks; c.cost = cost; c.ready = ready;
    return c;
}

// A rogue-ish kit: Every Man for Himself (shared) and the medallion.
std::vector<BreakCandidate> Medallion()
{
    return { Brk(59752, CC_MEDALLION, BREAK_COST_SHARED), Brk(42292, CC_MEDALLION, BREAK_COST_MEDALLION) };
}

BreakFacts Stunned(Tier tier = Tier::EXPERT)
{
    BreakFacts f;
    f.tier = tier;
    f.guid = 99;
    f.effectKey = CleanKey(tier, 99);
    f.kinds = CC_STUN;
    f.worstKind = CC_STUN;
    f.remainingMs = 4000;
    f.hpPct = 100.0f;
    return f;
}
}  // namespace

TEST(pvp_def_a_go_is_damage_in_three_seconds_or_two_enemies)
{
    DefFacts f;
    f.attackers = 1;
    f.lossPct3s = 5.0f;
    CHECK(!GoOn(f));
    f.lossPct3s = 20.0f;
    CHECK(GoOn(f));
    f.lossPct3s = 0.0f;
    f.attackers = 2;
    CHECK(GoOn(f));
    f.attackers = 0;
    f.lossPct3s = 60.0f;
    CHECK(!GoOn(f));  // nobody hits it: not a go
}

TEST(pvp_def_the_smaller_defensive_goes_early_in_a_go_and_the_immunity_waits)
{
    DefFacts f = Go(Tier::EXPERT, 75.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    DefPick p = Pick(f, Paladin(), false);
    CHECK(p.use && Paladin()[static_cast<std::size_t>(p.index)].spellId == 498);  // Divine Protection at 75 percent
    f.hpPct = 85.0f;
    CHECK(!Pick(f, Paladin(), false).use);  // above the expert line (80)
}

TEST(pvp_def_the_immunity_goes_last_at_thirty_percent_when_the_tempo_is_gone)
{
    DefFacts f = Go(Tier::EXPERT, 28.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    f.lossPct3s = 10.0f;  // not lethal
    f.attackers = 1;
    std::vector<Candidate> used = { Tempo(498, 60000, Fit::ALL, true, false), Immunity(642) };
    DefPick p = Pick(f, used, false);
    CHECK(p.use && used[static_cast<std::size_t>(p.index)].spellId == 642);
    // With the tempo ready at 28 percent the tempo still goes first.
    p = Pick(f, Paladin(), false);
    CHECK(p.use && Paladin()[static_cast<std::size_t>(p.index)].spellId == 498);
    // Above thirty percent without a tempo defensive: the immunity waits.
    f.hpPct = 45.0f;
    CHECK(!Pick(f, used, false).use);
}

TEST(pvp_def_one_defensive_per_go)
{
    DefFacts f = Go(Tier::EXPERT, 40.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    f.lossPct3s = 25.0f;
    f.sinceLastDefensiveMs = 2000;
    DefPick p = Pick(f, Paladin(), false);
    CHECK(!p.use);
    CHECK(std::string(p.why) == "one-per-go");
    f.sinceLastDefensiveMs = 4500;
    CHECK(Pick(f, Paladin(), false).use);
    f.sinceLastDefensiveMs = 0xFFFFFFFFu;
    f.defensiveRunning = true;
    CHECK(!Pick(f, Paladin(), false).use);
}

TEST(pvp_def_predicted_death_breaks_the_one_per_go_rule_and_takes_the_immunity)
{
    DefFacts f = Go(Tier::EXPERT, 30.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    f.lossPct3s = 30.0f;  // lost its whole remaining health in 3 s
    CHECK(PredictedLethal(f));
    f.sinceLastDefensiveMs = 1000;
    f.defensiveRunning = true;
    DefPick const p = Pick(f, Paladin(), false);
    CHECK(p.use && Paladin()[static_cast<std::size_t>(p.index)].spellId == 642);
    CHECK(std::string(p.why) == "predicted-lethal-immunity");
}

TEST(pvp_def_the_shortest_cooldown_of_the_tempo_defensives_goes_first)
{
    DefFacts f = Go(Tier::EXPERT, 60.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    // A druid: Survival Instincts (3 min) and Barkskin (1 min): the smaller one first.
    std::vector<Candidate> druid = { Tempo(61336, 180000), Tempo(22812, 60000) };
    DefPick const p = Pick(f, druid, false);
    CHECK(p.use && druid[static_cast<std::size_t>(p.index)].spellId == 22812);
}

TEST(pvp_def_a_defensive_that_answers_the_wrong_damage_is_not_pressed)
{
    DefFacts f = Go(Tier::EXPERT, 60.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    std::vector<Candidate> rogue = { Tempo(5277, 120000, Fit::PHYSICAL), Tempo(31224, 60000, Fit::MAGIC) };
    f.meleeThreat = false;
    f.casterThreat = true;
    DefPick p = Pick(f, rogue, false);
    CHECK(p.use && rogue[static_cast<std::size_t>(p.index)].spellId == 31224);  // Cloak against casters
    f.meleeThreat = true;
    f.casterThreat = false;
    p = Pick(f, rogue, false);
    CHECK(p.use && rogue[static_cast<std::size_t>(p.index)].spellId == 5277);   // Evasion against melee
    f.meleeThreat = false;
    CHECK(!Pick(f, rogue, false).use);                                           // a hunter's arrows: neither
}

TEST(pvp_def_a_silence_blocked_defensive_goes_before_the_silence_for_an_expert)
{
    DefFacts f = Go(Tier::EXPERT, 88.0f);
    f.goKey = CleanKey(Tier::EXPERT, f.guid);
    CHECK(!Pick(f, Paladin(), false).use);   // 88 is above the expert line of 80
    f.silenceExpected = true;
    CHECK(Pick(f, Paladin(), false).use);    // 88 is inside the early rise (80 + 10)
    f.hpPct = 92.0f;
    CHECK(!Pick(f, Paladin(), false).use);   // above it
    f.hpPct = 85.0f;
    // A defensive without the flag is not raised.
    std::vector<Candidate> dispersion = { Tempo(47585, 120000, Fit::ALL, false) };
    CHECK(!Pick(f, dispersion, false).use);
}

TEST(pvp_def_tiers_press_at_their_own_lines)
{
    for (Tier tier : { Tier::EXPERT, Tier::GOOD, Tier::CASUAL })
    {
        DefFacts f = Go(tier, 100.0f);
        f.goKey = CleanKey(tier, f.guid);
        float const line = LinesOf(tier).tempoUnderGo;
        f.hpPct = line + 1.0f;
        CHECK(!Pick(f, Paladin(), false).use);
        f.hpPct = line - 1.0f;
        CHECK(Pick(f, Paladin(), false).use);
    }
    CHECK(LinesOf(Tier::EXPERT).tempoUnderGo > LinesOf(Tier::GOOD).tempoUnderGo);
    CHECK(LinesOf(Tier::GOOD).tempoUnderGo > LinesOf(Tier::CASUAL).tempoUnderGo);
}

TEST(pvp_def_without_a_go_the_lines_are_low)
{
    DefFacts f;
    f.tier = Tier::EXPERT;
    f.guid = 99;
    f.goKey = CleanKey(Tier::EXPERT, 99);
    f.attackers = 1;
    f.lossPct3s = 3.0f;
    f.hpPct = 70.0f;
    CHECK(!Pick(f, Paladin(), false).use);   // somebody hits it, but not a go and above 50
    f.hpPct = 45.0f;
    CHECK(Pick(f, Paladin(), false).use);
}

TEST(pvp_def_the_caller_may_declare_a_defensive_due_and_the_rest_of_the_rules_still_pick)
{
    DefFacts f;
    f.tier = Tier::GOOD;
    f.guid = 99;
    f.goKey = CleanKey(Tier::GOOD, 99);
    f.hpPct = 55.0f;
    f.attackers = 1;
    DefPick const p = Pick(f, Paladin(), true);  // the arena plan says it is due
    CHECK(p.use && Paladin()[static_cast<std::size_t>(p.index)].spellId == 498);  // the tempo one, not the Divine Shield
}

TEST(pvp_def_a_good_bot_sometimes_presses_late)
{
    int late = 0;
    for (std::uint32_t key = 1; key <= 1000; ++key)
    {
        DefFacts f = Go(Tier::GOOD, 62.0f);  // the line is 65, the late press 50
        f.goKey = key;
        if (!Pick(f, Paladin(), false).use)
            ++late;
    }
    CHECK(late > 90 && late < 210);  // about 15 percent
    // An expert never does.
    for (std::uint32_t key = 1; key <= 200; ++key)
    {
        DefFacts f = Go(Tier::EXPERT, 78.0f);
        f.goKey = key;
        CHECK(Pick(f, Paladin(), false).use);
    }
}

TEST(pvp_def_nothing_ready_means_nothing_pressed)
{
    DefFacts f = Go(Tier::EXPERT, 20.0f);
    std::vector<Candidate> cold = { Tempo(498, 60000, Fit::ALL, true, false), Immunity(642, 300000, false) };
    CHECK(!Pick(f, cold, false).use);
    CHECK(!Pick(f, {}, false).use);
}

// --- breaks ------------------------------------------------------------------------------------------------------------

TEST(pvp_break_a_class_break_is_used_before_the_racial_and_the_medallion)
{
    std::vector<BreakCandidate> kit = { Brk(18499, CC_FEAR | CC_INCAPACITATE, BREAK_COST_CLASS), Brk(59752, CC_MEDALLION, BREAK_COST_SHARED),
                                        Brk(42292, CC_MEDALLION, BREAK_COST_MEDALLION) };
    BreakFacts f = Stunned();
    f.kinds = CC_FEAR; f.worstKind = CC_FEAR;
    BreakPick p = PickBreak(f, kit);
    CHECK(p.use && kit[static_cast<std::size_t>(p.index)].spellId == 18499);   // Berserker Rage on a fear, at full health
    // A stun is not removed by Berserker Rage: the medallion family, and only if the role wants it.
    f.kinds = CC_STUN; f.worstKind = CC_STUN;
    p = PickBreak(f, kit);
    CHECK(!p.use);
}

TEST(pvp_break_a_racial_with_its_own_cooldown_goes_before_the_medallion)
{
    std::vector<BreakCandidate> kit = { Brk(7744, CC_FEAR | CC_CHARM | CC_SLEEP, BREAK_COST_OWN), Brk(42292, CC_MEDALLION, BREAK_COST_MEDALLION) };
    BreakFacts f = Stunned();
    f.kinds = CC_FEAR; f.worstKind = CC_FEAR;
    BreakPick const p = PickBreak(f, kit);
    CHECK(p.use && kit[static_cast<std::size_t>(p.index)].spellId == 7744);
    CHECK(std::string(p.why) == "cheap-break");
}

TEST(pvp_break_a_short_effect_is_eaten)
{
    BreakFacts f = Stunned();
    f.remainingMs = 900;
    f.hpPct = 10.0f;
    CHECK(!PickBreak(f, Medallion()).use);
}

TEST(pvp_break_a_healer_trinkets_in_the_enemy_kill_window_and_eats_it_otherwise)
{
    BreakFacts f = Stunned(Tier::EXPERT);
    f.isHealer = true;
    CHECK(!PickBreak(f, Medallion()).use);        // everybody healthy, no burst: eat the stun
    f.teammateLow = true;
    BreakPick p = PickBreak(f, Medallion());
    CHECK(p.use && std::string(p.why) == "medallion");
    f.teammateLow = false;
    f.enemyBurst = true;
    CHECK(PickBreak(f, Medallion()).use);
    // A half length control (under 3 s) in the window is eaten.
    f.remainingMs = 2500;
    CHECK(!PickBreak(f, Medallion()).use);
}

TEST(pvp_break_a_damage_dealer_trinkets_for_a_kill_or_in_danger_not_at_full_health)
{
    BreakFacts f = Stunned(Tier::GOOD);
    CHECK(!PickBreak(f, Medallion()).use);                        // TR4: stunned at full health, no burst
    f.enemyHealerHeld = true;
    f.killTargetHp = 30.0f;
    CHECK(PickBreak(f, Medallion()).use);                         // TR3: healer locked and the kill target low
    f.killTargetHp = 70.0f;
    CHECK(!PickBreak(f, Medallion()).use);
    f.enemyHealerHeld = false;
    f.hpPct = 60.0f;
    f.lossPctPerSec = 10.0f;                                      // 60 - 10 * 4 = 20: would be at the edge when it ends
    CHECK(PickBreak(f, Medallion()).use);
}

TEST(pvp_break_an_expert_holds_the_medallion_for_a_follow_up_unless_it_would_die)
{
    BreakFacts f = Stunned(Tier::EXPERT);
    f.enemyHealerHeld = true;
    f.killTargetHp = 30.0f;
    f.followUpPossible = true;
    BreakPick p = PickBreak(f, Medallion());
    CHECK(!p.use && std::string(p.why) == "hold-for-followup");
    f.hpPct = 50.0f;
    f.lossPctPerSec = 10.0f;                                       // 50 - 40 = 10: lethal
    CHECK(PickBreak(f, Medallion()).use);
    // A good bot does not read the follow-up.
    BreakFacts g = Stunned(Tier::GOOD);
    g.enemyHealerHeld = true;
    g.killTargetHp = 30.0f;
    g.followUpPossible = true;
    CHECK(PickBreak(g, Medallion()).use);
}

TEST(pvp_break_a_movement_impairment_alone_is_broken_only_when_the_bot_needs_to_move)
{
    std::vector<BreakCandidate> kit = { Brk(20589, CC_ROOT | CC_SNARE, BREAK_COST_OWN), Brk(42292, CC_MEDALLION, BREAK_COST_MEDALLION) };
    BreakFacts f = Stunned();
    f.kinds = CC_ROOT; f.worstKind = 0; f.remainingMs = 5000;
    BreakPick p = PickBreak(f, kit);
    CHECK(!p.use && std::string(p.why) == "eat-slow");
    f.needsToMove = true;
    p = PickBreak(f, kit);
    CHECK(p.use && kit[static_cast<std::size_t>(p.index)].spellId == 20589);  // Escape Artist, not the medallion
}

TEST(pvp_break_nothing_to_break_and_nothing_ready_do_nothing)
{
    BreakFacts f = Stunned();
    f.kinds = 0; f.worstKind = 0;
    CHECK(!PickBreak(f, Medallion()).use);
    BreakFacts g = Stunned();
    g.hpPct = 20.0f;
    std::vector<BreakCandidate> cold = { Brk(42292, CC_MEDALLION, BREAK_COST_MEDALLION, false) };
    CHECK(!PickBreak(g, cold).use);
    // A silence is not removed by the medallion: no candidate covers it.
    BreakFacts s = Stunned();
    s.kinds = CC_SILENCE | CC_STUN; s.worstKind = CC_STUN;
    CHECK(!PickBreak(s, {}).use);
}

TEST(pvp_break_a_mistake_on_purpose_trinkets_at_full_health_at_the_tier_rate)
{
    int casual = 0, expert = 0;
    for (std::uint32_t key = 1; key <= 1000; ++key)
    {
        BreakFacts c = Stunned(Tier::CASUAL);
        c.effectKey = key;
        if (PickBreak(c, Medallion()).use) ++casual;
        BreakFacts e = Stunned(Tier::EXPERT);
        e.effectKey = key;
        if (PickBreak(e, Medallion()).use) ++expert;
    }
    CHECK(casual > 240 && casual < 360);   // about 30 percent
    CHECK(expert > 20 && expert < 90);     // about 5 percent
}

TEST(pvp_def_loss_over_a_window_reads_the_highest_sample_less_the_newest)
{
    std::vector<HealthSample> s = { { 1000, 100.0f }, { 2000, 90.0f }, { 3000, 70.0f }, { 3900, 60.0f } };
    CHECK_NEAR(LossOver(s, 4000, 3000), 40.0f, 0.001);   // 100 (1000 ms ago is inside 3 s) down to 60
    CHECK_NEAR(LossOver(s, 5500, 3000), 10.0f, 0.001);   // only 70 and 60 inside
    CHECK_NEAR(LossOver({}, 4000, 3000), 0.0f, 0.001);
    std::vector<HealthSample> healing = { { 1000, 50.0f }, { 3900, 80.0f } };
    CHECK_NEAR(LossOver(healing, 4000, 3000), 0.0f, 0.001);
}

// --- the 1v1 engine reads the verdict -----------------------------------------------------------------------------------

#include "OneVsOnePolicy.h"

TEST(pvp_def_the_1v1_engine_follows_the_shared_verdict_instead_of_its_health_line)
{
    OneVsOne::Facts f;
    f.tier = OneVsOne::Tier::EXPERT;
    f.guidLow = 1234;
    f.self = OneVsOne::Role::MELEE;
    f.enemy = OneVsOne::Role::MELEE;
    f.dist = 3.0f;
    f.hpPct = 72.0f;                 // above the expert health line (55)
    f.defensiveReady = true;
    f.lowHpForMs = 5000;
    f.nowMs = 3000 * 1;              // inside a clean window of this guid
    // Find a clean window for this guid so that the slip roll does not interfere (expert never slips).
    OneVsOne::Option const old = OneVsOne::Choose(f);
    CHECK(old.choice != OneVsOne::Choice::DEFENSIVE);        // the old line: 72 is above 55
    f.defensivePolicy = true;
    OneVsOne::Option const now = OneVsOne::Choose(f);
    CHECK(now.choice == OneVsOne::Choice::DEFENSIVE);        // the shared rules said now
    // The reaction time still applies.
    f.lowHpForMs = 10;
    CHECK(OneVsOne::Choose(f).choice != OneVsOne::Choice::DEFENSIVE);
    // And nothing is pressed when the policy has no defensive ready.
    f.lowHpForMs = 5000;
    f.defensiveReady = false;
    CHECK(OneVsOne::Choose(f).choice != OneVsOne::Choice::DEFENSIVE);
}
