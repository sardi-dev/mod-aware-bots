/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "ShamanTriggers.h"

#include "AiFactory.h"
#include "BotSkill.h"
#include "Creature.h"
#include "Helper.h"
#include "HealerTriggers.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "SpellAuras.h"
#include "Unit.h"
#include <ctime>

static constexpr uint32 SPELL_CALL_OF_THE_ELEMENTS = 66842;
static constexpr uint32 SPELL_TOTEMIC_RECALL = 36936;
static constexpr uint32 SPELL_ELEMENTAL_MASTERY = 16166;
static constexpr uint32 SPELL_FLAMETONGUE_WEAPON = 8024;
static constexpr uint32 SPELL_WINDFURY_WEAPON = 8232;
static constexpr uint32 SPELL_EARTHLIVING_WEAPON = 51730;
static constexpr uint32 SPIRIT_WALK_SPELL_ID = 58875;

// ---------------------------------------------------------------------------
// Totem slot helpers
//
// AC resolves an element through bot->m_SummonSlot[SUMMON_SLOT_TOTEM_<ELEMENT>] in
// No<Totem>Trigger::IsActive, CallOfTheElementsTrigger::IsActive and TotemicRecallTrigger::IsActive
// (.cpp:262-447), and through bot->GetActionButton(TOTEM_BAR_SLOT_*) in SetTotemTrigger::IsActive
// (.cpp:465). This fork has the summon slot (Unit.h:1008-1021 for the enum, Unit.h:2083 for the
// std::array<BotGuid, SUMMON_SLOT_MAX> m_SummonSlot storage) but no Player::GetActionButton, so
// the bar read is substituted with the creature actually standing in the element's summon slot.
//
// AC then compares the standing creature's UNIT_CREATED_BY_SPELL against the spell the active element
// strategy wants. MoP re-numbered every shaman totem, so AC's WotLK spell-id tables
// (Strategy/TotemsShamanStrategy.h) no longer resolve here; the summon creature's name is used
// instead. That is the identity the rest of this fork already uses - HasTotemValue.cpp:20 compares the
// same string and ShamanActions.h casts every totem by name.
// ---------------------------------------------------------------------------

// #72 perf + AC parity: AC gates every totem trigger on the spellbook only - bot->HasSpell(<baseline totem rank 1>)
// in No<Element>TotemTrigger (AC ShamanTriggers.cpp:290, 331, 373, 416), in CallOfTheElementsTrigger's per-slot count
// (AC :169-173) and in SetTotemTrigger (AC :451-461). This fork called botAI->CanCastSpell() there, i.e. a full
// Spell + Spell::CheckCast per trigger check (93 us vs AC 2.5 us per "no water totem" check, w15 pmon). MoP renumbered
// the totems, so the spell is resolved by name through the cached "spell id" value (0 when the bot does not know it),
// the same lookup CanCastSpell(name) starts with; cooldown, range and cast checks stay in the cast action, as in AC.
static bool BotKnowsTotemSpell(PlayerbotAI* botAI, Player* bot, std::string const& spellName)
{
    uint32 const spellId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", spellName)->Get();
    return spellId && bot->HasSpell(spellId);
}

// The creature standing in one of the four element summon slots, or nullptr when the slot is empty
// or the creature is no longer loaded.
static Creature* TotemInSlot(Player* bot, uint8 elementSlot)
{
    BotGuid const guid = bot->m_SummonSlot[elementSlot];
    if (guid.IsEmpty())
        return nullptr;

    return bot->GetMap()->GetCreature(guid);
}

// AC's per-element "is this slot occupied" core: the slot holds a totem of the wanted kind that the
// bot can still see (AC's 30.0f yard range check).
static bool TotemSlotHolds(Player* bot, uint8 elementSlot, char const* summonTotemName)
{
    Creature* totem = TotemInSlot(bot, elementSlot);
    return totem && totem->GetDistance(bot) <= 30.0f && caseInsensitiveEqual(totem->GetName().c_str(), summonTotemName);
}

// AC's GetRequiredTotemSpellId (.cpp:265-285) answers "which totem does the strategy for this slot
// want?" by asking botAI->HasStrategy() for each per-element strategy name. This fork only registers
// the single generic "totems" strategy (ShamanAiObjectContext.cpp:25); #431 added the per-element
// strategies (ShamanAiObjectContext.cpp, Earth/Fire/Water/AirTotemStrategyFactoryInternal), so the
// callers below now see them. With none active every caller still lands on AC's "no relevant strategy"
// branch, which only cares whether the slot is empty or out of range.
static bool HasTotemStrategy(PlayerbotAI* botAI, char const* strategyName)
{
    return botAI->HasStrategy(strategyName, BOT_STATE_COMBAT);
}

// #431 (AC parity, AC GetRequiredTotemSpellId .cpp:265-285): the per-totem strategies now exist
// (TotemsShamanStrategy.h, registered in ShamanAiObjectContext.cpp), so the slot triggers resolve the totem the
// active element strategy wants. AC returns the highest known spell id of the first active strategy; the fork
// compares summon creature names (see the file header), so this returns that creature's name, or nullptr when no
// element strategy is active or the bot does not know the assigned totem (AC's "return 0" branch, which makes
// the caller only care whether the slot is empty or out of range).
struct TotemChoice
{
    char const* strategy;
    char const* spell;
    char const* creature;
};

static char const* GetRequiredTotemCreature(PlayerbotAI* botAI, TotemChoice const* choices, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (!HasTotemStrategy(botAI, choices[i].strategy))
            continue;

        if (botAI->GetAiObjectContext()->GetValue<uint32>("spell id", choices[i].spell)->Get())
            return choices[i].creature;
    }

    return nullptr;
}

// The totem strategies that exist in 5.4.8, one table per element slot (the totems the 5.0.4 and 4.0.1 patches removed, Strength of
// Earth, Stoneskin, Flametongue, Totem of Wrath, Mana Spring, Cleansing, Wrath of Air, Windfury and the resistance totems, are gone).
static TotemChoice const EARTH_TOTEM_CHOICES[] = {
    {"tremor", "tremor totem", "Tremor Totem"},
    {"earthbind", "earthbind totem", "Earthbind Totem"}};
static TotemChoice const FIRE_TOTEM_CHOICES[] = {
    {"searing", "searing totem", "Searing Totem"},
    {"magma", "magma totem", "Magma Totem"}};
static TotemChoice const WATER_TOTEM_CHOICES[] = {
    {"healing stream", "healing stream totem", "Healing Stream Totem"}};
static TotemChoice const AIR_TOTEM_CHOICES[] = {
    {"grounding", "grounding totem", "Grounding Totem"}};

// Temporary enchant id a weapon imbue spell applies (its SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY MiscValue), 0 when none.
static uint32 ImbueEnchantId(uint32 spellId)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
    if (!info)
        return 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        if (info->Effects[i].Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY)
            return uint32(info->Effects[i].MiscValue);
    return 0;
}

// #858 (rot fix C, wave 73): AC only imbues an UNIMBUED main hand. A spec switch (ele -> enh -> resto) leaves the previous
// spec's imbue on the weapon for an hour, so Windfury Weapon / Earthliving never fired after a respec.
// Beyond AC: a main hand that carries a different imbue than the spec's own (ele Flametongue, enh Windfury, resto
// Earthliving) counts as "no imbue", at most once per 5 minutes so an enchant id that differs from the spell's effect
// cannot loop.
bool MainHandWeaponNoImbueTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
    if (!itemForSpell)
        return false;

    uint32 const current = itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT);
    if (!current)
        return true;

    uint32 wanted = 0;
    switch (AiFactory::GetPlayerSpecTab(bot))
    {
        case Specializations::SPEC_SHAMAN_ELEMENTAL: wanted = SPELL_FLAMETONGUE_WEAPON; break;
        case Specializations::SPEC_SHAMAN_ENHANCEMENT: wanted = SPELL_WINDFURY_WEAPON; break;
        case Specializations::SPEC_SHAMAN_RESTORATION: wanted = SPELL_EARTHLIVING_WEAPON; break;
        default: break;
    }
    if (!wanted || !bot->HasSpell(wanted))
        return false;

    uint32 const wantedEnchant = ImbueEnchantId(wanted);
    if (!wantedEnchant || wantedEnchant == current)
        return false;

    time_t const now = time(nullptr);
    if (now - lastWrongImbue < 300)
        return false;
    lastWrongImbue = now;
    return true;
}

bool OffHandWeaponNoImbueTrigger::IsActive()
{
    Item* const itemForSpell = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!itemForSpell)
        return false;

    uint32 invType = itemForSpell->GetTemplate()->InventoryType;
    bool allowedType = (invType == INVTYPE_WEAPON) || (invType == INVTYPE_WEAPONOFFHAND);
    if (itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) || !allowedType)
        return false;

    return true;
}

// AC (.cpp:287-326). The earth slot is one summon slot, not four name lookups: AC resolves
// SUMMON_SLOT_TOTEM_EARTH and fires when that slot is empty, holds a different totem than the active
// earth strategy wants, or its totem is out of range.
bool NoEarthTotemTrigger::IsActive()
{
    // AC (.cpp:289-291): the element's baseline totem gates the trigger. #382: AC's baseline (Stoneskin) was removed
    // in 5.0.4 (cls.py shaman: NOT-LEARNABLE), which made this gate permanently false; Earthbind Totem (2484, L26) is
    // the lowest earth totem a 5.4.8 shaman learns (cls.py shaman 'earthbind totem' YES 2484/skill/L26).
    if (!BotKnowsTotemSpell(botAI, bot, "earthbind totem"))
        return false;

    BotGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_EARTH];
    Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_EARTH);
    bool slotEmptyOrFar = guid.IsEmpty() || !totem || totem->GetDistance(bot) > 30.0f;

    // AC EXCEPTION (.cpp:313-318): Stoneclaw Totem out and in range counts as the slot being occupied.
    // #382: Stoneclaw is gone in 5.4.8; its successor Stone Bulwark Totem (108270) holds the slot instead.
    if (totem && totem->GetDistance(bot) <= 30.0f && caseInsensitiveEqual(totem->GetName().c_str(), "Stone Bulwark Totem"))
        return false;

    // AC (.cpp:305-311): which earth totem the active earth strategy wants.
    char const* required = GetRequiredTotemCreature(botAI, EARTH_TOTEM_CHOICES, sizeof(EARTH_TOTEM_CHOICES) / sizeof(EARTH_TOTEM_CHOICES[0]));

    // AC (.cpp:320-325): no relevant strategy, only care that the slot is unfilled; otherwise also fire
    // when the wrong totem stands there.
    return slotEmptyOrFar || (required && !TotemSlotHolds(bot, SUMMON_SLOT_TOTEM_EARTH, required));
}

// AC (.cpp:328-368).
bool NoFireTotemTrigger::IsActive()
{
    if (!bot->HasSpell(3599) || !BotKnowsTotemSpell(botAI, bot, "searing totem"))
        return false;

    BotGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE];
    Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_FIRE);
    bool slotEmptyOrFar = guid.IsEmpty() || !totem || totem->GetDistance(bot) > 30.0f;

    // AC EXCEPTION (.cpp:355-360): Fire Elemental Totem out and in range holds the slot.
    if (totem && totem->GetDistance(bot) <= 30.0f && caseInsensitiveEqual(totem->GetName().c_str(), "Fire Elemental Totem"))
        return false;

    char const* required = GetRequiredTotemCreature(botAI, FIRE_TOTEM_CHOICES, sizeof(FIRE_TOTEM_CHOICES) / sizeof(FIRE_TOTEM_CHOICES[0]));

    return slotEmptyOrFar || (required && !TotemSlotHolds(bot, SUMMON_SLOT_TOTEM_FIRE, required));
}

// AC (.cpp:370-411).
// Damage shaman sheets: a healer-less party can use the water slot for Healing Stream.
static bool HasGroupHealer(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group) return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->IsInWorld() &&
            member->FindMap() == bot->FindMap() && PlayerBotSpec::IsHeal(member))
            return true;
    }
    return false;
}

bool NoWaterTotemTrigger::IsActive()
{
    Specializations spec = AiFactory::GetPlayerSpecTab(bot);
    if ((spec == Specializations::SPEC_SHAMAN_ELEMENTAL || spec == Specializations::SPEC_SHAMAN_ENHANCEMENT) &&
        HasGroupHealer(bot))
        return false;
    if (!bot->HasSpell(5394) || !BotKnowsTotemSpell(botAI, bot, "healing stream totem"))
        return false;

    BotGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_WATER];
    Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_WATER);
    bool slotEmptyOrFar = guid.IsEmpty() || !totem || totem->GetDistance(bot) > 30.0f;

    // AC EXCEPTION (.cpp:396-401): Mana Tide Totem out and in range holds the slot.
    // Rotation task 3 (#858): Healing Tide Totem holds the slot the same way.
    if (totem && totem->GetDistance(bot) <= 30.0f &&
        (caseInsensitiveEqual(totem->GetName().c_str(), "Mana Tide Totem") ||
         caseInsensitiveEqual(totem->GetName().c_str(), "Healing Tide Totem")))
        return false;

    char const* required = GetRequiredTotemCreature(botAI, WATER_TOTEM_CHOICES, sizeof(WATER_TOTEM_CHOICES) / sizeof(WATER_TOTEM_CHOICES[0]));

    return slotEmptyOrFar || (required && !TotemSlotHolds(bot, SUMMON_SLOT_TOTEM_WATER, required));
}

// AC (.cpp:413-447).
//
// Two AC details are corrected here rather than copied:
//  - AC's air gate is SPELL_NATURE_RESISTANCE_TOTEM_RANK_1 (10595), a WotLK spell that MoP no longer
//    has. #382: the earlier MoP baseline "grace of air totem" does not exist in 5.4.8 either (cls.py shaman:
//    NO-SUCH-NAME), so the gate never opened; Grounding Totem (8177, L38) is the air totem a 5.4.8 shaman learns.
//  - AC passes numStrategies = 3 to its own four-entry air name/spell array (.cpp:437), so AC never
//    consults the fourth entry. All four air strategies are checked here.
bool NoAirTotemTrigger::IsActive()
{
    if (!BotKnowsTotemSpell(botAI, bot, "grounding totem"))
        return false;

    BotGuid const guid = bot->m_SummonSlot[SUMMON_SLOT_TOTEM_AIR];
    Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_AIR);
    bool slotEmptyOrFar = guid.IsEmpty() || !totem || totem->GetDistance(bot) > 30.0f;

    // The grounding strategy is named "grounding" (AC TotemsShamanStrategy.h getName); AC's trigger array lists
    // "grounding totem" there, a name no strategy has.
    char const* required = GetRequiredTotemCreature(botAI, AIR_TOTEM_CHOICES, sizeof(AIR_TOTEM_CHOICES) / sizeof(AIR_TOTEM_CHOICES[0]));

    return slotEmptyOrFar || (required && !TotemSlotHolds(bot, SUMMON_SLOT_TOTEM_AIR, required));
}


bool WaterWalkingTrigger::IsActive()
{
    return bot->HasSpell(546) && BuffTrigger::IsActive() && AI_VALUE2(bool, "swimming", "self target");
}

bool WaterWalkingOnPartyTrigger::IsActive()
{
    return bot->HasSpell(546) && BuffOnPartyTrigger::IsActive() && AI_VALUE2(bool, "swimming", "self target");
}

// Checks if Chain Lightning is on Cooldown, and prevents activation if it is.
// #858 (rot fix C, wave 73): AC fires this only while Chain Lightning is on cooldown (WotLK: Elemental Mastery made the next
// cast instant). In 5.4.8 it is a plain 20 % haste cooldown and the MoP rotation casts it whenever it is ready
// (condition "always"), so a single-target leg never put Chain Lightning on cooldown
// and it was never cast. Active whenever the spell is known, ready and the buff is not running.
bool ElementalMasteryTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_ELEMENTAL_MASTERY) || bot->HasSpellCooldown(SPELL_ELEMENTAL_MASTERY) ||
        bot->HasAura(SPELL_ELEMENTAL_MASTERY))
        return false;
    return true;
}

// Enhancement class sheet: the shaman breaks roots and closes distant pulls itself.
bool SpiritWalkTrigger::IsActive()
{
    if (!bot->HasSpell(SPIRIT_WALK_SPELL_ID) || bot->HasSpellCooldown(SPIRIT_WALK_SPELL_ID))
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    return bot->HasUnitState(UNIT_STATE_ROOT) || bot->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED) ||
        (target && bot->GetDistance(target) > 10.0f);
}

bool ShockTrigger::IsActive()
{
    return SpellTrigger::IsActive() &&
        !botAI->HasAura("flame shock", GetTarget(), false, true) &&
        !botAI->HasAura("frost shock", GetTarget(), false, true);
}

bool FlameShockTrigger::IsActive()
{
    return bot->HasSpell(8050) && BuffTrigger::IsActive();
}

// Checks if the target's health is above 25%/1500 hp. Returns false if either are true.
bool EarthShockExecuteTrigger::IsActive()
{
    if (!bot->HasSpell(8042))
        return false;
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    float percent = 100.0f * target->GetHealth() / target->GetMaxHealth();
    if (percent >= 25.0f)
        return false;

    if (target->GetHealth() >= 1500)
        return false;

    return true;
}

// Checks if the bot knows Call of the Elements, and prevents the trigger firing if it doesn't.
// Fires the trigger if at least 2 of the totem slots are empty or out of range.
// AC (.cpp:133-179): per summon slot, not per totem name.
bool CallOfTheElementsTrigger::IsActive()
{
    if (!botAI->CanCastSpell(SPELL_CALL_OF_THE_ELEMENTS, bot, true))
        return false;

    // AC (.cpp:169-173) only counts a slot as possibly empty when the bot knows that element's
    // baseline totem; MoP's air baseline is Grace of Air Totem rather than AC's Nature Resistance.
    struct ElementSlot
    {
        uint8 slot;
        char const* baselineTotem;
    };

    ElementSlot const elements[] = {
        {SUMMON_SLOT_TOTEM_EARTH, "earthbind totem"},  // #382: Stoneskin removed in 5.0.4, see NoEarthTotemTrigger
        {SUMMON_SLOT_TOTEM_FIRE, "searing totem"},
        {SUMMON_SLOT_TOTEM_WATER, "healing stream totem"},
        {SUMMON_SLOT_TOTEM_AIR, "grounding totem"},  // #382: "grace of air totem" does not exist in 5.4.8
    };

    int emptyCount = 0;
    for (ElementSlot const& element : elements)
    {
        Creature* totem = TotemInSlot(bot, element.slot);
        if (totem && totem->GetDistance(bot) <= 30.0f)
            continue;

        if (BotKnowsTotemSpell(botAI, bot, element.baselineTotem))
            ++emptyCount;
    }

    return emptyCount >= 2;
}

// Totemic Recall - prevents the trigger firing under the following circumstances:
// 1. The bot does not have Totemic Recall.
// 2. The bot is in an instance and any boss encounter is in progress.
// 3. Any group member or their pet is in combat.
// 4. If Mana Tide Totem or Fire Elemental Totem is active and within 30 yards of the bot.
// 5. Finally, if any totem summon slot is not empty, the trigger will fire.
// AC (.cpp:187-262).
bool TotemicRecallTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_TOTEMIC_RECALL))
        return false;

    Map* map = bot->GetMap();
    if (map && map->IsDungeon())
    {
        InstanceScript* instance = ((InstanceMap*)map)->GetInstanceScript();
        if (instance)
        {
            if (instance->IsEncounterInProgress())
                return false;
        }
    }

    Group* group = bot->GetGroup();
    if (group)
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
                continue;

            if (member->IsInCombat())
                return false;

            Pet* pet = member->GetPet();
            if (pet && pet->IsInCombat())
                return false;
        }
    }

    // AC (.cpp:224-256): the two long-lived totems hold the recall while they are in range.
    if (Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_WATER))
    {
        if (totem->GetDistance(bot) <= 30.0f && (caseInsensitiveEqual(totem->GetName().c_str(), "Mana Tide Totem") ||
                                                  caseInsensitiveEqual(totem->GetName().c_str(), "Healing Tide Totem")))
            return false;
    }

    if (Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_FIRE))
    {
        if (totem->GetDistance(bot) <= 30.0f && caseInsensitiveEqual(totem->GetName().c_str(), "Fire Elemental Totem"))
            return false;
    }

    // AC (.cpp:258-261): any occupied summon slot makes the recall fire.
    return bot->m_SummonSlot[SUMMON_SLOT_TOTEM_EARTH] != 0 ||
           bot->m_SummonSlot[SUMMON_SLOT_TOTEM_FIRE] != 0 ||
           bot->m_SummonSlot[SUMMON_SLOT_TOTEM_WATER] != 0 ||
           bot->m_SummonSlot[SUMMON_SLOT_TOTEM_AIR] != 0;
}

// AC (.cpp:449-473). AC reads the action button for this totem's TOTEM_BAR_SLOT_* index and reports
// active unless that button already holds the highest rank of this totem the bot knows; the fork has
// no Player::GetActionButton, so the element's summon slot answers the same question - the trigger is
// inactive only when that slot already holds this totem and the bot knows the spell at all.
bool SetTotemTrigger::IsActive()
{
    if (!BotKnowsTotemSpell(botAI, bot, std::string(totemName)))
        return false;

    // The situational totems the No<Element>Totem triggers treat as occupying the slot (AC .cpp:313-318,
    // 355-360, 396-401) must not be overwritten by a strategy-assigned totem either; AC cannot overwrite them
    // because its "set" action only edits the bar, here the action casts, so the same guard is applied.
    Creature* standing = TotemInSlot(bot, elementSlot);
    if (standing && standing->GetDistance(bot) <= 30.0f &&
        (caseInsensitiveEqual(standing->GetName().c_str(), "Stone Bulwark Totem") ||
         caseInsensitiveEqual(standing->GetName().c_str(), "Fire Elemental Totem") ||
         caseInsensitiveEqual(standing->GetName().c_str(), "Mana Tide Totem") ||
         caseInsensitiveEqual(standing->GetName().c_str(), "Healing Tide Totem")))
        return false;

    return !TotemSlotHolds(bot, elementSlot, summonTotemName);
}

// Wind Shear on any casting enemy, battleground / arena only (task 0 values, #855).
bool PvpEnemyCasterTrigger::IsActive()
{
    return AI_VALUE(bool, "in pvp instance") && AI_VALUE(Unit*, "enemy caster target") != nullptr;
}

// >= 3 group members at or below the "low" health threshold (config lowHealth, 45).
bool RestoAscendanceTrigger::IsActive()
{
    return AI_VALUE2(uint8, "aoe heal", "low") >= 3;
}

namespace
{
constexpr uint32 GLYPH_OF_TELLURIC_CURRENTS_AURA = 55453;
// Above this the mana from the filler would be wasted; the healer stands ready instead.
constexpr uint8 TELLURIC_FILLER_MANA_PCT = 90;
}  // namespace

bool TelluricCurrentsFillerTrigger::IsActive()
{
    // A "good" line of the class sheet: casual bots do not weave damage into their healing.
    if (!bot->IsInCombat() || !bot->HasAura(GLYPH_OF_TELLURIC_CURRENTS_AURA) ||
        !BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    return AI_VALUE2(uint8, "mana", "self target") < TELLURIC_FILLER_MANA_PCT;
}

// Class sheets: inspect the existing attacker value, never a new grid search.
bool ShamanSheetTrigger::IsActive()
{
    constexpr uint32 LIGHTNING_SHIELD = 324, MAELSTROM_WEAPON = 53817;
    if (kind == 6 || kind == 7)
    {
        Trigger* imbue = botAI->GetAiObjectContext()->GetTrigger("main hand weapon no imbue");
        return bot->HasSpell(8024) && (bot->HasSpell(8232) == (kind == 6)) && imbue && imbue->IsActive();
    }
    // These optional decisions are above the casual tier in the class sheets.
    if ((kind == 0 || kind == 1 || kind == 2 || kind == 5) &&
        !BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return false;
    if (kind == 0)
    {
        Aura* shield = bot->GetAura(LIGHTNING_SHIELD);
        return bot->HasSpell(8042) && shield && shield->GetStackAmount() >= 3;
    }
    if (kind == 1)
    {
        Aura* maelstrom = bot->GetAura(MAELSTROM_WEAPON);
        return !HasGroupHealer(bot) && maelstrom && maelstrom->GetStackAmount() >= 3 &&
            AI_VALUE2(uint8, "health", "self target") < 70;
    }
    if (kind == 2)
    {
        // Before the knockback-removing glyph, protect a tank's positioning.
        return bot->HasSpell(51490) && (bot->getLevel() >= 25 || !bot->GetGroup() ||
            AI_VALUE2(uint8, "mana", "self target") < 40) &&
            (AI_VALUE2(uint8, "mana", "self target") < 65 ||
             (AI_VALUE(Unit*, "current target") && bot->GetDistance(AI_VALUE(Unit*, "current target")) < 5.0f));
    }
    Unit* center = kind == 5 ? static_cast<Unit*>(bot) : AI_VALUE(Unit*, "current target");
    if (!center)
        return false;
    uint8 mana = AI_VALUE2(uint8, "mana", "self target");
    if ((kind == 3 && (!bot->HasSpell(421) || mana <= 40)) ||
        (kind == 4 && (!bot->HasSpell(61882) || mana <= 65)) ||
        (kind == 5 && !bot->HasSpell(8190)))
        return false;
    if (kind == 5)
    {
        Creature* totem = TotemInSlot(bot, SUMMON_SLOT_TOTEM_FIRE);
        if (totem && !caseInsensitiveEqual(totem->GetName().c_str(), "Searing Totem"))
            return false;
    }
    unsigned count = 0;
    for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (enemy && enemy->IsAlive() && enemy->GetDistance(center) <= 8.0f)
            ++count;
    }
    return count >= (kind == 4 ? 6u : 3u);
}

// Check the spellbook at trigger time so learning a spell needs no strategy reset.
bool ShamanKnownTrigger::IsActive()
{
    if (!bot->HasSpell(spellId)) return false;
    Trigger* trigger = botAI->GetAiObjectContext()->GetTrigger(underlying);
    return trigger && trigger->IsActive();
}

bool LightningShieldTrigger::IsActive()
{
    return bot->HasSpell(324) && BuffTrigger::IsActive();
}

// BT-157: Earth Shield (level 26) is the shaman's second pre-pull spell (good). Before this it ignored mana altogether.
bool EarthShieldOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "earth shield");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    return HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::GOOD) && BuffOnMainTankTrigger::IsActive();
}

// Riptide has a 6 second cooldown, so the cooldown is read before the target is.
bool RiptideOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "riptide");
    if (!spellId || !bot->HasSpell(spellId) || bot->HasSpellCooldown(spellId))
        return false;

    return HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::CASUAL) && BuffOnMainTankTrigger::IsActive();
}

// BT-677 to BT-681: see ShamanTriggers.h.
bool PvpElementalTrigger::IsActive()
{
    return PvpElementalGlue::Due(botAI, bot, rule);
}
