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
