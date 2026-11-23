#include "InvalidTargetValue.h"

#include "AttackersValue.h"
#include "Playerbots.h"
#include "Unit.h"

namespace
{
    // #572 (MoP-only, beyond AC): with Battleground.IgnoreFaction = 1 (the 5.4.8 dist default) a player can play for the
    // other faction's side (BattlegroundQueue.cpp:190-240, Battleground.cpp:1605/1682), so in a battleground a teammate
    // is a player of the same BG team (EnemyPlayerValue.cpp IsOpposingSide, BGTactics::BgTeamIdOf). Elsewhere AC's
    // original-race team (AC GetTeamId(true); MoP GetTeamId() is the race team).
    bool IsSameSide(Player* bot, Player* other)
    {
        if (bot->InBattleground() && other->InBattleground() && bot->GetBattlegroundId() == other->GetBattlegroundId())
            return bot->GetBGTeam() == other->GetBGTeam();

        return other->GetTeamId() == bot->GetTeamId();
    }
}

bool InvalidTargetValue::Calculate()
{
    Unit* target = AI_VALUE(Unit*, qualifier);
    Unit* enemy = AI_VALUE(Unit*, "enemy player target");
    if (target && enemy && target == enemy && target->IsAlive())
        return false;

    if (target && qualifier == "current target")
    {
        return target->GetMapId() != bot->GetMapId() || target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) ||
            target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) || target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_16) ||
            !target->IsVisible() || !target->IsAlive() || target->IsPolymorphed() ||
            // #693, AC mod-playerbots#2855 (InvalidTargetValue.cpp:23-25): only a mind-controlled teammate is off
            // limits, a player-driven vehicle is charmed too. MoP GetTeamId() is the race team (AC GetTeamId(true)).
            // #572 (MoP-only): inside a battleground "teammate" is the BG side (IsSameSide), as in EnemyPlayerValue.
            (target->IsCharmed() && (target->GetTypeId() == TypeID::TYPEID_PLAYER) && IsSameSide(bot, target->ToPlayer())) ||
            target->HasAuraType(SPELL_AURA_MOD_FEAR) || target->HasUnitState(UNIT_STATE_ISOLATED) || target->IsFriendlyTo(bot) ||
            !AttackersValue::IsPossibleTarget(target, bot) ||
            // BT-187: the stale combat target stays without sight for a while, so the bot walks to it.
            (!bot->IsWithinLOSInMap(target) && !AttackersValue::KeepsStaleTargetWithoutSight(botAI, target));
    }

    return !target;
}
