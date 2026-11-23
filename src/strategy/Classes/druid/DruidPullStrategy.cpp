/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Class/Druid/Strategy/DruidPullStrategy.cpp.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "DruidPullStrategy.h"

#include "AiObjectContext.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

namespace
{
// #408: AC's PlayerbotAI::HasSpell(std::string const) is not ported on this fork. This is AC's own
// implementation, kept file-local so PlayerbotAI.h does not have to change.
bool HasKnownSpell(PlayerbotAI* botAI, std::string const spellName)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spellName)->Get();
    return spellId && botAI->GetBot()->HasSpell(spellId);
}
} // namespace

std::string DruidPullStrategy::GetPullActionName() const
{
    // Class sheet, guardian druid (BT-131): below level 28 Faerie Fire is not learned, and with Growl (level 8) on its
    // cooldown the choice below fell through to the unknown "faerie fire" and held every other action for the 15 second
    // pull limit. Without either spell the pull is refused, like the warrior's (WarriorPullStrategy.cpp), and the bot
    // fights in melee; with Growl known the pull waits for its cooldown (8 seconds) instead.
    bool const knowsFaerieFire = HasKnownSpell(botAI, "faerie fire");
    bool const knowsGrowl = HasKnownSpell(botAI, "growl");
    if (!knowsFaerieFire && !knowsGrowl)
        return std::string();

    std::string const pullActionName = PullStrategy::GetPullActionName();
    std::string const actionName =
        HasKnownSpell(botAI, "faerie fire (feral)") &&
            (botAI->HasStrategy("bear", BOT_STATE_COMBAT) || botAI->HasStrategy("cat", BOT_STATE_COMBAT))
        ? "faerie fire (feral)"
        : pullActionName;

    Unit* target = GetTarget();
    if (!target)
        return knowsFaerieFire ? actionName : std::string("growl");

    if (!botAI->CanCastSpell(actionName, target) && botAI->CanCastSpell("growl", target))
        return "growl";

    // Faerie Fire is not learned: Growl is the only ranged pull, castable again after its cooldown.
    if (!knowsFaerieFire)
        return "growl";

    return actionName;
}

std::string DruidPullStrategy::GetPreActionName() const
{
    Player* bot = botAI->GetBot();
    if (!bot || !botAI->HasStrategy("bear", BOT_STATE_COMBAT))
        return "";

    // Class sheet, guardian druid (BT-131): Growl needs Bear Form, and Bear Form has the armor and the threat bonus
    // for any pull. PullStartAction counts a useless pre-action as a failed start (CastBearFormAction is useless in
    // Bear Form), so the name is given only while the shift is needed and can be cast now; otherwise the pull goes on.
    if (botAI->HasAnyAuraOf(bot, "bear form", "dire bear form", nullptr) || !botAI->CanCastSpell("bear form", bot))
        return "";

    return PullStrategy::GetPreActionName();
}
