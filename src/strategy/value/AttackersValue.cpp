/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "AttackersValue.h"

#include "CellImpl.h"
#include "Config.h"
#include "Group.h"
#include "Log.h"
#include "GridObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "ReputationMgr.h"
#include "ServerFacade.h"
#include "StaleCombat.h"
#include "StaleCombatPolicy.h"
#include "TrainingDummyPolicy.h"

namespace
{
// BT-141: how long an untouched mark waits for the tank's marked pull before the others open on it anyway (a tank
// that cannot pull yet, or a group that never gets ready, must not leave the mark standing for good).
constexpr time_t HELD_MARK_FALLBACK_SECONDS = 45;

// A bot tank of this group that pulls marks itself (MarkPullStrategy): only then is an untouched mark its to open.
// A real player tank leads the pull himself, so nothing is held for him.
bool GroupHasMarkPullTank(Player* bot, Group* group)
{
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != bot->FindMap() || !PlayerBotSpec::IsTank(member))
            continue;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (memberAI && memberAI->HasStrategy("mark pull", BOT_STATE_NON_COMBAT))
            return true;
    }
    return false;
}
}  // namespace

// BT-141: a skull nobody fights yet belongs to the tank's marked pull, which waits until the group is ready. As an
// "attacker" it let tank assist and the damage dealers open on it at once. Five-player groups only: raids have several
// tanks and battlegrounds their own targets.
bool AttackersValue::HoldMarkForTank(Unit* marked)
{
    // The same switch as the tank's marked pull (MarkTriggers.cpp, DpsTargetValue.cpp IsTanksPull).
    static bool const tankPullsMarks = sConfigMgr->GetBoolDefault("AiPlayerbot.TankPullsMarks", true);
    Group* group = bot->GetGroup();
    if (!tankPullsMarks || marked->IsInCombat() || !group || group->isRaidGroup() || bot->InBattleground() ||
        !GroupHasMarkPullTank(bot, group))
        return false;

    time_t const now = time(nullptr);
    if (heldMark != marked->GetGUID())
    {
        heldMark = marked->GetGUID();
        heldMarkSince = now;
        heldMarkReleaseLogged = false;
    }
    if (now - heldMarkSince < HELD_MARK_FALLBACK_SECONDS)
        return true;
    // BT-160: record the fallback once for this bot and mark.
    if (!heldMarkReleaseLogged)
    {
        SF_LOG_INFO("playerbots", "[MARKPULL] %s releases held mark after %u seconds", bot->GetName().c_str(),
                    unsigned(HELD_MARK_FALLBACK_SECONDS));
        heldMarkReleaseLogged = true;
    }
    return false;
}

GuidVector AttackersValue::Calculate()
{
    std::unordered_set<Unit*> targets;

    GuidVector result;
    if (!botAI->AllowActivity(ALL_ACTIVITY))
        return result;

    AddAttackersOf(bot, targets);

    if (Group* group = bot->GetGroup())
        AddAttackersOf(group, targets);

    RemoveNonThreating(targets);

    // prioritized target
    GuidVector prioritizedTargets = AI_VALUE(GuidVector, "prioritized targets");
    for (BotGuid target : prioritizedTargets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && IsValidTarget(unit, bot))
        {
            targets.insert(unit);
        }
    }
    if (Group* group = bot->GetGroup())
    {
        BotGuid skullGuid = group->GetTargetIcon(7);
        Unit* skullTarget = botAI->GetUnit(skullGuid);
        if (skullTarget && IsValidTarget(skullTarget, bot) && !HoldMarkForTank(skullTarget))
        {
            targets.insert(skullTarget);
        }
    }

    for (Unit* unit : targets)
        result.push_back(unit->GetGUID());

    AddStaleCombatTarget(result);

    if (bot->duel && bot->duel->opponent)
        result.push_back(bot->duel->opponent->GetGUID());

    // players are never on threat lists: add valid enemy players in arenas, and within 40 yd in battlegrounds (the
    // distance an idle bot picks enemy players from)
    if (bot->InBattleground())
    {
        GuidVector possibleTargets = AI_VALUE(GuidVector, "possible targets");
        for (BotGuid const guid : possibleTargets)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (unit && (unit->GetTypeId() == TypeID::TYPEID_PLAYER) && IsValidTarget(unit, bot) &&
                (bot->InArena() || bot->IsWithinDist(unit, 40.0f)))
            {
                result.push_back(unit->GetGUID());
            }
        }
    }

    return result;
}

// BT-168: see StaleCombatPolicy.h. A bot flagged in combat with nothing attacking it in sight for 15 s attacks the
// nearest creature that still holds it in combat, without the line of sight test (the Mine Bunny stood out of sight), so
// the combat engine walks to it instead of idling in a fight that is not there.
void AttackersValue::AddStaleCombatTarget(GuidVector& result)
{
    time_t const now = time(nullptr);
    // BT-179: an attacker out of the bot's sight lets the clock run (StaleCombat::AttackedInSight); the targets in
    // `result` already passed the sight test.
    bool const attacked = !result.empty() || StaleCombat::AttackedInSight(bot);
    quietSince = StaleCombatPolicy::NextQuietSince(bot->IsInCombat(), attacked, quietSince, now);
    if (!StaleCombatPolicy::IsStale(bot->IsInCombat(), quietSince, now))
    {
        staleTarget = 0;
        staleTargetSince = 0;
        return;
    }

    Unit* nearest = nullptr;
    for (HostileReference* ref = bot->getHostileRefManager().getFirst(); ref; ref = ref->next())
    {
        Unit* holder = ref->GetSource()->GetOwner();
        if (!holder || !holder->ToCreature() || !holder->IsAlive() || !IsPossibleTarget(holder, bot) ||
            sServerFacade->GetDistance2d(bot, holder) >= sPlayerbotAIConfig->sightDistance)
            continue;
        if (!nearest || bot->GetDistance(holder) < bot->GetDistance(nearest))
            nearest = holder;
    }
    if (!nearest)
        return;

    result.push_back(nearest->GetGUID());
    if (staleTarget != nearest->GetGUID())
    {
        staleTarget = nearest->GetGUID();
        staleTargetSince = now;
    }
    if (staleLoggedFor != nearest->GetGUID())
    {
        staleLoggedFor = nearest->GetGUID();
        SF_LOG_INFO("playerbots", "[DEFEND] %s flagged in combat with nothing attacking for %u s, attacks %s (entry %u) at %.1f yd",
                    bot->GetName().c_str(), unsigned(now - quietSince), nearest->GetName().c_str(), nearest->GetEntry(),
                    bot->GetDistance(nearest));
    }
}

bool AttackersValue::KeepsStaleTargetWithoutSight(PlayerbotAI* botAI, Unit* target)
{
    if (!botAI || !target)
        return false;

    // The stored state of the last reading is enough: the value is read every tick in combat and the window is 20 s.
    auto* attackers = dynamic_cast<AttackersValue*>(botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers"));
    return attackers && attackers->staleTarget == target->GetGUID() &&
        StaleCombatPolicy::KeepWithoutSight(attackers->staleTargetSince, time(nullptr));
}

void AttackersValue::AddAttackersOf(Group* group, std::unordered_set<Unit*>& targets)
{
    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || !member->IsAlive() || member == bot || member->GetMapId() != bot->GetMapId() ||
            sServerFacade->GetDistance2d(bot, member) > sPlayerbotAIConfig->sightDistance)
            continue;

        AddAttackersOf(member, targets);
    }
}

struct AddGuardiansHelper
{
    explicit AddGuardiansHelper(std::vector<Unit*>& units) : units(units) {}

    void operator()(Unit* target) const { units.push_back(target); }

    std::vector<Unit*>& units;
};

void AttackersValue::AddAttackersOf(Player* player, std::unordered_set<Unit*>& targets)
{
    if (!player || !player->IsInWorld() || player->IsBeingTeleported())
        return;

    for (HostileReference* ref = player->getHostileRefManager().getFirst(); ref; ref = ref->next())
    {
        Unit* attacker = ref->GetSource()->GetOwner();
        if (!attacker)
            continue;

        if (player->IsValidAttackTarget(attacker) &&
            player->GetDistance2d(attacker) < sPlayerbotAIConfig->sightDistance)
        {
            targets.insert(attacker);
        }
    }
}

void AttackersValue::RemoveNonThreating(std::unordered_set<Unit*>& targets)
{
    for (std::unordered_set<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        Unit* unit = *tIter;
        if (bot->GetMapId() != unit->GetMapId() || !hasRealThreat(unit) || !IsValidTarget(unit, bot))
        {
            std::unordered_set<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
            ++tIter;
    }
}

bool AttackersValue::hasRealThreat(Unit* attacker)
{
    return attacker && attacker->IsInWorld() && attacker->IsAlive() && !attacker->IsPolymorphed() &&
        // !attacker->isInRoots() &&
        !attacker->IsFriendlyTo(bot);
    (attacker->getThreatManager().getCurrentVictim() || dynamic_cast<Player*>(attacker));
}

// #72 perf + AC parity: AC AttackersValue::IsPossibleTarget (AC Ai/Base/Value/AttackersValue.cpp:130-244), ported 1:1.
// This fork still had the old single-expression form: for every candidate unit of "possible targets", "nearest enemy
// players", "attackers" and "invalid target" it first looked up the raid target icon, the master's threat and the
// master's PlayerbotAI and ran the PvP-zone check, and only then the cheap rejects. AC rejects with early returns in
// the order below and only reaches the creature-claim checks for a creature that passed every generic check.
// Behaviour follows AC: the "attack tagged" strategy is no longer gated by a bot-only group, a creature already in
// combat with the bot counts as claimed, a creature fighting another player's pet/guardian is that player's, and a
// unit immune to all damage is skipped. The #397 PvP-prohibited duel exemption is AC's own (AC :174-197).
bool AttackersValue::IsPossibleTarget(Unit* attacker, Player* bot, float /*range*/)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!botAI)
        return false;

    // Basic check
    if (!attacker)
        return false;

    // bool inCannon = botAI->IsInVehicle(false, true);
    // bool enemy = botAI->GetAiObjectContext()->GetValue<Unit*>("enemy player target")->Get();

    // Validity checks
    if (!attacker->IsVisible() || !attacker->IsInWorld() || attacker->GetMapId() != bot->GetMapId())
        return false;

    if (attacker->isDead() || attacker->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        return false;

    // Flag checks
    if (attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNK_16))
        return false;

    if (attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC) || attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE))
        return false;

    // Skip targets that are immune to all damage (e.g., Ice Block, Divine Shield)
    if (attacker->IsImmunedToDamage(SPELL_SCHOOL_MASK_NORMAL) && attacker->IsImmunedToDamage(SPELL_SCHOOL_MASK_MAGIC))
        return false;

    // Relationship checks
    if (attacker->IsFriendlyTo(bot))
        return false;

    // Critter exception
    if (attacker->GetCreatureType() == CREATURE_TYPE_CRITTER && !attacker->IsInCombat())
        return false;

    // BT-163: immortal training dummies only when the human leader picked one.
    if (Creature* creature = attacker->ToCreature())
    {
        CreatureTemplate const* info = creature->GetCreatureTemplate();
        bool const dummy = info && TrainingDummyPolicy::IsDummyScript(sObjectMgr->GetScriptName(info->ScriptID));
        Player* master = botAI->GetMaster();
        bool const leaderTargetsIt = master && !GET_PLAYERBOT_AI(master) && master->GetTarget() == attacker->GetGUID();
        if (!TrainingDummyPolicy::Allowed(dummy, leaderTargetsIt))
            return false;
    }

    // Visibility check
    if (!bot->CanSeeOrDetect(attacker))
        return false;

    // PvP prohibition checks (skip for duels)
    if ((BotGuid(attacker->GetGUID()).IsPlayer() || BotGuid(attacker->GetGUID()).IsPet()) &&
        (!bot->duel || bot->duel->opponent != attacker) &&
        (sPlayerbotAIConfig->IsPvpProhibited(attacker->GetZoneId(), attacker->GetAreaId()) ||
            sPlayerbotAIConfig->IsPvpProhibited(bot->GetZoneId(), bot->GetAreaId())))
    {
        // This will stop aggresive pets from starting an attack.
        // This will stop currently attacking pets from continuing their attack.
        // This will first require the bot to change from a combat strat. It will
        // not be reached if the bot only switches targets, including NPC targets.
        for (Unit::ControlList::const_iterator itr = bot->m_Controlled.begin(); itr != bot->m_Controlled.end(); ++itr)
        {
            Creature* creature = dynamic_cast<Creature*>(*itr);
            if (creature && creature->GetVictim() == attacker)
            {
                creature->AttackStop();
                if (CharmInfo* charmInfo = creature->GetCharmInfo())
                    charmInfo->SetIsCommandAttack(false);
            }
        }

        return false;
    }

    // Unflagged player check
    if ((attacker->GetTypeId() == TypeID::TYPEID_PLAYER) && !attacker->IsPvP() && !BotIsFFAPvP(attacker) &&
        (!bot->duel || bot->duel->opponent != attacker))
        return false;

    // Creature-specific checks
    Creature* c = attacker->ToCreature();
    if (c)
    {
        if (c->IsInEvadeMode())
            return false;

        bool leaderHasThreat = false;
        if (bot->GetGroup() && botAI->GetMaster())
            leaderHasThreat = attacker->getThreatManager().getThreat(botAI->GetMaster());

        // A player claims a creature by damaging it or landing a hostile spell on it, which is what
        // sets its loot recipient. The below code covers anti-kill-stealing mechanics.
        //
        // (1) Whatever the claim, the bot may attack if it (a) is in a raid/group and has a master
        //     holding nonzero threat on the creature, (b) has, or has a raid/group member that has,
        //     already tapped it, or (c) is already in combat with the creature.
        if (leaderHasThreat || c->isTappedBy(bot) || BotCombatManager(c).IsInCombatWith(bot))
            return true;

        // (2) Nobody has claimed the creature, so ask who it is attacking. If it is not attacking
        //     anything, or if it is attacking (a) something with no player behind it (e.g., a
        //     critter), (b) the bot, (c) the bot's master, or (d) a member of the bot's raid/group,
        //     then the victim is considered to belong to the bot and may be attacked. Clauses (b)
        //     through (d) also include a player's pets, guardians, totems, and charms.
        if (!c->hasLootRecipient())
        {
            Unit* victim = c->GetVictim();
            Player* victimOwner = victim ? victim->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
            if (!victim || !victimOwner || victimOwner == bot || victimOwner == botAI->GetMaster() ||
                (bot->GetGroup() && bot->GetGroup() == victimOwner->GetGroup()))
                return true;
        }

        // (3) Last because it is applied automatically only in battlegrounds and arenas: the
        //     "attack tagged" strategy always allows for attacking of creatures.
        return botAI->HasStrategy("attack tagged", BOT_STATE_NON_COMBAT);
    }

    return true;
}

bool AttackersValue::IsValidTarget(Unit* attacker, Player* bot)
{
    return IsPossibleTarget(attacker, bot) && bot->IsWithinLOSInMap(attacker);
    // (attacker->GetThreatMgr().getCurrentVictim() || attacker->GetGuidValue(UNIT_FIELD_TARGET) ||
    // BotGuid(attacker->GetGUID()).IsPlayer() || attacker->GetGUID() ==
    // GET_PLAYERBOT_AI(bot)->GetAiObjectContext()->GetValue<BotGuid>("pull target")->Get());
}

bool PossibleAddsValue::Calculate()
{
    GuidVector possible = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();

    for (BotGuid const guid : possible)
    {
        if (find(attackers.begin(), attackers.end(), guid) != attackers.end())
            continue;

        if (Unit* add = botAI->GetUnit(guid))
        {
            if (!add->GetTarget() && !add->getThreatManager().getCurrentVictim() && add->IsHostileTo(bot))
            {
                for (BotGuid const attackerGUID : attackers)
                {
                    Unit* attacker = botAI->GetUnit(attackerGUID);
                    if (!attacker)
                        continue;

                    float dist = sServerFacade->GetDistance2d(attacker, add);
                    if (sServerFacade->IsDistanceLessOrEqualThan(dist, sPlayerbotAIConfig->aoeRadius * 1.5f))
                        continue;

                    if (sServerFacade->IsDistanceLessOrEqualThan(dist, sPlayerbotAIConfig->aggroDistance))
                        return true;
                }
            }
        }
    }

    return false;
}
