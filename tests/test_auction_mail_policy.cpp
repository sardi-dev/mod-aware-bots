/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */
#include "AuctionMailPolicy.h"
#include "TestHarness.h"

using namespace AuctionMailPolicy;

namespace
{
MailFacts Delivered(std::string subject, uint64_t money, uint32_t items)
{
    MailFacts mail;
    mail.messageType = MESSAGE_TYPE_AUCTION;
    mail.delivered = true;
    mail.subject = std::move(subject);
    mail.money = money;
    mail.itemCount = items;
    return mail;
}
}  // namespace

TEST(auction_mail_subject_is_parsed_into_its_fields)
{
    Subject subject;
    CHECK(ParseSubject("2589:0:2:1234:20", subject));
    CHECK(subject.itemEntry == 2589 && subject.answer == ANSWER_SUCCESSFUL && subject.auctionId == 1234 && subject.count == 20);
    CHECK(ParseSubject("1:0:6:4294967295:1", subject));
    CHECK(subject.answer == ANSWER_SALE_PENDING && subject.auctionId == 4294967295u);
}

TEST(auction_mail_malformed_subjects_are_refused)
{
    Subject subject;
    CHECK(!ParseSubject("", subject));
    CHECK(!ParseSubject("2589:0:2:1234", subject));          // a field short
    CHECK(!ParseSubject("2589:0:2:1234:20:1", subject));     // a field too many
    CHECK(!ParseSubject("2589:1:2:1234:20", subject));       // the second field is always 0
    CHECK(!ParseSubject("2589:0:7:1234:20", subject));       // an answer the core never sends
    CHECK(!ParseSubject("2589:0:2:-5:20", subject));
    CHECK(!ParseSubject("2589:0:2:12a4:20", subject));
    CHECK(!ParseSubject("2589:0:2::20", subject));
    CHECK(!ParseSubject("4294967296:0:2:1:1", subject));     // does not fit 32 bits
    CHECK(!ParseSubject("Hello friend", subject));
}

TEST(auction_mail_money_comes_first_then_items_then_the_empty_mail_goes)
{
    Subject subject;
    CHECK(NextStep(Delivered("5:0:2:9:1", 12345, 0), subject) == Step::TakeMoney);
    CHECK(subject.answer == ANSWER_SUCCESSFUL);
    CHECK(NextStep(Delivered("5:0:3:9:1", 0, 1), subject) == Step::TakeItem);
    CHECK(NextStep(Delivered("5:0:1:9:1", 0, 3), subject) == Step::TakeItem);
    CHECK(NextStep(Delivered("5:0:6:9:1", 0, 0), subject) == Step::DeleteEmpty);   // a sale notice holds nothing
    // Money before items, so a mail that holds both is emptied in order.
    CHECK(NextStep(Delivered("5:0:5:9:1", 10, 1), subject) == Step::TakeMoney);
}

TEST(auction_mail_only_delivered_auction_mail_is_touched)
{
    Subject subject;
    MailFacts mail = Delivered("5:0:2:9:1", 100, 0);
    mail.messageType = 0;   // MAIL_NORMAL, from a player
    CHECK(NextStep(mail, subject) == Step::Ignore);

    mail = Delivered("5:0:2:9:1", 100, 0);
    mail.messageType = 3;   // from a creature
    CHECK(NextStep(mail, subject) == Step::Ignore);

    mail = Delivered("5:0:2:9:1", 100, 0);
    mail.delivered = false;
    CHECK(NextStep(mail, subject) == Step::Ignore);

    // A player's mail with an auction-like subject is still a player's mail.
    mail = Delivered("5:0:2:9:1", 100, 0);
    mail.messageType = 0;
    CHECK(NextStep(mail, subject) == Step::Ignore);
}

TEST(auction_mail_cash_on_delivery_is_never_paid)
{
    Subject subject;
    MailFacts mail = Delivered("5:0:1:9:1", 0, 1);
    mail.cod = 5;
    CHECK(NextStep(mail, subject) == Step::Ignore);
}

TEST(auction_mail_an_unparsable_auction_mail_is_left_alone_not_deleted)
{
    Subject subject;
    CHECK(NextStep(Delivered("not a subject", 100, 1), subject) == Step::Ignore);
    CHECK(NextStep(Delivered("not a subject", 0, 0), subject) == Step::Ignore);
}

TEST(auction_mail_answers_have_readable_names)
{
    CHECK(std::string(AnswerName(ANSWER_SUCCESSFUL)) == "sold");
    CHECK(std::string(AnswerName(ANSWER_EXPIRED)) == "expired");
    CHECK(std::string(AnswerName(ANSWER_WON)) == "won");
    CHECK(std::string(AnswerName(99)) == "unknown");
}

TEST(auction_mail_a_refused_take_stops_only_its_own_kind)
{
    CHECK(StopsCollection(TakeResult::BagsFull, Step::TakeItem));
    CHECK(!StopsCollection(TakeResult::BagsFull, Step::TakeMoney));
    CHECK(StopsCollection(TakeResult::TooMuchGold, Step::TakeMoney));
    CHECK(!StopsCollection(TakeResult::TooMuchGold, Step::TakeItem));
    CHECK(!StopsCollection(TakeResult::Failed, Step::TakeItem));
    CHECK(!StopsCollection(TakeResult::Done, Step::TakeItem));
}
