/*
 * AC parity (#408): ported from AC mod-playerbots Ai/Class/Paladin/Strategy/PaladinPullStrategy.cpp.
 *
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option) any later version.
 */

#include "PaladinPullStrategy.h"

#include "AttackersValue.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PullBouncePolicy.h"

namespace
{
// BT-167: counts living enemies near the mark that Avenger's Shield could jump to. It reads the cached "possible
// targets" value, so it adds no grid scan, and the answer is kept per mark for a few seconds because the pull code asks
// for the action name several times a tick.
constexpr time_t BOUNCE_CACHE_SECONDS = 3;

unsigned NeighboursNearMark(PlayerbotAI* botAI, Player* bot, Unit* mark)
{
    unsigned count = 0;
    for (BotGuid const guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit == mark || !unit->IsAlive() || !AttackersValue::IsPossibleTarget(unit, bot))
            continue;
        if (PullBouncePolicy::InBounceRadius(unit->GetExactDist(mark)))
            ++count;
    }
    return count;
}
}  // namespace

std::string PaladinPullStrategy::GetPullActionName() const
{
    Unit* target = GetTarget();
    if (!target ||
        (!botAI->HasStrategy("tank", BOT_STATE_COMBAT) && !botAI->HasStrategy("tank", BOT_STATE_NON_COMBAT)))
        return PullStrategy::GetPullActionName();

    // BT-167: the shield bounces onto other enemies near the mark; open with a single target spell then.
    time_t const now = time(nullptr);
    if (bounceMark != target->GetGUID() || now - bounceCheckedAt >= BOUNCE_CACHE_SECONDS)
    {
        bounceMark = target->GetGUID();
        bounceCheckedAt = now;
        bounceRisk = PullBouncePolicy::ShieldWouldBounce(NeighboursNearMark(botAI, botAI->GetBot(), target));
    }
    if (!bounceRisk && botAI->CanCastSpell("avenger's shield", target))
        return "avenger's shield";

    // BT-050: 5.4.8 names the taunt Reckoning (62124); "hand of reckoning" never resolved, so the pull fell to Judgment
    if (botAI->CanCastSpell("reckoning", target))
        return "hand of reckoning";

    // Judgment is the single target fallback (the pull strategy's own action), as before this change.
    return PullStrategy::GetPullActionName();
}

std::string PaladinPullStrategy::GetPreActionName() const
{
    if (botAI->HasStrategy("tank", BOT_STATE_COMBAT) || botAI->HasStrategy("tank", BOT_STATE_NON_COMBAT))
        return "";

    return PullStrategy::GetPreActionName();
}
