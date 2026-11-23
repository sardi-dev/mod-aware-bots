/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots craft their gear upgrades like a player (design decision 0021): when a recipe the bot knows makes an
 * upgrade for its spec and it carries the reagents and stands where the recipe can be made (anvil, forge, tools), it
 * crafts it; the item push then equips it. Nothing is handed out; the reagents are its own.
 */

#ifndef MOD_AWARE_BOTS_BOTCRAFTING_H
#define MOD_AWARE_BOTS_BOTCRAFTING_H

#include "Action.h"
#include "Define.h"

class Player;
class PlayerbotAI;

namespace BotCrafting
{
    // AiPlayerbot.AltBotsCraftGear (default 1).
    bool Enabled();

    // A known recipe that makes a gear upgrade for the bot's spec and can be cast right here, or 0.
    // The answer is kept for a while per bot, since checking every recipe is expensive.
    uint32 FindCraftableUpgrade(PlayerbotAI* botAI, Player* bot);

    // Forget the kept answer, after crafting.
    void Forget(Player* bot);
}

class CraftUpgradeAction : public Action
{
public:
    CraftUpgradeAction(PlayerbotAI* botAI) : Action(botAI, "craft upgrade") {}

    bool Execute(Event event) override;
};

#endif
