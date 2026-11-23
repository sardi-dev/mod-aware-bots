/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * Ported from the mod-playerbots module for AzerothCore (Ai/Base/Value/CcTargetValue.cpp) into the
 * Legends of Azeroth Pandaria fork (issue #149).
 */

#include "CcTargetValue.h"

#include "CcDutyTargetValue.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "ServerFacade.h"

class FindTargetForCcStrategy : public FindTargetStrategy
{
public:
    FindTargetForCcStrategy(PlayerbotAI* botAI, std::string const spell)
        : FindTargetStrategy(botAI), spell(spell), maxDistance(0.f)
    {
    }

public:
    void CheckAttacker(Unit* creature, ThreatManager* /*threatMgr*/) override
    {
        Player* bot = botAI->GetBot();
        // BT-165: a control mark belongs to its one controller. This bot's own duty target is answered before the
        // attacker walk (Calculate), so here it is skipped, and so is a mark that another bot controls.
        if (bot->GetGroup())
        {
            Unit* duty = botAI->GetAiObjectContext()->GetValue<Unit*>("cc duty target")->Get();
            if (creature == duty || CcDuty::IsOtherControllersMark(botAI, creature))
                return;
        }

        // #605: a solo bot can only ever return the RTI cc target (the group check below returns otherwise), so test
        // that before the CanCastSpell probe. Same result as AC (Ai/Base/Value/CcTargetValue.cpp:23-46); it only stops
        // the per-attacker CheckCast that logged Scare Beast 1513 / Freezing Trap "checkcast" 27-41x per solo hunter
        // leg on the mechanical dummy (the cc action itself never ran: no "scare beast on cc" A: line in wave 10).
        // BT-028 and BT-064: a solo mage polymorphs and a solo warlock fears the second attacker like a player, so
        // Polymorph and Fear skip this shortcut.
        if (!bot->GetGroup() && spell != "polymorph" && spell != "fear" && botAI->GetAiObjectContext()->GetValue<Unit*>("rti cc target")->Get() != creature)
            return;

        if (!botAI->CanCastSpell(spell, creature))
            return;

        if (botAI->GetAiObjectContext()->GetValue<Unit*>("rti cc target")->Get() == creature)
        {
            result = creature;
            return;
        }

        if (botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == creature)
            return;

        uint8 health = static_cast<uint8>(creature->GetHealthPct());
        if (health < sPlayerbotAIConfig->mediumHealth)
            return;

        float minDistance = botAI->GetRange("spell");
        Group* group = bot->GetGroup();
        if (!group)
        {
            result = creature;
            return;
        }

        if (botAI->GetAiObjectContext()->GetValue<uint8>("aoe count")->Get() > 2)
        {
            WorldLocation aoe = botAI->GetAiObjectContext()->GetValue<WorldLocation>("aoe position")->Get();
            if (sServerFacade->IsDistanceLessOrEqualThan(
                    sServerFacade->GetDistance2d(creature, aoe.GetPositionX(), aoe.GetPositionY()),
                    sPlayerbotAIConfig->aoeRadius))
                return;
        }

        uint32 tankCount = 0;
        uint32 dpsCount = 0;
        GetPlayerCount(creature, &tankCount, &dpsCount);
        if (!tankCount || !dpsCount)
        {
            result = creature;
            return;
        }

        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player* member = ObjectAccessor::FindPlayer(itr->guid);
            if (!member || !member->IsAlive() || member == bot)
                continue;

            if (!PlayerBotSpec::IsTank(member))
                continue;

            float distance = sServerFacade->GetDistance2d(member, creature);
            if (distance < minDistance)
                minDistance = distance;
        }

        if (!result || minDistance > maxDistance)
        {
            result = creature;
            maxDistance = minDistance;
        }
    }

private:
    std::string const spell;
    float maxDistance;
};

Unit* CcTargetValue::Calculate()
{
    // BT-165: the controller works on its mark before the pull too, when the mob is not an attacker yet.
    if (bot->GetGroup())
    {
        Unit* duty = AI_VALUE(Unit*, "cc duty target");
        // CanCastSpell lets an out of range cast through, so the range is checked here: out of range the approach
        // action walks the controller closer first (CcDuty::NeedsApproach).
        if (duty && CcDuty::SpellFitsTarget(qualifier, duty) && CcDuty::InControlRange(bot, duty) &&
            botAI->CanCastSpell(qualifier, duty))
            return duty;
    }

    FindTargetForCcStrategy strategy(botAI, qualifier);
    return FindTarget(&strategy);
}
