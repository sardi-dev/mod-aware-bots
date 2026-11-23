/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpDefenseActions.h for what this is.
 */

#include "PvpDefenseActions.h"

#include "Arena/ArenaPlan.h"
#include "BotSkill.h"
#include "Event.h"
#include "Item.h"
#include "OneVsOne.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpDefense.h"
#include "PvpRotationValues.h"
#include "Timer.h"

namespace
{
    // The engine, the arena plan and this strategy never act on the same fight: a 1v1 belongs to the engine, a ladder arena
    // team to the arena plan (which asks the same rules for the spell).
    bool Mine(PlayerbotAI* botAI, Player* bot)
    {
        if (!PvpConfig::DefensiveRulesEnabled() || !bot->IsAlive() || !(bot->InBattleground() || bot->InArena()))
            return false;
        return !OneVsOneGlue::Owns(botAI, bot);
    }

    bool Held(Player* bot)
    {
        return bot->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED | UNIT_STATE_ROOT) ||
               bot->HasAuraType(SPELL_AURA_MOD_STUN) || bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
               bot->HasAuraType(SPELL_AURA_MOD_FEAR_2) || bot->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
               bot->HasAuraType(SPELL_AURA_MOD_ROOT);
    }
}  // namespace

bool PvpDefensiveDueTrigger::IsActive()
{
    if (!Mine(botAI, bot) || ArenaPlan::Active(bot))
    {
        _wantSince = 0;
        return false;
    }
    if (!PvpDefenseGlue::Defensive(botAI, bot, false).use)
    {
        _wantSince = 0;
        return false;
    }
    std::uint32_t const now = getMSTime();
    if (!_wantSince)
        _wantSince = now;
    return getMSTimeDiff(_wantSince, now) >= OneVsOne::ReactionMs(BotSkill::TierOf(bot), bot->GetGUIDLow());
}

bool PvpBreakDueTrigger::IsActive()
{
    if (!Mine(botAI, bot) || !Held(bot))
    {
        _heldSince = 0;
        return false;
    }
    std::uint32_t const now = getMSTime();
    if (!_heldSince)
        _heldSince = now;
    if (getMSTimeDiff(_heldSince, now) < OneVsOne::ReactionMs(BotSkill::TierOf(bot), bot->GetGUIDLow()))
        return false;
    return PvpDefenseGlue::Break(botAI, bot, CarryingObjectiveValue::IsCarrying(bot)).use;
}

bool PvpDefensiveAction::Execute(Event /*event*/)
{
    PvpDefenseGlue::DefensiveVerdict const v = PvpDefenseGlue::Defensive(botAI, bot, false);
    if (!v.use || !botAI->CastSpell(v.spellId, bot))
        return false;
    PvpDefenseGlue::NoteDefensiveCast(bot);
    return true;
}

bool PvpBreakAction::Execute(Event /*event*/)
{
    PvpDefenseGlue::BreakVerdict const v = PvpDefenseGlue::Break(botAI, bot, CarryingObjectiveValue::IsCarrying(bot));
    if (!v.use)
        return false;
    if (v.trinket)
    {
        if (!v.item)
            return false;
        // The item spell of a PvP trinket, used the way a player's click does.
        SpellCastTargets targets;
        targets.SetUnitTarget(bot);
        bot->CastItemUseSpell(v.item, targets, 0, 0);
        return true;
    }
    return botAI->CastSpell(v.spellId, bot);
}
