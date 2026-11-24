/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef AWARE_BOTS_ERRAND_POLICY_H
#define AWARE_BOTS_ERRAND_POLICY_H

#include <cstdint>

// BT-253 (owner 2026-11-16): in a town a bot runs errands with a purpose instead of hopping between random NPCs every 8 s.
// It walks straight to a real target (repairer, vendor, class trainer, mailbox, banker, auctioneer, innkeeper), stays
// there like a player does (still, facing the NPC, now and then a gesture, seated at the inn), then runs one more errand
// or leaves town. What the bot needs comes first; otherwise it picks a place people idle at.
namespace ErrandPolicy
{
enum class Errand : uint8_t
{
    None = 0,
    Repair,
    Vendor,
    Trainer,
    Mailbox,
    Bank,
    Auction,
    Inn,
};

// What the bot needs and which errand targets stand within reach.
struct Town
{
    bool needsRepair = false;
    bool needsVendor = false;   // full bags, or low on food or drink
    bool needsTraining = false; // spells to learn at its class trainer
    bool hasMail = false;
    // A soak bot (AiPlayerbot.Auction.OnlyBots) whose auction visit is allowed now (BotAuction::SoakVisitDue).
    bool auctionDue = false;
    bool repairer = false;
    bool vendor = false;
    bool trainer = false;
    bool mailbox = false;
    bool banker = false;
    bool auctioneer = false;
    bool innkeeper = false;
};

// Weights of the idle errands, out of their sum: the bank and the auction house draw most players, then the inn.
constexpr uint32_t BANK_WEIGHT = 30;
constexpr uint32_t AUCTION_WEIGHT = 30;
constexpr uint32_t INN_WEIGHT = 25;
constexpr uint32_t MAILBOX_WEIGHT = 15;

// roll: any number in [0, 100).
constexpr Errand Choose(Town const& town, uint32_t roll)
{
    if (town.needsRepair && town.repairer)
        return Errand::Repair;
    if (town.needsVendor && town.vendor)
        return Errand::Vendor;
    if (town.needsTraining && town.trainer)
        return Errand::Trainer;
    if (town.hasMail && town.mailbox)
        return Errand::Mailbox;
    // The soak must see auction visits: a named bot at an auction house goes to it before it idles anywhere else.
    if (town.auctionDue && town.auctioneer)
        return Errand::Auction;

    uint32_t const bank = town.banker ? BANK_WEIGHT : 0;
    uint32_t const auction = town.auctioneer ? AUCTION_WEIGHT : 0;
    uint32_t const inn = town.innkeeper ? INN_WEIGHT : 0;
    uint32_t const mail = town.mailbox ? MAILBOX_WEIGHT : 0;
    uint32_t const sum = bank + auction + inn + mail;
    if (!sum)
        return Errand::None;
    uint32_t const pick = roll % sum;
    if (pick < bank)
        return Errand::Bank;
    if (pick < bank + auction)
        return Errand::Auction;
    if (pick < bank + auction + inn)
        return Errand::Inn;
    return Errand::Mailbox;
}

// How long the bot stays at the target, from a roll in [0, 100). A need is done quickly; idling takes longer.
constexpr uint32_t DwellMs(Errand errand, uint32_t roll)
{
    uint32_t low = 15000, high = 30000;
    switch (errand)
    {
        case Errand::Repair:
        case Errand::Vendor:
        case Errand::Trainer:
            low = 10000;
            high = 20000;
            break;
        case Errand::Mailbox:
            low = 10000;
            high = 25000;
            break;
        case Errand::Bank:
            low = 20000;
            high = 60000;
            break;
        case Errand::Auction:
            low = 30000;
            high = 90000;
            break;
        case Errand::Inn:
            low = 60000;
            high = 180000;
            break;
        case Errand::None:
            break;
    }
    return low + (high - low) * (roll % 100) / 99;
}

// A bot sits down at the inn; elsewhere it stands.
constexpr bool SitsThere(Errand errand)
{
    return errand == Errand::Inn;
}

// Time to the next gesture (an emote) while the bot stays, from a roll in [0, 100): 20 to 60 s.
constexpr uint32_t NextGestureMs(uint32_t roll)
{
    return 20000 + 40000 * (roll % 100) / 99;
}

// After an errand: another one (60 % after the first, 30 % after the second, never after the third), else the bot
// leaves town (its next status is drawn as usual).
constexpr bool AnotherErrand(uint32_t errandsDone, uint32_t roll)
{
    uint32_t const chance = errandsDone <= 1 ? 60 : errandsDone == 2 ? 30 : 0;
    return roll % 100 < chance;
}

constexpr char const* Name(Errand errand)
{
    switch (errand)
    {
        case Errand::Repair: return "repair";
        case Errand::Vendor: return "vendor";
        case Errand::Trainer: return "trainer";
        case Errand::Mailbox: return "mailbox";
        case Errand::Bank: return "bank";
        case Errand::Auction: return "auction house";
        case Errand::Inn: return "inn";
        case Errand::None: break;
    }
    return "none";
}
}  // namespace ErrandPolicy

#endif
