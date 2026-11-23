/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Alt bots learn their class spells the way a player does (design decision 0017): they walk to a class trainer
 * of their class when one is near, pay for what they can afford, and otherwise tell their master that they have spells
 * to train. Random bots keep learning their class spells on level-up (AutoMaintenanceOnLevelupAction), but learn
 * their professions the same paid way at a trade skill trainer they pass (BT-197).
 */

#ifndef MOD_AWARE_BOTS_BOTTRAINING_H
#define MOD_AWARE_BOTS_BOTTRAINING_H

#include "Define.h"

class Creature;
class Player;
class PlayerbotAI;

namespace BotTraining
{
    // AiPlayerbot.AltBotsVisitTrainers (default 1).
    bool Enabled();

    // AiPlayerbot.RandomBotsVisitTrainers (default 1): random bots learn their professions at trainers (BT-197).
    bool RandomBotsEnabled();

    // The setting that applies to this bot: RandomBotsEnabled for a random bot, Enabled for an alt.
    bool EnabledFor(Player* bot);

    // AiPlayerbot.AltBotTrainerRange: how far an alt walks to a trainer it can see, in yards (default 100).
    float Range();

    // Class spells the bot could learn now at a trainer of its class (level, prerequisites and known spells checked;
    // money is not).
    uint32 CountLearnable(Player* bot);

    // The nearest living class trainer of the bot's class (for a random bot also a trade skill trainer of its own
    // professions) within Range() that teaches it a spell it can learn now, or nullptr.
    Creature* FindNearbyTrainer(PlayerbotAI* botAI, Player* bot);

    // Forget the kept FindNearbyTrainer answer, after learning.
    void Forget(Player* bot);

    // Spells this trainer teaches the bot now (money not checked).
    uint32 CountLearnableAt(Player* bot, Creature* trainer);

    // Learns every spell the trainer teaches the bot now and the bot can afford, paying for each. Returns the count.
    uint32 LearnAffordable(PlayerbotAI* botAI, Player* bot, Creature* trainer);
}

#endif
