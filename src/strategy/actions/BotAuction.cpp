/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The auction house visit of random bots (BotAuction.h). The decisions are in the Utils headers; this file reads the bot,
 * the items, the house and the mail, calls the core helpers (AuctionHouseMgr::ListItem and PlaceBid, Player::TakeMailMoney,
 * TakeMailItem and DeleteEmptyMail) and writes one log line per action.
 */

#include "BotAuction.h"

#include "AuctionHouseMgr.h"
#include "AuctionMailPolicy.h"
#include "AuctionVisitPolicy.h"
#include "BotAuctionPolicy.h"
#include "BotSale.h"
#include "BotSalePolicy.h"
#include "BotShopping.h"
#include "Creature.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "Item.h"
#include "ItemUsageValue.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PriceTable.h"
#include "PricePolicy.h"
#include "RandomPlayerbotMgr.h"
#include "ReservePolicy.h"
#include "ScriptMgr.h"
#include "Timer.h"
#include "Util.h"
#include "World.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    struct RecipeCounters { uint64 bought = 0, learned = 0, budget = 0, limit = 0; };
    struct BankCounters { uint64 visits = 0, deposited = 0, withdrawn = 0, full = 0; };
    BankCounters sBankCounters;
    std::mutex sRecipeLock;
    RecipeCounters sRecipeCounters;
    std::mutex sListingLock;
    std::unordered_set<uint32> sRandomCharacters;
    std::unordered_map<uint32, std::vector<BotAuction::Listing>> sActiveListings;

    void CacheListing(AuctionEntry const* entry)
    {
        if (!entry) return;
        std::lock_guard<std::mutex> lock(sListingLock);
        if (!sRandomCharacters.count(entry->owner)) return;
        auto& entries = sActiveListings[entry->owner];
        entries.erase(std::remove_if(entries.begin(), entries.end(), [entry](BotAuction::Listing const& item)
            { return item.auctionId == entry->Id; }), entries.end());
        entries.push_back({entry->Id, entry->itemEntry, entry->itemCount, entry->buyout, entry->expire_time});
    }

    void RemoveListing(AuctionEntry const* entry)
    {
        if (!entry) return;
        std::lock_guard<std::mutex> lock(sListingLock);
        auto found = sActiveListings.find(entry->owner);
        if (found == sActiveListings.end()) return;
        auto& entries = found->second;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [entry](BotAuction::Listing const& item)
            { return item.auctionId == entry->Id; }), entries.end());
        if (entries.empty()) sActiveListings.erase(found);
    }

    using namespace AuctionVisitPolicy;
    namespace AuctionMail = AuctionMailPolicy;

    // Reading what a listing needs costs a usage value per item; a visit looks at no more than this many.
    constexpr uint32 MAX_NEED_EVALUATIONS = 40;

    // The skill lines the reserve counts: gathering and the secondary professions (the crafting ones are in ReservePolicy).
    constexpr uint32 OTHER_PROFESSION_SKILLS[] = { SKILL_HERBALISM, SKILL_MINING, SKILL_SKINNING, SKILL_COOKING, SKILL_FIRST_AID,
                                                   SKILL_FISHING, SKILL_ARCHAEOLOGY };

    // The spell of the flight licence (Flight Master's License), part of the riding steps of the reserve.
    constexpr uint32 FLIGHT_LICENCE_SPELL = 90267;

    // The first gold of the day-long price mood changes at midnight of the server clock.
    constexpr uint32 SECONDS_PER_DAY = 24 * 60 * 60;

    // ---------------------------------------------------------------------------------------------------------------
    // What the map threads and the world thread share: one record per bot and the requests posted by the errand.
    // ---------------------------------------------------------------------------------------------------------------
    struct BotRecord
    {
        bool busy = false;
        bool cancel = false;
        bool visited = false;
        uint32 lastEndMs = 0;
    };

    struct Request
    {
        uint64 bot = 0;
        uint64 target = 0;
        uint32 mapId = 0;
        Kind kind = Kind::Auction;
    };

    std::mutex sLock;
    std::unordered_map<uint64, BotRecord> sBots;
    std::vector<Request> sRequests;

    // ---------------------------------------------------------------------------------------------------------------
    // World thread only from here on.
    // ---------------------------------------------------------------------------------------------------------------
    struct PlannedSale
    {
        uint32 stack = 0;        // index into Visit::stacks
        uint64 itemGuid = 0;
    };

    struct Visit
    {
        uint64 botGuid = 0;
        std::string name;
        uint64 targetGuid = 0;
        uint32 mapId = 0;
        uint32 level = 0;
        Kind kind = Kind::Auction;
        Phase phase = Phase::Mail;
        uint32 waitMs = 0;
        uint32 elapsedMs = 0;
        uint32 failures = 0;
        bool done = false;
        EndReason reason = EndReason::Finished;

        uint64 moneyOnArrival = 0;
        ReservePolicy::VisitMoney money;
        BotAuctionPolicy::SellPlan plan;
        std::vector<BotAuctionPolicy::SnapshotListing> snapshot;
        std::vector<BotAuctionPolicy::StackFacts> stacks;
        std::vector<PlannedSale> sales;
        size_t nextSale = 0;
        bool buyPlanned = false;
        std::vector<BotAuctionPolicy::Purchase> purchases;
        size_t nextPurchase = 0;
        VisitResult result;
    };

    std::vector<Visit> sVisits;
    Totals sTotals;
    IntervalStats sInterval;
    uint32 sLastSummaryMs = 0;
    bool sSummaryStarted = false;
    bool sTotalsDirty = false;

    std::string Money(uint64 copper) { return FormatCopper(copper); }

    double Roll() { return double(urand(0, 999999)) / 1000000.0; }

    template <typename Visitor>
    void ForEachBagItem(Player* bot, Visitor visit)
    {
        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                visit(item);

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
            if (Bag* bag = static_cast<Bag*>(bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot)))
                for (uint32 i = 0; i < bag->GetBagSize(); ++i)
                    if (Item* item = bag->GetItemByPos(i))
                        visit(item);
    }

    std::string ItemName(uint32 entry)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        return proto ? proto->Name1 : std::string("?");
    }

    // The faction of the house the auctioneer belongs to, as the core's own GetAuctionsMap reads it.
    PricePolicy::House HouseOf(Creature* auctioneer)
    {
        FactionTemplateEntry const* faction = sFactionTemplateStore.LookupEntry(auctioneer->getFaction());
        if (!faction)
            return PricePolicy::House::Neutral;
        if (faction->ourMask & FACTION_MASK_ALLIANCE)
            return PricePolicy::House::Alliance;
        if (faction->ourMask & FACTION_MASK_HORDE)
            return PricePolicy::House::Horde;
        return PricePolicy::House::Neutral;
    }

    bool OwnHouse(Player* bot, Creature* auctioneer)
    {
        return !sWorld->GetBoolConfig(WorldBoolConfigs::CONFIG_ALLOW_TWO_SIDE_INTERACTION_AUCTION) &&
               PricePolicy::HouseAllowed(HouseOf(auctioneer), bot->GetTeam() == ALLIANCE);
    }

    ReservePolicy::Facts ReserveFactsOf(Player* bot)
    {
        ReservePolicy::Facts facts;
        facts.level = bot->getLevel();
        facts.classId = bot->getClass();
        // Random bots learn their class spells for free (AiPlayerbot.AutoLearnTrainerSpells, design decision 0016). With it off the table
        // window of the spells ahead still counts; the "train cost" value of the spells learnable now is not added here.
        facts.classSpellsPaid = !sPlayerbotAIConfig->autoLearnTrainerSpells;
        for (uint32 const skill : ReservePolicy::CRAFTING_SKILLS)
            if (bot->HasSkill(skill) && bot->GetSkillValue(skill) > 0)
            {
                ++facts.professionCount;
                ++facts.craftingProfessionCount;
            }
        for (uint32 const skill : OTHER_PROFESSION_SKILLS)
            if (bot->HasSkill(skill) && bot->GetSkillValue(skill) > 0)
                ++facts.professionCount;

        facts.ridingSkill = bot->GetPureSkillValue(SKILL_RIDING);
        facts.knowsFlightLicence = bot->HasSpell(FLIGHT_LICENCE_SPELL);
        facts.knowsMount = BotShopping::KnowsMount(bot);
        return facts;
    }

    ReservePolicy::Params ReserveParams()
    {
        ReservePolicy::Params params;
        params.lookaheadLevels = sPlayerbotAIConfig->auctionReserveLookaheadLevels;
        params.goalSharePercent = sPlayerbotAIConfig->auctionReserveGoalSharePercent;
        params.recipeSharePercent = sPlayerbotAIConfig->auctionReserveRecipeSharePercent;
        return params;
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Mail (docs/economy/auction-wiring.md section 5).
    // ---------------------------------------------------------------------------------------------------------------
    bool MailboxNear(Player* bot)
    {
        return bot->FindNearestGameObjectOfType(GAMEOBJECT_TYPE_MAILBOX, MAILBOX_YARDS) != nullptr;
    }

    // Takes the money and the items of the bot's delivered auction mail, at most MAIL_PER_COLLECTION mails, and deletes the
    // mail that is empty afterwards. Mail of players and creatures is never touched. A take the core refuses leaves the mail as
    // it is.
    void CollectMail(Player* bot, Visit& visit)
    {
        // Observe the entire delivered auction mailbox before the collection limit is applied.
        uint64 backlog = 0;
        for (auto itr = bot->GetMailBegin(); itr != bot->GetMailEnd(); ++itr)
            if (*itr && (*itr)->messageType == AuctionMail::MESSAGE_TYPE_AUCTION && (*itr)->IsDelivered() &&
                ((*itr)->money || !(*itr)->items.empty()))
                ++backlog;
        sInterval.MailBacklog(backlog);

        // The core frees a deleted mail while saving, so work from the ids and look each mail up again.
        std::vector<uint32> ids;
        for (auto itr = bot->GetMailBegin(); itr != bot->GetMailEnd() && ids.size() < MAIL_PER_COLLECTION; ++itr)
            if (*itr && (*itr)->messageType == AuctionMail::MESSAGE_TYPE_AUCTION && (*itr)->IsDelivered())
                ids.push_back((*itr)->messageID);

        bool itemsStopped = false;
        bool moneyStopped = false;
        bool touched = false;
        for (uint32 const id : ids)
        {
            ::Mail* mail = bot->GetMail(id);
            if (!mail)
                continue;

            AuctionMail::Subject subject;
            for (uint32 guard = 0; guard < 64; ++guard)
            {
                mail = bot->GetMail(id);
                if (!mail)
                    break;

                AuctionMail::MailFacts facts;
                facts.messageType = mail->messageType;
                facts.delivered = mail->IsDelivered();
                facts.subject = mail->subject;
                facts.money = mail->money;
                facts.itemCount = uint32(mail->items.size());
                facts.cod = mail->COD;
                AuctionMail::Step const step = AuctionMail::NextStep(facts, subject);
                touched = touched || step != AuctionMail::Step::Ignore;
                if (step == AuctionMail::Step::Ignore)
                    break;

                if (step == AuctionMail::Step::TakeMoney)
                {
                    if (moneyStopped)
                        break;

                    uint64 const money = mail->money;
                    uint32 equipError = 0;
                    MailResponseResult const result = bot->TakeMailMoney(id, money, equipError);
                    if (result == MAIL_OK)
                    {
                        visit.result.mailMoneyCopper += money;
                        if (subject.answer == AuctionMail::ANSWER_SUCCESSFUL)
                            visit.result.saleGoldCollectedCopper += money;
                        ++visit.result.mailMoneyMails;
                        SF_LOG_INFO("playerbots", "[AUCTION] %s mail %s entry %u [%s] x%u money %s ok", visit.name.c_str(),
                                    AuctionMail::AnswerName(subject.answer), subject.itemEntry, ItemName(subject.itemEntry).c_str(),
                                    subject.count, Money(money).c_str());
                        continue;
                    }

                    AuctionMail::TakeResult const taken = result == MAIL_ERR_EQUIP_ERROR && equipError == EQUIP_ERR_TOO_MUCH_GOLD
                                                       ? AuctionMail::TakeResult::TooMuchGold
                                                       : AuctionMail::TakeResult::Failed;
                    ++visit.result.mailBlocked;
                    moneyStopped = moneyStopped || AuctionMail::StopsCollection(taken, step);
                    SF_LOG_INFO("playerbots", "[AUCTION] %s mail %s entry %u [%s] x%u money %s refused (%u)", visit.name.c_str(),
                                AuctionMail::AnswerName(subject.answer), subject.itemEntry, ItemName(subject.itemEntry).c_str(),
                                subject.count, Money(money).c_str(), uint32(result));
                    break;
                }

                if (step == AuctionMail::Step::TakeItem)
                {
                    if (itemsStopped)
                        break;

                    uint32 equipError = 0;
                    uint32 itemCount = 0;
                    MailResponseResult const result = bot->TakeMailItem(id, mail->items.front().item_guid, equipError, itemCount);
                    if (result == MAIL_OK)
                    {
                        ++visit.result.mailItems;
                        SF_LOG_INFO("playerbots", "[AUCTION] %s mail %s entry %u [%s] x%u item ok", visit.name.c_str(),
                                    AuctionMail::AnswerName(subject.answer), subject.itemEntry, ItemName(subject.itemEntry).c_str(),
                                    itemCount);
                        continue;
                    }

                    AuctionMail::TakeResult const taken = result == MAIL_ERR_EQUIP_ERROR ? AuctionMail::TakeResult::BagsFull : AuctionMail::TakeResult::Failed;
                    ++visit.result.mailBlocked;
                    itemsStopped = itemsStopped || AuctionMail::StopsCollection(taken, step);
                    SF_LOG_INFO("playerbots", "[AUCTION] %s mail %s entry %u [%s] x%u item %s (%u)", visit.name.c_str(),
                                AuctionMail::AnswerName(subject.answer), subject.itemEntry, ItemName(subject.itemEntry).c_str(),
                                subject.count, taken == AuctionMail::TakeResult::BagsFull ? "full" : "error", uint32(equipError));
                    break;
                }

                // Empty: the core refuses to delete a mail that still holds money, items or a COD.
                if (!bot->DeleteEmptyMail(id))
                    ++visit.result.errors;
                break;
            }
        }

        if (!touched)
            return;

        // A bot reads a mailbox like a player: what it handled is read, and the errand no longer sends it back for it.
        for (auto itr = bot->GetMailBegin(); itr != bot->GetMailEnd(); ++itr)
            if (*itr && (*itr)->messageType == AuctionMail::MESSAGE_TYPE_AUCTION && (*itr)->IsDelivered() &&
                !((*itr)->checked & MAIL_CHECK_MASK_READ))
            {
                (*itr)->checked |= MAIL_CHECK_MASK_READ;
                (*itr)->state = MailState::MAIL_STATE_CHANGED;
            }
        bot->UpdateNextMailTimeAndUnreads();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The snapshot and the plan of a visit.
    // ---------------------------------------------------------------------------------------------------------------
    void TakeSnapshot(Player* bot, Creature* auctioneer, Visit& visit)
    {
        std::vector<AuctionListing> listings;
        sAuctionMgr->GetAuctionsMap(auctioneer->getFaction())->GetListings(listings, 0, SNAPSHOT_LIMIT);
        uint64 omitted = 0;
        if (listings.size() == SNAPSHOT_LIMIT)
        {
            // Count the remaining valid listings in bounded pages without retaining them in the snapshot.
            uint32 afterId = listings.back().id;
            std::vector<AuctionListing> page;
            do
            {
                page.clear();
                sAuctionMgr->GetAuctionsMap(auctioneer->getFaction())->GetListings(page, afterId, SNAPSHOT_LIMIT);
                omitted += page.size();
                if (!page.empty())
                    afterId = page.back().id;
            } while (page.size() == SNAPSHOT_LIMIT);
        }
        sInterval.Snapshot(listings.size(), SNAPSHOT_LIMIT, omitted);
        sTotalsDirty = true;

        visit.snapshot.reserve(listings.size());
        uint32 own = 0;
        for (AuctionListing const& listing : listings)
        {
            BotAuctionPolicy::SnapshotListing item;
            item.id = listing.id;
            item.entry = listing.itemEntry;
            item.count = listing.itemCount;
            item.buyout = listing.buyout;
            item.bid = listing.bid;
            item.own = listing.owner == bot->GetGUIDLow();
            own += item.own ? 1 : 0;
            visit.snapshot.push_back(item);
        }
        visit.plan.activeListings = own;
    }

    void PlanSales(Player* bot, PlayerbotAI* botAI, Visit& visit)
    {
        uint32 const professions = BotSale::ProfessionsOf(bot);
        std::vector<uint64> guids;
        ForEachBagItem(bot, [&](Item* item)
        {
            ItemTemplate const* proto = item->GetTemplate();
            BotAuctionPolicy::StackFacts stack;
            stack.entry = proto->ItemId;
            stack.count = item->GetCount();
            stack.banked = bot->GetItemCount(proto->ItemId, true) - bot->GetItemCount(proto->ItemId, false);
            stack.vendorSellPrice = proto->SellPrice;
            stack.hasRange = PriceTable::Find(proto->ItemId, stack.range);
            // The cheap checks first: an item with no price range or one the core would refuse is never listable.
            if (!stack.hasRange)
                return;

            stack.tradable = sAuctionMgr->CanAuctionItem(item, stack.count);
            if (!stack.tradable)
                return;

            stack.facts = BotSale::FactsOf(bot, item, professions);
            stack.material = BotSale::MaterialUseOf(bot, proto);
            stack.tradeGood = proto->Class == ITEM_CLASS_TRADE_GOODS;
            stack.crafted = item->GetUInt64Value(ITEM_FIELD_CREATOR) == bot->GetGUID();

            // The usage value is not free: ask only for what could be listed once the keep caps are applied.
            BotSalePolicy::ItemFacts open = stack.facts;
            open.professionMaterial = false;
            open.usableFoodOrDrink = false;
            open.hasSellPrice = true;
            open.gatheredMaterial = open.gatheredMaterial || stack.tradeGood;
            if (stack.crafted)
                open.unwantedLoot = true;
            stack.usageKeeps = BotSalePolicy::ShouldSell(open) && BotSale::UsageKeeps(botAI, proto);

            visit.stacks.push_back(stack);
            guids.push_back(item->GetGUID());
        });

        BotAuctionPolicy::SellPlan& plan = visit.plan;
        plan.longListingAboveCopper = uint64(sPlayerbotAIConfig->auctionLongListingMinGold) * PricePolicy::COPPER_PER_GOLD;
        plan.maxListings = sPlayerbotAIConfig->auctionMaxListingsPerVisit;
        plan.maxActiveListings = sPlayerbotAIConfig->auctionMaxActiveListings;
        plan.depositBudget = ReservePolicy::DepositLeft(visit.money, bot->GetMoney());

        for (BotAuctionPolicy::Listing const& listing : BotAuctionPolicy::SelectListings(visit.stacks, plan))
            visit.sales.push_back({ listing.stack, guids[listing.stack] });
    }

    // One step of the sell phase: re-check the stack, price it, list it. Returns false when nothing was attempted (the
    // caller moves on without waiting).
    bool SellNext(Player* bot, Creature* auctioneer, Visit& visit)
    {
        PlannedSale const planned = visit.sales[visit.nextSale++];
        BotAuctionPolicy::StackFacts const& stack = visit.stacks[planned.stack];
        std::string const label = "entry " + std::to_string(stack.entry) + " [" + ItemName(stack.entry) + "] x" + std::to_string(stack.count);

        // The item may have been used, traded or sold meanwhile.
        Item* item = bot->GetItemByGuid(planned.itemGuid);
        if (!item || item->GetEntry() != stack.entry || item->GetCount() != stack.count || !sAuctionMgr->CanAuctionItem(item, stack.count))
        {
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip %s: the stack changed", visit.name.c_str(), label.c_str());
            return false;
        }

        PricePolicy::Market const market = BotAuctionPolicy::MarketOf(visit.snapshot, stack.entry);
        double const mood = PricePolicy::DailyMood(uint32(sWorld->GetGameTime() / SECONDS_PER_DAY), stack.range.priceClass);
        BotAuctionPolicy::Quote quote;
        uint64 const depositLeft = ReservePolicy::DepositLeft(visit.money, bot->GetMoney());
        if (!BotAuctionPolicy::QuoteListing(stack, market, Roll(), Roll(), mood, visit.plan, depositLeft, quote))
        {
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip %s: no terms (deposit budget %s, range %s to %s)", visit.name.c_str(),
                        label.c_str(), Money(depositLeft).c_str(), Money(stack.range.minCopper).c_str(),
                        Money(stack.range.maxCopper).c_str());
            return false;
        }

        // The planned deposit must fit the visit's money before the core takes the real one.
        if (!ReservePolicy::SpendDeposit(visit.money, bot->GetMoney(), quote.terms.deposit))
        {
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip %s: deposit %s over the budget", visit.name.c_str(), label.c_str(),
                        Money(quote.terms.deposit).c_str());
            return false;
        }

        AuctionListOutcome outcome;
        outcome.auctionId = 0;
        outcome.deposit = 0;
        uint32 const hours = quote.terms.durationSeconds / (60 * 60);
        AuctionListResult const result =
            sAuctionMgr->ListItem(bot, auctioneer, planned.itemGuid, stack.count, quote.startBid, quote.buyout, hours, outcome);
        if (result == AuctionListResult::Ok)
        {
            // The core's deposit also carries Rate.Auction.Deposit; what is above the plan counts against the visit.
            if (outcome.deposit > quote.terms.deposit)
            {
                visit.money.depositsSpent += outcome.deposit - quote.terms.deposit;
                visit.money.wallet.committed += outcome.deposit - quote.terms.deposit;
            }

            CacheListing(sAuctionMgr->GetAuctionsMap(auctioneer->getFaction())->GetAuction(outcome.auctionId));
            ++visit.result.listings;
            visit.result.listedCopper += quote.buyout;
            visit.result.depositsCopper += outcome.deposit;
            BotAuctionPolicy::SnapshotListing own;
            own.id = outcome.auctionId;
            own.entry = stack.entry;
            own.count = stack.count;
            own.buyout = quote.buyout;
            own.own = true;
            visit.snapshot.push_back(own);
            ++visit.plan.activeListings;
            SF_LOG_INFO("playerbots",
                        "[AUCTION] %s list %s unit %s buyout %s start %s min %s max %s class %u hours %u deposit %s (listing %u) ok",
                        visit.name.c_str(), label.c_str(), Money(quote.unit).c_str(), Money(quote.buyout).c_str(),
                        Money(quote.startBid).c_str(), Money(stack.range.minCopper).c_str(), Money(stack.range.maxCopper).c_str(),
                        uint32(stack.range.priceClass), hours, Money(outcome.deposit).c_str(), outcome.auctionId);
            return true;
        }

        // The planned deposit was only reserved on the visit's books; the core took nothing.
        visit.money.depositsSpent -= std::min(visit.money.depositsSpent, quote.terms.deposit);
        visit.money.wallet.committed -= std::min(visit.money.wallet.committed, quote.terms.deposit);
        ++visit.failures;
        ++visit.result.refusals;
        SF_LOG_INFO("playerbots", "[AUCTION] %s list %s buyout %s refused: code %u", visit.name.c_str(), label.c_str(),
                    Money(quote.buyout).c_str(), uint32(result));
        switch (result)
        {
            case AuctionListResult::NotEnoughMoney:
                // The policy should have prevented it: a rate that is not 1, or gold spent elsewhere meanwhile.
                ++visit.result.errors;
                visit.nextSale = visit.sales.size();
                break;
            case AuctionListResult::InvalidDuration:
            case AuctionListResult::InvalidAuctioneer:
                ++visit.result.errors;
                visit.done = true;
                visit.reason = EndReason::Failures;
                break;
            default:
                break;
        }
        return true;
    }

    // What the bot wants to buy a listing for, from the systems that already say what it needs.
    PricePolicy::BuyNeed NeedFor(PlayerbotAI* botAI, Player* bot, uint32 entry, uint32 count)
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry);
        if (!proto)
            return PricePolicy::BuyNeed::None;

        bool gear = false;
        bool bag = false;
        if ((proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON || proto->Class == ITEM_CLASS_CONTAINER) &&
            bot->CanUseItem(proto) == EQUIP_ERR_OK)
        {
            ItemUsage const usage = botAI->GetAiObjectContext()->GetValue<ItemUsage>("item usage", std::to_string(entry))->Get();
            bool const upgrade = usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE;
            bag = upgrade && proto->Class == ITEM_CLASS_CONTAINER;
            gear = upgrade && !bag;
        }

        bool const material = BotAuctionPolicy::MaterialWanted(BotSale::MaterialUseOf(bot, proto), bot->GetItemCount(entry, true), count);
        bool const consumable = BotAuctionPolicy::ConsumableWanted(BotSale::IsUsableSupply(bot, proto), BotSale::SupplyHeld(bot, proto), count);
        return BotAuctionPolicy::NeedOf(gear, bag, material, consumable);
    }

    char const* NeedName(PricePolicy::BuyNeed need)
    {
        switch (need)
        {
            case PricePolicy::BuyNeed::GearUpgrade: return "gear";
            case PricePolicy::BuyNeed::Bag: return "bag";
            case PricePolicy::BuyNeed::ProfessionMaterial: return "material";
            case PricePolicy::BuyNeed::Consumable: return "consumable";
            case PricePolicy::BuyNeed::None: break;
        }
        return "none";
    }

    void PlanPurchases(Player* bot, PlayerbotAI* botAI, Visit& visit)
    {
        visit.buyPlanned = true;
        if (!sPlayerbotAIConfig->auctionBuyEnable)
            return;

        uint64 const moneyNow = bot->GetMoney();
        std::vector<BotAuctionPolicy::PurchaseCandidate> candidates;
        uint32 evaluated = 0;
        for (BotAuctionPolicy::SnapshotListing const& listing : visit.snapshot)
        {
            if (listing.own || !listing.buyout || !listing.count)
                continue;

            BotAuctionPolicy::PurchaseCandidate candidate;
            candidate.id = listing.id;
            candidate.entry = listing.entry;
            candidate.count = listing.count;
            candidate.buyout = listing.buyout;
            candidate.hasRange = PriceTable::Find(listing.entry, candidate.range);
            // The highest cap of any need is the gear one: a listing above even that is not worth reading a usage value for.
            if (!candidate.hasRange || !PricePolicy::WillBuy(listing.buyout, listing.count, candidate.range,
                                                             PricePolicy::BuyNeed::GearUpgrade, visit.money.wallet, moneyNow,
                                                             visit.money.reserve))
                continue;

            if (evaluated >= MAX_NEED_EVALUATIONS)
                break;

            ++evaluated;
            candidate.need = NeedFor(botAI, bot, listing.entry, listing.count);
            if (candidate.need != PricePolicy::BuyNeed::None)
                candidates.push_back(candidate);
        }

        visit.purchases = BotAuctionPolicy::SelectPurchases(candidates, visit.money, moneyNow, sPlayerbotAIConfig->auctionMaxPurchasesPerVisit);
        SF_LOG_INFO("playerbots", "[AUCTION] %s buy plan: %u listings read, %u needed, %u chosen", visit.name.c_str(), evaluated,
                    uint32(candidates.size()), uint32(visit.purchases.size()));
    }

    // One purchase: the listing is read again by id (never a stored pointer), checked against the plan and the wallet,
    // then bought out.
    void BuyNext(Player* bot, Creature* auctioneer, Visit& visit)
    {
        BotAuctionPolicy::Purchase const planned = visit.purchases[visit.nextPurchase++];
        std::string const label = "entry " + std::to_string(planned.entry) + " [" + ItemName(planned.entry) + "] x" + std::to_string(planned.count);

        AuctionHouseObject* house = sAuctionMgr->GetAuctionsMap(auctioneer->getFaction());
        AuctionEntry* auction = house->GetAuction(planned.id);
        if (!auction)
        {
            ++visit.result.refusals;
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip buy %s (listing %u): gone", visit.name.c_str(), label.c_str(), planned.id);
            return;
        }

        if (auction->itemEntry != planned.entry || auction->itemCount != planned.count || !auction->buyout ||
            auction->buyout > planned.buyout || auction->owner == bot->GetGUIDLow())
        {
            ++visit.result.refusals;
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip buy %s (listing %u): changed", visit.name.c_str(), label.c_str(), planned.id);
            return;
        }

        uint64 const price = auction->buyout;
        bool const ownerIsBot = sRandomPlayerbotMgr->IsRandomBot(auction->owner);
        if (!ReservePolicy::CanBuy(visit.money, bot->GetMoney(), price))
        {
            ++visit.result.refusals;
            SF_LOG_INFO("playerbots", "[AUCTION] %s skip buy %s (listing %u): %s is over the budget or the reserve", visit.name.c_str(),
                        label.c_str(), planned.id, Money(price).c_str());
            return;
        }

        AuctionBidOutcome outcome;
        AuctionBidResult const result = sAuctionMgr->PlaceBid(bot, house, planned.id, price, outcome);
        if (result == AuctionBidResult::Ok)
        {
            visit.money.wallet.committed += outcome.paid;
            ++visit.result.purchases;
            visit.result.purchasedCopper += outcome.paid;
            SF_LOG_INFO("playerbots", "[AUCTION] %s buy %s paid %s cap %s need %s from %s (listing %u)%s", visit.name.c_str(),
                        label.c_str(), Money(outcome.paid).c_str(), Money(uint64(planned.capUnit) * planned.count).c_str(),
                        NeedName(planned.need), ownerIsBot ? "bot" : "human", planned.id,
                        outcome.boughtOut ? "" : " BID ONLY, NOT A BUYOUT");
            if (!outcome.boughtOut)
                ++visit.result.errors;
            return;
        }

        ++visit.failures;
        ++visit.result.refusals;
        SF_LOG_INFO("playerbots", "[AUCTION] %s buy %s (listing %u) price %s refused: code %u", visit.name.c_str(), label.c_str(), planned.id,
                    Money(price).c_str(), uint32(result));
        if (result == AuctionBidResult::OwnAuction || result == AuctionBidResult::NotEnoughMoney)
        {
            ++visit.result.errors;
            visit.nextPurchase = visit.purchases.size();
        }
    }

    // ---------------------------------------------------------------------------------------------------------------
    // The visit's slices.
    // ---------------------------------------------------------------------------------------------------------------
    void End(Visit& visit, EndReason reason)
    {
        visit.done = true;
        visit.reason = reason;
    }

    void Advance(Visit& visit)
    {
        visit.phase = NextPhase(visit.phase, visit.kind);
        visit.waitMs = StepDelayMs(urand(0, STEP_MAX_MS));
    }

    // One operation of one visit. Sets visit.done when the visit is over.
    void Step(Visit& visit)
    {
        Player* bot = ObjectAccessor::FindPlayer(visit.botGuid);
        PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        if (!bot || !botAI || !bot->IsInWorld() || bot->IsBeingTeleported() || !bot->IsAlive() || bot->IsInCombat() ||
            bot->GetMapId() != visit.mapId)
        {
            End(visit, EndReason::Left);
            return;
        }

        Creature* auctioneer = nullptr;
        if (visit.kind == Kind::Auction)
        {
            auctioneer = bot->GetNPCIfCanInteractWith(visit.targetGuid, UNIT_NPC_FLAG_AUCTIONEER);
            if (!auctioneer)
            {
                End(visit, EndReason::Left);
                return;
            }
            if (!OwnHouse(bot, auctioneer))
            {
                End(visit, EndReason::WrongHouse);
                return;
            }
        }
        else
        {
            GameObject* mailbox = ObjectAccessor::GetGameObject(*bot, visit.targetGuid);
            if (!mailbox || !mailbox->IsInWorld() || bot->GetDistance(mailbox) > MAILBOX_YARDS)
            {
                End(visit, EndReason::Left);
                return;
            }
        }

        switch (visit.phase)
        {
            case Phase::Mail:
                if (MailboxNear(bot))
                    CollectMail(bot, visit);
                Advance(visit);
                break;

            case Phase::Snapshot:
            {
                AuctionHouseEntry const* houseEntry = sAuctionMgr->GetAuctionHouseEntry(auctioneer->getFaction());
                if (!houseEntry)
                {
                    ++visit.result.errors;
                    End(visit, EndReason::WrongHouse);
                    return;
                }

                visit.plan.rates = { houseEntry->depositPercent, houseEntry->cutPercent };
                visit.moneyOnArrival = bot->GetMoney();
                ReservePolicy::Reserve const reserve =
                    ReservePolicy::ComputeReserve(ReserveFactsOf(bot), ReserveParams(), visit.moneyOnArrival);
                visit.money = ReservePolicy::MakeVisitMoney(visit.moneyOnArrival, reserve);
                TakeSnapshot(bot, auctioneer, visit);
                PlanSales(bot, botAI, visit);
                SF_LOG_INFO("playerbots",
                            "[AUCTION] %s L%u visit: money %s reserve %s (repair %s goals %s held %s recipes %s) purchase budget %s deposit budget %s, "
                            "%u stacks, %u to list, %u of its own on the house, %u listings",
                            visit.name.c_str(), visit.level, Money(visit.moneyOnArrival).c_str(), Money(visit.money.reserve).c_str(),
                            Money(reserve.repair).c_str(), Money(reserve.goals.Total()).c_str(), Money(reserve.heldForGoals).c_str(),
                            Money(reserve.heldForRecipes).c_str(), Money(visit.money.wallet.budget).c_str(),
                            Money(visit.money.depositBudget).c_str(), uint32(visit.stacks.size()), uint32(visit.sales.size()),
                            visit.plan.activeListings, uint32(visit.snapshot.size()));
                Advance(visit);
                break;
            }

            case Phase::Sell:
                if (visit.nextSale >= visit.sales.size())
                {
                    Advance(visit);
                    break;
                }
                if (SellNext(bot, auctioneer, visit))
                    visit.waitMs = StepDelayMs(urand(0, STEP_MAX_MS));
                if (TooManyFailures(visit.failures) && !visit.done)
                    End(visit, EndReason::Failures);
                break;

            case Phase::Buy:
                if (!visit.buyPlanned)
                {
                    PlanPurchases(bot, botAI, visit);
                    visit.waitMs = StepDelayMs(urand(0, STEP_MAX_MS));
                    break;
                }
                if (visit.nextPurchase >= visit.purchases.size())
                {
                    Advance(visit);
                    break;
                }
                BuyNext(bot, auctioneer, visit);
                visit.waitMs = StepDelayMs(urand(0, STEP_MAX_MS));
                if (TooManyFailures(visit.failures) && !visit.done)
                    End(visit, EndReason::Failures);
                break;

            case Phase::Done:
                End(visit, EndReason::Finished);
                break;
        }

        if (!visit.done && visit.phase == Phase::Done)
            End(visit, EndReason::Finished);
    }

    void FinishVisit(Visit& visit)
    {
        visit.result.kind = visit.kind;
        visit.result.reason = visit.reason;
        Add(sTotals, visit.result);
        sTotalsDirty = true;
        VisitResult const& r = visit.result;
        SF_LOG_INFO("playerbots",
                    "[AUCTION] %s %s done (%s): listed %u (%s) deposits %s bought %u (%s) mail gold %s (%u sales) mail items %u blocked %u "
                    "refused %u errors %u in %u ms",
                    visit.name.c_str(), visit.kind == Kind::Auction ? "visit" : "mailbox visit", Name(visit.reason), r.listings,
                    Money(r.listedCopper).c_str(), Money(r.depositsCopper).c_str(), r.purchases, Money(r.purchasedCopper).c_str(),
                    Money(r.mailMoneyCopper).c_str(), r.mailMoneyMails, r.mailItems, r.mailBlocked, r.refusals, r.errors,
                    visit.elapsedMs);

        std::lock_guard<std::mutex> guard(sLock);
        BotRecord& record = sBots[visit.botGuid];
        record.busy = false;
        record.cancel = false;
        record.visited = true;
        record.lastEndMs = getMSTime();
    }

    bool CancelRequested(uint64 botGuid)
    {
        std::lock_guard<std::mutex> guard(sLock);
        auto const itr = sBots.find(botGuid);
        return itr != sBots.end() && itr->second.cancel;
    }

    void TakeRequests()
    {
        std::vector<Request> requests;
        {
            std::lock_guard<std::mutex> guard(sLock);
            requests.swap(sRequests);
        }

        for (Request const& request : requests)
        {
            Player* bot = ObjectAccessor::FindPlayer(request.bot);
            Visit visit;
            visit.botGuid = request.bot;
            visit.name = bot ? bot->GetName() : std::to_string(request.bot);
            visit.targetGuid = request.target;
            visit.mapId = request.mapId;
            visit.level = bot ? bot->getLevel() : 0;
            visit.kind = request.kind;
            visit.waitMs = FirstStepDelayMs(urand(0, FIRST_STEP_MAX_MS));
            sVisits.push_back(std::move(visit));
        }
    }

    // The core's auction script: every sale and expiry of a random bot's listing, whichever code path made it (a human
    // buying from the client is only seen here).
    class BotAuctionScript : public AuctionHouseScript
    {
    public:
        BotAuctionScript() : AuctionHouseScript("BotAuctionScript") {}

        void OnAuctionAdd(AuctionHouseObject*, AuctionEntry* entry) override { CacheListing(entry); }
        void OnAuctionRemove(AuctionHouseObject*, AuctionEntry* entry) override { RemoveListing(entry); }

        void OnAuctionSuccessful(AuctionHouseObject* /*ah*/, AuctionEntry* entry) override
        {
            if (!sPlayerbotAIConfig->auctionEnable || !entry || !sRandomPlayerbotMgr->IsRandomBot(entry->owner))
                return;

            bool const byHuman = entry->bidder && !sRandomPlayerbotMgr->IsRandomBot(entry->bidder);
            AddSale(sTotals, byHuman, entry->bid, entry->deposit, entry->GetAuctionCut());
            sTotalsDirty = true;
            std::string owner, bidder;
            sObjectMgr->GetPlayerNameByGUID(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER), owner);
            sObjectMgr->GetPlayerNameByGUID(MAKE_NEW_GUID(entry->bidder, 0, HIGHGUID_PLAYER), bidder);
            SF_LOG_INFO("playerbots", "[AUCTION] sale listing %u entry %u [%s] x%u owner %s (bot) bidder %s (%s) bid %s deposit %s cut %s",
                        entry->Id, entry->itemEntry, ItemName(entry->itemEntry).c_str(), entry->itemCount, owner.c_str(),
                        bidder.c_str(), byHuman ? "human" : "bot", Money(entry->bid).c_str(), Money(entry->deposit).c_str(),
                        Money(entry->GetAuctionCut()).c_str());
        }

        void OnAuctionExpire(AuctionHouseObject* /*ah*/, AuctionEntry* entry) override
        {
            if (!sPlayerbotAIConfig->auctionEnable || !entry || !sRandomPlayerbotMgr->IsRandomBot(entry->owner))
                return;
            // The core also reports a sold listing through this hook when loading expired auctions.
            if (entry->bidder)
            {
                OnAuctionSuccessful(nullptr, entry);
                return;
            }

            AddExpiry(sTotals);
            sTotalsDirty = true;
            std::string owner;
            sObjectMgr->GetPlayerNameByGUID(MAKE_NEW_GUID(entry->owner, 0, HIGHGUID_PLAYER), owner);
            SF_LOG_INFO("playerbots", "[AUCTION] expire listing %u entry %u [%s] x%u owner %s (bot) buyout %s deposit lost %s", entry->Id,
                        entry->itemEntry, ItemName(entry->itemEntry).c_str(), entry->itemCount, owner.c_str(),
                        Money(entry->buyout).c_str(), Money(entry->deposit).c_str());
        }
    };
}

// ---------------------------------------------------------------------------------------------------------------
// Map thread.
// ---------------------------------------------------------------------------------------------------------------
void BotAuction::AddRandomCharacter(uint32 guid)
{
    std::lock_guard<std::mutex> lock(sListingLock);
    sRandomCharacters.insert(guid);
}

std::vector<BotAuction::Listing> BotAuction::ActiveListings(uint32 guid)
{
    std::lock_guard<std::mutex> lock(sListingLock);
    auto found = sActiveListings.find(guid);
    if (found == sActiveListings.end()) return {};
    auto entries = found->second;
    time_t const now = time(nullptr);
    entries.erase(std::remove_if(entries.begin(), entries.end(), [now](Listing const& item)
        { return item.expiresAt <= now; }), entries.end());
    return entries;
}

void BotAuction::OnErrandArrival(PlayerbotAI* botAI, Player* bot, WorldObject* target, bool auctionHouse)
{
    if (!sPlayerbotAIConfig->auctionEnable || !target)
        return;

    Gate gate;
    gate.enabled = true;
    gate.randomBot = sRandomPlayerbotMgr->IsRandomBot(bot);
    gate.listed = NameAllowed(sPlayerbotAIConfig->auctionOnlyBots, bot->GetName());
    gate.humanLed = !BotSale::IsEligible(botAI, bot);
    gate.tableLoaded = PriceTable::Size() > 0;
    gate.cooldownMinutes = sPlayerbotAIConfig->auctionVisitCooldownMinutes;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(sLock);
    BotRecord& record = sBots[bot->GetGUID()];
    gate.busy = record.busy;
    // A mailbox is no reason to wait an hour: only the auction visit has a cooldown.
    gate.visitedBefore = auctionHouse && record.visited;
    gate.msSinceLastVisit = record.visited ? getMSTimeDiff(record.lastEndMs, now) : 0;

    Gating const gating = Check(gate);
    if (gating != Gating::Ok)
    {
        // Only the bots the soak names say why they stay out, at debug level.
        if (gate.listed && gate.randomBot && gating != Gating::Cooldown && gating != Gating::Busy)
            SF_LOG_DEBUG("playerbots", "[AUCTION] %s no visit: %s", bot->GetName().c_str(), Name(gating));
        return;
    }

    record.busy = true;
    record.cancel = false;
    Request request;
    request.bot = bot->GetGUID();
    request.target = target->GetGUID();
    request.mapId = bot->GetMapId();
    request.kind = auctionHouse ? Kind::Auction : Kind::MailboxOnly;
    sRequests.push_back(request);
}

bool BotAuction::SoakVisitDue(PlayerbotAI* botAI, Player* bot)
{
    if (!sPlayerbotAIConfig->auctionEnable || sPlayerbotAIConfig->auctionOnlyBots.empty())
        return false;

    Gate gate;
    gate.enabled = true;
    gate.randomBot = sRandomPlayerbotMgr->IsRandomBot(bot);
    gate.listed = NameAllowed(sPlayerbotAIConfig->auctionOnlyBots, bot->GetName());
    gate.humanLed = !BotSale::IsEligible(botAI, bot);
    gate.tableLoaded = PriceTable::Size() > 0;
    gate.cooldownMinutes = sPlayerbotAIConfig->auctionVisitCooldownMinutes;

    uint32 const now = getMSTime();
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sBots.find(bot->GetGUID());
    gate.busy = itr != sBots.end() && itr->second.busy;
    gate.visitedBefore = itr != sBots.end() && itr->second.visited;
    gate.msSinceLastVisit = gate.visitedBefore ? getMSTimeDiff(itr->second.lastEndMs, now) : 0;
    return SoakWantsVisit(true, Check(gate));
}

bool BotAuction::VisitWorking(Player* bot)
{
    if (!sPlayerbotAIConfig->auctionEnable)
        return false;

    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sBots.find(bot->GetGUID());
    return itr != sBots.end() && itr->second.busy;
}

void BotAuction::OnErrandEnd(Player* bot)
{
    std::lock_guard<std::mutex> guard(sLock);
    auto const itr = sBots.find(bot->GetGUID());
    if (itr != sBots.end() && itr->second.busy)
        itr->second.cancel = true;
}

// ---------------------------------------------------------------------------------------------------------------
// World thread.
// ---------------------------------------------------------------------------------------------------------------
void BotAuction::OnStartup()
{
    std::unordered_set<uint32> accounts(sPlayerbotAIConfig->randomBotAccounts.begin(), sPlayerbotAIConfig->randomBotAccounts.end());
    accounts.insert(sPlayerbotAIConfig->higherBatchAccounts.begin(), sPlayerbotAIConfig->higherBatchAccounts.end());
    std::string accountList;
    for (uint32 account : accounts)
    {
        if (!accountList.empty()) accountList += ",";
        accountList += std::to_string(account);
    }
    if (!accountList.empty())
        if (QueryResult result = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account IN (%s)", accountList.c_str()))
            do { AddRandomCharacter(result->Fetch()[0].GetUInt32()); } while (result->NextRow());

    // Discover each distinct house through its faction templates, including the neutral house.
    std::unordered_set<AuctionHouseObject*> houses;
    houses.insert(sAuctionMgr->GetAuctionsMap(0));
    for (uint32 i = 0; i < sFactionTemplateStore.GetNumRows(); ++i)
        if (sFactionTemplateStore.LookupEntry(i)) houses.insert(sAuctionMgr->GetAuctionsMap(i));
    for (AuctionHouseObject* house : houses)
    {
        std::vector<AuctionListing> listings;
        house->GetListings(listings);
        for (AuctionListing const& listing : listings)
            CacheListing(house->GetAuction(listing.id));
    }
    if (!sPlayerbotAIConfig->auctionEnable)
        return;

    // The policy's deposit and cut assume the default rates (docs/economy/auction-wiring.md section 4.3).
    float const deposit = sWorld->getRate(Rates::RATE_AUCTION_DEPOSIT);
    float const cut = sWorld->getRate(Rates::RATE_AUCTION_CUT);
    float const time = sWorld->getRate(Rates::RATE_AUCTION_TIME);
    SF_LOG_INFO("playerbots", "[AUCTION] ready: %u price ranges, core rates deposit %.2f cut %.2f time %.2f", PriceTable::Size(), deposit, cut, time);
    if (deposit != 1.0f || cut != 1.0f || time != 1.0f)
        SF_LOG_WARN("playerbots", "[AUCTION] Rate.Auction.Deposit, Cut or Time is not 1: the bots' deposit and cut arithmetic assumes 1");
}

void BotAuction::OnUpdate(uint32 diff)
{
    if (!sPlayerbotAIConfig->auctionEnable && !sPlayerbotAIConfig->bankErrandsEnable && !sConfigMgr->GetBoolDefault("AiPlayerbot.RecipeBuying.Enable", false))
        return;

    auto const tickStart = std::chrono::steady_clock::now();
    TakeRequests();

    uint32 slices = sPlayerbotAIConfig->auctionSlicesPerTick;
    for (Visit& visit : sVisits)
    {
        visit.elapsedMs += diff;
        if (CancelRequested(visit.botGuid))
            End(visit, EndReason::Cancelled);
        else if (TimedOut(visit.elapsedMs))
            End(visit, EndReason::TimedOut);
        else
        {
            visit.waitMs = visit.waitMs > diff ? visit.waitMs - diff : 0;
            if (!visit.waitMs && slices)
            {
                --slices;
                Step(visit);
            }
        }
    }

    for (Visit& visit : sVisits)
        if (visit.done)
            FinishVisit(visit);
    sVisits.erase(std::remove_if(sVisits.begin(), sVisits.end(), [](Visit const& visit) { return visit.done; }), sVisits.end());

    uint64 queue = 0;
    for (Visit const& visit : sVisits)
        if (!visit.waitMs)
            ++queue;
    {
        std::lock_guard<std::mutex> guard(sLock);
        queue += sRequests.size();
    }
    sInterval.queueDepth.Add(queue);
    sInterval.tickUs.Add(uint64(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - tickStart).count()));

    uint32 const now = getMSTime();
    if (!sSummaryStarted)
    {
        sSummaryStarted = true;
        sLastSummaryMs = now;
    }
    else if ((sTotalsDirty || [&]() { std::lock_guard<std::mutex> guard(sRecipeLock); return sRecipeCounters.bought || sRecipeCounters.budget || sRecipeCounters.limit || sBankCounters.visits; }()) && SummaryDue(getMSTimeDiff(sLastSummaryMs, now), sPlayerbotAIConfig->auctionSummaryMinutes))
        LogSummary("interval");
}

void BotAuction::RecordBank(uint32 deposited, uint32 withdrawn, bool full)
{
    std::lock_guard<std::mutex> guard(sRecipeLock);
    ++sBankCounters.visits;
    sBankCounters.deposited += deposited;
    sBankCounters.withdrawn += withdrawn;
    sBankCounters.full += full ? 1 : 0;
}

void BotAuction::RecordRecipe(uint32 bought, uint32 learned, uint32 budget, uint32 limit)
{
    std::lock_guard<std::mutex> guard(sRecipeLock);
    sRecipeCounters.bought += bought;
    sRecipeCounters.learned += learned;
    sRecipeCounters.budget += budget;
    sRecipeCounters.limit += limit;
}

void BotAuction::LogSummary(char const* why)
{
    if (!sPlayerbotAIConfig->auctionEnable && !sPlayerbotAIConfig->bankErrandsEnable && !sConfigMgr->GetBoolDefault("AiPlayerbot.RecipeBuying.Enable", false))
        return;

    RecipeCounters recipes;
    BankCounters bank;
    {
        std::lock_guard<std::mutex> guard(sRecipeLock);
        bank = sBankCounters;
        sBankCounters = BankCounters();
        recipes = sRecipeCounters;
        sRecipeCounters = RecipeCounters();
    }
    sLastSummaryMs = getMSTime();
    sTotalsDirty = false;
    SF_LOG_INFO("playerbots", "[AUCTION] summary (%s): %s tickAvgMs=%.3f tickMaxMs=%.3f tickSamples=%llu queueAvg=%.2f queueMax=%llu snapshotsAtCap=%llu snapshotsTruncated=%llu listingsOmitted=%llu mailBacklogMax=%llu recipeBought=%llu recipeLearned=%llu recipeSkippedBudget=%llu recipeSkippedLimit=%llu bankVisits=%llu bankDeposited=%llu bankWithdrawn=%llu bankFull=%llu",
                why, FormatTotals(sTotals).c_str(), sInterval.tickUs.Average() / 1000.0, double(sInterval.tickUs.max) / 1000.0,
                static_cast<unsigned long long>(sInterval.tickUs.count), sInterval.queueDepth.Average(),
                static_cast<unsigned long long>(sInterval.queueDepth.max), static_cast<unsigned long long>(sInterval.snapshotsAtCap),
                static_cast<unsigned long long>(sInterval.snapshotsTruncated), static_cast<unsigned long long>(sInterval.listingsOmitted),
                static_cast<unsigned long long>(sInterval.mailBacklogMax), static_cast<unsigned long long>(recipes.bought),
                static_cast<unsigned long long>(recipes.learned), static_cast<unsigned long long>(recipes.budget),
                static_cast<unsigned long long>(recipes.limit), static_cast<unsigned long long>(bank.visits),
                static_cast<unsigned long long>(bank.deposited), static_cast<unsigned long long>(bank.withdrawn),
                static_cast<unsigned long long>(bank.full));
    if (sInterval.snapshotsTruncated)
        SF_LOG_WARN("playerbots", "[AUCTION] snapshot truncation: %llu snapshots, %llu listings omitted in this interval",
                    static_cast<unsigned long long>(sInterval.snapshotsTruncated), static_cast<unsigned long long>(sInterval.listingsOmitted));
    sInterval = IntervalStats();
}

void AddSC_bot_auction()
{
    new BotAuctionScript();
}
