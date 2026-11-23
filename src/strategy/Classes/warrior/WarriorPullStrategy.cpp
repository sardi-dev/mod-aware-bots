/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Class/Warrior/Strategy/WarriorPullStrategy.cpp.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "WarriorPullStrategy.h"

#include "AiObjectContext.h"
#include "Player.h"
#include "PlayerbotAI.h"

namespace
{
// What a warrior pulls with, best first. Heroic Throw is learned at 22 and has a 30 second cooldown; Taunt (level 12)
// reaches 30 yards and makes the target come; Charge is the last resort.
char const* const PULL_ACTIONS[] = { "heroic throw", "taunt pull", "charge" };
char const* const PULL_SPELLS[] = { "heroic throw", "taunt", "charge" };
}  // namespace

std::string WarriorPullStrategy::GetPullActionName() const
{
    // BT-035, BT-124: a warrior cannot Shoot. Keep the first known action as a fallback while it is unavailable.
    std::string fallback;
    Unit* target = GetTarget();
    for (size_t index = 0; index < sizeof(PULL_ACTIONS) / sizeof(PULL_ACTIONS[0]); ++index)
    {
        uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", PULL_SPELLS[index])->Get();
        if (!spellId || !botAI->GetBot()->HasSpell(spellId) ||
            !botAI->GetAiObjectContext()->GetAction(PULL_ACTIONS[index]))
            continue;

        if (fallback.empty())
            fallback = PULL_ACTIONS[index];
        if (target && botAI->CanCastSpell(PULL_SPELLS[index], target))
            return PULL_ACTIONS[index];
    }

    return fallback;
}
