/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/Base/Actions/LfgActions.h) into the
 * Legends of Azeroth Pandaria fork (#210 S10 G1-G3). See LfgActions.cpp for the full port notes.
 * #142: LfgJoinAction/LfgLeaveAction (AC's random-bot self-queue, Ai/Base/Actions/LfgActions.h:14-56) are ported below.
 */

#ifndef _PLAYERBOT_LFGACTIONS_H
#define _PLAYERBOT_LFGACTIONS_H

#include "Action.h"

#include <set>

class Player;
class PlayerbotAI;

// The one LFG role (tank, healer or damage) a random bot queues as, from its specialization.
uint32 LfgGetBotRoles(Player* bot);

// Queues a bot for the dungeons now; world thread only (LFGMgr has no locking). False when JoinLfg refused it.
bool LfgQueueBotNow(Player* bot, uint32 roleMask, std::set<uint32> const& dungeons);

// #142 (AC LfgActions.h:14-25): "lfg join" - a random bot queues itself for the dungeons real players
// are queued for (RandomPlayerbotMgr::LfgDungeons). AC derives it from InventoryAction (only for a
// commented-out gear-score block) and derives LfgAccept/LfgRoleCheck from it to share GetRoles(); this
// fork keeps those two as plain Actions with the free GetLfgRoles() in LfgActions.cpp.
class LfgJoinAction : public Action
{
public:
    LfgJoinAction(PlayerbotAI* botAI, std::string const name = "lfg join") : Action(botAI, name) {}

    bool Execute(Event event) override;
    bool isUseful() override;

protected:
    bool JoinLFG();
};

// #142 (AC LfgActions.h:42-49): "lfg leave".
class LfgLeaveAction : public Action
{
public:
    LfgLeaveAction(PlayerbotAI* botAI) : Action(botAI, "lfg leave") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

class LfgRoleCheckAction : public Action{
public:
    LfgRoleCheckAction(PlayerbotAI* botAI) : Action(botAI, "lfg role check") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

// A group role poll (SMSG_GROUP_ROLE_POLL_INFORM): the core refuses a group's finder join until every member has a
// group role, and polls them when one is missing. A player answers by picking a role; the bot sets its own, by spec.
class GroupRolePollAction : public Action
{
public:
    GroupRolePollAction(PlayerbotAI* botAI) : Action(botAI, "group role poll") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

// A vote to kick a member of a finder group (SMSG_LFD_BOOT_PROPOSAL_UPDATE). A bot never kicks a player: it votes yes
// to remove a bot a player started the vote against, and no when the victim is a player. The victim does not vote.
class LfgBootVoteAction : public Action
{
public:
    LfgBootVoteAction(PlayerbotAI* botAI) : Action(botAI, "lfg boot vote") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

// A bot of a finder group that the group's dungeon never took in (it was fighting, falling or dead when the proposal
// closed, so LFGMgr::TeleportPlayer refused it) asks for the teleport again once it can: the "Teleport" button a player
// presses. Without it the bot stayed where it was for the whole run.
class LfgTeleportRetryAction : public Action
{
public:
    LfgTeleportRetryAction(PlayerbotAI* botAI) : Action(botAI, "lfg teleport retry") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

class LfgAcceptAction : public Action
{
public:
    LfgAcceptAction(PlayerbotAI* botAI) : Action(botAI, "lfg accept") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

class LfgTeleportAction : public Action
{
public:
    LfgTeleportAction(PlayerbotAI* botAI) : Action(botAI, "lfg teleport") {}

    bool Execute(Event event) override;
    bool isUseful() override { return true; }
};

#endif
