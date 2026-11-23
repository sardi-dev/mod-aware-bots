/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * Summons as a player gets them. A warlock's Ritual of Summoning and a meeting stone both send the target a summon
 * request (SMSG_SUMMON_REQUEST) that the player accepts or declines. A bot in the summoner's group accepts after a
 * short pause with a thank-you line, or declines with a reason when it is in a fight or dead; a stranger's summon is
 * declined. A bot that stands at a meeting stone with a group mate far away summons that mate itself, as a player
 * would. This replaces the instant teleport of a click on a stone (UseMeetingStoneAction).
 *
 * Config: AiPlayerbot.AcceptSummons.Enable, .DelayMinMs, .DelayMaxMs, .CombatWaitSeconds,
 * AiPlayerbot.StoneSummons.Enable, .CooldownSeconds (see conf/playerbots.conf.dist).
 */

#ifndef _PLAYERBOT_ACCEPTSUMMONACTION_H
#define _PLAYERBOT_ACCEPTSUMMONACTION_H

#include "MovementActions.h"

class Player;
class PlayerbotAI;

namespace AcceptSummon
{
    // AiPlayerbot.AcceptSummons.Enable
    bool Enabled();

    // AiPlayerbot.StoneSummons.Enable
    bool StoneSummonsEnabled();

    // True while the bot has a summon request it has not answered yet.
    bool HasPending(Player* bot);
}

// SMSG_SUMMON_REQUEST: checks the summoner and schedules the answer.
class AcceptSummonAction : public Action
{
public:
    AcceptSummonAction(PlayerbotAI* botAI) : Action(botAI, "accept summon") {}

    bool Execute(Event event) override;
};

// Answers a pending summon request once the pause is over (waits for a fight to end, up to a limit).
class CompleteSummonAction : public Action
{
public:
    CompleteSummonAction(PlayerbotAI* botAI) : Action(botAI, "complete summon") {}

    bool Execute(Event event) override;
};

// At a meeting stone with a group mate far away: walks up to the stone and summons the mate.
class StoneSummonAction : public MovementAction
{
public:
    StoneSummonAction(PlayerbotAI* botAI) : MovementAction(botAI, "stone summon") {}

    bool Execute(Event event) override;
};

#endif
