/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "NonCombatActions.h"

#include "Event.h"
#include "Playerbots.h"

namespace
{
// AC Ai/Base/Actions/NonCombatActions.cpp:11-34.
constexpr uint32 BG_WS_SPELL_WARSONG_FLAG = 23333;
constexpr uint32 BG_WS_SPELL_SILVERWING_FLAG = 23335;
constexpr uint32 BG_EY_NETHERSTORM_FLAG_SPELL = 34976;

bool IsDisallowedShapeshiftForm(Player* bot)
{
    if (bot->getClass() == CLASS_DRUID)
    {
        // FORM_DIREBEAR is still in the 5.4.8 enum (Unit.h, "Removed in 4.0.1"); kept for a 1:1 list.
        ShapeshiftForm form = bot->GetShapeshiftForm();
        return form == FORM_TRAVEL || form == FORM_AQUA || form == FORM_FLIGHT || form == FORM_FLIGHT_EPIC ||
               form == FORM_BEAR || form == FORM_DIREBEAR || form == FORM_CAT;
    }
    else if (bot->getClass() == CLASS_PRIEST)
    {
        return bot->GetShapeshiftForm() == FORM_SPIRITOFREDEMPTION;
    }

    return false;
}

}  // namespace

// AC NonCombatActions.cpp:36-73 without the food cheat (design decision 0016): a bot drinks what it carries.
bool DrinkAction::Execute(Event event) { return UseItemAction::Execute(event); }

// AC NonCombatActions.cpp:75-79.
bool DrinkAction::isUseful()
{
    return UseItemAction::isUseful() && AI_VALUE2(bool, "has mana", "self target") &&
           AI_VALUE2(uint8, "mana", "self target") < 100;
}

// AC NonCombatActions.cpp:81-93.
bool DrinkAction::isPossible()
{
    if (bot->IsInCombat() || bot->IsMounted() || IsDisallowedShapeshiftForm(bot))
        return false;

    // #754 (beyond AC, part of the owner-approved wake "A"): in a battleground never sit down to eat/drink while a unit is
    // attacking the bot. The combat flag lapses for a moment between hits, and AC's !IsInCombat() test then lets the bot
    // sit with attackers on it (w21 TP: Haralas 7 s, Tanderen and Delenelia with 1-4 attackers).
    if (bot->InBattleground() && !bot->getAttackers().empty())
        return false;

    if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
        bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
        return false;

    return UseItemAction::isPossible();
}

// AC NonCombatActions.cpp:95-132 without the food cheat (design decision 0016): a bot eats what it carries.
bool EatAction::Execute(Event event) { return UseItemAction::Execute(event); }

// AC NonCombatActions.cpp:134.
bool EatAction::isUseful() { return UseItemAction::isUseful() && AI_VALUE2(uint8, "health", "self target") < 100; }

// AC NonCombatActions.cpp:136-148.
bool EatAction::isPossible()
{
    if (bot->IsInCombat() || bot->IsMounted() || IsDisallowedShapeshiftForm(bot))
        return false;

    // #754 (beyond AC): as DrinkAction::isPossible - no eating with attackers on the bot in a battleground.
    if (bot->InBattleground() && !bot->getAttackers().empty())
        return false;

    if (bot->HasAura(BG_WS_SPELL_WARSONG_FLAG) || bot->HasAura(BG_WS_SPELL_SILVERWING_FLAG) ||
        bot->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL))
        return false;

    return UseItemAction::isPossible();
}
