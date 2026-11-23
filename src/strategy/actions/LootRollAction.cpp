/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from AC mod-playerbots Ai/Base/Actions/LootRollAction.cpp for issue #188 (S6 of epic #163).
 *
 * Same shape as AC: LootRollAction::Execute() scans group->GetRolls() (a one-line accessor added to
 * Groups/Group.h, the same one AC's core has) and votes on every roll where the bot is still
 * NOT_EMITED_YET. It runs on the "very often" trigger like AC, plus the SMSG_LOOT_START_ROLL
 * packet trigger for an immediate first vote. An earlier version parsed SMSG_LOOT_START_ROLL
 * instead, but the bot's packet trigger keeps only the latest packet, so of two rolls started
 * together one never got a vote (#188 T15). AC's separate MasterLootRollAction is not ported:
 * master-loot rolls are in GetRolls() too, and like AC's LootRollAction this passes on them.
 *
 * Other 5.4.8 / this-fork adaptations vs AC:
 *  - "item usage" qualifier: chat->FormatItem / std::to_string are equivalent here (see
 *    ItemUsageValue.cpp's file header) - std::to_string is used directly, no ChatHelper dependency.
 *  - bot->getClass() -> bot->getClass() (CanBotUseToken).
 *  - ITEM_FLAG_UNIQUE_EQUIPPABLE -> ITEM_PROTO_FLAG_UNIQUE_EQUIPPED, no generic HasFlag() helper (see
 *    ItemUsageValue.cpp's file header) - direct bitwise test (RollUniqueCheck).
 *  - Group::CountRollVote() takes the player GUID, item GUID and RollType on SkyFire.
 *  - QueryItemUsageAction (AC's base class, a separate "query item usage" chat-info action with its
 *    own quest/price/quest-objective lookups) does not exist on this fork and was not ported for
 *    #188 (out of scope - it is its own chat command, not loot/gear behaviour) - LootRollAction
 *    inherits directly from Action instead.
 *  - RollAction (the "roll" dice-command action, unrelated to Group::RollId): AC resolves the chat
 *    parameter via chat->parseItems(link); this fork's ChatHelper has no such parser, so a small local
 *    ParseFirstItemId() (plain numeric id, or an "item:<id>" link segment) is used instead - the same
 *    approach as LootStrategyAction.cpp's ParseItemIds().
 */

#include "LootRollAction.h"

#include "LootEtiquetteActions.h"

#include "AiChat.h"
#include "DungeonChat.h"
#include "Event.h"
#include "Group.h"
#include "ItemUsageValue.h"
#include "LootAction.h"
#include "BotGuid.h"
#include "Config.h"
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "WorldPacket.h"

#include <cctype>
#include <cstring>

namespace
{
    // AiPlayerbot.LootOwnerFirst (default 1), read once like the other local settings (BotTraining.cpp).
    bool OwnerFirst()
    {
        static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.LootOwnerFirst", true);
        return enabled;
    }

    enum class OwnerChoice
    {
        NOT_INTERESTED,   // passed, greeded, cannot roll on it or cannot use it
        DECIDING,         // can use it and has not voted yet
        NEEDS
    };

    // Design decision 0021: the bot's human owner rolls first. Whether the item is an upgrade is the owner's call.
    OwnerChoice GetOwnerChoice(PlayerbotAI* botAI, Roll const* roll, ItemTemplate const* proto)
    {
        Player* owner = botAI->GetMaster();
        if (!owner || !botAI->HasRealPlayerMaster())
            return OwnerChoice::NOT_INTERESTED;

        auto const vote = roll->playerVote.find(owner->GetGUID());
        if (vote == roll->playerVote.end())
            return OwnerChoice::NOT_INTERESTED;

        if (vote->second == RollType::ROLL_NEED)
            return OwnerChoice::NEEDS;

        if (vote->second != RollType::MAX_ROLL_TYPE)
            return OwnerChoice::NOT_INTERESTED;

        return owner->CanUseItem(proto) == EQUIP_ERR_OK ? OwnerChoice::DECIDING : OwnerChoice::NOT_INTERESTED;
    }
}

bool LootRollAction::Execute(Event /*event*/)
{
    // 5.4.8: AC's shape - vote on every pending group roll (Group::GetRolls, the same accessor AC's
    // core has) instead of parsing SMSG_LOOT_START_ROLL. The bot's packet trigger keeps only the
    // latest packet, so two rolls started together lost a vote (#188 T15).
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Roll*> rolls = group->GetRolls();
    bool voted = false;
    for (Roll*& roll : rolls)
    {
        auto voteItr = roll->playerVote.find(bot->GetGUID());
        if (voteItr == roll->playerVote.end() || voteItr->second != RollType::MAX_ROLL_TYPE)
            continue;

        uint32 itemId = roll->itemid;
        int32 itemRandomPropId = roll->itemRandomPropId;
        uint32 itemRandomSuffix = roll->itemRandomSuffix;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            continue;

        int32 randomProperty = 0;
        if (itemRandomPropId)
            randomProperty = int32(itemRandomPropId);
        else if (itemRandomSuffix)
            randomProperty = -int32(itemRandomSuffix);

        std::string itemUsageParam;
        if (randomProperty != 0)
            itemUsageParam = std::to_string(itemId) + "," + std::to_string(randomProperty);
        else
            itemUsageParam = std::to_string(itemId);

        ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", itemUsageParam);

        RollType vote = RollType::ROLL_PASS;

        // Armor Tokens are classed as MISC JUNK (Class 15, Subclass 0), luckily no other items I found have class bits and epic quality.
        if (proto->Class == ITEM_CLASS_MISCELLANEOUS && proto->SubClass == ITEM_SUBCLASS_JUNK && proto->Quality == ITEM_QUALITY_EPIC)
        {
            if (CanBotUseToken(proto, bot))
                vote = RollType::ROLL_NEED;  // Eligible for "Need"
            else
                vote = RollType::ROLL_GREED;  // Not eligible, so "Greed"
        }
        else if (usage == ITEM_USAGE_DISENCHANT)
            vote = sPlayerbotAIConfig->lootRollDisenchant ? RollType::ROLL_DISENCHANT : RollType::ROLL_GREED;
        else
        {
            switch (proto->Class)
            {
                case ITEM_CLASS_WEAPON:
                case ITEM_CLASS_ARMOR:
                    if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BAD_EQUIP)
                        vote = RollType::ROLL_NEED;
                    else if (usage != ITEM_USAGE_NONE)
                        vote = RollType::ROLL_GREED;
                    break;
                case ITEM_CLASS_RECIPE:
                    if (!sPlayerbotAIConfig->lootRollRecipe)
                        vote = RollType::ROLL_PASS;
                    else if (usage == ITEM_USAGE_SKILL)
                        vote = RollType::ROLL_NEED;  // Bot can learn this recipe
                    else if (proto->Bonding != BIND_ON_ACQUIRE)
                        vote = RollType::ROLL_GREED;  // BoE recipe bot can't learn - GREED for AH/trade
                    break;
                default:
                    if (StoreLootAction::IsLootAllowed(itemId, botAI))
                        vote = CalculateRollVote(proto, usage);
                    break;
            }
        }

        if (vote == RollType::ROLL_NEED)
        {
            if (sPlayerbotAIConfig->lootNeedRollLevel == 0 || RollUniqueCheck(proto, bot))
                vote = RollType::ROLL_PASS;
            else if (sPlayerbotAIConfig->lootNeedRollLevel == 1)
                vote = RollType::ROLL_GREED;
        }
        else if (vote == RollType::ROLL_GREED && !sPlayerbotAIConfig->lootGreedRollLevel)
            vote = RollType::ROLL_PASS;

        // What the bot wanted before it gave way to its owner, for the AI layer's memory (AiChat::NoteLootRoll).
        bool const wantedNeed = vote == RollType::ROLL_NEED;

        // The owner first: the bot needs only what its owner passed on or cannot use, and waits while the owner decides
        // (this action runs again shortly).
        if (vote == RollType::ROLL_NEED && OwnerFirst())
        {
            OwnerChoice const owner = GetOwnerChoice(botAI, roll, proto);
            if (owner == OwnerChoice::DECIDING)
            {
                LootEtiquette::AskAnyoneNeeds(botAI, proto);
                continue;
            }
            if (owner == OwnerChoice::NEEDS)
                vote = RollType::ROLL_GREED;
        }

        // Master loot / free-for-all: nothing to roll for, always pass (matches AC).
        switch (group->GetLootMethod())
        {
            case LootMethod::MASTER_LOOT:
            case LootMethod::FREE_FOR_ALL:
                vote = RollType::ROLL_PASS;
                break;
            default:
                break;
        }

        if (!(roll->rollVoteMask & (1u << uint8(vote))))
            vote = (sPlayerbotAIConfig->lootGreedRollLevel && (roll->rollVoteMask & uint8(RollMask::ROLL_FLAG_TYPE_GREED))) ? RollType::ROLL_GREED : RollType::ROLL_PASS;

        // Noted before the vote: the last vote ends the roll and deletes it.
        AiChat::NoteLootRoll(bot, roll, proto, wantedNeed, vote == RollType::ROLL_NEED);
        group->CountRollVote(bot->GetGUID(), roll->itemGUID, vote);
        DungeonChat::OnLootRoll(botAI, proto, uint32(vote));
        voted = true;
    }

    return voted;
}

RollType LootRollAction::CalculateRollVote(ItemTemplate const* proto, ItemUsage usage)
{
    if (usage == ITEM_USAGE_NONE)
        usage = AI_VALUE2(ItemUsage, "item usage", std::to_string(proto->ItemId));

    RollType needVote = RollType::ROLL_PASS;
    switch (usage)
    {
        case ITEM_USAGE_EQUIP:
        case ITEM_USAGE_REPLACE:
        case ITEM_USAGE_GUILD_TASK:
        case ITEM_USAGE_BAD_EQUIP:
            needVote = RollType::ROLL_NEED;
            break;
        case ITEM_USAGE_SKILL:
        case ITEM_USAGE_USE:
        case ITEM_USAGE_AH:
        case ITEM_USAGE_VENDOR:
            needVote = RollType::ROLL_GREED;
            break;
        case ITEM_USAGE_DISENCHANT:
            needVote = sPlayerbotAIConfig->lootRollDisenchant ? RollType::ROLL_DISENCHANT : RollType::ROLL_GREED;
            break;
        default:
            break;
    }

    return StoreLootAction::IsLootAllowed(proto->ItemId, botAI) ? needVote : RollType::ROLL_PASS;
}

bool CanBotUseToken(ItemTemplate const* proto, Player* bot)
{
    // Get the bitmask for the bot's class
    uint32 botClassMask = (1 << (bot->getClass() - 1));

    // Check if the bot's class is allowed to use the token
    if (proto->AllowableClass & botClassMask)
        return true;  // Bot's class is eligible to use this token

    return false;  // Bot's class cannot use this token
}

bool RollUniqueCheck(ItemTemplate const* proto, Player* bot)
{
    // Count the total number of the item (equipped + in bags)
    uint32 totalItemCount = bot->GetItemCount(proto->ItemId, true);

    // Count the number of the item in bags only
    uint32 bagItemCount = bot->GetItemCount(proto->ItemId, false);

    // Determine if the unique item is already equipped
    bool isEquipped = (totalItemCount > bagItemCount);
    // 5.4.8: ITEM_FLAG_UNIQUE_EQUIPPABLE -> ITEM_PROTO_FLAG_UNIQUE_EQUIPPED, no generic HasFlag()
    // helper on this fork's ItemTemplate (see file header).
    bool const isUniqueEquippable = (proto->Flags & ITEM_PROTO_FLAG_UNIQUE_EQUIPPED) != 0;
    if (isEquipped && isUniqueEquippable)
        return true;  // Unique Item is already equipped
    else if (isUniqueEquippable && (bagItemCount > 1))
        return true;  // Unique item already in bag, don't roll for it
    return false;  // Item is not equipped or in bags, roll for it
}

namespace
{
// 5.4.8: Player has no DoRandomRoll; run the core's own CMSG_RANDOM_ROLL handler (max, min, unk).
void DoRandomRoll(Player* bot, uint32 minimum, uint32 maximum)
{
    WorldPacket packet(CMSG_RANDOM_ROLL, 4 + 4 + 1);
    packet << uint32(maximum) << uint32(minimum) << uint8(0);
    bot->GetSession()->HandleRandomRollOpcode(packet);
}

// 5.4.8: see the RollAction note in the file header.
uint32 ParseFirstItemId(std::string const& text)
{
    size_t pos = text.find("item:");
    if (pos != std::string::npos)
    {
        pos += strlen("item:");
        size_t end = pos;
        while (end < text.size() && isdigit((unsigned char)text[end]))
            ++end;
        if (end > pos)
            return uint32(atoi(text.substr(pos, end - pos).c_str()));
        return 0;
    }

    bool allDigits = !text.empty();
    for (char c : text)
        if (!isdigit((unsigned char)c))
        {
            allDigits = false;
            break;
        }

    return allDigits ? uint32(atoi(text.c_str())) : 0;
}
}

bool RollAction::Execute(Event event)
{
    std::string link = event.getParam();

    if (link.empty())
    {
        DoRandomRoll(bot, 0, 100);
        return false;
    }

    uint32 itemId = ParseFirstItemId(link);
    if (!itemId)
        return false;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return false;

    ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", std::to_string(itemId));
    switch (proto->Class)
    {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
            if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE || usage == ITEM_USAGE_BAD_EQUIP)
            {
                DoRandomRoll(bot, 0, 100);
            }
    }
    return true;
}
