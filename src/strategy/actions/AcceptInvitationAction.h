#ifndef _PLAYERBOT_ACCEPTINVITATIONACTION_H
#define _PLAYERBOT_ACCEPTINVITATIONACTION_H

#include "Action.h"
#include "UseMeetingStoneAction.h"

class Player;
class PlayerbotAI;

// #177 (AC AcceptInvitationAction.h:15): base changed from Action to SummonAction so Execute() can
// call Teleport() for AC's summonWhenGroup behaviour (see the .cpp).
class AcceptInvitationAction : public SummonAction
{
public:
    AcceptInvitationAction(PlayerbotAI* botAI, const std::string name = "accept invitation");

    bool Execute(Event event) override;

    // A player asked this bot to join ("join" command): the next invite is accepted without the goal check.
    static void NoteJoinRequest(Player* bot);
};

#endif
