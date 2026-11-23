/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ShamanActions.h"

#include "AiFactory.h"
#include "Item.h"
#include "Playerbots.h"
#include "Totem.h"

// Wave-74 fix (#858), see the class comment in ShamanActions.h.
std::string const CastUnleashElementsAction::GetTargetName()
{
    return AiFactory::GetPlayerSpecTab(bot) == Specializations::SPEC_SHAMAN_RESTORATION ? "self target" : "current target";
}

bool CastTotemAction::isUseful()
{
    if (needLifeTime > 0.1f && AI_VALUE(uint8, "attacker count") < 3)
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (!target)
        {
            return false;
        }
        float dps = AI_VALUE(float, "estimated group dps");
        if (target->GetHealth() / dps < needLifeTime)
        {
            return false;
        }
    }
    return CastBuffSpellAction::isUseful() && !AI_VALUE2(bool, "has totem", name) && !botAI->HasAura(buff, bot);
}

bool CastMagmaTotemAction::isUseful() {
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !bot->IsWithinMeleeRange(target))
        return false;

    return CastTotemAction::isUseful() && !AI_VALUE2(bool, "has totem", name); 
}

// 5.4.8 Fire Nova (1535) is cast from the caster and hits the enemies around it that carry the caster's Flame Shock
// (spell_sha_fire_nova); the Cataclysm-era fire totem it needed in WotLK is not involved. So it is worth casting
// when the bot's own Flame Shock is on the current target and that target is inside the nova's reach.
bool CastFireNovaAction::isUseful()
{
    if (!bot->HasSpell(1535))
        return false;
    // Enhancement class sheet: a second afflicted attacker must be in the nova's reach.
    unsigned count = 0;
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (enemy && enemy->IsAlive() && bot->GetDistance(enemy) <= FIRE_NOVA_REACH &&
            botAI->HasAura("flame shock", enemy, false, true) && ++count >= 2)
            return CastSpellAction::isUseful();
    }
    return false;

}

// #382 (AC GenericSpellActions.cpp:327-364): per-hand weapon imbues. AC's isPossible() rejects a missing item, a
// miscellaneous weapon, a fishing pole (main hand only in AC) and an item that already carries a temporary enchant,
// then asks CanCastSpell with the item as the target. BT-022: the main hand here accepts an imbued weapon, so the
// "main hand weapon no imbue" trigger can replace the previous spec's imbue after a respec (it fires at most every 5
// minutes and only for a wrong imbue the bot can replace). 5.4.8 spells the constant ITEM_SUBCLASS_WEAPON_MISCELLANEOUS
// (same value as AC's ITEM_SUBCLASS_WEAPON_MISC, see actions/SellAction.cpp:106-108).
bool CastShamanEnchantMainHandAction::Execute(Event /*event*/)
{
    Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    return item && botAI->CastSpell(spell, bot, item);
}

bool CastShamanEnchantMainHandAction::isPossible()
{
    Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!item || item->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_MISCELLANEOUS ||
        item->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    {
        return false;
    }

    return botAI->CanCastSpell(spell, bot, item);
}

bool CastShamanEnchantOffHandAction::Execute(Event /*event*/)
{
    Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    return item && botAI->CastSpell(spell, bot, item);
}

bool CastShamanEnchantOffHandAction::isPossible()
{
    Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!item || item->GetTemplate()->SubClass == ITEM_SUBCLASS_WEAPON_MISCELLANEOUS ||
        item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        return false;
    }

    return botAI->CanCastSpell(spell, bot, item);
}

// #431 (AC parity, AC ShamanActions.cpp:94-117 SetTotemAction::Execute): see the class comment in ShamanActions.h
// for why the totem is cast instead of written to the (non-existent on MoP) totem bar.
bool SetTotemAction::isUseful()
{
    return botAI->CanCastSpell(totemName, bot);
}

bool SetTotemAction::Execute(Event /*event*/)
{
    return botAI->CastSpell(totemName, bot);
}

// BT-677 to BT-681: see ShamanActions.h.
bool PvpElementalAction::isUseful()
{
    return PvpElementalGlue::Due(botAI, bot, rule);
}

bool PvpElementalAction::Execute(Event /*event*/)
{
    return PvpElementalGlue::Cast(botAI, bot, rule);
}
