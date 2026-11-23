/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaPlanActions.h"

#include "ArenaPlan.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpDefense.h"

bool ArenaFocusOffTrigger::IsActive()
{
    Unit* focus = ArenaPlan::FocusTarget(bot);
    return focus && bot->GetVictim() != focus;
}

bool ArenaDefensiveDueTrigger::IsActive()
{
    return ArenaPlan::DefensiveDue(bot) != 0;
}

bool ArenaCoverDueTrigger::IsActive()
{
    float x, y, z;
    return ArenaPlan::CoverDue(bot, x, y, z);
}

bool ArenaDefensiveAction::Execute(Event /*event*/)
{
    uint32 const spell = ArenaPlan::DefensiveDue(bot);
    if (!spell || !botAI->CastSpell(spell, bot))
        return false;

    ArenaPlan::NoteDefensiveCast(bot);
    PvpDefenseGlue::NoteDefensiveCast(bot);
    return true;
}

bool ArenaCoverAction::Execute(Event /*event*/)
{
    float x, y, z;
    if (!ArenaPlan::CoverDue(bot, x, y, z))
        return false;

    bot->CastStop();
    ArenaPlan::NoteCoverTaken(bot);
    return MoveTo(bot->GetMapId(), x, y, z, false, true);
}
