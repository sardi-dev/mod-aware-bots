/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ONEVSONEACTION_H
#define _PLAYERBOT_ONEVSONEACTION_H

#include "MovementActions.h"
#include "OneVsOne.h"

class PlayerbotAI;

// Carries out the 1v1 policy's choice: casts the class spell it picked (aimed at the enemy, at the bot, at the enemy's
// position, or after turning toward or away from the enemy) steps back from a slowed or held melee enemy, walks to cover or holds still in it.
class OneVsOneAction : public MovementAction
{
public:
    OneVsOneAction(PlayerbotAI* botAI) : MovementAction(botAI, "1v1") {}

    bool Execute(Event event) override;

private:
    bool CastKitSpell(OneVsOneGlue::Decision const& d);
};

#endif
