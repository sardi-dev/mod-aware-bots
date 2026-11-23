#include "ThreatStrategy.h"

#include "GenericSpellActions.h"
#include "DpsThreatPolicy.h"
#include "DpsOffhealPolicy.h"
#include "DpsCooldownPolicy.h"
#include "PvpBurst.h"
#include "Creature.h"
#include <set>
#include "Group.h"
#include "PlayerbotSpec.h"
#include "Map.h"
#include "Playerbots.h"

float ThreatMultiplier::GetValue(Action* action)
{
    if (AI_VALUE(bool, "neglect threat"))
    {
        return 1.0f;
    }

    if (!action || action->getThreatType() == Action::ActionThreatType::None)
        return 1.0f;

    if (!AI_VALUE(bool, "group"))
        return 1.0f;

    // BT-161: utility and self-defence must not be suppressed by the damage gate.
    CastSpellAction* spell = dynamic_cast<CastSpellAction*>(action);
    bool const exempt = dynamic_cast<CastHealingSpellAction*>(action) ||
        dynamic_cast<CastBuffSpellAction*>(action) || dynamic_cast<CastProtectSpellAction*>(action) ||
        (spell && DpsThreatPolicy::IsInterruptOrControl(spell->getSpell()));
    if (exempt)
        return 1.0f;
    Unit* target = action->GetTarget();
    bool const victimIsBot = target && target->GetVictim() == botAI->GetBot();
    bool const area = action->getThreatType() == Action::ActionThreatType::Aoe;
    uint8 const areaThreat = area ? AI_VALUE2(uint8, "threat", "aoe") : 0;
    uint8 const singleThreat = AI_VALUE2(uint8, "threat", "current target");
    if (DpsThreatPolicy::Block(false, victimIsBot, area, singleThreat, areaThreat))
        return 0.0f;

    return 1.0f;
}

void ThreatStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new ThreatMultiplier(botAI));
}

float FocusMultiplier::GetValue(Action* action)
{
    if (!action)
    {
        return 1.0f;
    }
    if (action->getThreatType() == Action::ActionThreatType::Aoe && !dynamic_cast<CastHealingSpellAction*>(action))
    {
        return 0.0f;
    }
    if (dynamic_cast<CastDebuffSpellOnAttackerAction*>(action))
    {
        return 0.0f;
    }
    return 1.0f;
}

void FocusStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new FocusMultiplier(botAI));
}

// BT-161: emergency offhealing uses the normal party-heal target and spell actions.
bool DpsOffhealTrigger::IsActive()
{
    Group* group = bot->GetGroup();
    if (!group) { enabled = false; return false; }
    bool hasHealer = false;
    bool lowMana = false;
    float highestMana = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap() || !PlayerBotSpec::IsHeal(member))
            continue;
        hasHealer = true;
        float const mana = BotGetPowerPct(member, POWER_MANA);
        if (mana > highestMana) highestMana = mana;
        if (mana < DpsOffhealPolicy::StartMana) lowMana = true;
    }
    enabled = DpsOffhealPolicy::Enabled(enabled, hasHealer, highestMana, lowMana);
    Unit* target = AI_VALUE(Unit*, "party member to heal");
    return target && target->ToPlayer() && DpsOffhealPolicy::Heal(enabled, target->GetHealthPct());
}

// BT-161: existing hybrid party-heal triggers must obey the same stop condition.
float DpsOffhealMultiplier::GetValue(Action* action)
{
    HealPartyMemberAction* heal = dynamic_cast<HealPartyMemberAction*>(action);
    if (!heal) return 1.0f;
    // BT-161: a solo hybrid heals itself as before; the limit is only for a group with a healer.
    if (!botAI->GetBot()->GetGroup()) return 1.0f;
    // BT-161: these protective abilities use the heal action base but do not heal.
    if (heal->getSpell() == "divine protection" || heal->getSpell() == "guardian spirit") return 1.0f;
    Trigger* trigger = botAI->GetAiObjectContext()->GetTrigger("dps emergency offheal");
    return trigger && trigger->IsActive() ? 1.0f : 0.0f;
}

void DpsOffhealStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new DpsOffhealMultiplier(botAI));
}

void DpsOffhealStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    char const* action = nullptr;
    switch (botAI->GetBot()->getClass())
    {
        case CLASS_PRIEST: action = "flash heal on party"; break;
        case CLASS_SHAMAN: action = "healing surge on party"; break;
        case CLASS_DRUID: action = "healing touch on party"; break;
        case CLASS_PALADIN: action = "flash of light on party"; break;
        case CLASS_MONK: action = "surging mist on party"; break;
    }
    if (action)
        triggers.push_back(new TriggerNode("dps emergency offheal", NextAction::array(0,
            new NextAction(action, ACTION_EMERGENCY - 1), nullptr)));
}

// BT-161: one multiplier covers spell-specific boost triggers as well as Bloodlust and Heroism.
float DpsCooldownMultiplier::GetValue(Action* action)
{
    Player* bot = botAI->GetBot();
    if (!action || PlayerBotSpec::IsHeal(bot) || PlayerBotSpec::IsTank(bot))
        return 1.0f;
    // BT-161: solo bots keep their cooldowns for leveling, and a hurt bot may always use a long defensive (Ice Block,
    // Divine Shield) to survive; the gate only saves damage cooldowns in a group.
    if (!bot->GetGroup() || bot->GetHealthPct() < DpsCooldownPolicy::SelfDefenceHealthPct)
        return 1.0f;
    CastSpellAction* cast = dynamic_cast<CastSpellAction*>(action);
    // A combat resurrection (Rebirth, Raise Ally: ten minutes of recovery) brings back a dead member whatever the pack
    // is; gated like Heroism it was cast only against a boss or two elites, and so was Innervate on a healer without mana.
    if (!cast || dynamic_cast<CastHealingSpellAction*>(action) || dynamic_cast<CastProtectSpellAction*>(action) ||
        dynamic_cast<ResurrectPartyMemberAction*>(action))
        return 1.0f;
    std::string const name = cast->getSpell();
    if (DpsCooldownPolicy::ServesTheGroup(name))
        return 1.0f;
    uint32 const spellId = AI_VALUE2(uint32, "spell id", name);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    bool const lust = name == "bloodlust" || name == "heroism";
    if (!DpsCooldownPolicy::NeedsGate(info ? info->GetRecoveryTime() : 0, lust))
        return 1.0f;

    // BT-620: against an enemy player the boss and elite gate below can never open (one target, no pack), so in a battleground
    // or an arena group every cooldown of three minutes or more (Berserk, Mirror Image, Alter Time, Icy Veins ...) was held for
    // good. A player target is judged by the burst window instead: the same moment the class trigger asks for.
    Unit* playerTarget = AI_VALUE(Unit*, "current target");
    if (PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, playerTarget))
        return PvpBurstGlue::Open(botAI, bot, playerTarget) ? 1.0f : 0.0f;

    std::set<Unit*> attackers;
    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap()) continue;
            for (Unit* attacker : member->getAttackers())
                if (attacker && attacker->IsAlive() && attacker->GetVictim() == member && member->IsHostileTo(attacker))
                    attackers.insert(attacker);
        }
    }
    else
        for (Unit* attacker : bot->getAttackers())
            if (attacker && attacker->IsAlive() && attacker->GetVictim() == bot && bot->IsHostileTo(attacker))
                attackers.insert(attacker);

    // BT-161: the existing threat-based value also sees casters that are not auto-attacking.
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* attacker = botAI->GetUnit(guid);
        Unit* victim = attacker ? attacker->GetVictim() : nullptr;
        Player* player = victim ? victim->ToPlayer() : nullptr;
        if (attacker && attacker->IsAlive() && player && player->IsAlive() &&
            (player == bot || (group && player->GetGroup() == group)) && player->IsHostileTo(attacker))
            attackers.insert(attacker);
    }

    unsigned elites = 0;
    bool boss = false;
    for (Unit* attacker : attackers)
    {
        Creature* creature = attacker->ToCreature();
        if (!creature) continue;
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        boss |= creature->IsDungeonBoss() || (info && info->rank == CREATURE_ELITE_WORLDBOSS);
        if (creature->isElite()) ++elites;
    }
    return DpsCooldownPolicy::Allowed(boss, elites) ? 1.0f : 0.0f;
}

void DpsCooldownStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new DpsCooldownMultiplier(botAI));
}
