/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpTeamStrategy.h for what this is.
 */

#include "PvpTeamStrategy.h"

#include "Playerbots.h"
#include "PvpTeamActions.h"

void PvpTeamStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below Presence of Mind (ACTION_INTERRUPT + 11) so that a mage's instant Polymorph is cast on the aura, and above every
    // rotation spell: a control link that comes late breaks the chain.
    triggers.push_back(new TriggerNode("pvp control due", NextAction::array(0, new NextAction("pvp control", ACTION_INTERRUPT + 10), nullptr)));
}

void PvpTeamStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new PvpTeamMultiplier(botAI));
}
