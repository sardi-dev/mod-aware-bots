#ifndef AWARE_BOTS_TRADE_OUTCOME_POLICY_H
#define AWARE_BOTS_TRADE_OUTCOME_POLICY_H

#include <cstdint>

// Whether a trade the bot just accepted really went through. The core closes the trade window both when the trade
// completes and when it cancels it at the last step (for example a full bag), so a closed window alone does not tell
// the two apart. The bot's side is compared before and after its accept: money that changed, an offered item that left
// its bags, or the other side's item that arrived means the trade happened.
namespace TradeOutcomePolicy
{
constexpr bool Completed(uint64_t moneyBefore, uint64_t moneyAfter, uint32_t givenStillHeld, uint32_t givenCount,
                         uint32_t takenNowHeld)
{
    return moneyBefore != moneyAfter || givenStillHeld < givenCount || takenNowHeld > 0;
}
}

#endif
