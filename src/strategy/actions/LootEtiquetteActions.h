/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Loot manners of a bot in a group with a player, spoken through the bot voice (BotVoice.h):
 *  - congratulates a group mate who won a rare or better item in a roll;
 *  - asks "anyone need this?" before it needs a big upgrade the player could use as well (the bot waits for the
 *    player's vote first, see LootRollAction);
 *  - offers an item it won but cannot use to the player when the player can use it, and trades it over when the
 *    player answers "me" (or similar) in chat.
 *
 * Config: AiPlayerbot.LootEtiquette.Enable, .GratsChance, .OfferSeconds, .AskQuality (see conf/playerbots.conf.dist).
 */

#ifndef _PLAYERBOT_LOOTETIQUETTEACTIONS_H
#define _PLAYERBOT_LOOTETIQUETTEACTIONS_H

#include "Action.h"

class Player;
class PlayerbotAI;
struct ItemTemplate;

namespace LootEtiquette
{
    // AiPlayerbot.LootEtiquette.Enable
    bool Enabled();

    // The bot is about to wait for the player's vote on an item it would need: asks once whether anyone needs it.
    void AskAnyoneNeeds(PlayerbotAI* botAI, ItemTemplate const* proto);

    // A trade the bot opened for an offer is open: puts the offered item in. True when it did (the inventory list the
    // trade window normally gets is then skipped).
    bool PlaceOffer(PlayerbotAI* botAI, Player* trader);

    // True for the trade of an offered item to the player, which the bot accepts as a free gift.
    bool IsGift(PlayerbotAI* botAI, Player* trader);
}

// SMSG_LOOT_ROLL_WON: congratulates a mate, or offers the bot's own win to the player.
class LootRollWonChatAction : public Action
{
public:
    LootRollWonChatAction(PlayerbotAI* botAI) : Action(botAI, "loot roll won chat") {}

    bool Execute(Event event) override;
};

// "me", "mine", "i do", "i need": the player takes up an offer, the bot opens a trade.
class AcceptLootOfferAction : public Action
{
public:
    AcceptLootOfferAction(PlayerbotAI* botAI) : Action(botAI, "accept loot offer") {}

    bool Execute(Event event) override;
};

#endif
