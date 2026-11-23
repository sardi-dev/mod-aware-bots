/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PartyMemberToHealValue.h"

#include "Config.h"
#include "DpsOffhealPolicy.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "ServerFacade.h"
#include "WsgPlan.h"
#include "AbPlan.h"

class IsTargetOfHealingSpell : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MAX_HEALTH ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_HEAL_MECHANICAL)
                return true;
        }

        return false;
    }
};

Unit* PartyMemberToHeal::Calculate()
{
    IsTargetOfHealingSpell predicate;

    Group* group = bot->GetGroup();
    if (!group)
        return bot;

    // BT-161: an emergency hybrid heal must pick an injured player, not a pet or a
    // favored but healthier battleground target from the general healer ranking.
    if (botAI->HasStrategy("dps emergency offheal", BOT_STATE_COMBAT))
    {
        Player* lowest = nullptr;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || member->IsGameMaster() ||
                !DpsOffhealPolicy::Heal(true, member->GetHealthPct()) || !Check(member))
                continue;
            if (!lowest || member->GetHealthPct() < lowest->GetHealthPct()) lowest = member;
        }
        if (lowest) return lowest;
    }

    bool isRaid = bot->GetGroup()->isRaidGroup();
    MinValueCalculator calc(100);

    // #163 S8 (AC parity, PartyMemberToHeal.cpp:41-67): when the "focus heal targets" strategy is
    // active, heal only the hand-picked list instead of the whole group.
    if (botAI->HasStrategy("focus heal targets", BOT_STATE_COMBAT))
    {
        GuidVector const focusHealTargets = AI_VALUE(GuidVector, "focus heal targets");

        for (BotGuid const& focusHealTarget : focusHealTargets)
        {
            Player* player = ObjectAccessor::FindPlayer(focusHealTarget);
            if (!player || !player->IsInWorld() || !player->IsAlive() || !player->IsInSameGroupWith(bot))
                continue;

            float health = player->GetHealthPct();
            if (isRaid || health < sPlayerbotAIConfig->mediumHealth || !IsTargetOfSpellCast(player, predicate))
            {
                float probeValue = 100.0f;
                if (player->GetDistance2d(bot) > sPlayerbotAIConfig->healDistance)
                    probeValue = health + 30.0f;
                else
                    probeValue = health + player->GetDistance2d(bot) / 10.0f;

                if (probeValue < calc.minValue && Check(player))
                    calc.probe(probeValue, player);
            }
        }

        return (Unit*)calc.param;
    }

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        // BT-043: test the reference's player before its first use
        if (!player || player->IsGameMaster())
            continue;
        if (player->IsAlive())
        {
            uint8 health = player->GetHealthPct();
            if (isRaid || health < sPlayerbotAIConfig->mediumHealth || !IsTargetOfSpellCast(player, predicate))
            {
                uint32 probeValue = 100;
                if (player->GetDistance2d(bot) > sPlayerbotAIConfig->healDistance)
                {
                    probeValue = health + 30;
                }
                else
                {
                    probeValue = health + player->GetDistance2d(bot) / 10;
                }
                // BT-153 (Warsong Gulch playbook G6, good rule): the healer orders the team's flag carrier first once it is
                // hurt; a full-health carrier gets no head start. BT-156 (Arathi Basin playbook G7, good rule): the player
                // in the middle of the capture cast, and the attacked player next to one of our banners. Each returns 0
                // on the other map.
                if (health < 95)
                {
                    uint32 const favour = WsgPlan::HealPriority(bot, player) + AbPlan::HealPriority(bot, player);
                    probeValue = probeValue > favour ? probeValue - favour : 0;
                }
                // delay Check player to here for better performance
                if (probeValue < calc.minValue && Check(player))
                {
                    calc.probe(probeValue, player);
                }
            }
        }

        Pet* pet = player->GetPet();
        if (pet && pet->IsAlive())
        {
            uint8 health = ((Unit*)pet)->GetHealthPct();
            uint32 probeValue = 100;
            if (isRaid || health < sPlayerbotAIConfig->mediumHealth)
                probeValue = health + 30;
            // delay Check pet to here for better performance
            if (probeValue < calc.minValue && Check(pet))
            {
                calc.probe(probeValue, pet);
            }
        }

        Unit* charm = player->GetCharm();
        if (charm && charm->IsAlive())
        {
            uint8 health = charm->GetHealthPct();
            uint32 probeValue = 100;
            if (isRaid || health < sPlayerbotAIConfig->mediumHealth)
                probeValue = health + 30;
            // delay Check charm to here for better performance
            if (probeValue < calc.minValue && Check(charm))
            {
                calc.probe(probeValue, charm);
            }
        }
    }
    return (Unit*)calc.param;
}

bool PartyMemberToHeal::Check(Unit* player)
{
    // return player && player != bot && player->GetMapId() == bot->GetMapId() && player->IsInWorld() &&
    //     sServerFacade->GetDistance2d(bot, player) < ((player->GetTypeId() == TypeID::TYPEID_PLAYER) && botAI->IsTank((Player*)player) ? 50.0f
    //     : 40.0f);
    return player->GetMapId() == bot->GetMapId() && !player->IsCharmed() &&
        bot->GetDistance2d(player) < sPlayerbotAIConfig->healDistance * 2 && bot->IsWithinLOSInMap(player);
}

// BT-129: the body below was dead since the first import (AC's own copy of the value starts with the same return), so
// "protect party member" never fired. Repaired, not ported: no reference copy of the module has a working version.
// A group member, not the bot, who is the victim of an enemy within 30 yards of him and is about to die (a tank below 10
// percent, anyone else below 30) is the one to protect; the lowest health first. The value is recalculated on every read
// (check interval 1) and keeps no unit pointer. AiPlayerbot.ProtectPartyMember = 0 returns nothing, as before.
Unit* PartyMemberToProtect::Calculate()
{
    // Read once on first use, like the other local settings (MarkTriggers.cpp).
    static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.ProtectPartyMember", true);
    if (!enabled)
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // One pass with the lowest health kept; the attackers list is read in place (it has its own one second cache).
    Player* needProtect = nullptr;

    GuidVector const& attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->RefGet();
    for (GuidVector::const_iterator i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = botAI->GetUnit(*i);
        if (!unit || !unit->IsAlive())
            continue;

        Unit* pVictim = unit->GetVictim();
        Player* victim = pVictim ? pVictim->ToPlayer() : nullptr;
        if (!victim || victim == bot || !victim->IsAlive())
            continue;

        // An enemy of the group's fight can also be on a player of another group nearby, who is not ours to protect.
        if (victim->GetGroup() != group)
            continue;

        float attackDistance = 30.0f;
        if (sServerFacade->GetDistance2d(victim, unit) > attackDistance)
            continue;

        if (PlayerBotSpec::IsTank(victim) && victim->GetHealthPct() > 10)
            continue;
        else if (victim->GetHealthPct() > 30)
            continue;

        if (needProtect && victim->GetHealthPct() >= needProtect->GetHealthPct())
            continue;

        // PartyMemberValue::Check (last, it traces a line of sight): same map, not a game master, a sane distance from
        // the bot and in line of sight.
        if (Check(victim))
            needProtect = victim;
    }

    return needProtect;
}

// #163 S8 (AC parity, PartyMemberToHeal.cpp:138-162): the group's healer with the lowest mana %.
Unit* HealerLowMana::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    MinValueCalculator calc(100);

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->GetSource();
        if (!player || player == bot)
            continue;
        if (player->IsGameMaster() || !player->IsAlive())
            continue;
        if (!PlayerBotSpec::IsHeal(player))
            continue;

        float mana = BotGetPowerPct(player, POWER_MANA);
        if (mana < calc.minValue)
            calc.probe(mana, player);
    }

    return (Unit*)calc.param;
}
