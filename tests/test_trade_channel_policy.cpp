#include "TestHarness.h"
#include "TradeChannelPolicy.h"

TEST(trade_channel_uses_joined_city_channel)
{
    CHECK(TradeChannelPolicy::Accept(true, false, true, "Trade - City", "Stormwind City"));
    CHECK(!TradeChannelPolicy::Accept(true, false, false, "Trade - City", "Stormwind City"));
    CHECK(!TradeChannelPolicy::Accept(true, false, true, "", "Stormwind City"));
    CHECK(TradeChannelPolicy::Accept(false, false, false, "General - Stormwind City", "Stormwind City"));
    CHECK(!TradeChannelPolicy::Accept(false, false, true, "General - City", "Stormwind City"));
    CHECK(TradeChannelPolicy::Accept(false, true, false, "LookingForGroup", "Stormwind City"));
}
