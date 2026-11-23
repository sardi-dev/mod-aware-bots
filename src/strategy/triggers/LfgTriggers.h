/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/Base/Trigger/LfgTriggers.h) into the
 * Legends of Azeroth Pandaria fork (#210 S10). See LfgTriggers.cpp for the port notes.
 */

#ifndef _PLAYERBOT_LFGTRIGGERS_H
#define _PLAYERBOT_LFGTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// AC LfgTriggers.h:14-20. Dormant on both cores (nothing ever sets "lfg proposal" to a non-zero
// value outside LfgAcceptAction's own dead-code symmetry branch - see that file's header comment),
// kept only for AC parity and any future caller (e.g. a harness verb) that stores a proposal id
// through the value instead of a packet.
class LfgProposalActiveTrigger : public Trigger
{
public:
    LfgProposalActiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg proposal active", 20 * 2000) {}

    bool IsActive() override;
};

// AC LfgTriggers.h:22-28: the master is inside a dungeon instance the bot does not recognise (it
// followed in without ever getting invited to that group's own LFG proposal, e.g. an alt-tabbed
// manual group). Schedules "give leader in dungeon" via the (not-yet-AiFactory-wired, #142) "lfg"
// strategy.
class UnknownDungeonTrigger : public Trigger
{
public:
    UnknownDungeonTrigger(PlayerbotAI* botAI) : Trigger(botAI, "unknown dungeon", 20 * 2000) {}

    bool IsActive() override;
};

// Beyond AC: the bot's finder group is in its dungeon run but the bot is not inside and could ask for the teleport now (it
// was fighting, falling or dead when the proposal closed). Checked every five seconds (LfgTeleportRetryAction).
class LfgTeleportDueTrigger : public Trigger
{
public:
    LfgTeleportDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg teleport due", 5) {}

    bool IsActive() override;
};

// Beyond AC: a random bot the Dungeon Finder put in a group has a line to say or the dungeon behind it (see
// LfgMannersAction.h). Checked every two seconds, so the lines come at human pace.
class LfgMannersTrigger : public Trigger
{
public:
    LfgMannersTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg manners", 2) {}

    bool IsActive() override;
};

// Beyond AC: a bot of a bot-only finder run has a step to take through the dungeon (see LfgCrawlAction.h). Checked every
// tick (the cheap group test comes first), so a leader that waits is never handed to the default roaming action.
class LfgCrawlTrigger : public Trigger
{
public:
    LfgCrawlTrigger(PlayerbotAI* botAI) : Trigger(botAI, "lfg crawl") {}

    bool IsActive() override;
};

#endif
