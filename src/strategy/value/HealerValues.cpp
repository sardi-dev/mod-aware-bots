/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "HealerValues.h"

#include "AttackersValue.h"
#include "BotGuid.h"
#include "BotSkill.h"
#include "Group.h"
#include "Log.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "MarkTriggers.h"
#include "Playerbots.h"
#include "RtiTargetValue.h"
#include "Timer.h"

namespace
{
// BT-157: a hostile the tank can pull next: alive, not fighting, attackable by the group and near the tank.
bool IsFreshPullTarget(Unit* mob, Unit* tank, Player* bot)
{
    return mob && mob->IsAlive() && !mob->IsInCombat() && AttackersValue::IsPossibleTarget(mob, bot) &&
        tank->IsWithinDistInMap(mob, HealerGuardrails::PRE_PULL_TARGET_RANGE);
}
}  // namespace

bool HealerPrePullValue::Calculate()
{
    Group* group = bot->GetGroup();
    Unit* tank = group && bot->IsAlive() ? AI_VALUE(Unit*, "main tank") : nullptr;

    HealerGuardrails::PrePullFacts facts{ tank != nullptr, tank && tank->IsInCombat(), false, false };
    Unit* pullTarget = nullptr;
    if (tank && !facts.tankInCombat)
    {
        // The leader's mark leads the tank the way it leads a player (the same lookup as the tank's own pull trigger).
        int32 const index = RtiTargetValue::GetRtiIndex(AI_VALUE(std::string, "rti"));
        uint64 const markGuid = index >= 0 ? group->GetTargetIcon(uint8(index)) : 0;
        Unit* marked = markGuid ? botAI->GetUnit(BotGuid(markGuid)) : nullptr;
        // The tank bot pulls a mark only when the whole group is ready; before that the healer may still be drinking, and a
        // shield spent now would only lengthen the wait.
        Player* tankPlayer = tank->ToPlayer();
        if (IsFreshPullTarget(marked, tank, bot) && tankPlayer && MarkedTargetReadyTrigger::GroupReady(group, tankPlayer, marked))
        {
            facts.markOnFreshHostile = true;
            pullTarget = marked;
        }
        else
        {
            uint64 const selectedGuid = tank->GetTarget();
            Unit* selected = selectedGuid ? botAI->GetUnit(BotGuid(selectedGuid)) : nullptr;
            if (IsFreshPullTarget(selected, tank, bot))
            {
                facts.tankTargetsFreshHostile = true;
                pullTarget = selected;
            }
        }
    }

    uint64 const targetId = HealerGuardrails::PullImminent(facts) && pullTarget ? pullTarget->GetGUID() : 0;
    return window.Allows(targetId, getMSTime());
}

Unit* HealerProactiveTargetValue::Calculate()
{
    if (!bot->IsAlive() || !bot->GetGroup())
        return nullptr;

    Unit* target = AI_VALUE(Unit*, "party member to heal");
    if (!target || !target->ToPlayer())
        return nullptr;

    // Seen on every sample, also when the rules below say no, so that a drop is not missed while the mana is low.
    bool const tookDamage = damage.Observe(target->GetGUID(), uint32(target->GetHealthPct()), getMSTime());

    if (!target->IsAlive() || target->GetHealthPct() >= float(HealerGuardrails::PROACTIVE_HEALTH_CEILING_PCT))
        return nullptr;

    // The tank has his own upkeep with its own mana rule.
    Unit* tank = AI_VALUE(Unit*, "main tank");
    if (target == tank)
        return nullptr;

    // Before the fight the non-combat top-ups and the pre-pull upkeep do the work.
    if (!bot->IsInCombat() && !(tank && tank->IsInCombat()) && !target->IsInCombat())
        return nullptr;

    uint32 const mana = uint32(BotGetPowerPct(bot, POWER_MANA));
    if (!HealerGuardrails::MemberProactiveAllowed(mana, tookDamage))
        return nullptr;

    // Core healers answer damage only; a good one tops up above 60 percent mana; an expert also tops up a member who was
    // just hit while the mana is in the middle band.
    BotSkillMath::Tier const line =
        mana > HealerGuardrails::PROACTIVE_MEMBER_MANA_PCT ? BotSkillMath::Tier::GOOD : BotSkillMath::Tier::EXPERT;
    return BotSkill::Uses(bot, line) ? target : nullptr;
}

Unit* HealerTankToReachValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group || !bot->IsAlive())
        return nullptr;

    Player* tank = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->IsInWorld() && member->FindMap() == bot->FindMap() &&
            PlayerBotSpec::IsMainTank(member))
        {
            tank = member;
            break;
        }
    }

    if (tank)
    {
        // Far away is the follow and travel code's job (the group left the healer behind), not a heal position. In a
        // fight that code never runs, so a flagged healer with no member in range is the rejoin walk's job (BT-202).
        float const distance = bot->GetDistance2d(tank);
        if (distance <= sPlayerbotAIConfig->sightDistance &&
            (distance > sPlayerbotAIConfig->healDistance || !bot->IsWithinLOSInMap(tank)))
            return tank;
    }

    // BT-175: a hurt member the healer cannot heal. The group is five players, so this reads every member once per
    // check (2 s) and no more.
    time_t const now = time(nullptr);
    uint32 const mana = uint32(BotGetPowerPct(bot, POWER_MANA));
    Unit* walkTo = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->FindMap() != bot->FindMap())
            continue;
        time_t& since = hurtSince[member->GetGUID()];
        if (member->GetHealthPct() >= float(HealerGuardrails::UNHEALED_HEALTH_PCT))
        {
            since = 0;
            continue;
        }
        if (!since)
            since = now;
        if (!HealerGuardrails::MemberStaysHurt(uint32(member->GetHealthPct()), uint32(now - since)))
            continue;
        float const distance = bot->GetDistance2d(member);
        if (distance > sPlayerbotAIConfig->sightDistance)
            continue;
        bool const inSight = bot->IsWithinLOSInMap(member);
        bool const inRange = distance <= sPlayerbotAIConfig->healDistance;
        char const* reason = HealerGuardrails::NoHealReason(inSight, inRange, mana);
        if (!reason[0])
            continue;
        constexpr time_t HEAL_LOG_GAP_SECONDS = 10;
        if (!lastHealLog || now - lastHealLog >= HEAL_LOG_GAP_SECONDS)
        {
            lastHealLog = now;
            SF_LOG_INFO("playerbots", "[HEAL] %s cannot heal %s at %u percent for %u s: %s (%.1f yd)", bot->GetName().c_str(),
                        member->GetName().c_str(), unsigned(member->GetHealthPct()), unsigned(now - since), reason, distance);
        }
        if (!walkTo && HealerGuardrails::ReasonNeedsWalk(inSight, inRange))
            walkTo = member;
    }
    return walkTo;
}
