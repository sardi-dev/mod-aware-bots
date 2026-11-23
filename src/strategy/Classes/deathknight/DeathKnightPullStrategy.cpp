/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Class/Dk/Strategy/DeathKnightPullStrategy.cpp.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "DeathKnightPullStrategy.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

namespace
{
// Class sheet, blood death knight (BT-134): a hostile within this distance of the mark joins it when it is gripped.
constexpr float GRIP_PACK_RADIUS = 8.0f;

// The module has no "is caster" test for enemy units, so a caster is a unit that uses mana (the same test the module uses
// for its own mana users).
bool UsesMana(Unit* unit) { return unit->getPowerType() == POWER_MANA && unit->GetMaxPower(POWER_MANA) > 0; }

// No other hostile unit stands within GRIP_PACK_RADIUS of the mark: dragging it to the tank leaves nothing behind that the
// tank would have to pick up on its own.
bool StandsAlone(PlayerbotAI* botAI, Unit* mark)
{
    Player* bot = botAI->GetBot();
    GuidVector const enemies = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get();
    for (BotGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || enemy == mark || !enemy->IsAlive() || enemy->IsFriendlyTo(bot))
            continue;

        if (mark->IsWithinDistInMap(enemy, GRIP_PACK_RADIUS))
            return false;
    }

    return true;
}
}  // namespace

std::string DeathKnightPullStrategy::GetPullActionName() const
{
    Unit* target = GetTarget();
    if (!target || (!botAI->HasStrategy("blood", BOT_STATE_COMBAT) && !botAI->HasStrategy("blood", BOT_STATE_NON_COMBAT)))
        return PullStrategy::GetPullActionName();

    // Class sheet, blood death knight (BT-134): Death Grip yanks the mark out of its pack, so it pulls only a caster (which
    // would stand and cast from range) or a mark that stands alone. A mark inside a pack is pulled with Icy Touch (below),
    // and the pack walks in together. Death Grip as a taunt on lost aggro is unchanged (BloodDKStrategy.cpp).
    if ((UsesMana(target) || StandsAlone(botAI, target)) && botAI->CanCastSpell("death grip", target))
        return "death grip";

    if (!botAI->CanCastSpell("icy touch", target) && botAI->CanCastSpell("dark command", target))
        return "dark command";

    return PullStrategy::GetPullActionName();
}
