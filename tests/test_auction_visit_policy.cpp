/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "AuctionVisitPolicy.h"
#include "TestHarness.h"

using namespace AuctionVisitPolicy;

namespace
{
Gate ReadyGate()
{
    Gate gate;
    gate.enabled = true;
    gate.randomBot = true;
    gate.listed = true;
    gate.tableLoaded = true;
    return gate;
}
}  // namespace

TEST(auction_visit_step_delay_stays_between_two_and_five_seconds)
{
    for (uint32_t roll = 0; roll < 20000; roll += 7)
    {
        CHECK(StepDelayMs(roll) >= STEP_MIN_MS);
        CHECK(StepDelayMs(roll) <= STEP_MAX_MS);
        CHECK(FirstStepDelayMs(roll) >= FIRST_STEP_MIN_MS);
        CHECK(FirstStepDelayMs(roll) <= FIRST_STEP_MAX_MS);
    }
    CHECK(StepDelayMs(0) == STEP_MIN_MS);
    CHECK(StepDelayMs(UINT32_MAX) <= STEP_MAX_MS);
}

TEST(auction_visit_name_list_is_split_lowered_and_deduplicated)
{
    std::vector<std::string> const names = ParseNameList("  Colata, zanderio;SIMES  colata ,, ");
    CHECK(names.size() == 3);
    CHECK(names[0] == "colata" && names[1] == "zanderio" && names[2] == "simes");
    CHECK(ParseNameList("").empty());
    CHECK(ParseNameList(" , ;; ").empty());
}

TEST(auction_visit_an_empty_name_list_allows_everyone_else_only_the_named)
{
    CHECK(NameAllowed({}, "Anyone"));
    std::vector<std::string> const names = ParseNameList("Colata,Zanderio");
    CHECK(NameAllowed(names, "Colata"));
    CHECK(NameAllowed(names, "zanderio"));
    CHECK(!NameAllowed(names, "Colat"));
    CHECK(!NameAllowed(names, "Simes"));
    CHECK(!NameAllowed(names, ""));
}

TEST(auction_visit_gate_opens_only_for_an_enabled_listed_random_bot_with_a_table)
{
    CHECK(Check(ReadyGate()) == Gating::Ok);

    Gate gate = ReadyGate();
    gate.enabled = false;
    CHECK(Check(gate) == Gating::Disabled);

    gate = ReadyGate();
    gate.randomBot = false;
    CHECK(Check(gate) == Gating::NotRandomBot);

    gate = ReadyGate();
    gate.listed = false;
    CHECK(Check(gate) == Gating::NotListed);

    gate = ReadyGate();
    gate.humanLed = true;
    CHECK(Check(gate) == Gating::HumanLed);

    gate = ReadyGate();
    gate.tableLoaded = false;
    CHECK(Check(gate) == Gating::NoPriceTable);

    gate = ReadyGate();
    gate.busy = true;
    CHECK(Check(gate) == Gating::Busy);
}

TEST(auction_visit_gate_reports_the_first_closed_door)
{
    Gate gate;   // everything off
    CHECK(Check(gate) == Gating::Disabled);
    gate.enabled = true;
    CHECK(Check(gate) == Gating::NotRandomBot);
    gate.randomBot = true;
    gate.listed = false;
    CHECK(Check(gate) == Gating::NotListed);
}

TEST(auction_visit_cooldown_counts_from_the_last_visit)
{
    Gate gate = ReadyGate();
    gate.visitedBefore = true;
    gate.cooldownMinutes = 60;
    gate.msSinceLastVisit = 59ull * 60 * 1000;
    CHECK(Check(gate) == Gating::Cooldown);
    gate.msSinceLastVisit = 60ull * 60 * 1000;
    CHECK(Check(gate) == Gating::Ok);

    gate.visitedBefore = false;
    gate.msSinceLastVisit = 0;
    CHECK(Check(gate) == Gating::Ok);

    gate.visitedBefore = true;
    gate.cooldownMinutes = 0;
    CHECK(Check(gate) == Gating::Ok);
}

TEST(auction_visit_phases_run_mail_snapshot_sell_buy_done)
{
    Phase phase = Phase::Mail;
    std::vector<Phase> order = { phase };
    while (phase != Phase::Done)
    {
        phase = NextPhase(phase, Kind::Auction);
        order.push_back(phase);
    }
    CHECK(order.size() == 5);
    CHECK(order[1] == Phase::Snapshot && order[2] == Phase::Sell && order[3] == Phase::Buy && order[4] == Phase::Done);
    CHECK(NextPhase(Phase::Done, Kind::Auction) == Phase::Done);
}

TEST(auction_visit_a_mailbox_visit_ends_after_the_mail)
{
    CHECK(NextPhase(Phase::Mail, Kind::MailboxOnly) == Phase::Done);
}

TEST(auction_visit_ends_on_timeout_or_three_failures)
{
    CHECK(!TimedOut(VISIT_TIMEOUT_MS - 1));
    CHECK(TimedOut(VISIT_TIMEOUT_MS));
    CHECK(!TooManyFailures(2));
    CHECK(TooManyFailures(3));
    // The errand dwell at an auctioneer is 30 to 90 seconds; the visit never outlasts it by much.
    CHECK(VISIT_TIMEOUT_MS <= 90 * 1000);
}

TEST(auction_visit_summary_adds_visits_and_formats_gold)
{
    Totals totals;
    VisitResult sold;
    sold.listings = 3;
    sold.listedCopper = 123456;
    sold.depositsCopper = 305;
    sold.purchases = 1;
    sold.purchasedCopper = 20000;
    sold.mailMoneyCopper = 500;
    sold.mailMoneyMails = 1;
    sold.mailItems = 2;
    Add(totals, sold);

    VisitResult mailbox;
    mailbox.kind = Kind::MailboxOnly;
    mailbox.reason = EndReason::Left;
    mailbox.mailBlocked = 1;
    Add(totals, mailbox);

    CHECK(totals.visits == 1 && totals.mailboxVisits == 1 && totals.cutShort == 1);
    CHECK(totals.listings == 3 && totals.listedCopper == 123456 && totals.depositsCopper == 305);
    CHECK(totals.purchases == 1 && totals.purchasedCopper == 20000);
    CHECK(totals.mailMoneyCopper == 500 && totals.mailItems == 2 && totals.mailBlocked == 1);

    std::string const text = FormatTotals(totals);
    CHECK(text.find("listed=3") != std::string::npos);
    CHECK(text.find("listedValue=12g34s56c") != std::string::npos);
    CHECK(text.find("deposits=0g3s5c") != std::string::npos);
    CHECK(text.find("paid=2g0s0c") != std::string::npos);
}

TEST(auction_visit_summary_is_due_after_the_interval_and_never_when_off)
{
    CHECK(!SummaryDue(29ull * 60 * 1000, 30));
    CHECK(SummaryDue(30ull * 60 * 1000, 30));
    CHECK(!SummaryDue(UINT32_MAX, 0));
}

TEST(auction_visit_names_are_never_empty)
{
    for (Gating g : { Gating::Ok, Gating::Disabled, Gating::NotRandomBot, Gating::NotListed, Gating::HumanLed, Gating::NoPriceTable,
                      Gating::Busy, Gating::Cooldown })
        CHECK(std::string(Name(g)).size() > 0);
    for (Phase p : { Phase::Mail, Phase::Snapshot, Phase::Sell, Phase::Buy, Phase::Done })
        CHECK(std::string(Name(p)).size() > 0);
    for (EndReason r : { EndReason::Finished, EndReason::Left, EndReason::TimedOut, EndReason::Failures, EndReason::Cancelled,
                         EndReason::WrongHouse, EndReason::Disabled })
        CHECK(std::string(Name(r)).size() > 0);
}

TEST(auction_visit_copper_is_written_as_gold_silver_copper)
{
    CHECK(FormatCopper(0) == "0g0s0c");
    CHECK(FormatCopper(5) == "0g0s5c");
    CHECK(FormatCopper(12345678) == "1234g56s78c");
    CHECK(FormatCopper(9999999999ull) == "999999g99s99c");
}

TEST(auction_visit_summary_counts_sales_expiries_and_human_buyers)
{
    Totals totals;
    AddSale(totals, false, 1000);
    AddSale(totals, true, 25000);
    AddExpiry(totals);
    CHECK(totals.sales == 2 && totals.salesToHumans == 1 && totals.salesCopper == 26000 && totals.expired == 1);
    std::string const text = FormatTotals(totals);
    CHECK(text.find("sales=2") != std::string::npos);
    CHECK(text.find("salesToHumans=1") != std::string::npos);
    CHECK(text.find("salesValue=2g60s0c") != std::string::npos);
    CHECK(text.find("expired=1") != std::string::npos);
}

TEST(auction_visit_the_soak_wants_a_visit_only_with_a_name_list_and_an_open_gate)
{
    CHECK(SoakWantsVisit(true, Gating::Ok));
    CHECK(!SoakWantsVisit(false, Gating::Ok));
    CHECK(!SoakWantsVisit(true, Gating::Cooldown));
    CHECK(!SoakWantsVisit(true, Gating::Busy));
    CHECK(!SoakWantsVisit(true, Gating::HumanLed));
    CHECK(!SoakWantsVisit(true, Gating::NotListed));
}

TEST(auction_interval_samples_and_snapshot_boundaries)
{
    IntervalStats stats;
    CHECK(stats.tickUs.Average() == 0.0 && stats.tickUs.max == 0);
    stats.tickUs.Add(500);
    stats.tickUs.Add(2500);
    CHECK(stats.tickUs.count == 2 && stats.tickUs.Average() == 1500.0 && stats.tickUs.max == 2500);
    stats.Snapshot(4999, 5000, 0);
    stats.Snapshot(5000, 5000, 0);
    stats.Snapshot(5000, 5000, 12);
    stats.Snapshot(5000, 5000, 3);
    CHECK(stats.snapshotsAtCap == 3 && stats.snapshotsTruncated == 2 && stats.listingsOmitted == 15);
    stats = IntervalStats();
    CHECK(stats.tickUs.count == 0 && stats.snapshotsAtCap == 0 && stats.listingsOmitted == 0);
}

TEST(auction_reconciliation_distinguishes_sale_proceeds_and_timeouts)
{
    Totals totals;
    AddSale(totals, true, 10000, 500, 1000);
    VisitResult visit;
    visit.reason = EndReason::TimedOut;
    visit.depositsCopper = 500;
    visit.mailMoneyCopper = 12000;
    visit.saleGoldCollectedCopper = 9500;
    Add(totals, visit);
    CHECK(totals.timeouts == 1 && totals.depositsCopper == 500 && totals.depositsReturnedCopper == 500);
    CHECK(totals.saleGoldMailedCopper == 9500 && totals.saleGoldCollectedCopper == 9500);
    CHECK(totals.mailMoneyCopper == 12000);
    std::string const text = FormatTotals(totals);
    CHECK(text.find("timeouts=1") != std::string::npos);
    CHECK(text.find("saleGoldMailed=0g95s0c") != std::string::npos);
    CHECK(text.find("saleGoldCollected=0g95s0c") != std::string::npos);
}

TEST(auction_visit_mail_backlog_is_the_interval_maximum)
{
    IntervalStats stats;
    CHECK(stats.mailBacklogMax == 0);
    stats.MailBacklog(12);
    stats.MailBacklog(3);
    CHECK(stats.mailBacklogMax == 12);
    stats.MailBacklog(17);
    CHECK(stats.mailBacklogMax == 17);
    stats = IntervalStats();
    CHECK(stats.mailBacklogMax == 0);
}
