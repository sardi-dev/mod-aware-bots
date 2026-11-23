/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * BT-320, BT-323: PvP talent and glyph sets. A random bot buys Dual Talent Specialization at its class trainer with its
 * own gold (design decision 0016), keeps its PvE set in the primary talent group and its spec's PvP set
 * (AiPlayerbot.PremadeSpecLinkPvp and PremadeSpecGlyphPvp) in the secondary one, and switches with the Activate Spec
 * cast out of combat: to PvP when it queues for or enters a battleground or arena, back to PvE at an inn, a city or a
 * dungeon. The rules are in Utils/PvpTalentSetPolicy.h; this file reads the bot and applies them.
 */

#ifndef MOD_AWARE_BOTS_BOTPVPTALENTS_H
#define MOD_AWARE_BOTS_BOTPVPTALENTS_H

#include "Define.h"
#include "PvpTalentSetPolicy.h"

#include <vector>

class Creature;
class Player;
class PlayerbotAI;

namespace BotPvpTalents
{
    // AiPlayerbot.PvpTalentSets (default 1).
    bool Enabled();

    // The step the bot would take now (PvpTalentSetPolicy::Decide). World thread.
    PvpTalentSetPolicy::Step Evaluate(Player* bot);

    // Takes that step: starts the Activate Spec cast (the AI waits for it), or puts the PvP set into the active
    // secondary group. Returns true when something started or changed. World thread, bot in the world.
    bool Apply(PlayerbotAI* botAI, Player* bot);

    // The PvP link or glyph list of the premade entry `premadeIndex` when the bot is a random bot on its secondary
    // talent group and that entry has a PvP set; nullptr otherwise. The factory uses them to fill tiers unlocked while
    // the bot is on its PvP group, so a level-up there learns the PvP pick.
    std::vector<uint16> const* PvpLinkOfActiveGroup(Player* bot, uint32 premadeIndex);
    std::vector<uint32> const* PvpGlyphsOfActiveGroup(Player* bot, uint32 premadeIndex);

    // Whether the bot should buy Dual Talent Specialization now (PvpTalentSetPolicy::WantsDualSpec); the town errand
    // then takes it to a class trainer of its class.
    bool WantsDualSpec(Player* bot);

    // At a class trainer of its class: pays DUAL_SPEC_COST and learns the second talent group as the trainer's gossip
    // option does (Player::OnGossipSelect, GOSSIP_OPTION_LEARNDUALSPEC). Returns true when bought.
    bool BuyDualSpec(Player* bot, Creature* trainer);
}

#endif
