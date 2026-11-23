/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOAACTIONCONTEXT_H
#define PLAYERBOTS_VOAACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "VoAActions.h"

// AC Ai/Raid/VoA/VoAActionContext.h, without the fall-from-floor teleport and the resistance aura actions
class RaidVoAActionContext : public NamedObjectContext<Action>
{
public:
    RaidVoAActionContext()
    {
        creators["emalon mark boss action"] = &RaidVoAActionContext::emalon_mark_boss_action;
        creators["emalon lighting nova action"] = &RaidVoAActionContext::emalon_lighting_nova_action;
        creators["emalon overcharge action"] = &RaidVoAActionContext::emalon_overcharge_action;
    }

private:
    static Action* emalon_mark_boss_action(PlayerbotAI* botAI) { return new EmalonMarkBossAction(botAI); }
    static Action* emalon_lighting_nova_action(PlayerbotAI* botAI) { return new EmalonLightingNovaAction(botAI); }
    static Action* emalon_overcharge_action(PlayerbotAI* botAI) { return new EmalonOverchargeAction(botAI); }
};

#endif
