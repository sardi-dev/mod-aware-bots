/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Bot-led dungeons: how a group of random bots with no player in it (a bot-only finder run, LfgFill.h) works through a
 * dungeon. The tank leads (a tank that is not the leader takes the lead through the world thread), the others follow it
 * through the follow strategy they already have. The leader walks, over the instance's navmesh, to the nearest living
 * boss of the dungeon (the encounters of `instance_encounters`, at their creature spawns), the final boss last. On the
 * way it pulls one pack at a time: it waits for the group's health, mana and closeness before every pull and before the
 * walk goes on, so nothing is chain-pulled. A boss counts as dead when the instance's encounter mask says so or the
 * leader sees the corpse. A boss it cannot reach, or one that has no spawn (a script summons it), is skipped. The run
 * ends the way every finder run does: the core marks the group finished with the final boss and the bots leave
 * (LfgMannersAction), or the bot-only run's time limit wraps it up.
 *
 * It only acts for the maps in AiPlayerbot.LfgCrawl.Maps (the finder dungeons below level 60 that it can walk, by default; a wing map such as Maraudon only
 * uses the bosses of the run's own wing) and
 * only for the bots of a bot-only run or of a party the bots formed by hand (BotLfm) that has walked in, in both cases
 * with no player in the group. Per-map event steps (LfgCrawlAction.cpp: EventSteps, EventBosses, BossOrder, PassDoors) let the leader use
 * an object, answer a creature's gossip and walk with it, stand by a creature, clear around an object, take a slipstream, wait for
 * an event-summoned boss and open doors, which the dungeons with events need (AiPlayerbot.LfgCrawl.EventSteps). A party keeps the leader BotLfm gave it (it prefers a tank), and since the core does not
 * know its group as a finder group, the leader notes the final boss's death (PartyFinished) and the bots leave. Settings are AiPlayerbot.LfgCrawl.* (conf/playerbots.conf.dist).
 */

#ifndef _PLAYERBOT_LFGCRAWLACTION_H
#define _PLAYERBOT_LFGCRAWLACTION_H

#include "AttackActions.h"

class Player;
class PlayerbotAI;

namespace LfgCrawl
{
    // The trigger's question: does this bot have a step to take now (lead the way, or take the lead).
    bool Wants(PlayerbotAI* botAI, Player* bot);

    // A party the bots formed by hand (BotLfm) has killed its dungeon's final boss: its members leave (LfgMannersAction).
    bool PartyFinished(uint64 groupGuid);
}

// "lfg crawl": the leader's next step through the dungeon, or a tank taking the lead.
class LfgCrawlAction : public AttackAction
{
public:
    LfgCrawlAction(PlayerbotAI* botAI) : AttackAction(botAI, "lfg crawl") {}

    bool Execute(Event event) override;
};

#endif
