#include "DpsTargetValue.h"

#include "AiObjectContext.h"
#include "AttackersValue.h"
#include "CcDutyTargetValue.h"
#include "Config.h"
#include "Creature.h"
#include "DefendPolicy.h"
#include "DpsTargetPolicy.h"
#include "Timer.h"
#include <set>
#include "Group.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "Strategy.h"

// #413 (AC DpsTargetValue.cpp:12-18): common base supplying the Dps exclusion type.
class DpsFindTargetStrategy : public FindTargetStrategy
{
public:
    DpsFindTargetStrategy(PlayerbotAI* botAI, Unit* held = nullptr) : FindTargetStrategy(botAI), heldMark(held) {}

    TargetValueExclusionType GetExclusionType() override { return TargetValueExclusionType::Dps; }

protected:
    Unit* heldMark;
};

class FindMaxThreatGapTargetStrategy : public DpsFindTargetStrategy
{
public:
    FindMaxThreatGapTargetStrategy(PlayerbotAI* botAI) : DpsFindTargetStrategy(botAI), minThreat(0) {}

    void CheckAttacker(Unit* attacker, ThreatManager* threatMgr) override
    {
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        Unit* victim = attacker->GetVictim();
        if (!result || CalcThreatGap(attacker, threatMgr) > CalcThreatGap(result, &result->getThreatManager()))
            result = attacker;
    }
    float CalcThreatGap(Unit* attacker, ThreatManager* threatMgr)
    {
        Unit* victim = attacker->GetVictim();
        return threatMgr->getThreat(victim) - threatMgr->getThreat(attacker);
    }

protected:
    float minThreat;
};

// caster
class CasterFindTargetSmartStrategy : public DpsFindTargetStrategy
{
public:
    CasterFindTargetSmartStrategy(PlayerbotAI* botAI, float dps, Unit* held)
        : DpsFindTargetStrategy(botAI, held), dps_(dps), targetExpectedLifeTime(1000000)
    {
        result = nullptr;
    }

    void CheckAttacker(Unit* attacker, ThreatManager* threatMgr) override
    {
        if (attacker == heldMark)
            return;
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            BotGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
            // BT-165: a square or triangle mob under control is left alone until the control breaks.
            if (CcDuty::IsControlledMark(group, attacker))
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        // [5-30] > (5-0] > (20-inf)
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        int32_t level = new_level;
        if (level % 10 == 2 || level % 10 == 0)
        {
            return new_time < old_time;
        }
        // dont switch targets when all of them with low health
        Unit* currentTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (currentTarget == new_unit)
        {
            return true;
        }
        if (currentTarget == old_unit)
        {
            return false;
        }
        return new_time > old_time;
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            PlayerBotSpec::IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        if (time >= 5 && time <= 30)
        {
            return level + 2;
        }
        if (time > 30)
        {
            return level;
        }
        return level + 1;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

// non caster
class NonCasterFindTargetSmartStrategy : public DpsFindTargetStrategy
{
public:
    NonCasterFindTargetSmartStrategy(PlayerbotAI* botAI, float dps, Unit* held)
        : DpsFindTargetStrategy(botAI, held), dps_(dps), targetExpectedLifeTime(1000000)
    {
    }

    void CheckAttacker(Unit* attacker, ThreatManager* threatMgr) override
    {
        if (attacker == heldMark)
            return;
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            BotGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
            // BT-165: a square or triangle mob under control is left alone until the control breaks.
            if (CcDuty::IsControlledMark(group, attacker))
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        // [5-20] > (5-0] > (20-inf)
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        // attack enemy in range and with lowest health
        int level = new_level;
        if (level == 10)
        {
            return new_time < old_time;
        }
        // all targets are far away, choose the closest one
        return botAI->GetBot()->GetDistance(new_unit) < botAI->GetBot()->GetDistance(old_unit);
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            PlayerBotSpec::IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        return level;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

// combo
class ComboFindTargetSmartStrategy : public DpsFindTargetStrategy
{
public:
    ComboFindTargetSmartStrategy(PlayerbotAI* botAI, float dps, Unit* held)
        : DpsFindTargetStrategy(botAI, held), dps_(dps), targetExpectedLifeTime(1000000)
    {
    }

    void CheckAttacker(Unit* attacker, ThreatManager* threatMgr) override
    {
        if (attacker == heldMark)
            return;
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            BotGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
            // BT-165: a square or triangle mob under control is left alone until the control breaks.
            if (CcDuty::IsControlledMark(group, attacker))
                return;
        }
        if (!attacker->IsAlive())
        {
            return;
        }
        if (foundHighPriority)
        {
            return;
        }
        if (IsHighPriority(attacker))
        {
            result = attacker;
            foundHighPriority = true;
            return;
        }
        float expectedLifeTime = attacker->GetHealth() / dps_;
        // Unit* victim = attacker->GetVictim();
        if (!result || IsBetter(attacker, result))
        {
            targetExpectedLifeTime = expectedLifeTime;
            result = attacker;
        }
    }
    bool IsBetter(Unit* new_unit, Unit* old_unit)
    {
        float new_time = new_unit->GetHealth() / dps_;
        float old_time = old_unit->GetHealth() / dps_;
        // [5-20] > (5-0] > (20-inf)
        int new_level = GetIntervalLevel(new_unit);
        int old_level = GetIntervalLevel(old_unit);
        if (new_level != old_level)
        {
            return new_level > old_level;
        }
        // attack enemy in range and with lowest health
        int level = new_level;
        Player* bot = botAI->GetBot();
        if (level == 10)
        {
            if (new_unit->GetGUID() == bot->GetComboTarget())
            {
                return true;
            }
            return new_time < old_time;
        }
        // all targets are far away, choose the closest one
        return bot->GetDistance(new_unit) < bot->GetDistance(old_unit);
    }
    int32_t GetIntervalLevel(Unit* unit)
    {
        float time = unit->GetHealth() / dps_;
        float dis = unit->GetDistance(botAI->GetBot());
        float attackRange =
            PlayerBotSpec::IsRanged(botAI->GetBot()) ? sPlayerbotAIConfig->spellDistance : sPlayerbotAIConfig->meleeDistance;
        attackRange += 5.0f;
        int level = dis < attackRange ? 10 : 0;
        return level;
    }

protected:
    float dps_;
    float targetExpectedLifeTime;
};

namespace
{
// BT-113: a marked target nobody fights yet is the tank's to pull (MarkPullStrategy waits for the group to be ready).
// A damage dealer that is not fighting either leaves it alone while the group has a living tank on its map.
bool IsTanksPull(Player* bot, Unit* marked, bool& attacked)
{
    // The same switch as the tank's marked pull (MarkTriggers.cpp): without it nobody would open on the mark.
    static bool const tankPullsMarks = sConfigMgr->GetBoolDefault("AiPlayerbot.TankPullsMarks", true);
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    attacked = false;
    bool hasTank = false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap())
            continue;
        hasTank |= member != bot && PlayerBotSpec::IsTank(member);
        for (Unit* unit : member->getAttackers())
            if (unit && unit->ToCreature() && unit->IsAlive() && unit->GetVictim() == member &&
                member->IsHostileTo(unit))
                attacked = true;
    }
    // BT-155: an unpulled mark yields to attackers even after the damage dealer enters combat.
    return DefendPolicy::HoldMark(marked->IsInCombat(), attacked, tankPullsMarks,
                                  bot->IsInCombat(), PlayerBotSpec::IsTank(bot), hasTank);
}
}  // namespace

Unit* DpsTargetValue::Calculate()
{
    Unit* rti = RtiTargetValue::Calculate();
    bool memberAttacked = false;
    bool holdMark = rti && IsTanksPull(bot, rti, memberAttacked);
    // BT-160: the attacker value releases an untouched mark after its 45-second fallback.
    // Keep defending higher priority if a group member is already under attack.
    if (holdMark && !memberAttacked && !bot->IsInCombat())
    {
        auto* attackers = dynamic_cast<AttackersValue*>(botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers"));
        if (attackers)
        {
            attackers->Get();
            if (attackers->HasReleasedMark(rti->GetGUID()))
                holdMark = false;
        }
    }
    Unit* held = holdMark ? rti : nullptr;
    Group* group = bot->GetGroup();
    if (group)
    {
        // BT-161: consider only pulled marks, the tank's engaged target, and group attackers.
        std::set<Unit*> candidates;
        std::set<uint64> tankTargets;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap()) continue;
            for (Unit* attacker : member->getAttackers())
                if (attacker && attacker->GetVictim() == member && member->IsHostileTo(attacker))
                    candidates.insert(attacker);
            if (PlayerBotSpec::IsTank(member) && member->GetTarget())
                tankTargets.insert(member->GetTarget());
        }
        GuidVector const existing = AI_VALUE(GuidVector, "attackers");
        for (BotGuid guid : existing)
            if (Unit* unit = botAI->GetUnit(guid)) candidates.insert(unit);
        for (unsigned icon : {DpsTargetPolicy::SkullIcon, DpsTargetPolicy::CrossIcon})
            if (BotGuid guid = group->GetTargetIcon(icon))
                if (Unit* unit = botAI->GetUnit(guid))
                    if (unit->IsInCombat()) candidates.insert(unit);
        for (uint64 guid : tankTargets)
            if (Unit* unit = botAI->GetUnit(BotGuid(guid)))
                if (unit->IsInCombat()) candidates.insert(unit);

        GuidSet const exclusions = GatherStrategyTargetExclusions(botAI, TargetValueExclusionType::Dps);
        Unit* best = nullptr;
        Unit* sticky = nullptr;
        DpsTargetPolicy::Priority bestRank = DpsTargetPolicy::Existing;
        for (Unit* unit : candidates)
        {
            if (!unit || !unit->ToCreature() || unit == held || !unit->IsAlive() || !unit->IsInCombat() ||
                unit->GetGUID() == group->GetTargetIcon(4) || CcDuty::IsControlledMark(group, unit) ||
                exclusions.count(unit->GetGUID()) ||
                !AttackersValue::IsValidTarget(unit, bot)) continue;
            if (unit->GetGUID() == stickyGuid) sticky = unit;
            Player* victim = unit->GetVictim() ? unit->GetVictim()->ToPlayer() : nullptr;
            bool const memberVictim = victim && victim->GetGroup() == group && !PlayerBotSpec::IsTank(victim);
            auto const rank = DpsTargetPolicy::Rank(memberVictim && PlayerBotSpec::IsHeal(victim), memberVictim,
                unit->GetGUID() == group->GetTargetIcon(DpsTargetPolicy::SkullIcon),
                unit->GetGUID() == group->GetTargetIcon(DpsTargetPolicy::CrossIcon),
                tankTargets.count(unit->GetGUID()) != 0);
            if (rank < bestRank) { bestRank = rank; best = unit; }
        }
        uint32 const now = getMSTime();
        if (DpsTargetPolicy::Keep(sticky != nullptr, now, stickySince)) return sticky;
        if (best)
        {
            if (stickyGuid != best->GetGUID()) { stickyGuid = best->GetGUID(); stickySince = now; }
            return best;
        }
        stickyGuid = 0;
    }
    else
        stickyGuid = 0;

    if (rti && !holdMark)
        return rti;

    // FindLeastHpTargetStrategy strategy(botAI);
    float dps = AI_VALUE(float, "estimated group dps");
    Unit* result = nullptr;
    if (group && PlayerBotSpec::IsCaster(bot))
    {
        CasterFindTargetSmartStrategy strategy(botAI, dps, held);
        result = TargetValue::FindTarget(&strategy);
    }
    else if (PlayerBotSpec::IsCombo(bot))
    {
        ComboFindTargetSmartStrategy strategy(botAI, dps, held);
        result = TargetValue::FindTarget(&strategy);
    }
    else
    {
        NonCasterFindTargetSmartStrategy strategy(botAI, dps, held);
        result = TargetValue::FindTarget(&strategy);
    }
    if (group && result && result->IsInCombat())
    {
        stickyGuid = result->GetGUID();
        stickySince = getMSTime();
    }
    return result;
}

class FindMaxHpTargetStrategy : public DpsFindTargetStrategy
{
public:
    FindMaxHpTargetStrategy(PlayerbotAI* botAI) : DpsFindTargetStrategy(botAI), maxHealth(0) {}

    void CheckAttacker(Unit* attacker, ThreatManager* threatMgr) override
    {
        if (Group* group = botAI->GetBot()->GetGroup())
        {
            BotGuid guid = group->GetTargetIcon(4);
            if (guid && attacker->GetGUID() == guid)
                return;
            // BT-165: a square or triangle mob under control is left alone until the control breaks.
            if (CcDuty::IsControlledMark(group, attacker))
                return;
        }

        if (!result || result->GetHealth() < attacker->GetHealth())
            result = attacker;
    }

protected:
    float maxHealth;
};

Unit* DpsAoeTargetValue::Calculate()
{
    Unit* rti = RtiTargetValue::Calculate();
    if (rti)
        return rti;

    FindMaxHpTargetStrategy strategy(botAI);
    return TargetValue::FindTarget(&strategy);
}
