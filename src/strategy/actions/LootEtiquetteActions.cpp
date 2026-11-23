/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See LootEtiquetteActions.h.
 *
 * The packet layout of SMSG_LOOT_ROLL_WON is the writer in Group::SendLootRollWon (Group.cpp): two guids in masks and
 * byte sequences, with the item id, the roll type and the count among the bytes.
 */

#include "LootEtiquetteActions.h"

#include "AiChat.h"
#include "BotVoice.h"
#include "Config.h"
#include "Event.h"
#include "ItemUsageValue.h"
#include "Playerbots.h"
#include "Timer.h"
#include "TradeData.h"
#include "WorldPacket.h"

#include <mutex>
#include <unordered_map>

namespace
{
    struct EtiquetteSettings
    {
        bool enabled;
        uint32 gratsChance;
        uint32 offerMs;
        uint32 askQuality;
    };

    EtiquetteSettings const& Settings()
    {
        static EtiquetteSettings const settings = []
        {
            EtiquetteSettings s;
            s.enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.LootEtiquette.Enable", true);
            s.gratsChance = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.LootEtiquette.GratsChance", 40));
            s.offerMs = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.LootEtiquette.OfferSeconds", 60)) * IN_MILLISECONDS;
            s.askQuality = uint32(sConfigMgr->GetIntDefault("AiPlayerbot.LootEtiquette.AskQuality", ITEM_QUALITY_RARE));
            return s;
        }();
        return settings;
    }

    // Congratulations are for rare items and better.
    uint32 const GRATS_QUALITY = ITEM_QUALITY_RARE;
    // An offer is made for gear of at least this quality.
    uint32 const OFFER_QUALITY = ITEM_QUALITY_UNCOMMON;
    uint32 const FULL_CHANCE = 100;
    size_t const TRIM_SIZE = 256;

    struct Offer
    {
        uint32 itemId = 0;
        uint32 offeredMs = 0;
        bool tradeRequested = false;
        bool itemPlaced = false;
    };

    std::mutex lock;
    std::unordered_map<uint32, Offer> offers;                 // bot guid -> its open offer
    std::unordered_map<uint64, uint32> asked;                 // bot guid and item -> when it asked "anyone need this?"

    uint64 AskKey(Player* bot, uint32 itemId) { return (uint64(bot->GetGUIDLow()) << 32) | itemId; }

    bool OfferFresh(Offer const& offer) { return getMSTimeDiff(offer.offeredMs, getMSTime()) < Settings().offerMs; }

    // Armor and weapons the player could wear or wield.
    bool PlayerCouldUse(Player* player, ItemTemplate const* proto)
    {
        if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
            return false;

        return proto->Quality >= OFFER_QUALITY && player->CanUseItem(proto) == EQUIP_ERR_OK;
    }
}

namespace LootEtiquette
{
    bool Enabled() { return Settings().enabled; }

    void AskAnyoneNeeds(PlayerbotAI* botAI, ItemTemplate const* proto)
    {
        if (!Settings().enabled || !proto || proto->Quality < Settings().askQuality)
            return;

        Player* bot = botAI->GetBot();
        uint32 const now = getMSTime();
        {
            std::lock_guard<std::mutex> guard(lock);
            auto itr = asked.find(AskKey(bot, proto->ItemId));
            if (itr != asked.end() && getMSTimeDiff(itr->second, now) < Settings().offerMs)
                return;

            asked[AskKey(bot, proto->ItemId)] = now;
            if (asked.size() >= TRIM_SIZE)
                asked.clear();
        }

        BotVoice::Say(botAI, "voice_loot_anyone", "Anyone need %item? I'll wait for your roll",
                      { { "%item", proto->Name1 } }, BotVoice::Channel::PARTY, BotVoice::Priority::CHATTER, FULL_CHANCE);
    }

    bool PlaceOffer(PlayerbotAI* botAI, Player* trader)
    {
        Player* bot = botAI->GetBot();
        uint32 itemId = 0;
        {
            std::lock_guard<std::mutex> guard(lock);
            auto itr = offers.find(bot->GetGUIDLow());
            if (itr == offers.end() || !itr->second.tradeRequested || itr->second.itemPlaced || !OfferFresh(itr->second) ||
                trader != botAI->GetMaster())
                return false;

            itemId = itr->second.itemId;
            itr->second.itemPlaced = true;
        }

        Event event("t", "item:" + std::to_string(itemId), trader);
        if (!botAI->DoSpecificAction("trade", event, true))
        {
            std::lock_guard<std::mutex> guard(lock);
            offers.erase(bot->GetGUIDLow());
            return false;
        }

        BotVoice::Say(botAI, "voice_loot_given", "Here you go", {}, BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
        return true;
    }

    bool IsGift(PlayerbotAI* botAI, Player* trader)
    {
        Player* bot = botAI->GetBot();
        uint32 itemId = 0;
        {
            std::lock_guard<std::mutex> guard(lock);
            auto itr = offers.find(bot->GetGUIDLow());
            if (itr == offers.end() || !itr->second.itemPlaced || trader != botAI->GetMaster())
                return false;

            itemId = itr->second.itemId;
        }

        TradeData* mine = bot->GetTradeData();
        TradeData* theirs = trader->GetTradeData();
        if (!mine || !theirs || theirs->GetMoney())
            return false;

        bool offered = false;
        for (uint8 slot = 0; slot < TRADE_SLOT_TRADED_COUNT; ++slot)
        {
            if (theirs->GetItem(TradeSlots(slot)))
                return false;

            Item* item = mine->GetItem(TradeSlots(slot));
            if (item && item->GetEntry() == itemId)
                offered = true;
        }

        if (offered)
        {
            std::lock_guard<std::mutex> guard(lock);
            offers.erase(bot->GetGUIDLow());
        }

        return offered;
    }
}

bool LootRollWonChatAction::Execute(Event event)
{
    EtiquetteSettings const& settings = Settings();
    WorldPacket p = BotFreshPacket(event.getPacket());
    p.rpos(0);

    BotGuid winner;
    BotGuid looted;
    winner[0] = p.ReadBit();
    looted[3] = p.ReadBit();
    p.ReadBits(2);
    winner[3] = p.ReadBit();
    winner[2] = p.ReadBit();
    winner[6] = p.ReadBit();
    winner[7] = p.ReadBit();
    p.ReadBit();
    looted[7] = p.ReadBit();
    looted[1] = p.ReadBit();
    winner[4] = p.ReadBit();
    winner[1] = p.ReadBit();
    looted[4] = p.ReadBit();
    winner[5] = p.ReadBit();
    p.ReadBits(3);
    looted[0] = p.ReadBit();
    looted[2] = p.ReadBit();
    p.ReadBit();
    p.ReadBit();
    looted[6] = p.ReadBit();
    looted[5] = p.ReadBit();

    p.read_skip<uint32>();  // always 0
    p.read_skip<uint32>();  // display id
    p.ReadByteSeq(winner[2]);
    p.ReadByteSeq(looted[7]);
    p.read_skip<uint32>();  // random property
    p.ReadByteSeq(winner[5]);
    p.ReadByteSeq(looted[3]);
    p.ReadByteSeq(winner[7]);
    p.ReadByteSeq(looted[1]);
    p.ReadByteSeq(looted[2]);
    p.ReadByteSeq(looted[0]);
    p.ReadByteSeq(winner[3]);
    p.read_skip<int32>();   // the winning roll
    p.read_skip<uint32>();  // random suffix
    p.ReadByteSeq(looted[6]);
    p.ReadByteSeq(winner[1]);
    p.ReadByteSeq(winner[4]);
    uint8 const rollType = p.read<uint8>();
    p.ReadByteSeq(winner[6]);
    p.read_skip<uint8>();   // item slot
    p.ReadByteSeq(looted[4]);
    uint32 const itemId = p.read<uint32>();
    p.ReadByteSeq(looted[5]);
    p.ReadByteSeq(winner[0]);

    // Read whatever the etiquette says: the AI layer's memory notes passed and ninja loot (AiChat::OnLootRollWon).
    AiChat::OnLootRollWon(bot, uint64(winner), itemId, rollType);
    if (!settings.enabled)
        return false;

    if (rollType != uint8(RollType::ROLL_NEED) && rollType != uint8(RollType::ROLL_GREED))
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    Group* group = bot->GetGroup();
    if (!proto || !group || !botAI->HasRealPlayerMaster())
        return false;

    if (uint64(winner) != bot->GetGUID())
    {
        // A mate's win: a short word for a rare or better item.
        Player* mate = ObjectAccessor::FindPlayer(winner);
        if (!mate || !group->IsMember(mate->GetGUID()) || proto->Quality < GRATS_QUALITY)
            return true;

        std::map<std::string, std::string> const placeholders{
            { "%name", mate->GetName() }, { "%item", proto->Name1 } };
        // Design decision 0031: a human's rare or epic win is an opener moment, its words from the AI layer when it is on.
        if (mate->GetSession() && !mate->GetSession()->IsBot())
        {
            std::string const kind = proto->Quality >= ITEM_QUALITY_EPIC ? ", an epic item," : ", a rare item,";
            BotVoice::SayToHuman(botAI, "voice_loot_grats", "Grats %name on %item!", placeholders, mate,
                mate->GetName() + " just won " + proto->Name1 + kind + " in a loot roll. Congratulate them.",
                settings.gratsChance);
        }
        else
            BotVoice::Say(botAI, "voice_loot_grats", "Grats %name on %item!", placeholders, BotVoice::Channel::PARTY,
                          BotVoice::Priority::CHATTER, settings.gratsChance);
        return true;
    }

    // The bot's own win: an item it cannot use that the player can is offered to the player.
    Player* master = botAI->GetMaster();
    if (!sRandomPlayerbotMgr->IsRandomBot(bot) || !group->IsMember(master->GetGUID()) || !PlayerCouldUse(master, proto))
        return false;

    ItemUsage const usage = AI_VALUE2(ItemUsage, "item usage", std::to_string(itemId));
    if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BAD_EQUIP ||
        usage == ITEM_USAGE_SKILL || usage == ITEM_USAGE_USE || usage == ITEM_USAGE_GUILD_TASK)
        return false;

    {
        std::lock_guard<std::mutex> guard(lock);
        Offer offer;
        offer.itemId = itemId;
        offer.offeredMs = getMSTime();
        offers[bot->GetGUIDLow()] = offer;
        if (offers.size() >= TRIM_SIZE)
            offers.clear();
    }

    BotVoice::Say(botAI, "voice_loot_offer", "I won %item but can't use it. Say me if you want it",
                  { { "%item", proto->Name1 } }, BotVoice::Channel::PARTY, BotVoice::Priority::NOTICE);
    SF_LOG_DEBUG("playerbots", "[LOOT] %s offers %s to %s", bot->GetName().c_str(), proto->Name1.c_str(),
                 master->GetName().c_str());
    return true;
}

bool AcceptLootOfferAction::Execute(Event event)
{
    Player* asker = event.getOwner();
    if (!Settings().enabled || !asker || asker != botAI->GetMaster())
        return false;

    {
        std::lock_guard<std::mutex> guard(lock);
        auto itr = offers.find(bot->GetGUIDLow());
        if (itr == offers.end() || itr->second.tradeRequested || !OfferFresh(itr->second))
            return false;

        itr->second.tradeRequested = true;
    }

    // Opens the trade; the item goes in when the player has opened the window (TradeStatusAction -> PlaceOffer).
    if (!botAI->DoSpecificAction("trade", Event("t", "", asker), true))
    {
        std::lock_guard<std::mutex> guard(lock);
        offers.erase(bot->GetGUIDLow());
        return false;
    }

    return true;
}
