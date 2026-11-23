/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "EoETriggers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

// AC Ai/Raid/EoE/EoEActions.cpp:9. 5.4.8: AC PlayerbotAI::IsMainTank -> PlayerBotSpec::IsMainTank, IsClass ->
// getClass; AC's commented-out death knight spark pull is left out.
bool MalygosPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
        return false;

    uint8 const phase = MalygosTrigger::getPhase(bot, boss);
    float constexpr distance = 5.0f;

    if (phase != 1)
        return false;

    Unit* spark = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (BotGuid const& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            spark = unit;
            break;
        }
    }

    // Position tank
    if (PlayerBotSpec::IsMainTank(bot))
    {
        if (bot->GetDistance2d(MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second) > distance)
            return MoveTo(EOE_MAP_ID, MALYGOS_MAINTANK_POSITION.first, MALYGOS_MAINTANK_POSITION.second,
                          bot->GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        return false;
    }

    if (spark)
        return false;

    if (bot->getClass() != CLASS_HUNTER &&
        bot->GetDistance2d(MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second) > (distance * 3.0f))
        return MoveTo(EOE_MAP_ID, MALYGOS_STACK_POSITION.first, MALYGOS_STACK_POSITION.second, bot->GetPositionZ(),
                      false, false, false, false, MovementPriority::MOVEMENT_COMBAT);

    return false;
}

// AC Ai/Raid/EoE/EoEActions.cpp:70. Phase 1: everyone but healers on Malygos. Phase 2: ranged on the Scions of
// Eternity, the rest on the Nexus Lords.
bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
        return false;

    uint8 const phase = MalygosTrigger::getPhase(bot, boss);
    if ((phase != 1 && phase != 2) || PlayerBotSpec::IsHeal(bot))
        return false;

    Unit* newTarget = nullptr;
    if (phase == 1)
        newTarget = boss;
    else
    {
        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (BotGuid const& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit)
                continue;

            if (unit->GetEntry() == NPC_NEXUS_LORD)
                nexusLord = unit;
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
                scionOfEternity = unit;
        }

        newTarget = (PlayerBotSpec::IsRangedDps(bot) && scionOfEternity) ? scionOfEternity : nexusLord;
    }

    if (!newTarget)
        return false;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget || currentTarget->GetEntry() != newTarget->GetEntry())
        return Attack(newTarget);

    return false;
}

// AC Ai/Raid/EoE/EoEActions.cpp:224
bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON;
}

// AC Ai/Raid/EoE/EoEActions.cpp:229. The bot steers its drake into formation behind the leader's drake, like the
// Oculus port (strategy/dungeons/OC/OCActions.cpp OccFlyDrakeAction). 5.4.8: AC's disabled boss branch ("boss &&
// false") is dropped; the raid size comes from Map::Is25ManRaid. AC also forced the drake's flight flag; the Skytalon
// is an air creature (InhabitType 4) that flies on its own, so the bot only steers it.
bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();
    if (!master)
        return false;

    Unit* masterVehicle = master->GetVehicleBase();
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !masterVehicle)
        return false;

    if (vehicleBase->GetExactDist(masterVehicle) <= 5.0f)
        return false;

    uint8 const numPlayers = bot->GetMap()->Is25ManRaid() ? 25 : 10;
    // 3/4 of a circle, with frontal cone 90 deg unobstructed
    float const angle = botAI->GetGroupSlotIndex(bot) * (2 * M_PI - M_PI_2) / numPlayers + M_PI_2;
    vehicleBase->GetMotionMaster()->MoveFollow(masterVehicle, 3.0f, angle);
    return true;
}

// AC Ai/Raid/EoE/EoEActions.cpp:273
bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON;
}

// AC Ai/Raid/EoE/EoEActions.cpp:279. The first members by guid heal (4 in 10 player mode, 10 in 25), the rest attack.
bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "possible targets");
        for (BotGuid const& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (unit && unit->GetEntry() == NPC_MALYGOS)
            {
                boss = unit;
                break;
            }
        }
    }
    if (!boss)
        return false;

    uint8 const numHealers = bot->GetMap()->Is25ManRaid() ? 10 : 4;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<uint64> sortedMembers;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        if (Player* member = itr->GetSource())
            sortedMembers.push_back(member->GetGUID());
    }
    std::sort(sortedMembers.begin(), sortedMembers.end());

    auto const found = std::find(sortedMembers.begin(), sortedMembers.end(), uint64(bot->GetGUID()));
    if (found == sortedMembers.end())
        return false;

    if (std::distance(sortedMembers.begin(), found) > numHealers)
        return DrakeDpsAction(boss);

    return DrakeHealAction();
}

// AC Ai/Raid/EoE/EoEActions.cpp:338. 5.4.8: AC's AddSpellCooldown(spellId, 0, 0) on the drake added nothing (a zero
// cooldown) and is dropped.
bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId)
{
    return botAI->CanCastVehicleSpell(spellId, target) && botAI->CastVehicleSpell(spellId, target);
}

uint8 EoEDrakeAttackAction::RiderComboPointsOn(Unit* target) const
{
    if (!target || bot->GetComboTarget() != target->GetGUID())
        return 0;

    return uint8(std::max<int8>(bot->GetComboPoints(), 0));
}

// AC Ai/Raid/EoE/EoEActions.cpp:349
bool EoEDrakeAttackAction::DrakeDpsAction(Unit* target)
{
    if (RiderComboPointsOn(target) >= 2)
        return CastDrakeSpellAction(target, SPELL_ENGULF_IN_FLAMES);

    return CastDrakeSpellAction(target, SPELL_FLAME_SPIKE);
}

// AC Ai/Raid/EoE/EoEActions.cpp:364. Revivify on the bot's own drake builds the points Life Burst spends.
bool EoEDrakeAttackAction::DrakeHealAction()
{
    if (RiderComboPointsOn(vehicleBase) >= 5)
        return CastDrakeSpellAction(vehicleBase, SPELL_LIFE_BURST);

    // AC: CanCastVehicleSpell reports bad targets for Revivify on a drake while the cast itself works, so it casts
    // without the check
    return botAI->CastVehicleSpell(SPELL_REVIVIFY, vehicleBase);
}
