/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_BOTSKILL_H
#define _PLAYERBOT_BOTSKILL_H

#include "BotSkillMath.h"
#include "Define.h"

class Player;

// Skill tiers of the random bots (owner, 2026-11-10): a few play perfectly, most play well, some are casual, in the
// world and in the owner's own groups alike. The tier adds a reaction delay to every decision and tells a class
// strategy which lines of its class sheet the bot uses. Alt bots are left as they are; battlegrounds play the tiers too
// (BT-153, owner 2026-11-12); in a match of the arena ladder (Arena/ArenaLadder.h) a bot plays at its team's tier
// (BT-154), other arena bots are left as they are.
namespace BotSkill
{
    BotSkillMath::Tier TierOf(Player* bot);

    // True when a bot of this tier plays a class sheet line marked for `line` (core lines are CASUAL).
    bool Uses(Player* bot, BotSkillMath::Tier line);

    // The extra time this bot takes before its next decision, in milliseconds (0 for an expert at the defaults).
    uint32 ExtraReactionMs(Player* bot);
}

#endif
