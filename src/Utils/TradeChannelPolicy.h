#ifndef AWARE_BOTS_TRADE_CHANNEL_POLICY_H
#define AWARE_BOTS_TRADE_CHANNEL_POLICY_H

#include <string>

namespace TradeChannelPolicy
{
inline bool Accept(bool trade, bool zoneIndependent, bool joined, std::string const& name, std::string const& zone)
{
    if (name.empty())
        return false;
    if (trade)
        return joined;
    return zoneIndependent || name.find(zone) != std::string::npos;
}
}
#endif
