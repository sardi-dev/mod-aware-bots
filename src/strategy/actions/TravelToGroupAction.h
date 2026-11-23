/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * A random bot that joined a player's group from far away travels to the group like a player would, instead of
 * being teleported: it says where it is and how long it needs, walks, rides, flies or sails there over the travel
 * graph (NewRpgBaseAction::MoveFarTo), reports once or twice on the way and says when it has arrived.
 * A group inside a dungeon is reached through the dungeon's entrance: the bot walks to the area trigger and sends it
 * like a player entering. Alt bots keep the summon (AcceptInvitationAction).
 */

#ifndef _PLAYERBOT_TRAVELTOGROUPACTION_H
#define _PLAYERBOT_TRAVELTOGROUPACTION_H

#include "NewRpgBaseAction.h"
#include "WorldPosition.h"

class Player;
class PlayerbotAI;

namespace GroupTravel
{
    // AiPlayerbot.TravelToGroup.Enable
    bool Enabled();

    // AiPlayerbot.TravelToGroup.SummonIntoDungeons (default off): a bot invited by a player who is inside a dungeon is
    // summoned there instead of walking to the dungeon's entrance and in.
    bool SummonIntoDungeons();

    // True when the dungeon or raid map has an entrance area trigger the bot can walk to.
    bool HasEntrance(uint32 instanceMapId);

    // An entrance of a dungeon or raid map: the nearest to the bot on its map, else the first one.
    struct EntranceInfo
    {
        uint32 triggerId = 0;
        WorldPosition pos;
        float radius = 0.0f;
    };
    bool FindEntranceFor(uint32 instanceMapId, Player* near, EntranceInfo& out);

    // How close to the trigger's position the bot must stand to walk in.
    float EnterDistance(float radius);

    // Walks the bot into the dungeon: sends the area trigger like a player entering. The bot must stand on it.
    void SendEntranceTrigger(Player* bot, uint32 triggerId);

    // True when the bot should be travelling to its group now (see the .cpp for the conditions).
    bool WantsTravel(PlayerbotAI* botAI, Player* bot);
}

class TravelToGroupAction : public NewRpgBaseAction
{
public:
    TravelToGroupAction(PlayerbotAI* botAI) : NewRpgBaseAction(botAI, "travel to group") {}

    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
