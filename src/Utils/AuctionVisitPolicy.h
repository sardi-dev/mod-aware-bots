/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_AUCTIONVISITPOLICY_H
#define MOD_AWARE_BOTS_AUCTIONVISITPOLICY_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

// Pure rules of one visit of a random bot to the auction house (design decision 0033, docs/economy/auction-wiring.md sections 2
// and 4): who may go, in which order the visit works, how long it waits between two operations, when it ends, and the
// counters the soak is read from. The glue (strategy/actions/BotAuction.cpp) does every operation on the world thread,
// one per bot per slice, and asks this header what comes next. No game core is used here.
namespace AuctionVisitPolicy
{
// A person at the counter does not list four stacks in one tick: 2 to 5 seconds pass between two operations.
constexpr uint32_t STEP_MIN_MS = 2000;
constexpr uint32_t STEP_MAX_MS = 5000;
// The first operation waits a little after the bot arrives, so it has stopped and faced the counter.
constexpr uint32_t FIRST_STEP_MIN_MS = 1000;
constexpr uint32_t FIRST_STEP_MAX_MS = 3000;

// The visit is dropped at the latest after this long. The errand dwell at an auctioneer is 30 to 90 seconds (ErrandPolicy)
// and the errand waits for a working visit, so this is the longest a bot stands there for the auction.
constexpr uint32_t VISIT_TIMEOUT_MS = 75 * 1000;

// Three failed operations end the visit (a stack the core refuses is not tried again, but a run of refusals is a sign
// that something changed under the bot).
constexpr uint32_t MAX_FAILURES = 3;

// At most this many mails are handled per collection (one write each).
constexpr uint32_t MAIL_PER_COLLECTION = 10;

// A house with more listings than this is read up to the limit only, and the visit says so.
constexpr uint32_t SNAPSHOT_LIMIT = 5000;

constexpr bool StartsAuctionCooldown(bool auctionVisit) { return auctionVisit; }
constexpr uint32_t SnapshotOmittedLowerBound(uint32_t fetched) { return fetched > SNAPSHOT_LIMIT ? 1 : 0; }

// A mailbox within this many yards of the bot lets it collect (a player must stand at the box).
constexpr float MAILBOX_YARDS = 25.0f;

// Defaults of the keys AiPlayerbot.Auction.MaxListingsPerVisit, MaxActiveListings, MaxPurchasesPerVisit,
// VisitCooldownMinutes and SlicesPerTick.
constexpr uint32_t DEFAULT_MAX_LISTINGS_PER_VISIT = 4;
constexpr uint32_t DEFAULT_MAX_ACTIVE_LISTINGS = 20;
constexpr uint32_t DEFAULT_MAX_PURCHASES_PER_VISIT = 3;
constexpr uint32_t DEFAULT_COOLDOWN_MINUTES = 60;
constexpr uint32_t DEFAULT_SLICES_PER_TICK = 3;

// roll: any number; the answer is in [STEP_MIN_MS, STEP_MAX_MS].
constexpr uint32_t StepDelayMs(uint32_t roll)
{
    return STEP_MIN_MS + roll % (STEP_MAX_MS - STEP_MIN_MS + 1);
}

constexpr uint32_t FirstStepDelayMs(uint32_t roll)
{
    return FIRST_STEP_MIN_MS + roll % (FIRST_STEP_MAX_MS - FIRST_STEP_MIN_MS + 1);
}

// ---------------------------------------------------------------------------------------------------------------
// Who goes. AiPlayerbot.Auction.OnlyBots is a list of character names; empty means every random bot (the soak sets it).
// ---------------------------------------------------------------------------------------------------------------
namespace detail
{
inline std::string Lower(std::string text)
{
    for (char& c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
}  // namespace detail

// Splits "A, B;C" at commas, semicolons and blanks into lower case names, without empty entries and without duplicates.
inline std::vector<std::string> ParseNameList(std::string const& text)
{
    std::vector<std::string> names;
    std::string current;
    auto flush = [&]()
    {
        if (!current.empty() && std::find(names.begin(), names.end(), current) == names.end())
            names.push_back(current);
        current.clear();
    };
    for (char const c : text)
    {
        if (c == ',' || c == ';' || std::isspace(static_cast<unsigned char>(c)))
            flush();
        else
            current.push_back(char(std::tolower(static_cast<unsigned char>(c))));
    }
    flush();
    return names;
}

// An empty list allows every bot; otherwise the name must be in it (names are compared without regard to case).
inline bool NameAllowed(std::vector<std::string> const& list, std::string const& botName)
{
    if (list.empty())
        return true;

    return std::find(list.begin(), list.end(), detail::Lower(botName)) != list.end();
}

// What a bot has to be and have before its errand at the auctioneer starts a visit. The glue fills it on arrival.
struct Gate
{
    bool enabled = false;          // AiPlayerbot.Auction.Enable
    bool randomBot = false;
    bool listed = true;            // NameAllowed(OnlyBots, name)
    bool humanLed = false;         // follows a real player, or is in a dungeon, a battleground or an arena
    bool tableLoaded = false;      // the price table has rows
    bool busy = false;             // a visit of this bot is queued or working
    bool visitedBefore = false;
    uint64_t msSinceLastVisit = 0;
    uint32_t cooldownMinutes = DEFAULT_COOLDOWN_MINUTES;
};

enum class Gating : uint8_t
{
    Ok,
    Disabled,
    NotRandomBot,
    NotListed,
    HumanLed,
    NoPriceTable,
    Busy,
    Cooldown
};

constexpr bool CooldownPassed(bool visitedBefore, uint64_t msSinceLastVisit, uint32_t cooldownMinutes)
{
    return !visitedBefore || msSinceLastVisit >= uint64_t(cooldownMinutes) * 60 * 1000;
}

constexpr Gating Check(Gate const& gate)
{
    if (!gate.enabled)
        return Gating::Disabled;
    if (!gate.randomBot)
        return Gating::NotRandomBot;
    if (!gate.listed)
        return Gating::NotListed;
    if (gate.humanLed)
        return Gating::HumanLed;
    if (!gate.tableLoaded)
        return Gating::NoPriceTable;
    if (gate.busy)
        return Gating::Busy;
    if (!CooldownPassed(gate.visitedBefore, gate.msSinceLastVisit, gate.cooldownMinutes))
        return Gating::Cooldown;
    return Gating::Ok;
}

// The soak (AiPlayerbot.Auction.OnlyBots names bots): a named bot that could start a visit now should use the next auction house
// it stands at. Without a list every bot keeps the ordinary errand odds.
constexpr bool SoakWantsVisit(bool onlyBotsSet, Gating gating)
{
    return onlyBotsSet && gating == Gating::Ok;
}

constexpr char const* Name(Gating gating)
{
    switch (gating)
    {
        case Gating::Ok: return "ok";
        case Gating::Disabled: return "disabled";
        case Gating::NotRandomBot: return "not a random bot";
        case Gating::NotListed: return "not in OnlyBots";
        case Gating::HumanLed: return "follows a player or is in an instance";
        case Gating::NoPriceTable: return "no price table";
        case Gating::Busy: return "visit under way";
        case Gating::Cooldown: return "visited lately";
    }
    return "";
}

// ---------------------------------------------------------------------------------------------------------------
// The order of a visit: mail, a snapshot of the house, selling, buying, done. A visit made only for the mailbox collects
// the mail and ends.
// ---------------------------------------------------------------------------------------------------------------
enum class Kind : uint8_t
{
    Auction,
    MailboxOnly
};

enum class Phase : uint8_t
{
    Mail,
    Snapshot,
    Sell,
    Buy,
    Done
};

constexpr Phase NextPhase(Phase phase, Kind kind)
{
    switch (phase)
    {
        case Phase::Mail: return kind == Kind::Auction ? Phase::Snapshot : Phase::Done;
        case Phase::Snapshot: return Phase::Sell;
        case Phase::Sell: return Phase::Buy;
        case Phase::Buy: return Phase::Done;
        case Phase::Done: break;
    }
    return Phase::Done;
}

constexpr char const* Name(Phase phase)
{
    switch (phase)
    {
        case Phase::Mail: return "mail";
        case Phase::Snapshot: return "snapshot";
        case Phase::Sell: return "sell";
        case Phase::Buy: return "buy";
        case Phase::Done: return "done";
    }
    return "";
}

// Why a visit ended; it goes into the visit's log line.
enum class EndReason : uint8_t
{
    Finished,
    Left,          // moved away, died, logged out, changed map, lost the auctioneer
    TimedOut,
    Failures,
    Cancelled,     // the errand ended
    WrongHouse,
    Disabled
};

constexpr char const* Name(EndReason reason)
{
    switch (reason)
    {
        case EndReason::Finished: return "finished";
        case EndReason::Left: return "left";
        case EndReason::TimedOut: return "timed out";
        case EndReason::Failures: return "too many failures";
        case EndReason::Cancelled: return "cancelled";
        case EndReason::WrongHouse: return "not the faction's house";
        case EndReason::Disabled: return "disabled";
    }
    return "";
}

constexpr bool TimedOut(uint32_t elapsedMs) { return elapsedMs >= VISIT_TIMEOUT_MS; }
constexpr bool TooManyFailures(uint32_t failures) { return failures >= MAX_FAILURES; }

// ---------------------------------------------------------------------------------------------------------------
// The summary the soak is read from: one counter set for the whole run, printed by the glue at an interval.
// ---------------------------------------------------------------------------------------------------------------
struct Totals
{
    uint64_t visits = 0;
    uint64_t mailboxVisits = 0;
    uint64_t timeouts = 0;
    uint64_t depositsReturnedCopper = 0;
    uint64_t saleGoldMailedCopper = 0;
    uint64_t saleGoldCollectedCopper = 0;
    uint64_t cutShort = 0;          // visits that ended any way but Finished
    uint64_t listings = 0;
    uint64_t listedCopper = 0;      // the buyout value of what was listed
    uint64_t depositsCopper = 0;
    uint64_t purchases = 0;
    uint64_t purchasedCopper = 0;
    uint64_t mailMoneyCopper = 0;   // gold taken from auction mail (sold items)
    uint64_t mailMoneyMails = 0;
    uint64_t mailItems = 0;         // items taken back from auction mail (expired, bought)
    uint64_t mailBlocked = 0;       // takes refused (bags full, too much gold)
    uint64_t refusals = 0;          // operations the core or the re-check refused
    uint64_t errors = 0;
    // From the core's auction script, whoever listed or bought (any code path), for the listings of random bots.
    uint64_t sales = 0;             // a listing of a random bot was bought
    uint64_t salesToHumans = 0;     // ... by a human
    uint64_t salesCopper = 0;       // what the buyers paid
    uint64_t expired = 0;           // a listing of a random bot ran out unsold (its deposit is lost)
};

struct VisitResult
{
    Kind kind = Kind::Auction;
    EndReason reason = EndReason::Finished;
    uint32_t listings = 0;
    uint64_t listedCopper = 0;
    uint64_t depositsCopper = 0;
    uint32_t purchases = 0;
    uint64_t purchasedCopper = 0;
    uint64_t mailMoneyCopper = 0;
    uint64_t saleGoldCollectedCopper = 0;
    uint32_t mailMoneyMails = 0;
    uint32_t mailItems = 0;
    uint32_t mailBlocked = 0;
    uint32_t refusals = 0;
    uint32_t errors = 0;
};

// One sale or expiry seen by the core's auction script; the seller is a random bot.
inline void AddSale(Totals& totals, bool boughtByHuman, uint64_t paidCopper, uint64_t deposit = 0, uint64_t cut = 0)
{
    ++totals.sales;
    if (boughtByHuman)
        ++totals.salesToHumans;
    totals.salesCopper += paidCopper;
    totals.depositsReturnedCopper += deposit;
    totals.saleGoldMailedCopper += paidCopper + deposit - std::min(paidCopper + deposit, cut);
}

inline void AddExpiry(Totals& totals) { ++totals.expired; }

inline void Add(Totals& totals, VisitResult const& visit)
{
    if (visit.kind == Kind::MailboxOnly)
        ++totals.mailboxVisits;
    else
        ++totals.visits;
    if (visit.reason == EndReason::TimedOut)
        ++totals.timeouts;
    if (visit.reason != EndReason::Finished)
        ++totals.cutShort;
    totals.listings += visit.listings;
    totals.listedCopper += visit.listedCopper;
    totals.depositsCopper += visit.depositsCopper;
    totals.purchases += visit.purchases;
    totals.purchasedCopper += visit.purchasedCopper;
    totals.mailMoneyCopper += visit.mailMoneyCopper;
    totals.saleGoldCollectedCopper += visit.saleGoldCollectedCopper;
    totals.mailMoneyMails += visit.mailMoneyMails;
    totals.mailItems += visit.mailItems;
    totals.mailBlocked += visit.mailBlocked;
    totals.refusals += visit.refusals;
    totals.errors += visit.errors;
}

// Samples are measured in microseconds to retain sub-millisecond work.
struct Samples
{
    uint64_t count = 0;
    uint64_t sum = 0;
    uint64_t max = 0;

    void Add(uint64_t value) { ++count; sum += value; max = std::max(max, value); }
    double Average() const { return count ? double(sum) / count : 0.0; }
};

struct IntervalStats
{
    Samples tickUs;
    Samples queueDepth;
    uint64_t snapshotsAtCap = 0;
    uint64_t snapshotsTruncated = 0;
    uint64_t listingsOmitted = 0;
    uint64_t mailBacklogMax = 0;

    void MailBacklog(uint64_t count) { mailBacklogMax = std::max(mailBacklogMax, count); }

    void Snapshot(uint64_t size, uint64_t limit, uint64_t omitted)
    {
        if (size >= limit)
            ++snapshotsAtCap;
        if (omitted)
        {
            ++snapshotsTruncated;
            listingsOmitted += omitted;
        }
    }
};

// The summary is due when the interval has passed since the last one and something happened meanwhile.
constexpr bool SummaryDue(uint64_t msSinceLast, uint32_t intervalMinutes)
{
    return intervalMinutes && msSinceLast >= uint64_t(intervalMinutes) * 60 * 1000;
}

// 12345678 copper is "1234g56s78c".
inline std::string FormatCopper(uint64_t copper)
{
    return std::to_string(copper / 10000) + "g" + std::to_string(copper / 100 % 100) + "s" + std::to_string(copper % 100) + "c";
}

inline std::string FormatTotals(Totals const& t)
{
    auto const gold = [](uint64_t copper) { return FormatCopper(copper); };
    return "visits=" + std::to_string(t.visits) + " mailbox=" + std::to_string(t.mailboxVisits) + " cutShort=" +
           std::to_string(t.cutShort) + " timeouts=" + std::to_string(t.timeouts) + " listed=" + std::to_string(t.listings) + " listedValue=" + gold(t.listedCopper) +
           " deposits=" + gold(t.depositsCopper) + " depositsReturned=" + gold(t.depositsReturnedCopper) + " bought=" + std::to_string(t.purchases) + " paid=" + gold(t.purchasedCopper) +
           " saleGoldMailed=" + gold(t.saleGoldMailedCopper) + " saleGoldCollected=" + gold(t.saleGoldCollectedCopper) + " mailGold=" + gold(t.mailMoneyCopper) + " mailSales=" + std::to_string(t.mailMoneyMails) + " mailItems=" +
           std::to_string(t.mailItems) + " mailBlocked=" + std::to_string(t.mailBlocked) + " refused=" + std::to_string(t.refusals) +
           " errors=" + std::to_string(t.errors) + " sales=" + std::to_string(t.sales) + " salesToHumans=" +
           std::to_string(t.salesToHumans) + " salesValue=" + gold(t.salesCopper) + " expired=" + std::to_string(t.expired);
}
}  // namespace AuctionVisitPolicy

#endif
