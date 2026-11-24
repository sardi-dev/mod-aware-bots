#include "TestHarness.h"
#include "LfgFillPolicy.h"

using LfgFillPolicy::QueueState;
using LfgFillPolicy::Track;

TEST(lfg_fill_judges_a_queued_bot)
{
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::Queued) == Track::Judge);
}

TEST(lfg_fill_keeps_a_bot_through_a_proposal)
{
    // BT-205: the proposal may fail and put the bot back in the queue; it must still be released later.
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::Proposal) == Track::Wait);
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::RoleCheck) == Track::Wait);
}

TEST(lfg_fill_forgets_a_bot_out_of_the_queue)
{
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::None) == Track::Forget);
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::Dungeon) == Track::Forget);
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::FinishedDungeon) == Track::Forget);
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::Boot) == Track::Forget);
    CHECK(LfgFillPolicy::TrackQueuedBot(QueueState::RaidBrowser) == Track::Forget);
}

TEST(lfg_fill_names_a_bot_back_from_a_failed_proposal_once)
{
    CHECK(LfgFillPolicy::BackFromFailedProposal(true, Track::Judge));
    CHECK(!LfgFillPolicy::BackFromFailedProposal(false, Track::Judge));
    CHECK(!LfgFillPolicy::BackFromFailedProposal(true, Track::Wait));
    CHECK(!LfgFillPolicy::BackFromFailedProposal(true, Track::Forget));
}

TEST(lfg_fill_census_counts_holds_and_free_roles)
{
    LfgFillPolicy::Census census;
    census.Add(LfgFillPolicy::Hold::Free, 0);
    census.Add(LfgFillPolicy::Hold::Free, 2);
    census.Add(LfgFillPolicy::Hold::Free, 2);
    census.Add(LfgFillPolicy::Hold::Grouped, 1);
    census.Add(LfgFillPolicy::Hold::Queued, 0);
    census.Add(LfgFillPolicy::Hold::Free, -1);
    CHECK(census.Total() == 6);
    CHECK(census.held[uint32_t(LfgFillPolicy::Hold::Free)] == 4);
    CHECK(census.held[uint32_t(LfgFillPolicy::Hold::Grouped)] == 1);
    CHECK(census.freeByRole[0] == 1);
    CHECK(census.freeByRole[1] == 0);
    CHECK(census.freeByRole[2] == 2);
}

TEST(lfg_fill_census_reach_is_the_level_window_from_the_finder_minimum)
{
    CHECK(LfgFillPolicy::InReach(25, 25, 10, 15));
    CHECK(LfgFillPolicy::InReach(15, 25, 10, 15));
    CHECK(LfgFillPolicy::InReach(35, 25, 10, 15));
    CHECK(!LfgFillPolicy::InReach(14, 18, 10, 15));
    CHECK(!LfgFillPolicy::InReach(36, 25, 10, 15));
    CHECK(!LfgFillPolicy::InReach(14, 25, 10, 15));
}

TEST(lfg_fill_party_role_shortage_needs_missing_tank_or_healer_and_no_free_bot)
{
    CHECK(LfgFillPolicy::RoleShortage(0, 1, 0));
    CHECK(LfgFillPolicy::RoleShortage(1, 1, 0));
    CHECK(!LfgFillPolicy::RoleShortage(0, 0, 0));
    CHECK(!LfgFillPolicy::RoleShortage(1, 1, 1));
    CHECK(!LfgFillPolicy::RoleShortage(2, 1, 0));
    CHECK(!LfgFillPolicy::RoleShortage(-1, 1, 0));
}

TEST(lfg_fill_party_bot_must_be_in_a_safe_bot_only_non_finder_party)
{
    CHECK(LfgFillPolicy::PartyEligible(true, true, false, true));
    CHECK(!LfgFillPolicy::PartyEligible(false, true, false, true));
    CHECK(!LfgFillPolicy::PartyEligible(true, false, false, true));
    CHECK(!LfgFillPolicy::PartyEligible(true, true, true, true));
    CHECK(!LfgFillPolicy::PartyEligible(true, true, false, false));
}

TEST(lfg_bot_run_must_leave_a_full_free_group)
{
    CHECK(LfgFillPolicy::KeepsFreeGroup(1, 1, 3));
    CHECK(LfgFillPolicy::KeepsFreeGroup(2, 3, 4));
    CHECK(!LfgFillPolicy::KeepsFreeGroup(0, 1, 3));
    CHECK(!LfgFillPolicy::KeepsFreeGroup(1, 0, 3));
    CHECK(!LfgFillPolicy::KeepsFreeGroup(1, 1, 2));
}

TEST(lfg_proposal_is_accepted_in_a_fight_and_declined_only_when_dead)
{
    CHECK(LfgFillPolicy::AcceptsProposal(false));
    CHECK(!LfgFillPolicy::AcceptsProposal(true));
}

TEST(lfg_fill_tank_floor_is_the_player_level_minus_the_margin)
{
    // Stockade proof run 2026-11-23: a level 21 tank for a level 24 player.
    CHECK(LfgFillPolicy::TankLevelFloor(24, 1, 20) == 23);
    CHECK(!LfgFillPolicy::TankLevelOk(21, 24, 1, 20));
    CHECK(!LfgFillPolicy::TankLevelOk(22, 24, 1, 20));
    CHECK(LfgFillPolicy::TankLevelOk(23, 24, 1, 20));
    CHECK(LfgFillPolicy::TankLevelOk(24, 24, 1, 20));
    CHECK(LfgFillPolicy::TankLevelOk(27, 24, 1, 20));
}

TEST(lfg_fill_tank_floor_never_goes_under_the_dungeon_minimum)
{
    CHECK(LfgFillPolicy::TankLevelFloor(22, 1, 22) == 22);
    CHECK(!LfgFillPolicy::TankLevelOk(21, 22, 1, 22));
    CHECK(LfgFillPolicy::TankLevelFloor(20, 5, 22) == 22);
}

TEST(lfg_fill_tank_floor_margin_is_configurable_and_safe_at_the_edges)
{
    CHECK(LfgFillPolicy::TankLevelFloor(24, 0, 20) == 24);
    CHECK(LfgFillPolicy::TankLevelFloor(24, 6, 20) == 20);
    CHECK(LfgFillPolicy::TankLevelFloor(3, 10, 0) == 0);
    CHECK(LfgFillPolicy::TankLevelOk(1, 3, 10, 0));
}
