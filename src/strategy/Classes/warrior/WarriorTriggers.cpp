#include "WarriorTriggers.h"
#include "PvpArms.h"
#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"

bool ShieldBarrierTrigger::IsActive()
{
    return BuffTrigger::IsActive() && AI_VALUE2(uint8, "rage", "self target") >= 40;
}

// #196 (AC WarriorTriggers.cpp:26-48): fire unless a group member already carries the bot's own Vigilance.
// The action (CastVigilanceAction::GetTarget) picks the highest-gear-score DPS; the old body here duplicated
// the tank-first pick with the same bot->GetHealth() bug.
bool VigilanceTrigger::IsActive()
{
    if (!bot->HasSpell(114030))
    {
        return false;
    }

    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        // The action casts Vigilance on dps, but the trigger fails if any party member has
        // Vigilance from the bot (so the player can have a bot cast Vigilance on any group member).
        if (botAI->HasAura("vigilance", member, false, true))
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Issue #357 slice 10 - bodies for the trigger classes ported from AC
// Ai/Class/Warrior/WarriorTriggers.cpp:20-24 and :50-73. They must live here:
// AI_VALUE2 expands to `context->GetValue<...>()` (an AiObject member) and
// PlayerbotAI::HasAura()/HasSpellCooldown() need a complete Player type.
// ---------------------------------------------------------------------------
bool BloodrageBuffTrigger::IsActive()
{
    // AC WarriorTriggers.cpp:20-24. Bloodrage itself was removed in Cataclysm,
    // so this stays inert on 5.4.8 (no strategy references it); the class is
    // registered only for AC name parity.
    return AI_VALUE2(uint8, "health", "self target") >= sPlayerbotAIConfig->mediumHealth &&
           AI_VALUE2(uint8, "rage", "self target") < 20;
}

namespace
{
// AC WarriorTriggers.cpp:13-16 - unchanged on 5.4.8.
constexpr uint32 SPELL_SHATTERING_THROW = 64382;
constexpr uint32 SPELL_DIVINE_SHIELD = 642;
constexpr uint32 SPELL_ICE_BLOCK = 45438;
// BT-034: 1022 is the player's Hand of Protection; 41450 the creature spell the port checked alone
constexpr uint32 SPELL_HAND_OF_PROTECTION = 1022;
constexpr uint32 SPELL_BLESSING_OF_PROTECTION = 41450;
// Class sheet, protection warrior (BT-125).
constexpr uint32 SPELL_SHIELD_WALL = 871;
constexpr uint32 SPELL_LAST_STAND = 12975;
constexpr uint32 SPELL_ENRAGED_REGENERATION = 55694;
}  // namespace

bool ShatteringThrowTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_SHATTERING_THROW) || bot->HasSpellCooldown(SPELL_SHATTERING_THROW))
        return false;

    GuidVector enemies = AI_VALUE(GuidVector, "possible targets");

    for (BotGuid const& guid : enemies)
    {
        Unit* enemy = botAI->GetUnit(guid);
        if (!enemy || !enemy->IsAlive() || enemy->IsFriendlyTo(bot))
            continue;

        if (bot->IsWithinDistInMap(enemy, 25.0f) &&
            (enemy->HasAura(SPELL_DIVINE_SHIELD) ||
             enemy->HasAura(SPELL_ICE_BLOCK) ||
             enemy->HasAura(SPELL_BLESSING_OF_PROTECTION) || enemy->HasAura(SPELL_HAND_OF_PROTECTION)))
        {
            return true;
        }
    }

    return false;
}

// #522 (AC Ai/Base/Trigger/GenericTriggers.cpp:483-488): AC's base "fear sleep sap" trigger is not registered in
// this fork's shared TriggerContext, so the warrior context registers its own copy (Warrior folder only).
bool WarriorFearSleepSapTrigger::IsActive()
{
    return bot->HasAuraType(SPELL_AURA_MOD_FEAR) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_SLEEP) ||
           bot->HasAuraWithMechanic(1 << MECHANIC_SAPPED);
}

// Task 11 (#866): see ThunderClapTrigger in WarriorTriggers.h.
bool ThunderClapTrigger::IsActive()
{
    constexpr uint32 SPELL_THUNDER_CLAP = 6343;
    constexpr uint32 SPELL_WEAKENED_BLOWS = 115798;
    if (!bot->HasSpell(SPELL_THUNDER_CLAP) || bot->HasSpellCooldown(SPELL_THUNDER_CLAP))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive() || target->IsFriendlyTo(bot) || !bot->IsWithinDistInMap(target, 7.0f))
        return false;

    return !target->HasAura(SPELL_WEAKENED_BLOWS, bot->GetGUID());
}

// Task 11 (#866): see WarriorSelfHealthTrigger in WarriorTriggers.h.
bool WarriorSelfHealthTrigger::IsActive()
{
    return bot->IsAlive() && bot->GetHealthPct() <= float(pct);
}

// Class sheet, protection warrior (BT-125): see TankShieldWallHealthTrigger in WarriorTriggers.h.
bool TankShieldWallHealthTrigger::IsActive()
{
    if (!bot->HasSpell(SPELL_SHIELD_WALL) || bot->HasSpellCooldown(SPELL_SHIELD_WALL))
        return false;

    return WarriorSelfHealthTrigger::IsActive();
}

bool TankLastStandTrigger::IsActive()
{
    if (!bot->IsAlive() || bot->GetHealthPct() > float(TANK_LAST_STAND_HEALTH_PCT))
        return false;

    if (!bot->HasSpell(SPELL_LAST_STAND) || bot->HasSpellCooldown(SPELL_LAST_STAND))
        return false;

    // Shield Wall goes first (its trigger fires at a higher threshold while it is ready); Last Stand follows when the
    // wall is not available or is already running and the health still falls.
    return !bot->HasSpell(SPELL_SHIELD_WALL) || bot->HasSpellCooldown(SPELL_SHIELD_WALL);
}

bool TankEnragedRegenerationHealthTrigger::IsActive()
{
    if (!BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) || !bot->HasSpell(SPELL_ENRAGED_REGENERATION))
        return false;

    return WarriorSelfHealthTrigger::IsActive();
}

// The stance trigger above only exists in the combat engine; a tank that enters combat in Battle Stance swaps in the
// first tick. Same condition as CastDefensiveStanceAction::isUseful, plus a group (solo play keeps its rage income) and
// not mounted (casting a stance would dismount the bot).
bool TankDefensiveStanceTrigger::IsActive()
{
    if (!bot->GetGroup() || bot->IsMounted() || !PlayerBotSpec::IsTank(bot))
        return false;

    return BuffTrigger::IsActive();
}

// Arms and Fury sheets: preserve Berserker Rage for when Enrage has fallen off.
bool WarriorEnrageMissingTrigger::IsActive()
{
    constexpr uint32 ENRAGE_AURA = 12880;
    constexpr uint8 MIN_RAGE = 20;
    return !bot->HasAura(ENRAGE_AURA) && AI_VALUE2(uint8, "rage", "self target") >= MIN_RAGE;
}

bool RagingBlowTwoStacksTrigger::IsActive()
{
    constexpr uint32 RAGING_BLOW_AURA = 131116;
    Aura* aura = bot->GetAura(RAGING_BLOW_AURA);
    return aura && aura->GetStackAmount() >= 2;
}

// Fury sheet: build enough Meat Cleaver stacks before consuming Raging Blow in a pack.
// Reuse the existing attacker-based area triggers, without a new grid search.
bool FuryPackWhirlwindTrigger::IsActive()
{
    constexpr uint32 MEAT_CLEAVER_AURA = 85739;
    constexpr uint8 MEAT_CLEAVER_LEVEL = 58;
    if (bot->getLevel() < MEAT_CLEAVER_LEVEL || !MediumAoeTrigger::IsActive())
        return false;
    HighAoeTrigger fourTargets(botAI);
    uint8 needed = fourTargets.IsActive() ? 3 : 2;
    Aura* aura = bot->GetAura(MEAT_CLEAVER_AURA);
    return !aura || aura->GetStackAmount() < needed;
}

// Damage sheets: a ranged filler for expert bots while the gap closer is unavailable.
bool WarriorExpertThrowTrigger::IsActive()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT) && EnemyOutOfMeleeTrigger::IsActive();
}

bool ArmsColossusSmashWindowUp(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && botAI->HasAura("colossus smash", target, false, true);
}

bool ArmsHeroicStrikeRageTrigger::IsActive()
{
    bool const expert = BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT);
    uint32 const needed = DpsRotationMath::Warrior::HeroicStrikeRage(bot->getLevel(), expert,
        expert && ArmsColossusSmashWindowUp(botAI));
    return AI_VALUE2(uint8, "rage", "self target") >= needed;
}

bool ArmsColossusSmashWindowTrigger::IsActive()
{
    if (!DpsRotationMath::Warrior::WindowRules(bot->getLevel(), BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT)))
        return false;
    return DpsRotationMath::Warrior::SlamBeforeOverpower(bot->getLevel(), true, ArmsColossusSmashWindowUp(botAI),
        AI_VALUE2(uint8, "rage", "self target"));
}

// BT-673 to BT-676: see WarriorTriggers.h.
bool ArmsPvpStunTrigger::IsActive()
{
    return PvpArmsGlue::StunDue(botAI, bot);
}

bool ArmsPvpDisarmTrigger::IsActive()
{
    return PvpArmsGlue::DisarmDue(botAI, bot);
}
