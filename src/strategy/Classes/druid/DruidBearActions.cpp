/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "DruidBearActions.h"

#include "DruidTriggers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"

namespace
{
constexpr uint32 SPELL_SAVAGE_DEFENSE = 62606;
// The old rage floor of Maul, kept for a bear that is not a tank or has no Savage Defense to save rage for.
constexpr uint8 MAUL_RAGE_FLOOR = 45;
}  // namespace

// Class sheet, guardian druid (BT-131): a tank bear that knows Savage Defense (level 10) spends rage on Maul only above
// the cost of Savage Defense plus Maul, so the dodge is never starved; before level 10 and for other bears it is the old floor.
bool CastMaulAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;

    bool const saveForSavageDefense = PlayerBotSpec::IsTank(bot) && bot->HasSpell(SPELL_SAVAGE_DEFENSE);
    uint8 const minimumRage = saveForSavageDefense ? MAUL_RAGE_WITH_SAVAGE_DEFENSE : MAUL_RAGE_FLOOR;
    return AI_VALUE2(uint8, "rage", "self target") >= minimumRage;
}
