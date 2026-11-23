/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "OneVsOneAction.h"

#include "Event.h"
#include "OneVsOne.h"
#include "Playerbots.h"
#include "PvpDefense.h"
#include "ServerFacade.h"

#include <cmath>

bool OneVsOneAction::Execute(Event /*event*/)
{
    OneVsOneGlue::Decision d;
    if (!OneVsOneGlue::Decide(botAI, bot, d) || !d.enemy)
        return false;

    switch (d.option.choice)
    {
        case OneVsOne::Choice::STEP_BACK:
            if (d.hasPoint)
                return MoveTo(bot->GetMapId(), d.pointX, d.pointY, d.pointZ, false, false, true, true,
                              MovementPriority::MOVEMENT_COMBAT);
            return MoveAway(d.enemy, d.stepBack, false);
        case OneVsOne::Choice::COVER:
            return d.hasPoint && MoveTo(bot->GetMapId(), d.pointX, d.pointY, d.pointZ, false, false, true, true,
                                        MovementPriority::MOVEMENT_COMBAT);
        case OneVsOne::Choice::COVER_HOLD:
            return true;  // hold still: the rotation would walk back into the line of sight
        default:
            break;
    }

    if (!d.hasSpell)
        return false;

    if (d.trinket)
    {
        // The item spell of a PvP trinket, used the way a player's click does.
        SpellCastTargets targets;
        targets.SetUnitTarget(bot);
        bot->CastItemUseSpell(d.trinketItem, targets, 0, 0);
        return true;
    }

    bool const cast = CastKitSpell(d);
    if (cast && d.option.choice == OneVsOne::Choice::SPEED)
        OneVsOneGlue::NoteSpeedCast(bot);
    if (cast && d.option.choice == OneVsOne::Choice::DEFENSIVE)
        PvpDefenseGlue::NoteDefensiveCast(bot);
    return cast;
}

bool OneVsOneAction::CastKitSpell(OneVsOneGlue::Decision const& d)
{
    OneVsOne::KitSpell const& k = d.spell;
    switch (k.aim)
    {
        case OneVsOne::Aim::ENEMY:
            return botAI->CastSpell(k.spellId, d.enemy);
        case OneVsOne::Aim::SELF:
            return botAI->CastSpell(k.spellId, bot);
        case OneVsOne::Aim::SELF_AWAY:
            // Blink and Displacer Beast go where the bot faces: turn the back to the enemy first (as "blink back").
            bot->SetOrientation(bot->GetAngle(d.enemy) + float(M_PI));
            return botAI->CastSpell(k.spellId, bot);
        case OneVsOne::Aim::SELF_FACING:
            // Disengage leaps backward from the facing, Roll and the monk kicks go forward: face the enemy first.
            sServerFacade->SetFacingTo(bot, d.enemy, true);
            return botAI->CastSpell(k.spellId, bot);
        case OneVsOne::Aim::GROUND:
            return botAI->CastSpell(k.spellId, d.enemy->GetPositionX(), d.enemy->GetPositionY(), d.enemy->GetPositionZ());
    }
    return false;
}
