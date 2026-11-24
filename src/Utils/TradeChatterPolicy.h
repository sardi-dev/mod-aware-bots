/* Pure decisions for truthful trade chatter. Times are monotonic seconds supplied by the caller. */
#ifndef AWARE_BOTS_TRADE_CHATTER_POLICY_H
#define AWARE_BOTS_TRADE_CHATTER_POLICY_H

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace TradeChatterPolicy
{
struct Listing
{
    std::string name;
    uint64_t buyoutCopper = 0;
    bool active = false;
    bool owned = false;
};

enum class Kind { None, Sale, Recipes };
enum class Style { Other, Trader, GathererCrafter };
struct Advertisement
{
    Kind kind = Kind::None;
    std::string goods;
    uint64_t price = 0;
};

// The caller supplies only current observations; never infer a listing from inventory or a recipe from a skill alone.
inline Advertisement Choose(Style style, std::vector<Listing> const& listings,
                            std::vector<std::string> const& knownRecipes, size_t recipesPerLine)
{
    if (style == Style::Trader)
        for (auto const& listing : listings)
            if (listing.active && listing.owned && !listing.name.empty() && listing.buyoutCopper)
                return { Kind::Sale, listing.name, listing.buyoutCopper };
    if (style == Style::GathererCrafter)
    {
        std::string names;
        size_t count = 0;
        for (auto const& recipe : knownRecipes)
        {
            if (recipe.empty())
                continue;
            if (count == recipesPerLine)
                break;
            if (!names.empty())
                names += ", ";
            names += recipe;
            ++count;
        }
        if (count)
            return { Kind::Recipes, names, 0 };
    }
    return {};
}

// Rolling windows, including the boundary. Zero is a closed limit. Caller serializes check and commit.
inline void Prune(std::deque<uint64_t>& times, uint64_t now, uint64_t period)
{
    while (!times.empty() && (times.front() > now || now - times.front() >= period))
        times.pop_front();
}

enum class Limit { Allowed, Bot, Channel, Faction };
inline Limit Check(std::deque<uint64_t>& bot, std::deque<uint64_t>& channel, std::deque<uint64_t>& faction,
                   uint64_t now, size_t botPerHour, size_t channelPerMinute, size_t factionPerHour)
{
    Prune(bot, now, 3600);
    Prune(channel, now, 60);
    Prune(faction, now, 3600);
    if (bot.size() >= botPerHour) return Limit::Bot;
    if (channel.size() >= channelPerMinute) return Limit::Channel;
    if (faction.size() >= factionPerHour) return Limit::Faction;
    return Limit::Allowed;
}
inline void Commit(std::deque<uint64_t>& bot, std::deque<uint64_t>& channel, std::deque<uint64_t>& faction, uint64_t now)
{
    bot.push_back(now);
    channel.push_back(now);
    faction.push_back(now);
}

inline std::string Money(uint64_t copper)
{
    return std::to_string(copper / 10000) + "g " + std::to_string(copper / 100 % 100) + "s " +
        std::to_string(copper % 100) + "c";
}
inline std::string Text(Advertisement const& ad, unsigned personality)
{
    if (ad.kind == Kind::Sale)
    {
        static char const* const beginnings[] = { "Listed ", "I've listed ", "Fresh on the auction house: ", "Selling " };
        return std::string(beginnings[personality % 4]) + ad.goods + " for " + Money(ad.price) + ".";
    }
    if (ad.kind == Kind::Recipes)
    {
        static char const* const beginnings[] = { "I can craft ", "Happy to craft ", "Crafting on the menu: ", "Can craft " };
        return std::string(beginnings[personality % 4]) + ad.goods + ".";
    }
    return {};
}
}
#endif
