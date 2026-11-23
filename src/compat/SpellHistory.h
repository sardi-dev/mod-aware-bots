/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * TrinityCore keeps cooldowns in a per-unit SpellHistory (unit->GetSpellHistory()); SkyFire keeps them on Player and
 * Creature directly. BotSpellHistory gives the module the TrinityCore calls it uses on top of SkyFire's. The module
 * writes BotSpellHistory(unit) where TrinityCore code writes unit->GetSpellHistory(); the object works both as a
 * pointer (->) and in a null check.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_SPELLHISTORY_H
#define MOD_AWARE_BOTS_COMPAT_SPELLHISTORY_H

#include "Creature.h"
#include "Player.h"
#include "SpellInfo.h"
#include "Unit.h"

#include <ctime>
#include <map>

struct BotCooldownEntry
{
    uint32 SpellId = 0;
    time_t CooldownEnd = 0;
    uint32 ItemId = 0;
};

class BotSpellHistory
{
public:
    explicit BotSpellHistory(Unit const* unit) : _unit(const_cast<Unit*>(unit)) { }

    BotSpellHistory* operator->() { return this; }
    explicit operator bool() const { return _unit != nullptr; }

    bool HasCooldown(uint32 spellId) const
    {
        if (Player const* player = _unit->ToPlayer())
            return player->HasSpellCooldown(spellId);
        if (Creature const* creature = _unit->ToCreature())
            return creature->HasSpellCooldown(spellId);
        return false;
    }

    bool HasCooldown(SpellInfo const* spellInfo) const { return spellInfo && HasCooldown(spellInfo->Id); }

    bool IsReady(uint32 spellId, uint32 /*itemId*/ = 0, bool /*ignoreCategoryCooldown*/ = false) const { return !HasCooldown(spellId); }
    bool IsReady(SpellInfo const* spellInfo, uint32 itemId = 0, bool ignoreCategoryCooldown = false) const
    {
        return spellInfo && IsReady(spellInfo->Id, itemId, ignoreCategoryCooldown);
    }

    // SkyFire stores cooldown ends in whole seconds.
    void AddCooldown(uint32 spellId, uint32 itemId, int64 cooldownMs, uint32 /*category*/ = 0, int64 /*categoryCooldownMs*/ = 0)
    {
        time_t const end = time(nullptr) + time_t((cooldownMs + 999) / 1000);
        if (Player* player = _unit->ToPlayer())
            player->AddSpellCooldown(spellId, itemId, end);
        else if (Creature* creature = _unit->ToCreature())
            creature->_AddCreatureSpellCooldown(spellId, end);
    }

    std::map<uint32, BotCooldownEntry> GetCooldowns() const
    {
        std::map<uint32, BotCooldownEntry> cooldowns;
        if (Player const* player = _unit->ToPlayer())
            for (auto const& [spellId, cooldown] : player->GetSpellCooldownMap())
                cooldowns[spellId] = BotCooldownEntry{ spellId, cooldown.end, cooldown.itemid };
        return cooldowns;
    }

private:
    Unit* _unit;
};

#endif
