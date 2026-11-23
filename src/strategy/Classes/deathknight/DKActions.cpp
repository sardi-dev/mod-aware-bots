/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "DKActions.h"

#include "BotSkill.h"
#include "Duration.h"
#include "GenericSpellActions.h"
#include "Playerbots.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

// The pause this action keeps between Raise Dead casts (its original intent).
static constexpr time_t RAISE_DEAD_COOLDOWN_SECONDS = 3 * 60;

NextAction** CastUnholyMeleeSpellAction::getPrerequisites()
{
    return NextAction::merge(NextAction::array(0, new NextAction("unholy presence"), nullptr),
                             CastMeleeSpellAction::getPrerequisites());
}

NextAction** CastFrostMeleeSpellAction::getPrerequisites()
{
    return NextAction::merge(NextAction::array(0, new NextAction("frost presence"), nullptr),
                             CastMeleeSpellAction::getPrerequisites());
}

NextAction** CastBloodMeleeSpellAction::getPrerequisites()
{
    return NextAction::merge(NextAction::array(0, new NextAction("blood presence"), nullptr),
                             CastMeleeSpellAction::getPrerequisites());
}

bool CastRaiseDeadAction::Execute(Event event)
{
    bool result = CastBuffSpellAction::Execute(event);
    if (!result)
    {
        return false;
    }
    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    // const SpellInfo *spellInfo = sSpellMgr->GetSpellInfo(spellId);
    // AddSpellCooldown takes an absolute end time in seconds; the old 3 * 60 * 1000 ended in 1970 and erased the
    // real cooldown, so Raise Dead could be recast on every free global.
    // AddSpellCooldown replaces the entry, so only extend a shorter one, never cut the real cooldown.
    if (bot->GetSpellCooldownDelay(spellId) < uint32(RAISE_DEAD_COOLDOWN_SECONDS))
        bot->AddSpellCooldown(spellId, 0, time(nullptr) + RAISE_DEAD_COOLDOWN_SECONDS);
    return true;
}

bool CastPestilenceAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    if (!botAI->HasAura("blood plague", target, false, true) && !botAI->HasAura("frost fever", target, false, true))
        return false;

    return CastSpellAction::isUseful();
}

namespace
{
// Class sheet, blood death knight (BT-134): Raise Ally costs 30 Runic Power, 30 percent of the 100 maximum.
constexpr float RAISE_ALLY_RUNIC_POWER_PCT = 30.0f;
}

// "good" tier: the base action checks Mind Freeze's own range (melee) on the caster the task 0 value picked.
bool CastMindFreezeOnPackCasterAction::isUseful()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && CastSpellOnEnemyCasterAction::isUseful();
}

// The cast does not test the Runic Power (CanCastSpell skips the cost), so it is read here; the distance test is the
// druid's (Rebirth), so a bot far from the corpse does not leave the fight for it.
bool CastRaiseAllyAction::isUseful()
{
    return ResurrectPartyMemberAction::isUseful() && BotGetPowerPct(bot, POWER_RUNIC_POWER) >= RAISE_ALLY_RUNIC_POWER_PCT &&
           AI_VALUE2(float, "distance", GetTargetName()) <= sPlayerbotAIConfig->spellDistance;
}
