/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Names the newer mod-playerbots raid code uses that the SkyFire 5.4.8 core spells differently: the raid difficulty
 * enum (the core's DifficultyID has the same values) and AC's IsSelfBot (Bot/PlayerbotAI.cpp).
 */

#ifndef PLAYERBOTS_RAIDCOMPAT_H
#define PLAYERBOTS_RAIDCOMPAT_H

#include "DBCEnums.h"
#include "Player.h"
#include "Playerbots.h"

using Difficulty = DifficultyID;
constexpr DifficultyID RAID_DIFFICULTY_10MAN_NORMAL = DIFFICULTY_10MAN_NORMAL;
constexpr DifficultyID RAID_DIFFICULTY_25MAN_NORMAL = DIFFICULTY_25MAN_NORMAL;
constexpr DifficultyID RAID_DIFFICULTY_10MAN_HEROIC = DIFFICULTY_10MAN_HEROIC;
constexpr DifficultyID RAID_DIFFICULTY_25MAN_HEROIC = DIFFICULTY_25MAN_HEROIC;

// The difficulty of the raid the bot stands in. AC reads Player::GetRaidDifficulty, the player's preferred setting;
// the map's own difficulty is the one the boss scripts use.
inline Difficulty BotRaidDifficulty(Player* bot)
{
    return bot->GetMap()->GetDifficulty();
}

// A bot that is its own master (an alt bot its owner controls through itself)
inline bool IsSelfBot(Player* player)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(player);
    return botAI && botAI->GetMaster() == player;
}

#endif
