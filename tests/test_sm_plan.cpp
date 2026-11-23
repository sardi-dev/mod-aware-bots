#include "TestHarness.h"
#include "AiPvpDecisionPolicy.h"
#include "BgLeaderAdapters.h"
#include "SmPlanLogic.h"

// BT-610: Silvershard Mines plan logic (docs/bg-sheets/silvershard-mines.md): the rails, the bar, the cart options, the counts and
// the deal, the switches, the stance and the leash.

using namespace SmLogic;
using BgLeader::Stance;

namespace
{
CartFacts Facts(std::uint8_t cart, std::uint8_t keeper = TEAM_NONE, float remaining = 100.0f, float eta = 20.0f)
{
    CartFacts f;
    f.cart = cart;
    f.live = true;
    f.keeper = keeper;
    f.bar = keeper == TEAM_IDX_ALLIANCE ? 80 : keeper == TEAM_IDX_HORDE ? 20 : BAR_START;
    f.remainingS = remaining;
    f.etaS = eta;
    return f;
}

std::array<CartFacts, CART_COUNT> ThreeCarts()
{
    return { Facts(CART_EAST, TEAM_NONE, 149.0f), Facts(CART_WATERFALL, TEAM_NONE, 124.0f), Facts(CART_NORTH, TEAM_NONE, 176.0f) };
}

Hand HandOf(std::uint32_t guid, int offFit, int mainFit = 0, float eta = 20.0f, std::uint8_t prev = CART_NONE, Tier tier = Tier::GOOD)
{
    Hand h;
    h.guidLow = guid;
    h.tier = tier;
    h.offFit = offFit;
    h.mainFit = mainFit;
    h.eta = { eta, eta, eta };
    h.prevCart = prev;
    return h;
}
}  // namespace

// --- The rails ------------------------------------------------------------------------------------------------------------

TEST(sm_the_rides_take_the_times_of_the_waypoints)
{
    // 183, 297, 247, 352 and 457 yd at 2 yd/s (sheet section 2).
    CHECK_NEAR(RideSeconds(CART_EAST, true), 91.6, 1.0);
    CHECK_NEAR(RideSeconds(CART_EAST, false), 148.6, 1.0);
    CHECK_NEAR(RideSeconds(CART_WATERFALL, false), 123.6, 1.0);
    CHECK_NEAR(RideSeconds(CART_NORTH, false), 175.9, 1.0);
    CHECK_NEAR(RideSeconds(CART_NORTH, true), 228.3, 1.0);
}

TEST(sm_the_depot_follows_the_switch)
{
    CHECK(DepotOf(CART_EAST, false) == DEPOT_DIAMOND);
    CHECK(DepotOf(CART_EAST, true) == DEPOT_LAVA);
    CHECK(DepotOf(CART_NORTH, false) == DEPOT_TROLL);
    CHECK(DepotOf(CART_NORTH, true) == DEPOT_DIAMOND);
    CHECK(DepotOf(CART_WATERFALL, true) == DEPOT_WATERFALL);
    CHECK(BuildRoute(CART_EAST, true).depot == DEPOT_LAVA);
}

TEST(sm_the_junction_comes_at_38_and_66_seconds_and_only_for_two_carts)
{
    Route const east = BuildRoute(CART_EAST, false);
    Route const north = BuildRoute(CART_NORTH, false);
    Route const mid = BuildRoute(CART_WATERFALL, false);
    CHECK(east.hasJunction && north.hasJunction && !mid.hasJunction);
    CHECK_NEAR(SecondsToJunction(east, CART_START[CART_EAST].x, CART_START[CART_EAST].y), 37.6, 1.0);
    CHECK_NEAR(SecondsToJunction(north, CART_START[CART_NORTH].x, CART_START[CART_NORTH].y), 65.8, 1.0);
    CHECK(!BeforeJunction(mid, CART_START[CART_WATERFALL].x, CART_START[CART_WATERFALL].y));
    CHECK(BeforeJunction(east, CART_START[CART_EAST].x, CART_START[CART_EAST].y));
    // The junction point itself: the core picks the path within half a yard, so a cart there is no longer before it.
    CHECK(!BeforeJunction(east, JUNCTION_POS[TRACK_EAST].x, JUNCTION_POS[TRACK_EAST].y));
}

TEST(sm_a_cart_at_the_start_has_the_whole_ride_left_and_one_at_the_depot_none)
{
    Route const east = BuildRoute(CART_EAST, true);
    CHECK_NEAR(RemainingSeconds(east, CART_START[CART_EAST].x, CART_START[CART_EAST].y), RideSeconds(CART_EAST, true), 1.0);
    CHECK(RemainingSeconds(east, DEPOT_POS[DEPOT_LAVA].x, DEPOT_POS[DEPOT_LAVA].y) < 2.0f);
    // Halfway the remaining time is about half.
    Pt const mid = PointAlong(east, RouteLength(east) * 0.5f);
    CHECK_NEAR(RemainingSeconds(east, mid.x, mid.y), RideSeconds(CART_EAST, true) * 0.5, 1.5);
}

TEST(sm_a_point_ahead_on_the_rail_moves_forward_and_stops_at_the_depot)
{
    Route const north = BuildRoute(CART_NORTH, true);
    Pt const a = PointAlong(north, 50.0f);
    Pt const b = PointAlong(north, 100.0f);
    CHECK(Project(north, b.x, b.y).along > Project(north, a.x, a.y).along + 40.0f);
    Pt const end = PointAlong(north, 99999.0f);
    CHECK(Dist2d(end, DEPOT_POS[DEPOT_DIAMOND]) < 8.0f);
    CHECK(Dist2d(PointAlong(north, -5.0f), CART_START[CART_NORTH]) < 0.01f);
}

TEST(sm_the_depots_lie_within_6_yd_of_the_end_of_their_rails)
{
    // BG_SM_DEPOT_RANGE: the core stops a cart that comes within 6 yd (2D) of a depot of its pair.
    CHECK(Dist2d(BuildRoute(CART_EAST, true).pts.back(), DEPOT_POS[DEPOT_LAVA]) <= 6.0f);
    CHECK(Dist2d(BuildRoute(CART_EAST, false).pts.back(), DEPOT_POS[DEPOT_DIAMOND]) <= 6.0f);
    CHECK(Dist2d(BuildRoute(CART_WATERFALL, false).pts.back(), DEPOT_POS[DEPOT_WATERFALL]) <= 6.0f);
    CHECK(Dist2d(BuildRoute(CART_NORTH, false).pts.back(), DEPOT_POS[DEPOT_TROLL]) <= 6.0f);
    CHECK(Dist2d(BuildRoute(CART_NORTH, true).pts.back(), DEPOT_POS[DEPOT_DIAMOND]) <= 6.0f);
}

TEST(sm_the_first_legs_end_at_the_junctions_the_core_reads)
{
    CHECK(Dist2d(FirstLeg(CART_EAST).pts[FirstLeg(CART_EAST).count - 1], JUNCTION_POS[TRACK_EAST]) < JUNCTION_RADIUS);
    CHECK(Dist2d(FirstLeg(CART_NORTH).pts[FirstLeg(CART_NORTH).count - 1], JUNCTION_POS[TRACK_NORTH]) < JUNCTION_RADIUS);
}

TEST(sm_a_walker_meets_a_cart_ahead_of_it_and_never_one_about_to_dock)
{
    Route const route = BuildRoute(CART_NORTH, false);
    // A bot standing on the first leg 60 yd down the rail meets the cart within about half a minute.
    Pt const spot = PointAlong(route, 60.0f);
    Intercept const near = InterceptOf(route, 0.0f, spot.x, spot.y, false);
    CHECK(near.possible && near.seconds <= 30.0f);
    // A bot at the Alliance start meets the north cart before the junction, with the cart already out along the rail.
    Intercept const far = InterceptOf(route, 0.0f, TEAM_START[TEAM_IDX_ALLIANCE].x, TEAM_START[TEAM_IDX_ALLIANCE].y, false);
    CHECK(far.possible && far.seconds > 10.0f && far.seconds < 40.0f);
    // A cart 5 s from the depot cannot be reached from the other end of the map.
    float const nearEnd = RouteLength(route) - 10.0f;
    Intercept const late = InterceptOf(route, nearEnd, TEAM_START[TEAM_IDX_HORDE].x, TEAM_START[TEAM_IDX_HORDE].y, false);
    CHECK(!late.possible);
    // Mounted is faster.
    CHECK(WalkSeconds(100.0f, true) < WalkSeconds(100.0f, false));
}

// --- The bar --------------------------------------------------------------------------------------------------------------

TEST(sm_the_bar_moves_by_the_net_bodies_and_stops_at_the_ends)
{
    CHECK(StepBar(50, 3, 0, 4.0f) == 62);
    CHECK(StepBar(50, 0, 2, 5.0f) == 40);
    CHECK(StepBar(50, 3, 3, 30.0f) == 50);           // equal numbers freeze the bar
    CHECK(StepBar(95, 5, 0, 10.0f) == 100);
    CHECK(StepBar(4, 0, 3, 10.0f) == 0);
}

TEST(sm_the_keeper_is_the_side_of_50_and_the_aura_wins_over_the_estimate)
{
    CHECK(KeeperOfBar(51) == TEAM_IDX_ALLIANCE);
    CHECK(KeeperOfBar(49) == TEAM_IDX_HORDE);
    CHECK(KeeperOfBar(50) == TEAM_NONE);
    CHECK(ClampToKeeper(50, TEAM_IDX_ALLIANCE) == 51);
    CHECK(ClampToKeeper(70, TEAM_IDX_HORDE) == 49);
    CHECK(ClampToKeeper(80, TEAM_IDX_ALLIANCE) == 80);
    CHECK(ClampToKeeper(10, TEAM_NONE) == 50);
}

TEST(sm_a_flip_takes_the_margin_over_the_net_bodies)
{
    // From a bar at 100 the Horde needs 51 steps to get to 49: 3 net take 17 s, 5 net 11 s, 1 net 51 s.
    CHECK_NEAR(FlipSeconds(100, TEAM_IDX_HORDE, 3, 0), 17.0, 0.01);
    CHECK_NEAR(FlipSeconds(100, TEAM_IDX_HORDE, 5, 0), 10.2, 0.01);
    CHECK_NEAR(FlipSeconds(100, TEAM_IDX_HORDE, 6, 5), 51.0, 0.01);
    CHECK(FlipSeconds(100, TEAM_IDX_HORDE, 4, 4) >= NEVER_S);
    CHECK(FlipSeconds(100, TEAM_IDX_HORDE, 2, 4) >= NEVER_S);
    // A neutral bar needs one step; a cart a team already keeps needs none.
    CHECK_NEAR(FlipSeconds(50, TEAM_IDX_ALLIANCE, 1, 0), 1.0, 0.01);
    CHECK(FlipSeconds(60, TEAM_IDX_ALLIANCE, 1, 0) == 0.0f);
    CHECK(MarginOf(60, TEAM_IDX_ALLIANCE) == 10 && MarginOf(60, TEAM_IDX_HORDE) == 0);
}

// --- The options ----------------------------------------------------------------------------------------------------------

TEST(sm_a_neutral_cart_is_taken_for_its_worth_minus_the_walk)
{
    CartOption const o = ScoreCart(Facts(CART_EAST, TEAM_NONE, 100.0f, 20.0f), TEAM_IDX_ALLIANCE);
    CHECK(o.kind == OptionKind::TAKE);
    CHECK(o.score == 100 + DOCK_POINTS - 20);
}

TEST(sm_an_enemy_cart_is_flipped_for_twice_its_worth_when_the_bar_can_be_turned)
{
    CartFacts f = Facts(CART_NORTH, TEAM_IDX_HORDE, 120.0f, 20.0f);
    f.friends = 2;
    f.enemies = 0;
    CartOption const o = ScoreCart(f, TEAM_IDX_ALLIANCE);
    CHECK(o.kind == OptionKind::FLIP);
    CHECK(o.score > 2 * CartWorth(120.0f) - 20 - 20);          // the flip seconds (bar 20 to 51 with three of us) come off
    CHECK(o.score < 2 * CartWorth(120.0f));
    // The same cart with a group of enemies we cannot outnumber: no flip.
    f.enemies = 5;
    CHECK(ScoreCart(f, TEAM_IDX_ALLIANCE).kind == OptionKind::SKIP);
    // The bar is hard (0) and the cart docks soon: the flip does not fit.
    CartFacts late = Facts(CART_NORTH, TEAM_IDX_HORDE, 40.0f, 20.0f);
    late.bar = 0;
    late.friends = 1;
    CHECK(ScoreCart(late, TEAM_IDX_ALLIANCE).kind == OptionKind::SKIP);
}

TEST(sm_a_cart_we_keep_is_guarded_by_what_the_enemy_puts_at_risk)
{
    CartFacts quiet = Facts(CART_EAST, TEAM_IDX_ALLIANCE, 100.0f);
    quiet.friends = 1;
    CartOption const q = ScoreCart(quiet, TEAM_IDX_ALLIANCE);
    CHECK(q.kind == OptionKind::GUARD && q.score == CartWorth(100.0f) / 4);
    CartFacts hot = quiet;
    hot.enemies = 3;
    CartOption const h = ScoreCart(hot, TEAM_IDX_ALLIANCE);
    CHECK(h.score > q.score && h.score <= CartWorth(100.0f));
}

TEST(sm_a_cart_about_to_dock_or_out_of_reach_scores_nothing)
{
    CHECK(ScoreCart(Facts(CART_EAST, TEAM_NONE, 8.0f, 2.0f), TEAM_IDX_ALLIANCE).score == 0);
    CHECK(ScoreCart(Facts(CART_EAST, TEAM_NONE, 30.0f, 40.0f), TEAM_IDX_ALLIANCE).kind == OptionKind::SKIP);
    CartFacts gone = Facts(CART_EAST);
    gone.live = false;
    CHECK(ScoreCart(gone, TEAM_IDX_ALLIANCE).score == 0);
    CartFacts none = Facts(CART_EAST);
    none.etaS = NEVER_S;
    CHECK(ScoreCart(none, TEAM_IDX_HORDE).kind == OptionKind::SKIP);
}

TEST(sm_the_list_is_best_first_with_ties_to_the_lower_cart)
{
    auto facts = ThreeCarts();
    facts[CART_EAST].remainingS = 100.0f;
    facts[CART_WATERFALL].remainingS = 100.0f;
    facts[CART_NORTH].remainingS = 100.0f;
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);
    CHECK(list.size() == 3);
    CHECK(list[0].cart == CART_EAST && list[1].cart == CART_WATERFALL && list[2].cart == CART_NORTH);
    facts[CART_NORTH].remainingS = 170.0f;
    CHECK(ScoreCarts(facts, TEAM_IDX_ALLIANCE).front().cart == CART_NORTH);
    // Claim and arrival bonuses add to the score.
    facts[CART_WATERFALL].claimBonus = 200;
    CHECK(ScoreCarts(facts, TEAM_IDX_ALLIANCE).front().cart == CART_WATERFALL);
}

TEST(sm_the_opening_bonus_goes_to_the_team_that_reaches_the_cart_first)
{
    // The map is point-symmetric: what one team gains the other loses.
    for (std::uint8_t cart = 0; cart < CART_COUNT; ++cart)
        CHECK(OpeningBonus(TEAM_IDX_ALLIANCE, cart) == -OpeningBonus(TEAM_IDX_HORDE, cart));
    // The Alliance reaches the north cart first, the Horde the waterfall cart (sheet section 5), the east cart is even.
    CHECK(OpeningBonus(TEAM_IDX_ALLIANCE, CART_NORTH) > 0);
    CHECK(OpeningBonus(TEAM_IDX_HORDE, CART_WATERFALL) > 0);
    CHECK(OpeningBonus(TEAM_IDX_ALLIANCE, CART_WATERFALL) < 0);
    CHECK(std::abs(OpeningBonus(TEAM_IDX_ALLIANCE, CART_EAST)) <= 5);
    CHECK(OpeningBonus(TEAM_IDX_ALLIANCE, CART_NORTH) <= static_cast<int>(OPENING_BONUS_CAP_S * OPENING_BONUS_PER_S));
}

// --- Counts and the deal --------------------------------------------------------------------------------------------------

TEST(sm_the_off_counts_follow_the_table_of_the_sheet)
{
    // Section 4: balanced 2 off, offense 1, defense 3 from 8 players, never less than 3 in the main group.
    CHECK(OffCount(10, Stance::BALANCED, true) == 2);
    CHECK(OffCount(10, Stance::OFFENSE, true) == 1);
    CHECK(OffCount(10, Stance::DEFENSE, true) == 3);
    CHECK(OffCount(8, Stance::DEFENSE, true) == 3);
    CHECK(OffCount(7, Stance::DEFENSE, true) == 2);
    CHECK(OffCount(5, Stance::BALANCED, true) == 2);
    CHECK(OffCount(6, Stance::BALANCED, true) == 2);
    CHECK(OffCount(5, Stance::OFFENSE, true) == 1);
    CHECK(OffCount(4, Stance::BALANCED, true) == 1);
    CHECK(OffCount(3, Stance::BALANCED, true) == 0);
    CHECK(OffCount(10, Stance::ALL_IN, true) == 1 && OffCount(10, Stance::TURTLE, true) == 3);
    // No healer: nobody is an off bot.
    CHECK(OffCount(10, Stance::BALANCED, false) == 0);
}

TEST(sm_the_main_cart_is_the_top_score_and_the_off_carts_get_one_bot_each)
{
    auto facts = ThreeCarts();
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);
    Wants const w = WantCounts(list, facts, 10, Stance::BALANCED, true, CART_EAST);
    CHECK(w.mainCart == CART_NORTH);                              // 326 against 299 and 274
    CHECK(w.count[CART_NORTH] == 8 && w.count[CART_EAST] == 1 && w.count[CART_WATERFALL] == 1);
    Wants const off = WantCounts(list, facts, 10, Stance::OFFENSE, true, CART_EAST);
    CHECK(off.count[w.mainCart] == 9);
    Wants const def = WantCounts(list, facts, 10, Stance::DEFENSE, true, CART_EAST);
    CHECK(def.count[w.mainCart] == 8);                            // 3 off wanted but only two off carts exist
    // Five players: 3 in the main group.
    Wants const five = WantCounts(list, facts, 5, Stance::BALANCED, true, CART_EAST);
    CHECK(five.count[five.mainCart] == 3);
}

TEST(sm_an_off_cart_that_is_outnumbered_and_worth_it_gets_a_second_bot)
{
    auto facts = ThreeCarts();
    facts[CART_EAST].enemies = 3;
    facts[CART_EAST].friends = 0;
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);
    Wants const w = WantCounts(list, facts, 10, Stance::BALANCED, true, CART_EAST);
    CHECK(w.count[CART_EAST] == 2);
    CHECK(w.count[w.mainCart] == 7);
    // With a main group of 3 nobody is taken from it.
    Wants const small = WantCounts(list, facts, 5, Stance::BALANCED, true, CART_EAST);
    CHECK(small.count[small.mainCart] == 3 && small.count[CART_EAST] == 1);
}

TEST(sm_no_scored_cart_sends_everybody_to_the_fallback)
{
    auto facts = ThreeCarts();
    for (CartFacts& f : facts)
        f.etaS = NEVER_S;
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);
    Wants const w = WantCounts(list, facts, 6, Stance::BALANCED, true, CART_WATERFALL);
    CHECK(w.mainCart == CART_WATERFALL && w.count[CART_WATERFALL] == 6);
    CHECK(WantCounts(list, facts, 0, Stance::BALANCED, true, CART_WATERFALL).mainCart == CART_NONE);
}

TEST(sm_the_deal_gives_off_carts_to_durable_kits_and_never_to_a_healer)
{
    std::vector<Hand> hands;
    hands.push_back(HandOf(1, BgPlan::NOT_ELIGIBLE, 10));            // healer
    hands.push_back(HandOf(2, OffFitOf(BgPlan::CLS_MAGE, false, false, false)));
    hands.push_back(HandOf(3, OffFitOf(BgPlan::CLS_DEATH_KNIGHT, false, false, false)));
    hands.push_back(HandOf(4, OffFitOf(BgPlan::CLS_DRUID, false, false, true)));
    hands.push_back(HandOf(5, OffFitOf(BgPlan::CLS_WARLOCK, false, false, false)));
    Wants w;
    w.mainCart = CART_NORTH;
    w.count = { 1, 1, 3 };
    std::vector<std::uint8_t> const deal = Deal(hands, w);
    CHECK(deal[0] == CART_NORTH);                                    // the healer stays in the main group
    CHECK((deal[2] == CART_EAST || deal[2] == CART_WATERFALL));      // death knight and feral druid take the off carts
    CHECK((deal[3] == CART_EAST || deal[3] == CART_WATERFALL));
    CHECK(deal[2] != deal[3]);
    CHECK(deal[1] == CART_NORTH && deal[4] == CART_NORTH);
}

TEST(sm_a_bot_keeps_its_cart_while_the_counts_allow_it)
{
    std::vector<Hand> hands;
    hands.push_back(HandOf(10, OffFitOf(BgPlan::CLS_WARRIOR, false, false, false), 0, 20.0f, CART_NONE));
    hands.push_back(HandOf(11, OffFitOf(BgPlan::CLS_WARRIOR, false, false, false), 0, 22.0f, CART_WATERFALL));
    hands.push_back(HandOf(12, 0, 0, 5.0f, CART_NORTH));
    hands.push_back(HandOf(13, 0, 0, 5.0f, CART_NORTH));
    Wants w;
    w.mainCart = CART_NORTH;
    w.count = { 0, 1, 3 };
    std::vector<std::uint8_t> const deal = Deal(hands, w);
    CHECK(deal[1] == CART_WATERFALL);            // 11 keeps the cart although 10 is a little nearer
    CHECK(deal[0] == CART_NORTH);
}

TEST(sm_the_deal_prefers_the_nearer_bot_when_the_fit_is_equal)
{
    std::vector<Hand> hands;
    Hand a = HandOf(20, 20);
    a.eta = { 40.0f, 40.0f, 40.0f };
    Hand b = HandOf(21, 20);
    b.eta = { 10.0f, 10.0f, 10.0f };
    hands.push_back(a);
    hands.push_back(b);
    hands.push_back(HandOf(22, 0));
    Wants w;
    w.mainCart = CART_NORTH;
    w.count = { 1, 0, 2 };
    std::vector<std::uint8_t> const deal = Deal(hands, w);
    CHECK(deal[1] == CART_EAST && deal[0] == CART_NORTH);
    // A bot that cannot get to an off cart is not dealt it.
    hands[0].eta[CART_EAST] = NEVER_S;
    hands[1].eta[CART_EAST] = NEVER_S;
    hands[2].eta[CART_EAST] = NEVER_S;
    std::vector<std::uint8_t> const none = Deal(hands, w);
    CHECK(none[0] == CART_NORTH && none[1] == CART_NORTH && none[2] == CART_NORTH);
    CHECK(Deal(hands, Wants()).front() == CART_NONE);
}

TEST(sm_off_fit_puts_the_sturdy_kits_first)
{
    CHECK(OffFitOf(BgPlan::CLS_WARRIOR, false, true, false) > OffFitOf(BgPlan::CLS_WARRIOR, false, false, false));
    CHECK(OffFitOf(BgPlan::CLS_DRUID, false, false, true) > OffFitOf(BgPlan::CLS_DRUID, false, false, false));
    CHECK(OffFitOf(BgPlan::CLS_ROGUE, false, false, false) > OffFitOf(BgPlan::CLS_MAGE, false, false, false));
    CHECK(OffFitOf(BgPlan::CLS_PRIEST, true, false, false) == BgPlan::NOT_ELIGIBLE);
    CHECK(MainFitOf(true) > MainFitOf(false));
}

// --- The switches ---------------------------------------------------------------------------------------------------------

TEST(sm_each_team_prefers_the_ends_near_its_base)
{
    // Alliance: east closed (Diamond), north opened (Diamond). Horde: east opened (Lava); north makes no real difference.
    CHECK(!PreferredSwitch(TRACK_EAST, TEAM_IDX_ALLIANCE).opened && PreferredSwitch(TRACK_EAST, TEAM_IDX_ALLIANCE).gain >= SWITCH_MIN_GAIN);
    CHECK(PreferredSwitch(TRACK_NORTH, TEAM_IDX_ALLIANCE).opened && PreferredSwitch(TRACK_NORTH, TEAM_IDX_ALLIANCE).gain >= SWITCH_MIN_GAIN);
    CHECK(PreferredSwitch(TRACK_EAST, TEAM_IDX_HORDE).opened && PreferredSwitch(TRACK_EAST, TEAM_IDX_HORDE).gain >= SWITCH_MIN_GAIN);
    CHECK(PreferredSwitch(TRACK_NORTH, TEAM_IDX_HORDE).gain < SWITCH_MIN_GAIN);
    CHECK(IsHomeDepot(DEPOT_DIAMOND, TEAM_IDX_ALLIANCE) && !IsHomeDepot(DEPOT_DIAMOND, TEAM_IDX_HORDE));
    CHECK(IsHomeDepot(DEPOT_LAVA, TEAM_IDX_HORDE) && IsHomeDepot(DEPOT_WATERFALL, TEAM_IDX_HORDE) && !IsHomeDepot(DEPOT_TROLL, TEAM_IDX_HORDE));
}

namespace
{
SwitchFacts Duty(std::uint8_t track, std::uint8_t team, bool opened, Tier tier, float toJunction = 40.0f, float walk = 10.0f)
{
    SwitchFacts f;
    f.track = track;
    f.team = team;
    f.cartLive = true;
    f.beforeJunction = true;
    f.secondsToJunction = toJunction;
    f.opened = opened;
    f.walkS = walk;
    f.tier = tier;
    return f;
}
}  // namespace

TEST(sm_a_good_bot_clicks_a_wrong_switch_at_once)
{
    // The Horde wants the east switch opened and finds it closed (the default).
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::GOOD)) == SwitchAct::CLICK_NOW);
    // Already the way it wants it: no duty.
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, true, Tier::GOOD)) == SwitchAct::NONE);
    // The Alliance wants the north switch opened.
    CHECK(ChooseSwitch(Duty(TRACK_NORTH, TEAM_IDX_ALLIANCE, false, Tier::GOOD, 60.0f)) == SwitchAct::CLICK_NOW);
}

TEST(sm_an_expert_waits_for_the_last_window_and_a_casual_bot_never_holds_a_switch)
{
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::EXPERT, 30.0f)) == SwitchAct::WAIT_LAST);
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::EXPERT, LAST_WINDOW_S + SWITCH_CAST_S - 1.0f, 2.0f)) == SwitchAct::CLICK_NOW);
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::CASUAL)) == SwitchAct::NONE);
    CHECK(MayClickNow(SwitchAct::CLICK_NOW, 40.0f));
    CHECK(!MayClickNow(SwitchAct::WAIT_LAST, 30.0f));
    CHECK(MayClickNow(SwitchAct::WAIT_LAST, LAST_WINDOW_S));
    CHECK(!MayClickNow(SwitchAct::LEAVE, 1.0f));
}

TEST(sm_the_duty_ends_when_it_is_too_late_or_a_group_holds_the_switch)
{
    // The cart is 12 s from its junction and the walk is 10 s: no time for the cast.
    CHECK(ChooseSwitch(Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::GOOD, 12.0f, 10.0f)) == SwitchAct::LEAVE);
    SwitchFacts guarded = Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::GOOD);
    guarded.enemiesAtSwitch = 2;
    CHECK(ChooseSwitch(guarded) == SwitchAct::LEAVE);
    guarded.enemiesAtSwitch = 1;
    CHECK(ChooseSwitch(guarded) == SwitchAct::CLICK_NOW);
    // After the junction, or with no cart, nothing to change.
    SwitchFacts passed = Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::GOOD);
    passed.beforeJunction = false;
    CHECK(ChooseSwitch(passed) == SwitchAct::NONE);
    passed.beforeJunction = true;
    passed.cartLive = false;
    CHECK(ChooseSwitch(passed) == SwitchAct::NONE);
}

TEST(sm_a_cast_is_cancelled_when_the_enemy_already_turned_the_switch_the_right_way)
{
    SwitchFacts f = Duty(TRACK_EAST, TEAM_IDX_HORDE, false, Tier::EXPERT);
    CHECK(!CancelCast(f));
    f.opened = true;           // the toggle of the enemy: our click would turn it back
    CHECK(CancelCast(f));
}

TEST(sm_the_holder_is_a_good_or_expert_bot_that_can_be_there_in_time)
{
    std::vector<SwitchCandidate> list;
    list.push_back({ 1, Tier::CASUAL, 3.0f, true, true });
    list.push_back({ 2, Tier::GOOD, 30.0f, false, false });
    list.push_back({ 3, Tier::GOOD, 12.0f, true, false });
    list.push_back({ 4, Tier::EXPERT, 8.0f, false, false });
    CHECK(PickSwitchHolder(list, 40.0f) == 3);       // the off bot of the three that can make it
    CHECK(PickSwitchHolder(list, 22.0f) == 3);
    list[2].offBot = false;
    CHECK(PickSwitchHolder(list, 40.0f) == 4);       // no off bot: the near expert
    list[3].walkS = 40.0f;
    CHECK(PickSwitchHolder(list, 40.0f) == 3);       // the expert is too far: the nearer good bot
    list[0].onTheTrackCart = true;
    CHECK(PickSwitchHolder(list, 40.0f) == 3);       // a casual bot never holds a switch, even one riding the cart
    CHECK(PickSwitchHolder(list, 5.0f) == 0);        // nobody can make it
    CHECK(PickSwitchHolder({}, 40.0f) == 0);
}

// --- Stance, offers and the leash --------------------------------------------------------------------------------------

TEST(sm_the_stance_follows_the_lead_with_a_band)
{
    RaceFacts f;
    f.ourScore = 300;
    f.theirScore = 300;
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::BALANCED);
    CHECK(ChooseStance(BgPlan::Plan::OFFENSE, f, Stance::BALANCED) == Stance::OFFENSE);
    f.ourScore = 410;
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::DEFENSE);
    f.ourScore = 380;                 // 80 ahead: below the enter lead, above the leave lead
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::BALANCED);
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::DEFENSE) == Stance::DEFENSE);
    f.ourScore = 200;
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::OFFENSE);
    f.ourScore = 1100;
    f.theirScore = 940;
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::TURTLE);
    f.ourScore = 900;
    f.theirScore = 1100;
    CHECK(ChooseStance(BgPlan::Plan::BALANCED, f, Stance::BALANCED) == Stance::ALL_IN);
}

TEST(sm_the_race_clock_reads_the_kept_carts)
{
    CHECK(EtaSeconds(1000, 3) == 200);
    CHECK(EtaSeconds(1000, 1) == 600);
    CHECK(EtaSeconds(1000, 0) == 1200);          // at least half a point a second
    CHECK(EtaSeconds(1600, 2) == 0);
}

TEST(sm_the_stance_list_has_the_rule_first)
{
    RaceFacts f;
    BgLeader::Offer const level = StanceOffer(Stance::BALANCED, f);
    CHECK(level.Fallback() == static_cast<std::uint8_t>(Stance::BALANCED) && level.options.size() == 3);
    CHECK(BgLeader::IsMeaningful(level));
    f.ourScore = 500;
    BgLeader::Offer const ahead = StanceOffer(Stance::DEFENSE, f);
    CHECK(ahead.Fallback() == static_cast<std::uint8_t>(Stance::DEFENSE) && !BgLeader::IsMeaningful(ahead));
    BgLeader::Offer const late = StanceOffer(Stance::TURTLE, f);
    CHECK(late.Fallback() == static_cast<std::uint8_t>(Stance::TURTLE) && BgLeader::IsMeaningful(late));
}

TEST(sm_the_cart_list_and_the_switch_list_have_the_rule_first)
{
    auto facts = ThreeCarts();
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);
    BgLeader::Offer const carts = CartOffer(list);
    CHECK(carts.point == BgLeader::DecisionPoint::D18_SM_CART && carts.Fallback() == CART_NORTH && carts.options.size() == 3);
    BgLeader::Offer const sw = SwitchOffer(SwitchAct::WAIT_LAST);
    CHECK(sw.point == BgLeader::DecisionPoint::D19_SM_SWITCH && sw.Fallback() == static_cast<std::uint8_t>(SwitchAct::WAIT_LAST));
    CHECK(sw.options.size() == 3 && BgLeader::IsMeaningful(sw));
    CHECK(SwitchOffer(SwitchAct::LEAVE).Fallback() == static_cast<std::uint8_t>(SwitchAct::LEAVE));
    // Skipped carts are not offered.
    facts[CART_EAST].live = false;
    CHECK(CartOffer(ScoreCarts(facts, TEAM_IDX_ALLIANCE)).options.size() == 2);
}

TEST(sm_the_new_decision_points_are_wired_named_and_inside_the_book)
{
    using BgLeader::DecisionPoint;
    CHECK(AiPvp::IsWired(DecisionPoint::D18_SM_CART) && AiPvp::IsWired(DecisionPoint::D19_SM_SWITCH));
    CHECK(AiPvp::IsRepeatable(DecisionPoint::D18_SM_CART) && !AiPvp::IsRepeatable(DecisionPoint::D19_SM_SWITCH));
    CHECK(std::string(AiPvp::PointName(DecisionPoint::D18_SM_CART)) == "sm_cart");
    CHECK(std::string(AiPvp::PointName(DecisionPoint::D19_SM_SWITCH)) == "sm_switch");
    CHECK(static_cast<std::size_t>(DecisionPoint::D19_SM_SWITCH) < AiPvp::BOOK_SIZE);
    CHECK(AiPvp::PointPriority(DecisionPoint::D19_SM_SWITCH) > AiPvp::PointPriority(DecisionPoint::D18_SM_CART));
    CHECK(std::string(AiPvp::PointQuestion(DecisionPoint::D18_SM_CART)).find("cart") != std::string::npos);
    CHECK(std::string(BgLeader::CandidateName(DecisionPoint::D18_SM_CART, CART_EAST)) == "east_cart_lava");
    CHECK(std::string(BgLeader::CandidateName(DecisionPoint::D19_SM_SWITCH, 1)) == "wait_last");
}

TEST(sm_the_leash_returns_experts_at_once_good_bots_after_3_s_and_casual_bots_about_two_in_three)
{
    CHECK(!LeashReturns(Tier::EXPERT, 25.0f, 10000, 1, 1));
    CHECK(LeashReturns(Tier::EXPERT, 35.0f, 0, 1, 1));
    CHECK(!LeashReturns(Tier::GOOD, 35.0f, 1000, 1, 1));
    CHECK(LeashReturns(Tier::GOOD, 35.0f, 3000, 1, 1));
    unsigned returns = 0;
    for (std::uint32_t guid = 1; guid <= 300; ++guid)
        if (LeashReturns(Tier::CASUAL, 40.0f, 5000, guid, 7))
            ++returns;
    CHECK(returns > 150 && returns < 250);
    // The roll is fixed per bot and bucket.
    CHECK(LeashReturns(Tier::CASUAL, 40.0f, 5000, 77, 3) == LeashReturns(Tier::CASUAL, 40.0f, 5000, 77, 3));
}

TEST(sm_the_words_are_the_ones_the_players_use)
{
    CHECK(std::string(CartWord(CART_EAST)) == "lava" && std::string(CartWord(CART_WATERFALL)) == "mid" && std::string(CartWord(CART_NORTH)) == "top");
    CHECK(std::string(SwitchWord(TRACK_EAST)) == "east switch" && std::string(SwitchWord(TRACK_NORTH)) == "north switch");
    CHECK(std::string(DepotWord(DEPOT_DIAMOND)) == "long");
    CHECK(std::string(OptionKindName(OptionKind::FLIP)) == "flip");
}

TEST(sm_the_main_cart_changes_only_for_a_clearly_better_one)
{
    auto facts = ThreeCarts();
    std::vector<CartOption> const list = ScoreCarts(facts, TEAM_IDX_ALLIANCE);       // north 326 first, east 299, waterfall 274
    CHECK(HoldMain(list, CART_NONE) == CART_NORTH);
    CHECK(HoldMain(list, CART_EAST) == CART_EAST);              // 27 points better: not enough to move
    CHECK(HoldMain(list, CART_WATERFALL) == CART_NORTH);        // 52 points better: moves
    facts[CART_EAST].live = false;
    CHECK(HoldMain(ScoreCarts(facts, TEAM_IDX_ALLIANCE), CART_EAST) == CART_NORTH);   // the old cart is gone
    for (CartFacts& f : facts)
        f.live = false;
    CHECK(HoldMain(ScoreCarts(facts, TEAM_IDX_ALLIANCE), CART_EAST) == CART_NONE);
    std::vector<CartOption> const first = WithMainFirst(list, CART_WATERFALL);
    CHECK(first.front().cart == CART_WATERFALL && first.size() == 3 && first[1].cart == CART_NORTH);
}

TEST(sm_seen_enemies_and_the_circle_follow_the_core_distances)
{
    CHECK(SeenFrom(100.0f, 100.0f, 160.0f, 100.0f));
    CHECK(!SeenFrom(100.0f, 100.0f, 171.0f, 100.0f));
    Pt const cart{ 700.0f, 100.0f, 320.0f };
    CHECK(InCircle(cart, 715.0f, 100.0f, 320.0f));
    CHECK(!InCircle(cart, 723.0f, 100.0f, 320.0f));
    CHECK(!InCircle(cart, 700.0f, 100.0f, 343.0f));      // 3D: a player 23 yd above is out
    CHECK(InCircle(cart, 712.0f, 100.0f, 330.0f));
}

// --- The opening as built (sheet section 19) -------------------------------------------------------------------------------

namespace
{
std::array<CartFacts, CART_COUNT> OpeningFacts(std::uint8_t team)
{
    std::array<CartFacts, CART_COUNT> facts;
    for (std::uint8_t cart = 0; cart < CART_COUNT; ++cart)
    {
        CartFacts& f = facts[cart];
        f.cart = cart;
        f.live = true;
        f.remainingS = RideSeconds(cart, false);
        f.etaS = OpeningInterceptSeconds(team, cart, false);
        f.arrivalBonus = OpeningBonus(team, cart);
    }
    return facts;
}
}  // namespace

TEST(sm_the_two_teams_pick_different_main_carts_at_the_start_with_the_default_switches)
{
    // The Alliance reaches the north cart first, the Horde the waterfall cart; the east cart is the even one.
    auto const alliance = OpeningFacts(TEAM_IDX_ALLIANCE);
    auto const horde = OpeningFacts(TEAM_IDX_HORDE);
    std::vector<CartOption> const a = ScoreCarts(alliance, TEAM_IDX_ALLIANCE);
    std::vector<CartOption> const h = ScoreCarts(horde, TEAM_IDX_HORDE);
    CHECK(a.front().cart == CART_NORTH && a.front().kind == OptionKind::TAKE);
    CHECK(h.front().cart == CART_WATERFALL && h.front().kind == OptionKind::TAKE);
    // Every cart is reachable and worth a take at the start.
    for (CartOption const& o : a)
        CHECK(o.score > 150);
    for (CartOption const& o : h)
        CHECK(o.score > 150);
    // Ten players: 8 to the main cart and one on each off cart, whoever is dealt.
    Wants const wa = WantCounts(a, alliance, 10, Stance::BALANCED, true, CART_EAST);
    CHECK(wa.mainCart == CART_NORTH && wa.count[CART_NORTH] == 8 && wa.count[CART_EAST] == 1 && wa.count[CART_WATERFALL] == 1);
    Wants const wh = WantCounts(h, horde, 10, Stance::BALANCED, true, CART_EAST);
    CHECK(wh.mainCart == CART_WATERFALL && wh.count[CART_WATERFALL] == 8);
}

TEST(sm_a_cart_the_enemy_keeps_becomes_a_target_once_the_bar_can_be_turned)
{
    // The Horde keeps the north cart (bar 80 on the Horde's side means 20), our side has 8 bodies about to arrive: a flip, worth more than a take.
    auto facts = OpeningFacts(TEAM_IDX_ALLIANCE);
    CartFacts& north = facts[CART_NORTH];
    north.keeper = TEAM_IDX_HORDE;
    north.bar = 20;
    north.friends = 7;
    north.enemies = 1;
    north.remainingS = 140.0f;
    north.etaS = 15.0f;
    CartOption const flip = ScoreCart(north, TEAM_IDX_ALLIANCE);
    CHECK(flip.kind == OptionKind::FLIP && flip.score > CartWorth(140.0f));
    // The same cart held by a bar at 0 and four of them against our two cannot be turned in time.
    north.bar = 0;
    north.friends = 1;
    north.enemies = 4;
    CHECK(ScoreCart(north, TEAM_IDX_ALLIANCE).kind == OptionKind::SKIP);
}
