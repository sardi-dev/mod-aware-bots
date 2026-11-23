/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#ifndef MOD_AWARE_BOTS_AUCTIONMAILPOLICY_H
#define MOD_AWARE_BOTS_AUCTIONMAILPOLICY_H

#include <cstdint>
#include <string>

// Pure rules of the auction mail a bot collects at a mailbox (docs/economy/auction-wiring.md section 5). The core mails
// the proceeds of a sale, an expired item, a won item and the money of an outbid or cancelled bid, with a subject of the
// form "itemEntry:0:answer:auctionId:count" (AuctionEntry::BuildAuctionMailSubject). Nothing else is ever touched: mail
// from players and creatures stays with the existing mail code, and nothing is deleted unless it is empty.
namespace AuctionMailPolicy
{
// MailAuctionAnswer of the core.
enum Answer : uint32_t
{
    ANSWER_OUTBID = 0,
    ANSWER_WON = 1,
    ANSWER_SUCCESSFUL = 2,
    ANSWER_EXPIRED = 3,
    ANSWER_CANCELLED_TO_BIDDER = 4,
    ANSWER_CANCELLED = 5,
    ANSWER_SALE_PENDING = 6,
    ANSWER_LAST = ANSWER_SALE_PENDING
};

// MAIL_AUCTION of the core's MailMessageType.
constexpr uint8_t MESSAGE_TYPE_AUCTION = 2;

struct Subject
{
    uint32_t itemEntry = 0;
    uint32_t answer = 0;
    uint32_t auctionId = 0;
    uint32_t count = 0;
};

namespace detail
{
// An unsigned decimal number up to `end` (a ':' or the end of the text); false for an empty field, a sign, a letter or a
// number that does not fit.
inline bool ReadField(std::string const& text, size_t& pos, uint32_t& out)
{
    if (pos >= text.size() || text[pos] < '0' || text[pos] > '9')
        return false;

    uint64_t value = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9')
    {
        value = value * 10 + uint64_t(text[pos] - '0');
        if (value > UINT32_MAX)
            return false;
        ++pos;
    }
    out = uint32_t(value);
    return true;
}
}  // namespace detail

// "itemEntry:0:answer:auctionId:count"; the second field is always 0 in the core and must be 0 here.
inline bool ParseSubject(std::string const& text, Subject& out)
{
    size_t pos = 0;
    Subject subject;
    uint32_t zero = 1;
    uint32_t* const fields[] = { &subject.itemEntry, &zero, &subject.answer, &subject.auctionId, &subject.count };
    for (size_t i = 0; i < 5; ++i)
    {
        if (!detail::ReadField(text, pos, *fields[i]))
            return false;
        if (i < 4)
        {
            if (pos >= text.size() || text[pos] != ':')
                return false;
            ++pos;
        }
    }
    if (pos != text.size() || zero != 0 || subject.answer > ANSWER_LAST)
        return false;

    out = subject;
    return true;
}

// What the mail holds, as the glue reads it from the core's Mail.
struct MailFacts
{
    uint8_t messageType = 0;
    bool delivered = false;
    std::string subject;
    uint64_t money = 0;
    uint32_t itemCount = 0;      // attachments left
    uint64_t cod = 0;
};

enum class Step : uint8_t
{
    Ignore,         // not ours, not delivered, or something a bot must not take (cash on delivery)
    TakeMoney,
    TakeItem,
    DeleteEmpty     // a delivered auction mail with nothing left (a sale notice, or what was taken)
};

constexpr bool IsOurs(MailFacts const& mail) { return mail.messageType == MESSAGE_TYPE_AUCTION; }

// The next thing to do with one mail. Money first, then the items one by one; an empty mail is deleted.
inline Step NextStep(MailFacts const& mail, Subject& subject)
{
    if (!IsOurs(mail) || !mail.delivered || mail.cod || !ParseSubject(mail.subject, subject))
        return Step::Ignore;

    if (mail.money)
        return Step::TakeMoney;
    if (mail.itemCount)
        return Step::TakeItem;
    return Step::DeleteEmpty;
}

constexpr char const* AnswerName(uint32_t answer)
{
    switch (answer)
    {
        case ANSWER_OUTBID: return "outbid";
        case ANSWER_WON: return "won";
        case ANSWER_SUCCESSFUL: return "sold";
        case ANSWER_EXPIRED: return "expired";
        case ANSWER_CANCELLED_TO_BIDDER: return "bid returned";
        case ANSWER_CANCELLED: return "cancelled";
        case ANSWER_SALE_PENDING: return "sale pending";
        default: break;
    }
    return "unknown";
}

// A refused take: bags that are full or gold that would pass the cap leave the mail as it is and stop the collection of
// that kind for this call (a later visit tries again); anything else skips the mail only.
enum class TakeResult : uint8_t
{
    Done,
    BagsFull,
    TooMuchGold,
    Failed
};

constexpr bool StopsCollection(TakeResult result, Step step)
{
    return (result == TakeResult::BagsFull && step == Step::TakeItem) ||
           (result == TakeResult::TooMuchGold && step == Step::TakeMoney);
}
}  // namespace AuctionMailPolicy

#endif
