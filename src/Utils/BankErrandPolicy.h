#ifndef PLAYERBOTS_BANK_ERRAND_POLICY_H
#define PLAYERBOTS_BANK_ERRAND_POLICY_H

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace BankErrandPolicy
{
struct Stack
{
    uint32_t entry = 0;
    uint32_t count = 0;
    uint32_t banked = 0;
    uint32_t keepCap = 0;
    bool protectedItem = false;
    bool rareKept = false;
    bool auctionSellList = false;
};
inline bool Deposit(Stack const& s)
{
    return s.count && !s.protectedItem &&
        ((s.keepCap && uint64_t(s.banked) + s.count <= s.keepCap) || s.rareKept);
}
inline bool Withdraw(Stack const& s, bool auctionEnabled, uint32_t freeBag)
{
    return s.count && !s.protectedItem && s.keepCap && auctionEnabled && freeBag >= 6 && s.auctionSellList;
}
inline bool CanMove(uint32_t moved, uint32_t maximum) { return moved < maximum; }
inline std::vector<uint32_t> SelectDeposits(std::vector<Stack> const& stacks, uint32_t maximum)
{
    std::vector<uint32_t> result;
    std::unordered_map<uint32_t, uint64_t> added;
    for (uint32_t i = 0; i < stacks.size() && CanMove(result.size(), maximum); ++i)
    {
        Stack stack = stacks[i];
        uint64_t banked = uint64_t(stack.banked) + added[stack.entry];
        stack.banked = banked > UINT32_MAX ? UINT32_MAX : uint32_t(banked);
        if (Deposit(stack)) { result.push_back(i); added[stack.entry] += stack.count; }
    }
    return result;
}
inline std::vector<uint32_t> SelectWithdrawals(std::vector<Stack> const& stacks, bool auctionEnabled,
                                               uint32_t freeBag, uint32_t maximum)
{
    std::vector<uint32_t> result;
    for (uint32_t i = 0; i < stacks.size() && CanMove(result.size(), maximum); ++i)
        if (Withdraw(stacks[i], auctionEnabled, freeBag)) result.push_back(i);
    return result;
}
}
#endif
