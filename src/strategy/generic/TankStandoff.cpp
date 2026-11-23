/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TankStandoff.h"

#include "Log.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "TankStandoffPolicy.h"

bool TankStandoff::Observe(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    TankStandoffPolicy::State& state = botAI->tankStandoff;
    time_t const now = time(nullptr);
    if (!TankStandoffPolicy::ObserveDue(state, now))
        return false;

    Unit* target = nullptr;
    if (bot->IsAlive() && bot->IsInCombat() && bot->GetGroup() && PlayerBotSpec::IsTank(bot))
    {
        target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (!target)
            target = bot->GetVictim();
        if (target && !target->IsAlive())
            target = nullptr;
    }
    time_t const stalled = TankStandoffPolicy::Observe(state, target ? target->GetGUID() : 0, target ? target->GetHealth() : 0, now);
    return target && TankStandoffPolicy::ShouldLog(stalled, now, state.loggedAt);
}

void TankStandoff::Report(PlayerbotAI* botAI, std::string const& decision)
{
    Player* bot = botAI->GetBot();
    TankStandoffPolicy::State& state = botAI->tankStandoff;
    time_t const now = time(nullptr);
    state.loggedAt = now;
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (!target)
        target = bot->GetVictim();
    if (!target)
        return;

    Unit* victim = target->GetVictim();
    SF_LOG_INFO("playerbots", "[TANK] bot=%s target=%s entry=%u hp=%.0f stalled=%us dist=%.1f los=%u dz=%.1f victim=%s "
                "casting=%u target_moving=%u tank_moving=%u attackers=%u decision=%s",
                bot->GetName().c_str(), target->GetName().c_str(), target->GetEntry(), target->GetHealthPct(),
                unsigned(now - state.changedAt), bot->GetDistance(target), uint32(bot->IsWithinLOSInMap(target)),
                target->GetPositionZ() - bot->GetPositionZ(), victim ? victim->GetName().c_str() : "none",
                uint32(target->IsNonMeleeSpellCasted(false)), uint32(target->isMoving()), uint32(bot->isMoving()),
                uint32(bot->getAttackers().size()), decision.c_str());
}
