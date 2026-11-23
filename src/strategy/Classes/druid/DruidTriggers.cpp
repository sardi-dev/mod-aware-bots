/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "DruidTriggers.h"

#include "BotSkill.h"
#include "DynamicObject.h"
#include "HealerTriggers.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpBurst.h"
#include "ServerFacade.h"
#include "Spell.h"


namespace
{
constexpr uint8 LACERATE_MAX_STACKS = 3;

// Class sheet, restoration druid (BT-123).
constexpr uint8 LIFEBLOOM_MAX_STACKS = 3;
// The window CastLifebloomOnMainTankAction::isUseful already uses to refresh before the bloom runs out.
constexpr int32 LIFEBLOOM_REFRESH_BEFORE_MS = 2000;
constexpr uint32 SPELL_NATURES_CURE = 88423;

// Class sheet, guardian druid (BT-131).
constexpr uint32 SPELL_BEAR_FORM = 5487;
constexpr uint32 SPELL_FAERIE_FIRE = 770;

// BT-131: keep bear form only when a living healer can reach and see the tank.
bool GroupHasLivingHealer(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (member && member != bot && member->IsAlive() && member->IsInWorld() &&
            member->FindMap() == bot->FindMap() && PlayerBotSpec::IsHeal(member) &&
            sServerFacade->GetDistance2d(member, bot) <= sPlayerbotAIConfig->healDistance &&
            member->IsWithinLOSInMap(bot))
            return true;
    }
    return false;
}

bool TankKeepsBearForm(Player* bot) { return PlayerBotSpec::IsTank(bot) && GroupHasLivingHealer(bot); }
}

// #521 w13: see DruidTriggers.h - the 5.4.8 Eclipse auras have no duration, so check them without the duration filter.
bool EclipseSolarTrigger::IsActive() { return botAI->HasAura(getName(), GetTarget(), false, false, -1, false); }

bool EclipseLunarTrigger::IsActive() { return botAI->HasAura(getName(), GetTarget(), false, false, -1, false); }

// 67484 Eclipse Marker (lunar direction), spell_druid.cpp SPELL_DRUID_ECLIPSE_MARKER_LUNAR.
bool EclipseLunarDirectionTrigger::IsActive() { return bot->HasAura(67484); }

// 1:1 port of AC Ai/Class/Druid/DruidTriggers.cpp (WotLK mod-playerbots).
//
// 5.4.8 divergences applied here:
//  - ServerFacade::instance() returns a POINTER in this fork (Utils/ServerFacade.h:21, `sServerFacade` is
//    the #define for it), so every call is `sServerFacade->Method(` - the AC `ServerFacade::instance().X`
//    form is C2228 here.
//  - Every IsActive() body is here, not inline in the header: PlayerbotAI::HasAura(std::string, Unit*)
//    cannot bind a `bot` (Player*) from a header TU here (C2665).
//  - aquatic form reads `bot->GetLiquidStatus()` (Object.h:471, a ZLiquidStatus bitmask) rather than AC's
//    `bot->GetLiquidData().Status`; the constant is a bit flag, so it is tested with & - the same test
//    AC's `== LIQUID_MAP_UNDER_WATER` performs (Map.h:143).

bool MarkOfTheWildTrigger::IsActive()
{
    return BuffTrigger::IsActive() && !botAI->HasAura("gift of the wild", GetTarget());
}

bool NaturesSwiftnessTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "nature's swiftness");
    return spellId && bot->HasSpell(spellId) && BuffTrigger::IsActive();
}

// Wave-75 fix J (#860): see ProwlActiveTrigger in DruidTriggers.h. Prowl is a stealth aura with duration -1.
bool ProwlActiveTrigger::IsActive() { return botAI->HasAura("prowl", bot); }

// Wave-75 fix J (#868): see DruidNotBehindTargetTrigger in DruidTriggers.h.
bool DruidNotBehindTargetTrigger::IsActive()
{
    if (!IsNotBehindTargetTrigger::IsActive())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    return !(target && (target->GetTypeId() == TypeID::TYPEID_PLAYER) && AI_VALUE(bool, "in pvp instance"));
}

bool NaturesSwiftnessActiveTrigger::IsActive() { return botAI->HasAura("nature's swiftness", bot); }

bool RakeTrigger::IsActive() { return !botAI->HasAura("prowl", bot) && DebuffTrigger::IsActive(); }

bool InsectSwarmOnAttackerTrigger::IsActive() { return BuffTrigger::IsActive(); }

bool MoonfireOnAttackerTrigger::IsActive() { return BuffTrigger::IsActive(); }

// #624 (AC BalanceDruidStrategy.cpp:193, DruidTriggers.h FaerieFireTrigger): 5.x Faerie Fire 770's purpose is the Weakened
// Armor 113746 debuff (3 stacks; Wowhead mop-classic 770 + 113746, 2026-10-05), shared with Expose Armor/Sunder/Devastate.
// Do not recast it while any caster's Weakened Armor already sits at 3 stacks.
bool FaerieFireTrigger::IsActive()
{
    Unit* target = GetTarget();
    Creature* creature = target ? target->ToCreature() : nullptr;
    return creature && (creature->isElite() || creature->isWorldBoss()) && bot->HasSpell(SPELL_FAERIE_FIRE) &&
           WeakenedArmorNeedsRefresh(botAI, target);
}

bool FaerieFireFeralTrigger::IsActive()
{
    if (!bot->IsInCombat())
        return false;

    // Class sheet, guardian druid (BT-131): Bear: Faerie Fire has no cooldown, so it only refreshes Weakened Armor
    // (missing, under three stacks or about to run out) instead of taking every global from Mangle and Lacerate.
    if (botAI->HasAnyAuraOf(bot, "bear form", "dire bear form", nullptr))
    {
        Unit* target = GetTarget();
        return target && target->IsAlive() && target->IsInWorld() && bot->HasSpell(SPELL_FAERIE_FIRE) &&
               WeakenedArmorNeedsRefresh(botAI, target);
    }

    if (!botAI->HasAura("cat form", bot))
        return false;

    if (botAI->HasAura("prowl", bot))
        return false;

    // Feral druid sheet: only spend globals on armor for durable enemies.
    Unit* target = GetTarget();
    Creature* creature = target ? target->ToCreature() : nullptr;
    return creature && (creature->isElite() || creature->isWorldBoss()) && creature->IsAlive() && creature->IsInWorld() &&
           bot->HasSpell(SPELL_FAERIE_FIRE) && WeakenedArmorNeedsRefresh(botAI, target);
}

// Weakened Armor 113746 comes from any caster (Faerie Fire, Sunder Armor, Devastate), so the aura is read by name from any
// owner. A permanent one (no duration) never needs a refresh.
bool WeakenedArmorNeedsRefresh(PlayerbotAI* botAI, Unit* target)
{
    Aura* weakenedArmor = botAI->GetAura("weakened armor", target, false, false);
    if (!weakenedArmor)
        return true;

    if (weakenedArmor->GetStackAmount() < WEAKENED_ARMOR_MAX_STACKS)
        return true;

    int32 const remaining = weakenedArmor->GetDuration();
    return remaining >= 0 && remaining < WEAKENED_ARMOR_REFRESH_BEFORE_MS;
}

bool BearFormTrigger::IsActive() { return !botAI->HasAnyAuraOf(bot, "bear form", "dire bear form", nullptr); }

bool TreeFormTrigger::IsActive()
{
    constexpr uint32 SPELL_TREE_OF_LIFE = 33891;
    return !bot->HasAura(SPELL_TREE_OF_LIFE);
}

bool CatFormTrigger::IsActive() { return !botAI->HasAura("cat form", bot); }

bool AquaticFormTrigger::IsActive()
{
    return !bot->IsInCombat() && !botAI->HasAura("aquatic form", bot) &&
           bot->IsUnderWater();
}

bool MangleBearTrigger::IsActive()
{
    if (!bot->IsInCombat() || !botAI->HasAnyAuraOf(bot, "bear form", "dire bear form", nullptr))
        return false;
    Unit* target = GetTarget();
    return target && target->IsAlive() && target->IsInWorld();
}

bool LacerateTrigger::IsActive()
{
    if (!bot->IsInCombat() || !botAI->HasAnyAuraOf(bot, "bear form", "dire bear form", nullptr))
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !target->IsInWorld())
        return false;

    Aura* lacerate = botAI->GetAura("lacerate", target, false, false);
    if (!lacerate)
        return true;

    // BT-068: Lacerate stacks to 3 in 5.4.8 (SpellAuraOptions 1727), so 5 kept the trigger on for good
    if (lacerate->GetStackAmount() < LACERATE_MAX_STACKS)
        return true;

    return lacerate->GetDuration() <= 6000;
}

bool MangleCatTrigger::IsActive()
{
    if (botAI->HasAura("prowl", bot))
        return false;
    // #384: AC's second check (HasAura "mangle (bear)") is the same debuff under the 3.3.5 bear name; 5.x has one
    // Mangle, which DebuffTrigger::IsActive() already tests by its own name ("mangle"), so it is dropped here.
    return DebuffTrigger::IsActive() && !botAI->HasAura("trauma", GetTarget(), false, false, -1, true);
}

// #384 (AC DruidTriggers.cpp DemoralizeRoarTrigger::IsActive, retargeted at Thrash - see DruidTriggers.h).
// AC skips the roar when a curse of weakness / demoralizing shout / vindication already demoralizes the target;
// 5.x puts all of those under the single "Weakened Blows" debuff (Spell.dbc name; Wowhead mop-classic 106830).
bool ThrashTrigger::IsActive()
{
    return DebuffTrigger::IsActive() && !botAI->HasAura("weakened blows", GetTarget(), false, false, -1, true);
}

bool EntanglingRootsKiteTrigger::IsActive()
{
    return DebuffTrigger::IsActive() && AI_VALUE(uint8, "attacker count") < 3 && !GetTarget()->GetPower(POWER_MANA);
}

// Feral druid sheet: refresh before the Bite guard closes at six seconds.
bool SavageRoarTrigger::IsActive()
{
    return AI_VALUE2(uint8, "combo", "current target") >= 3 && BuffTrigger::IsActive();
}

bool BalanceHurricaneTrigger::IsActive()
{
    constexpr uint32 MANA_COST_PER_THOUSAND = 503;
    return bot->HasSpell(16914) && bot->GetPower(POWER_MANA) >= bot->GetCreateMana() * MANA_COST_PER_THOUSAND / 1000 &&
           AoeTrigger::IsActive();
}

bool FerociousBiteTimeTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    uint8 cp = AI_VALUE2(uint8, "combo", "current target");
    if (cp < 5)
        return false;

    // Rotation fix task A (#860), MoP rotation row 10302 (spec 103, prio group 2/2, "combo points 5-5" only; Savage Roar
    // 10300 refreshes at <= 6 s, Rip 10301 at <= 2 s): AC's 10 s guards (Savage Roar / Rip) are narrowed to those MoP
    // refresh windows. w73: with 10 s the 5-CP window with both buffs healthy was too short, and the one window that opened
    // ended in NOT_READY/NO_POWER, so Ferocious Bite never landed (checkcast 70 x3, power 88 x1).
    Aura* roar = botAI->GetAura("savage roar", bot);
    if (roar && roar->GetDuration() <= 6000)
        return false;

    Aura* rip = botAI->GetAura("rip", target, true);
    if (rip && rip->GetDuration() <= 2000)
        return false;

    return true;
}

bool FerociousBiteExecuteTrigger::IsActive()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // 5.4.8 divergence: AC calls botAI->HasSpell("ferocious bite") (a spell-name overload). This core only
    // has Player::HasSpell(uint32) (src/server/game/Entities/Player/Player.h:2017), so the name is
    // resolved through the "spell id" value - the same idiom PriestTriggers.cpp:37 uses. 0 = not known.
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "ferocious bite");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    if (AI_VALUE2(uint8, "combo", "current target") < 4)
        return false;

    if (target->GetHealthPct() >= 25.0f)
        return false;

    return true;
}

const std::set<uint32> HurricaneChannelCheckTrigger::HURRICANE_SPELL_IDS = {
    16914,  // Hurricane Rank 1
    17401,  // Hurricane Rank 2
    17402,  // Hurricane Rank 3
    27012,  // Hurricane Rank 4
    48467,  // Hurricane Rank 5
    106996  // Astral Storm, the Lunar Eclipse replacement
};

bool HurricaneChannelCheckTrigger::IsActive()
{
    if (Spell* spell = bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
    {
        if (!HURRICANE_SPELL_IDS.count(spell->m_spellInfo->Id))
            return false;

        // Find this bot's own Hurricane DynamicObject
        DynamicObject* dynObj = nullptr;
        for (uint32 spellId : HURRICANE_SPELL_IDS)
        {
            dynObj = bot->GetDynObject(spellId);
            if (dynObj)
                break;
        }

        if (!dynObj)
            return false;

        // Count attackers actually inside the Hurricane AoE
        float radius = dynObj->GetRadius();
        GuidVector attackers = AI_VALUE(GuidVector, "attackers");
        uint32 count = 0;
        for (BotGuid const& guid : attackers)
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive())
                continue;
            if (unit->GetDistance(dynObj) <= radius)
                count++;
        }

        return count < minEnemies;
    }

    return false;
}

bool NoHealerDpsStrategyTrigger::IsActive() { return !botAI->HasStrategy("healer dps", BOT_STATE_COMBAT); }

bool ProwlTrigger::IsActive()
{
    if (botAI->HasAura("prowl", bot) || bot->IsInCombat())
        return false;

    // 5.4.8 divergence: Player::HasSpell(uint32) only (Player.h:2017) - see FerociousBiteExecuteTrigger.
    uint32 const prowlId = AI_VALUE2(uint32, "spell id", "prowl");
    if (!prowlId || !bot->HasSpell(prowlId))
        return false;

    if (bot->HasSpellCooldown(prowlId))
        return false;

    float distance = 30.f;

    Unit* target = AI_VALUE(Unit*, "enemy player target");
    if (target && !target->IsInWorld())
        return false;
    if (!target)
        target = AI_VALUE(Unit*, "grind target");
    if (!target)
        target = AI_VALUE(Unit*, "dps target");
    if (!target)
        return false;

    if (target && target->GetVictim())
        distance -= 10;
    if (target->isMoving() && target->GetVictim())
        distance -= 10;
    if (bot->InBattleground())
        distance += 15;
    if (bot->InArena())
        distance += 15;

    return target && sServerFacade->GetDistance2d(bot, target) < distance;
}

// Rotation task 5 (#860): MoP rotation row 10310/10311 - Predatory Swiftness (69369) makes Healing Touch instant.
bool PredatorsSwiftnessHealTrigger::IsActive()
{
    if (!botAI->HasAura(69369, bot))
        return false;
    Unit* ally = AI_VALUE(Unit*, "party member to heal");
    return (ally && ally != bot && ally->IsAlive() && ally->GetHealthPct() < 60.0f &&
            bot->GetDistance2d(ally) <= sPlayerbotAIConfig->spellDistance && bot->IsWithinLOSInMap(ally)) ||
           AI_VALUE2(uint8, "health", "self target") <= maxHealth || bot->HasSpell(108373);
}

// Rotation task 5 (#860): MoP rotation row 10505 (Ironbark, tank health <= 40%).
bool IronbarkTrigger::IsActive()
{
    Unit* tank = AI_VALUE(Unit*, "main tank");
    if (!tank || !tank->IsAlive() || tank->GetHealthPct() > 40.0f)
        return false;

    return !botAI->HasAura("ironbark", tank);
}

// Class sheet, restoration druid (BT-123): see LifebloomOnMainTankTrigger in DruidTriggers.h. The spell is known from
// level 36; before that the spell id check keeps the edge silent.
bool LifebloomOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "lifebloom");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    // BT-157: Lifebloom was the one tank upkeep with no mana rule at all. It is the druid's second pre-pull spell (good).
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::GOOD))
        return false;

    Unit* tank = GetTarget();
    if (!tank || !tank->IsAlive())
        return false;

    Aura* lifebloom = botAI->GetAura("lifebloom", tank, true, true);
    return !lifebloom || lifebloom->GetStackAmount() < LIFEBLOOM_MAX_STACKS ||
           lifebloom->GetDuration() < LIFEBLOOM_REFRESH_BEFORE_MS;
}

// Class sheet, restoration druid (BT-123): see RejuvenationOnMainTankTrigger in DruidTriggers.h.
bool RejuvenationOnMainTankTrigger::IsActive()
{
    uint32 const spellId = AI_VALUE2(uint32, "spell id", "rejuvenation");
    if (!spellId || !bot->HasSpell(spellId))
        return false;

    // BT-157: the tank's upkeep follows the tank guardrail (more than 30 percent mana) instead of the 60 percent save-mana
    // line of the top-ups. Rejuvenation is the druid's first pre-pull spell (core).
    if (!HealerUpkeep::TankUpkeepAllowed(botAI, BotSkillMath::Tier::CASUAL))
        return false;

    return BuffOnMainTankTrigger::IsActive();
}

// Class sheet, restoration druid (BT-123): Magic dispels, a good line. The dispel edges run at ACTION_DISPEL, above every
// heal chain, so a member below the critical threshold is healed first.
bool NaturesCureMagicTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_NATURES_CURE) || !BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return false;

    if (AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->criticalHealth)
        return false;

    return NeedCureTrigger::IsActive();
}

bool PartyMemberNaturesCureMagicTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_NATURES_CURE) || !BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return false;

    if (AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->criticalHealth)
        return false;

    return PartyMemberNeedCureTrigger::IsActive();
}

// Class sheet, restoration druid (BT-123): Nature's Swiftness is saved for an emergency, so the instant Healing Touch
// that follows it waits for a member below the low health threshold. A casual bot (not a "good" line) keeps the old
// behaviour and heals whoever is lowest at once.
bool NaturesSwiftnessHealTrigger::IsActive()
{
    if (!botAI->HasAura("nature's swiftness", bot))
        return false;

    if (!BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return true;

    return AI_VALUE2(uint8, "health", "party member to heal") < sPlayerbotAIConfig->lowHealth;
}

// Class sheet, guardian druid (BT-131): see DruidSelfRejuvenationTrigger in DruidTriggers.h. Rebirth is not part of this.
bool DruidSelfRejuvenationTrigger::IsActive() { return !TankKeepsBearForm(bot) && !(bot->HasAura(768) && GroupHasLivingHealer(bot)) && LowHealthTrigger::IsActive(); }

bool DruidSelfHealingTouchTrigger::IsActive() { return !TankKeepsBearForm(bot) && !(bot->HasAura(768) && GroupHasLivingHealer(bot)) && LowHealthTrigger::IsActive(); }

bool TankBearFormTrigger::IsActive()
{
    if (!bot->IsAlive() || !bot->GetGroup() || !PlayerBotSpec::IsTank(bot) || !bot->HasSpell(SPELL_BEAR_FORM))
        return false;

    // Only from plain caster form: a mount, travel, aquatic and flight form (and the water) belong to other edges. The
    // aquatic form edge (AquaticFormTrigger) has the same relevance, so it must not meet this one in the water.
    if (bot->IsMounted() || bot->GetShapeshiftForm() != FORM_NONE || bot->IsUnderWater())
        return false;

    // Resurrection is cast from caster form (the revive node asks for it as a prerequisite).
    if (AI_VALUE(Unit*, "party member to resurrect"))
        return false;

    // Eating and drinking are impossible in Bear Form (DrinkAction::isPossible), so a bot that is about to refill stays
    // in caster form until it is done; same triggers as UseFoodStrategy.cpp.
    // BT-144: only when it can refill: a tank with no food or water would otherwise wait in caster form for good.
    Trigger* healthTrigger = context->GetTrigger("low health");
    Trigger* manaTrigger = context->GetTrigger("low mana");
    bool const canEat = AI_VALUE2(uint32, "item count", "food") > 0;
    bool const canDrink = AI_VALUE2(uint32, "item count", "water") > 0;
    if ((canEat && healthTrigger && healthTrigger->IsActive()) || (canDrink && manaTrigger && manaTrigger->IsActive()))
        return false;

    return true;
}

// Damage druid sheets: use the same refill and travel safeguards as the tank.
bool DruidDamageFormTrigger::IsActive()
{
    if (!bot->IsAlive() || BotGetSpecialization(bot) != (balance ? SPEC_DRUID_BALANCE : SPEC_DRUID_FERAL) ||
        !bot->HasSpell(balance ? 24858 : 768))
        return false;

    // Only from plain caster form: a mount, travel, aquatic and flight form (and the water) belong to other edges. The
    // aquatic form edge (AquaticFormTrigger) has the same relevance, so it must not meet this one in the water.
    if (bot->IsMounted() || bot->GetShapeshiftForm() != FORM_NONE || bot->IsUnderWater())
        return false;

    // Resurrection is cast from caster form (the revive node asks for it as a prerequisite).
    if (AI_VALUE(Unit*, "party member to resurrect"))
        return false;

    // Eating and drinking are impossible in Bear Form (DrinkAction::isPossible), so a bot that is about to refill stays
    // in caster form until it is done; same triggers as UseFoodStrategy.cpp.
    // BT-144: only when it can refill: a tank with no food or water would otherwise wait in caster form for good.
    Trigger* healthTrigger = context->GetTrigger("low health");
    Trigger* manaTrigger = context->GetTrigger("low mana");
    bool const canEat = AI_VALUE2(uint32, "item count", "food") > 0;
    bool const canDrink = AI_VALUE2(uint32, "item count", "water") > 0;
    if ((canEat && healthTrigger && healthTrigger->IsActive()) || (canDrink && manaTrigger && manaTrigger->IsActive()))
        return false;

    return true;
}

// Damage sheets: use major cooldowns on durable enemies or large pulls. Guardian keeps BT-131 timing.
// BT-620: against an enemy player there is no elite, boss or pack, so the PvE gate never opened (Berserk never fired in a duel,
// a battleground or an arena). A player target asks the burst window instead (the kill window of the player guides); the
// elite, boss and pack gate for everything else is as it was.
static bool DruidMajorPull(PlayerbotAI* ai)
{
    Player* self = ai->GetBot();
    Unit* target = ai->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    if (PvpBurstGlue::TargetIsEnemyPlayer(ai, self, target))
        return PvpBurstGlue::Open(ai, self, target);
    Creature* creature = target ? target->ToCreature() : nullptr;
    return (creature && (creature->isElite() || creature->isWorldBoss())) || ai->GetAiObjectContext()->GetValue<uint8>("attacker count")->Get() >= 3;
}
bool BerserkTrigger::IsActive()
{
    return BotGetSpecialization(bot) == SPEC_DRUID_GUARDIAN ? BoostTrigger::IsActive() :
        bot->HasSpell(106952) && DruidMajorPull(botAI) && BuffTrigger::IsActive();
}
bool ForceOfNatureTrigger::IsActive()
{
    return BotGetSpecialization(bot) == SPEC_DRUID_GUARDIAN ? BoostTrigger::IsActive() :
        bot->HasSpell(106737) && DruidMajorPull(botAI) && BuffTrigger::IsActive();
}
bool NaturesVigilTrigger::IsActive()
{
    return BotGetSpecialization(bot) == SPEC_DRUID_GUARDIAN ? BoostTrigger::IsActive() :
        bot->HasSpell(124974) && DruidMajorPull(botAI) && BuffTrigger::IsActive();
}
bool CelestialAlignmentTrigger::IsActive()
{
    int32 power = int32(bot->GetPower(POWER_ECLIPSE));
    bool ending = (bot->HasAura(48517) && power > 0 && power <= 20) ||
                  (bot->HasAura(48518) && power < 0 && power >= -20);
    return bot->HasSpell(112071) && ending && DruidMajorPull(botAI) && BuffTrigger::IsActive();
}

bool BalanceMovingTrigger::IsActive()
{
    return bot->isMoving() && AI_VALUE(Unit*, "current target") && bot->HasSpell(8921);
}

// BT-668 to BT-672: see DruidTriggers.h.
bool PvpRestoDruidTrigger::IsActive()
{
    return PvpRestoDruidGlue::Due(botAI, bot, rule);
}
