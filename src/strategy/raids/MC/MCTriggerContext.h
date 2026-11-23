/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_MCTRIGGERCONTEXT_H
#define PLAYERBOTS_MCTRIGGERCONTEXT_H

#include "MCTriggers.h"
#include "NamedObjectContext.h"

class RaidMcTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidMcTriggerContext()
    {
        creators["mc living bomb debuff"] = &RaidMcTriggerContext::living_bomb_debuff;
        creators["mc baron geddon inferno"] = &RaidMcTriggerContext::baron_geddon_inferno;
        creators["mc shazzrah ranged"] = &RaidMcTriggerContext::shazzrah_ranged;
        creators["mc golemagg mark boss"] = &RaidMcTriggerContext::golemagg_mark_boss;
        creators["mc golemagg is main tank"] = &RaidMcTriggerContext::golemagg_is_main_tank;
        creators["mc golemagg is assist tank"] = &RaidMcTriggerContext::golemagg_is_assist_tank;
        creators["mc core hound mark"] = &RaidMcTriggerContext::core_hound_mark;
        creators["mc in lava"] = &RaidMcTriggerContext::in_lava;
        creators["mc golemagg magma splash"] = &RaidMcTriggerContext::golemagg_magma_splash;
        creators["mc golemagg is healer"] = &RaidMcTriggerContext::golemagg_is_healer;
    }

private:
    static Trigger* living_bomb_debuff(PlayerbotAI* botAI) { return new McLivingBombDebuffTrigger(botAI); }
    static Trigger* baron_geddon_inferno(PlayerbotAI* botAI) { return new McBaronGeddonInfernoTrigger(botAI); }
    static Trigger* shazzrah_ranged(PlayerbotAI* botAI) { return new McShazzrahRangedTrigger(botAI); }
    static Trigger* golemagg_mark_boss(PlayerbotAI* botAI) { return new McGolemaggMarkBossTrigger(botAI); }
    static Trigger* golemagg_is_main_tank(PlayerbotAI* botAI) { return new McGolemaggIsMainTankTrigger(botAI); }
    static Trigger* golemagg_is_assist_tank(PlayerbotAI* botAI) { return new McGolemaggIsAssistTankTrigger(botAI); }
    static Trigger* core_hound_mark(PlayerbotAI* botAI) { return new McCoreHoundMarkTrigger(botAI); }
    static Trigger* in_lava(PlayerbotAI* botAI) { return new McInLavaTrigger(botAI); }
    static Trigger* golemagg_magma_splash(PlayerbotAI* botAI) { return new McGolemaggMagmaSplashTrigger(botAI); }
    static Trigger* golemagg_is_healer(PlayerbotAI* botAI) { return new McGolemaggIsHealerTrigger(botAI); }
};

#endif
