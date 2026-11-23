/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ChooseTargetActions.h"
#include "BotScenario.h"
#include <map>

#include "BotSkill.h"
#include "Event.h"
#include "LootObjectStack.h"
#include "Playerbots.h"
#include "PvpTriggers.h"
#include "RtiTargetValue.h"
#include "ServerFacade.h"
#include "WsgPlan.h"

bool AttackEnemyPlayerAction::isUseful()
{
    // AC parity: a flag carrier does not pick fights
    if (PlayerHasFlag::IsCapturingFlag(bot))
        return false;

    return !sPlayerbotAIConfig->IsPvpProhibited(bot->GetZoneId(), bot->GetAreaId());
}

namespace
{
    // BT-153: how far a hunter of the plan reaches for the enemy carrier (the pvp target range of EnemyPlayerValue).
    float const HUNTER_REACH = 45.0f;

    // The slow, root, stun or fear of each class, tried in order (spell names as the class strategies use them). The
    // carrier is controlled once: nothing is cast while it is already stunned, rooted, feared or slowed.
    // Sheet section 9: class jobs for the enemy carrier. Polymorph is not used on a carrier (damage breaks it).
    char const* const* ControlSpellsOf(uint8 cls, size_t& count)
    {
        static char const* const warrior[] = { "hamstring" };
        static char const* const paladin[] = { "hammer of justice" };
        static char const* const hunter[] = { "concussive shot", "wing clip" };
        static char const* const rogue[] = { "kidney shot" };
        static char const* const priest[] = { "psychic scream" };
        static char const* const shaman[] = { "frost shock", "earthbind totem" };
        static char const* const mage[] = { "frost nova", "frostbolt" };
        static char const* const warlock[] = { "curse of exhaustion", "fear" };
        static char const* const monk[] = { "leg sweep" };
        static char const* const deathKnight[] = { "chains of ice" };
        static char const* const druid[] = { "entangling roots" };
        switch (cls)
        {
            case CLASS_WARRIOR: count = std::size(warrior); return warrior;
            case CLASS_PALADIN: count = std::size(paladin); return paladin;
            case CLASS_HUNTER: count = std::size(hunter); return hunter;
            case CLASS_ROGUE: count = std::size(rogue); return rogue;
            case CLASS_PRIEST: count = std::size(priest); return priest;
            case CLASS_SHAMAN: count = std::size(shaman); return shaman;
            case CLASS_MAGE: count = std::size(mage); return mage;
            case CLASS_WARLOCK: count = std::size(warlock); return warlock;
            case CLASS_MONK: count = std::size(monk); return monk;
            case CLASS_DEATH_KNIGHT: count = std::size(deathKnight); return deathKnight;
            case CLASS_DRUID: count = std::size(druid); return druid;
            default: count = 0; return nullptr;
        }
    }

    bool IsControlled(Unit* target)
    {
        return target->HasAuraType(SPELL_AURA_MOD_STUN) || target->HasAuraType(SPELL_AURA_MOD_ROOT) ||
               target->HasAuraType(SPELL_AURA_MOD_FEAR) || target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
    }
}

bool AttackEnemyFlagCarrierAction::isUseful()
{
    // AC parity (AC ChooseTargetActions.cpp:27-32): a carrier attacks the enemy carrier within 100 yd
    Unit* target = context->GetValue<Unit*>("enemy flag carrier")->Get();
    if (!target || !sServerFacade->IsDistanceLessOrEqualThan(sServerFacade->GetDistance2d(bot, target), 100.0f))
        return false;

    if (PlayerHasFlag::IsCapturingFlag(bot))
        return true;

    // BT-153 (Warsong Gulch plan, G5): the two bots the plan names as hunters go for the carrier too, and focus it.
    Battleground* bg = bot->GetBattleground();
    return bg && WsgPlan::Enabled(bg) && sServerFacade->IsDistanceLessOrEqualThan(sServerFacade->GetDistance2d(bot, target), HUNTER_REACH) &&
           WsgPlan::IsEfcHunter(bg, bot);
}

bool AttackEnemyFlagCarrierAction::Execute(Event event)
{
    Unit* target = GetTarget();
    Battleground* bg = bot->GetBattleground();

    // Expert line (sheet section 11): slow, root, stun or fear the carrier before the damage, once.
    if (target && bg && WsgPlan::Enabled(bg) && !PlayerHasFlag::IsCapturingFlag(bot) &&
        BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && !IsControlled(target))
    {
        size_t count = 0;
        char const* const* spells = ControlSpellsOf(bot->getClass(), count);
        for (size_t i = 0; i < count; ++i)
            if (botAI->CanCastSpell(spells[i], target) && botAI->CastSpell(spells[i], target))
                return true;
    }

    return AttackAction::Execute(event);
}

// #269 (AC ChooseTargetActions.cpp:34-40).
bool AggressiveTargetAction::isUseful()
{
    if (bot->IsInCombat())
        return false;

    return true;
}

bool AttackAnythingAction::isUseful()
{
    // AC parity (issue #61): AC ChooseTargetActions.cpp:104-133. The MoP-only status/25-yd gate and
    // "can move around" check are dropped; AC has neither.
    if (!bot || !botAI)
        return false;

    // Test harness: a wander scenario started with "nofight" does not start fights.
    if (BotScenario::NoFight(bot))
        return false;

    if (!botAI->AllowActivity(GRIND_ACTIVITY))
        return false;

    if (botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT))
        return false;

    if (bot->IsInCombat())
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsInWorld())
        return false;

    std::string const name = std::string(target->GetName());
    if (!name.empty() &&
        (name.find("Dummy") != std::string::npos || name.find("Charge Target") != std::string::npos ||
         name.find("Melee Target") != std::string::npos || name.find("Ranged Target") != std::string::npos))
        return false;

    return true;
}

bool DropTargetAction::Execute(Event event)
{
    Unit* target = context->GetValue<Unit*>("current target")->Get();
    if (target && target->isDead())
    {
        BotGuid guid = target->GetGUID();
        // #151 (AC ChooseTargetActions.cpp:49): re-enabled now that "available loot" is a real
        // LootObjectStack* (loot-stack-port.md section 2 step 4.6).
        if (guid)
            context->GetValue<LootObjectStack*>("available loot")->Get()->Add(guid);
    }

    context->GetValue<Unit*>("current target")->Set(nullptr);

    bot->SetTarget(BotGuid::Empty);
    bot->SetSelection(BotGuid());
    botAI->ChangeEngine(BOT_STATE_NON_COMBAT);
    // #269 (AC ChooseTargetActions.cpp:65-70): stop the hunter's auto shot, otherwise the pet keeps
    // hitting the corpse the target was dropped from.
    if (bot->getClass() == CLASS_HUNTER)
    {
        Spell const* spell = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
        if (spell && spell->m_spellInfo->Id == 75)
            bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
    }
    bot->AttackStop();

    return true;
}

bool AttackAnythingAction::Execute(Event event)
{
    bool result = AttackAction::Execute(event);
    if (!sPlayerbotAIConfig->debugBotNames.empty() && sPlayerbotAIConfig->debugBotNames.count(bot->GetName()))
        SF_LOG_DEBUG("playerbots", "[ATTACK] %s attack-anything result=%d target=%s", bot->GetName().c_str(), result ? 1 : 0,
                     GetTarget() ? GetTarget()->GetName().c_str() : "(none)");
    if (result)
    {
        if (Unit* grindTarget = GetTarget())
        {
            if (char const* grindName = grindTarget->GetName().c_str())
            {
                context->GetValue<BotGuid>("pull target")->Set(grindTarget->GetGUID());
                bot->GetMotionMaster()->Clear();
            }
        }
    }

    return result;
}

bool AttackAnythingAction::isPossible()
{
    return AttackAction::isPossible() && GetTarget();
}

bool DpsAssistAction::isUseful()
{
    // #269 (AC ChooseTargetActions.cpp:142-148): the flag-carrier guard was commented out in this
    // fork; a bot that is capturing a flag does not pick a DPS-assist target.
    if (PlayerHasFlag::IsCapturingFlag(bot))
        return false;

    return true;
}

bool AttackRtiTargetAction::Execute(Event event)
{
    Unit* rtiTarget = AI_VALUE(Unit*, "rti target");

    // #269 (AC ChooseTargetActions.cpp:154-169): fallback - if the "rti target" value did not resolve
    // a valid unit yet, try to resolve the raid icon directly from the group.
    if (!rtiTarget)
    {
        if (Group* group = bot->GetGroup())
        {
            std::string const rti = AI_VALUE(std::string, "rti");
            int32 const index = RtiTargetValue::GetRtiIndex(rti);
            if (index >= 0)
            {
                BotGuid const guid = group->GetTargetIcon(index);
                if (!guid.IsEmpty())
                    rtiTarget = botAI->GetUnit(guid);
            }
        }
    }

    if (rtiTarget && rtiTarget->IsInWorld() && rtiTarget->GetMapId() == bot->GetMapId())
    {
        botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Set({ rtiTarget->GetGUID() });
        bool result = Attack(botAI->GetUnit(rtiTarget->GetGUID()));
        if (result)
        {
            context->GetValue<BotGuid>("pull target")->Set(rtiTarget->GetGUID());
            return true;
        }
    }
    else
    {
        botAI->TellError("I dont see my rti attack target");
    }

    return false;
}

bool AttackRtiTargetAction::isUseful()
{
    if (botAI->ContainsStrategy(STRATEGY_TYPE_HEAL))
        return false;

    return true;
}
