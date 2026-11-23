/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_VOATRIGGERS_H
#define PLAYERBOTS_VOATRIGGERS_H

#include "Trigger.h"

class Player;
class PlayerbotAI;
class Unit;

// AC Ai/Raid/VoA/VoATriggers.h enum VoAIDs. Checked for 5.4.8: spells in Spell.dbc, Emalon and the minion in
// creature_template and the core's boss_emalon.cpp (Lightning Nova 64216; 65279 is the 25 player version).
enum VoAIDs
{
    // Emalon the Storm Watcher
    AURA_OVERCHARGE = 64217,
    BOSS_EMALON = 33993,
    NPC_TEMPEST_MINION = 33998,
    SPELL_LIGHTNING_NOVA_10_MAN = 64216,
    SPELL_LIGHTNING_NOVA_25_MAN = 65279,
};

namespace VoAHelpers
{
    uint8 constexpr SKULL_ICON = 7;

    // The first Tempest Minion near the bot that carries Overcharged (it blows up the raid at 10 stacks)
    Unit* FindOverchargedMinion(PlayerbotAI* botAI);
}

// AC Ai/Raid/VoA/VoATriggers.h class EmalonMarkBossTrigger
class EmalonMarkBossTrigger : public Trigger
{
public:
    EmalonMarkBossTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon mark boss trigger") {}
    bool IsActive() override;
};

// AC Ai/Raid/VoA/VoATriggers.h class EmalonLightingNovaTrigger
class EmalonLightingNovaTrigger : public Trigger
{
public:
    EmalonLightingNovaTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon lighting nova trigger") {}
    bool IsActive() override;
};

// AC Ai/Raid/VoA/VoATriggers.h class EmalonOverchargeTrigger
class EmalonOverchargeTrigger : public Trigger
{
public:
    EmalonOverchargeTrigger(PlayerbotAI* botAI) : Trigger(botAI, "emalon overcharge trigger") {}
    bool IsActive() override;
};

#endif
