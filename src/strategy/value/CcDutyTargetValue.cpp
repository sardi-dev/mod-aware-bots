/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "CcDutyTargetValue.h"

#include "AttackersValue.h"
#include "Creature.h"
#include "CrowdControlDutyPolicy.h"
#include "Group.h"
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"

#include <algorithm>

namespace
{
using CrowdControlDutyPolicy::Spell;

// BT-165: the owner's control spells, one row per spell (owner decisions 2026-11-12).
struct ControlSpell
{
    uint8 playerClass;
    Spell policy;
    uint32 spellId;
    char const* name;
};

constexpr uint32 SpellPolymorph = 118;
constexpr uint32 SpellHex = 51514;
constexpr uint32 SpellRepentance = 20066;
constexpr uint32 SpellHibernate = 2637;
constexpr uint32 SpellCyclone = 33786;
constexpr uint32 SpellFreezingTrap = 1499;
constexpr uint32 SpellSap = 6770;
constexpr uint32 SpellShackleUndead = 9484;

ControlSpell const ControlSpells[] = {
    {CLASS_MAGE, Spell::Polymorph, SpellPolymorph, "polymorph"},
    {CLASS_SHAMAN, Spell::Hex, SpellHex, "hex"},
    {CLASS_PALADIN, Spell::Repentance, SpellRepentance, "repentance"},
    {CLASS_DRUID, Spell::Hibernate, SpellHibernate, "hibernate"},
    {CLASS_DRUID, Spell::Cyclone, SpellCyclone, "cyclone"},
    {CLASS_HUNTER, Spell::FreezingTrap, SpellFreezingTrap, "freezing trap"},
    {CLASS_ROGUE, Spell::Sap, SpellSap, "sap"},
    {CLASS_PRIEST, Spell::ShackleUndead, SpellShackleUndead, "shackle undead"},
};

// Scan order of the marks: the first mark this bot controls is its duty target.
unsigned const ControlIcons[] = {CrowdControlDutyPolicy::MoonIcon, CrowdControlDutyPolicy::SquareIcon,
                                 CrowdControlDutyPolicy::TriangleIcon};

constexpr uint32 CcDutyRefreshMs = 250;

// Yards inside the spell's maximum range where the controller stops walking, and the slack that keeps a bot standing
// at the stop point from walking again on the next decision.
constexpr float ApproachMarginYards = 5.0f;
constexpr float RangeSlackYards = 2.0f;

constexpr uint32 TypeMask(uint32 creatureType) { return 1u << (creatureType - 1); }
constexpr uint32 BeastOrDragonkinMask = TypeMask(CREATURE_TYPE_BEAST) | TypeMask(CREATURE_TYPE_DRAGONKIN);
constexpr uint32 UndeadMask = TypeMask(CREATURE_TYPE_UNDEAD);

bool IsControlIcon(unsigned icon)
{
    for (unsigned const controlIcon : ControlIcons)
        if (controlIcon == icon)
            return true;
    return false;
}

bool TypeFits(ControlSpell const& spell, Unit* target)
{
    Creature* creature = target->ToCreature();
    if (!creature || creature->isWorldBoss() || creature->IsDungeonBoss())
        return false;

    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell.spellId);
    if (!info || !info->CheckTargetCreatureType(target))
        return false;

    uint32 const mask = target->GetCreatureTypeMask();
    switch (spell.policy)
    {
        case Spell::Hibernate: return (mask & BeastOrDragonkinMask) != 0;
        case Spell::Cyclone: return (mask & BeastOrDragonkinMask) == 0;
        case Spell::ShackleUndead: return (mask & UndeadMask) != 0;
        case Spell::Sap: return !target->IsInCombat(); // Owner: Sap only on a non-combat target.
        default: return true;
    }
}

bool CanControl(Player* member, unsigned icon, Unit* mark)
{
    for (ControlSpell const& spell : ControlSpells)
        if (spell.playerClass == member->getClass() && CrowdControlDutyPolicy::MatchesMark(icon, spell.policy) &&
            member->HasSpell(spell.spellId) && TypeFits(spell, mark))
            return true;
    return false;
}

// The control spell this bot would cast on `target`: its class, a spell it knows, a type that fits.
ControlSpell const* OwnSpell(Player* bot, Unit* target)
{
    for (ControlSpell const& spell : ControlSpells)
        if (spell.playerClass == bot->getClass() && bot->HasSpell(spell.spellId) && TypeFits(spell, target))
            return &spell;
    return nullptr;
}

float MaxRangeOf(ControlSpell const& spell, Player* bot)
{
    SpellInfo const* info = sSpellMgr->GetSpellInfo(spell.spellId);
    return info ? info->GetMaxRange(false, bot) : 0.0f;
}

// The guid of the one bot that controls `icon`, or 0 when no group bot can.
uint64 ControllerGuid(Group* group, unsigned icon, Unit* mark)
{
    CrowdControlDutyPolicy::Selection selection;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != mark->FindMap())
            continue;

        // A bot without the cc strategy would never act on the mark, so it must not own it.
        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        bool const eligible =
            memberAI && memberAI->HasStrategy("cc", BOT_STATE_COMBAT) && CanControl(member, icon, mark);
        selection.Add({member->GetGUID(), eligible, PlayerBotSpec::IsTank(member), PlayerBotSpec::IsHeal(member)});
    }
    return selection.Guid();
}

// BT-165: the tank holds the mob, or a human is already hitting it: leave it alone.
bool IsMarkSafe(Group* group, Unit* mark)
{
    bool tankTarget = false;
    bool humanAttacking = false;
    uint64 const markGuid = mark->GetGUID();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || member->FindMap() != mark->FindMap())
            continue;

        bool const targetsMark = member->GetTarget() == markGuid ||
                                 (member->GetVictim() && member->GetVictim()->GetGUID() == markGuid);
        if (PlayerBotSpec::IsTank(member))
            tankTarget = tankTarget || targetsMark;
        else if (!GET_PLAYERBOT_AI(member))
            humanAttacking = humanAttacking || (member->GetVictim() && member->GetVictim()->GetGUID() == markGuid);
    }
    return CrowdControlDutyPolicy::SafeTarget(mark->IsAlive(), tankTarget, humanAttacking);
}

uint64 DutyGuid(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    for (unsigned const icon : ControlIcons)
    {
        Unit* mark = CcDuty::MarkedUnit(botAI, group, icon);
        if (mark && IsMarkSafe(group, mark) && ControllerGuid(group, icon, mark) == bot->GetGUID())
            return mark->GetGUID();
    }
    return 0;
}
}  // namespace

namespace CcDuty
{
Unit* MarkedUnit(PlayerbotAI* botAI, Group* group, unsigned icon)
{
    uint64 const guid = group->GetTargetIcon(icon);
    if (!guid)
        return nullptr;

    Unit* unit = botAI->GetUnit(guid);
    return unit && unit->IsAlive() && AttackersValue::IsPossibleTarget(unit, botAI->GetBot()) ? unit : nullptr;
}

bool SpellFitsTarget(std::string const& spell, Unit* target)
{
    if (!target)
        return false;
    for (ControlSpell const& entry : ControlSpells)
        if (spell == entry.name)
            return TypeFits(entry, target);
    return false;
}

static bool ControllerOf(PlayerbotAI* botAI, Unit* creature, uint64& controller)
{
    Group* group = botAI->GetBot()->GetGroup();
    if (!group || !creature)
        return false;

    for (unsigned const icon : ControlIcons)
        if (group->GetTargetIcon(icon) == creature->GetGUID())
        {
            controller = ControllerGuid(group, icon, creature);
            return true;
        }
    return false;
}

bool InControlRange(Player* bot, Unit* target)
{
    ControlSpell const* spell = OwnSpell(bot, target);
    if (!spell)
        return false;

    float const range = MaxRangeOf(*spell, bot);
    if (range <= 0.0f)
        return true;

    return bot->IsWithinCombatRange(target, range - RangeSlackYards) && bot->IsWithinLOSInMap(target);
}

float ApproachDistance(Player* bot, Unit* target)
{
    ControlSpell const* spell = OwnSpell(bot, target);
    if (!spell)
        return 0.0f;

    // Owner (2026-11-13): Sap walks to melee reach, and only while the mob is not in combat (TypeFits).
    if (spell->policy == CrowdControlDutyPolicy::Spell::Sap)
        return sPlayerbotAIConfig->meleeDistance;

    return std::max(sPlayerbotAIConfig->meleeDistance, MaxRangeOf(*spell, bot) - ApproachMarginYards);
}

bool NeedsApproach(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->GetGroup() || bot->IsInCombat())
        return false;

    Unit* duty = botAI->GetAiObjectContext()->GetValue<Unit*>("cc duty target")->Get();
    return duty && !InControlRange(bot, duty);
}

bool IsOtherControllersMark(PlayerbotAI* botAI, Unit* creature)
{
    uint64 controller = 0;
    return ControllerOf(botAI, creature, controller) && controller && controller != botAI->GetBot()->GetGUID();
}

bool IsClaimedMark(PlayerbotAI* botAI, Unit* creature)
{
    uint64 controller = 0;
    return ControllerOf(botAI, creature, controller) && controller;
}

bool IsControlledMark(Group* group, Unit* unit)
{
    if (!group || !unit)
        return false;

    uint64 const guid = unit->GetGUID();
    return (group->GetTargetIcon(CrowdControlDutyPolicy::SquareIcon) == guid ||
            group->GetTargetIcon(CrowdControlDutyPolicy::TriangleIcon) == guid) &&
           unit->HasBreakableByDamageCrowdControlAura();
}

Required RequiredTarget(PlayerbotAI* botAI)
{
    Required required;
    AiObjectContext* context = botAI->GetAiObjectContext();
    if (botAI->GetBot()->GetGroup())
    {
        required.target = context->GetValue<Unit*>("cc duty target")->Get();
        if (required.target)
        {
            required.duty = true;
            return required;
        }
    }

    Unit* marked = context->GetValue<Unit*>("rti cc target")->Get();
    if (marked && !IsClaimedMark(botAI, marked))
        required.target = marked;
    return required;
}
}  // namespace CcDuty

Unit* CcDutyTargetValue::Get()
{
    uint32 const now = getMSTime();
    if (!cachedAtMs || now - cachedAtMs >= CcDutyRefreshMs)
    {
        cachedAtMs = now ? now : 1;
        cachedGuid = DutyGuid(botAI);
    }

    Unit* unit = cachedGuid ? botAI->GetUnit(cachedGuid) : nullptr;
    return unit && unit->IsAlive() ? unit : nullptr;
}

Unit* CcDutyTargetValue::Calculate() { return botAI->GetUnit(DutyGuid(botAI)); }
