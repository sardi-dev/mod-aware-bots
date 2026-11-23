/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERCONTEXT_H
#define PLAYERBOTS_VOATRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "VoATriggers.h"

// AC Ai/Raid/VoA/VoATriggerContext.h, without the fall-from-floor and resistance aura triggers
class RaidVoATriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidVoATriggerContext()
    {
        creators["emalon mark boss trigger"] = &RaidVoATriggerContext::emalon_mark_boss_trigger;
        creators["emalon lighting nova trigger"] = &RaidVoATriggerContext::emalon_lighting_nova_trigger;
        creators["emalon overcharge trigger"] = &RaidVoATriggerContext::emalon_overcharge_trigger;
    }

private:
    static Trigger* emalon_mark_boss_trigger(PlayerbotAI* botAI) { return new EmalonMarkBossTrigger(botAI); }
    static Trigger* emalon_lighting_nova_trigger(PlayerbotAI* botAI) { return new EmalonLightingNovaTrigger(botAI); }
    static Trigger* emalon_overcharge_trigger(PlayerbotAI* botAI) { return new EmalonOverchargeTrigger(botAI); }
};

#endif
