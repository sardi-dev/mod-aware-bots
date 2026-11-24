#include "BankErrandPolicy.h"
#include "TestHarness.h"

TEST(bank_errand_whole_stacks_caps_protection_and_limit)
{
    using namespace BankErrandPolicy;
    Stack s; s.count = 20; s.keepCap = 60; s.banked = 40;
    CHECK(Deposit(s));
    s.banked = 41; CHECK(!Deposit(s));
    s.rareKept = true; CHECK(Deposit(s));
    s.protectedItem = true; CHECK(!Deposit(s));
    s.protectedItem = false; s.count = 0; CHECK(!Deposit(s));
    s.count = 20; s.banked = 0; CHECK(SelectDeposits({s, s, s}, 2).size() == 2);
    s.rareKept = false; s.banked = 40; CHECK(SelectDeposits({s, s}, 8).size() == 1);
    CHECK(SelectDeposits({s}, 0).empty());
}
TEST(bank_errand_auction_withdrawal_gates)
{
    using namespace BankErrandPolicy;
    Stack s; s.count = 5; s.keepCap = 20; s.auctionSellList = true;
    CHECK(Withdraw(s, true, 6));
    CHECK(SelectWithdrawals({s, s}, true, 6, 1).size() == 1);
    CHECK(!Withdraw(s, false, 6));
    CHECK(!Withdraw(s, true, 5));
    s.auctionSellList = false; CHECK(!Withdraw(s, true, 6));
    s.auctionSellList = true; s.protectedItem = true; CHECK(!Withdraw(s, true, 6));
}
