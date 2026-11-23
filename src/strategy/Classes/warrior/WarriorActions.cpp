#include "WarriorActions.h"
#include "PvpArms.h"
#include "WarriorTriggers.h"
#include "BotSkill.h"
#include "DpsRotationMath.h"
#include "Playerbots.h"
#include "PlayerbotSpec.h"
#include "ServerFacade.h"
#include "PathGenerator.h"

namespace
{
// #522 (AC WarriorActions.cpp:12-17). Shattering Throw 64382 is unchanged on 5.4.8 ("removing any
// invulnerabilities", Wowhead tooltip 2026-10-04); the immunity spells are the same ids as the
// existing ShatteringThrowTrigger in WarriorTriggers.cpp.
constexpr uint32 SPELL_DIVINE_SHIELD = 642;
constexpr uint32 SPELL_ICE_BLOCK = 45438;
// BT-034: 1022 is the player's Hand of Protection; 41450 the creature spell the port checked alone
constexpr uint32 SPELL_HAND_OF_PROTECTION = 1022;
constexpr uint32 SPELL_BLESSING_OF_PROTECTION = 41450;
constexpr uint32 SPELL_SHATTERING_THROW = 64382;
// Class sheet, protection warrior (BT-125): Shield Block 2565 costs 60 rage, Cleave 845 costs 30 (spell data).
constexpr uint32 SPELL_SHIELD_BLOCK = 2565;
constexpr uint8 SHIELD_BLOCK_RAGE_COST = 60;
constexpr uint8 CLEAVE_RAGE_COST = 30;
}  // namespace

// #645 (AC WarriorActions.cpp:48-73): no group -> never Sunder; a non-tank defers to a living tank warrior in the group
// on the same map. 5.4.8 Sunder Armor 7386 / Devastate 20243 apply Weakened Armor 113746 (max 3 stacks, not 5), never an
// aura named "sunder armor" (same lesson as the #624 Expose Armor fix), so the aura check reads Weakened Armor.
bool CastSunderArmorAction::isUseful()
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (!PlayerBotSpec::IsTank(bot, false))
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() ||
                member->GetMapId() != bot->GetMapId())
            {
                continue;
            }

            if (member->getClass() == CLASS_WARRIOR && PlayerBotSpec::IsTank(member, false))
                return false;
        }
    }

    Aura* aura = botAI->GetAura("weakened armor", GetTarget(), false, true);
    return !aura || aura->GetStackAmount() < 3 || aura->GetDuration() <= 6000;
}

bool CastHeroicLeapAction::isUseful()
{
    // Task 11 (#866, "Heroic leap", prio 8/0): target 6-40 yd away. Heroic Leap 6544 is an
    // instant dest-location jump with an 8-40 yd range (Spell::CheckRange skips instants), so the band here is 8-40 yd
    // of the target's exact position; the old >= 15 yd gate never allowed it at the 8 yd harness start. The MoP rotation puts
    // Heroic Leap on a 5 s cooldown while Charge is ready: here Charge (ACTION_MOVE + 10) is simply tried first.
    if (botAI->HasStrategy("stay", botAI->GetState()))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    // w75: CheckCast (Spell::CheckRange, Spell.cpp:7497-7502) measures min/max range with IsWithinCombatRange, i.e. plus both
    // combat reaches, so at the harness's 8.0 yd start every tick was a SPELL_FAILED_TOO_CLOSE (result=132) reject. Use the
    // same measure here so the action only turns useful once the cast can succeed.
    if (bot->IsWithinCombatRange(target, 8.0f) || !bot->IsWithinCombatRange(target, 40.0f))
        return false;

    // #886: the leap lands on the target's raw position (SetDst(*target)), so a target off the navmesh - a critter under a
    // floor - dropped the bot under the Shrine of Two Moons. Same reachability test AC's Charge makes (AC Spell.cpp:6472-6491).
    PathGenerator path(bot);
    path.CalculatePath(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false);
    // PORT: SkyFire does not expose the far-end polygon or destination Z checks; reject incomplete paths too.
    return !(path.GetPathType() & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE));
}

// #522 (AC ArmsWarriorStrategy.cpp:101-108): AC's Arms warrior holds Battle Stance. In 5.4.8 stances only change rage
// income (Battle Stance 2457 "generates high Rage from normal melee attacks", Berserker Stance 2458 "some Rage from
// normal melee attacks and some from damage taken", Wowhead mop-classic tooltips 2026-10-05), and wave 10 showed a
// solo Arms bot taking Berserker Stance (solo -> berserker for every non-tank). Arms now always uses Battle Stance.
bool CastBattleStanceAction::isUseful()
{
    return (botAI->GetBot() && !PlayerBotSpec::IsTank(botAI->GetBot()) &&
            (botAI->GetBot()->GetGroup() || botAI->HasStrategy("arms", BOT_STATE_COMBAT)));
}
bool CastBerserkerStanceAction::isUseful()
{
    return (botAI->GetBot() && !botAI->GetBot()->GetGroup() && !PlayerBotSpec::IsTank(botAI->GetBot()) &&
            !botAI->HasStrategy("arms", BOT_STATE_COMBAT));
}
bool CastDefensiveStanceAction::isUseful()
{
    return PlayerBotSpec::IsTank(botAI->GetBot());
}

// #196 (AC WarriorActions.cpp:94-125): pick the highest-gear-score DPS in range that has no Vigilance;
// bail out when the bot's own Vigilance is already on a member. The old body compared bot->GetHealth()
// (the caster's own health) instead of each member's, and preferred tanks (not in AC).
// Vigilance 114030 is a 5.4.8 talent: "reduce their damage taken by 30% for 12 sec" (Wowhead tooltip, 2026-10-04).
Unit* CastVigilanceAction::GetTarget()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* vigilanceTarget = nullptr;
    uint32 highestGearScore = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        // Same as the trigger: early return if the bot's own Vigilance is already applied to a member.
        if (botAI->HasAura("vigilance", member, false, true))
            return nullptr;

        // A valid target must not have Vigilance from any source, not just the bot...
        if (botAI->HasAura("vigilance", member))
            continue;

        // ...and it must be in range and be a dps.
        if (member->GetMapId() != bot->GetMapId() || !PlayerBotSpec::IsDps(member) ||
            bot->GetDistance(member) > sPlayerbotAIConfig->spellDistance)
        {
            continue;
        }

        // AC botAI->GetEquipGearScore(member) -> PlayerbotAI::GetMixedGearScore(member, true, false)
        // (established mapping, see Manager/PlayerbotSecurity.cpp:7).
        uint32 gearScore = PlayerbotAI::GetMixedGearScore(member, true, false);
        if (gearScore > highestGearScore)
        {
            highestGearScore = gearScore;
            vigilanceTarget = member;
        }
    }

    return vigilanceTarget;
}

bool CastVigilanceAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target || target == bot)
        return false;

    return botAI->CastSpell("vigilance", target);
}

// #522 (AC WarriorActions.cpp:176-243): Shattering Throw an enemy that is immune (Divine Shield, Ice Block,
// Blessing of Protection) within 25 yd.
Unit* CastShatteringThrowAction::GetTarget()
{
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
            return enemy;
        }
    }

    return nullptr;
}

bool CastShatteringThrowAction::Execute(Event /*event*/)
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return botAI->CastSpell("shattering throw", target);
}

bool CastShatteringThrowAction::isUseful()
{
    if (!bot->HasSpell(SPELL_SHATTERING_THROW) || bot->HasSpellCooldown(SPELL_SHATTERING_THROW))
        return false;

    return GetTarget() != nullptr;
}

bool CastShatteringThrowAction::isPossible()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    // Range check: Shattering Throw is 30 yards
    if (!bot->IsWithinDistInMap(target, 30.0f))
        return false;

    // Check line of sight
    if (!bot->IsWithinLOSInMap(target))
        return false;

    return true;
}

// ---------------------------------------------------------------------------
// Task 11 (#866, epic #854): MoP warrior rotation port.
// ---------------------------------------------------------------------------
namespace
{
bool CasterWithin(PlayerbotAI* botAI, Unit* caster, float dist)
{
    Player* bot = botAI->GetBot();
    return caster && bot && bot->IsWithinDistInMap(caster, dist) &&
           botAI->GetAiObjectContext()->GetValue<bool>("in pvp instance")->Get();
}
}  // namespace

// Shockwave (prio group 6): see the header comment for the sane frontal cone.
bool CastShockwaveAction::isUseful()
{
    if (!CastDebuffSpellAction::isUseful())
        return false;

    Unit* target = GetTarget();
    if (!target)
        return false;

    return bot->IsWithinDistInMap(target, 10.0f) && bot->isInFront(target, float(M_PI) / 2.0f) &&
           !AI_VALUE2(bool, "is cc", GetTargetName());
}

// Storm Bolt (prio group 5): only stun a target that is not already stunned/feared/rooted.
bool CastStormBoltAction::isUseful()
{
    return CastDebuffSpellAction::isUseful() && !AI_VALUE2(bool, "is cc", GetTargetName());
}

// (Pummel on a casting enemy): melee range only.
bool CastPummelOnEnemyCasterAction::isUseful()
{
    return CastSpellOnEnemyCasterAction::isUseful() && CasterWithin(botAI, GetTarget(), 5.0f);
}

// Disrupting Shout (caster within 10 yd).
bool CastDisruptingShoutOnEnemyCasterAction::isUseful()
{
    // Damage sheets: the same caster value works in dungeons as well as PvP.
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && CastSpellOnEnemyCasterAction::isUseful() &&
           bot->IsWithinDistInMap(GetTarget(), 10.0f);
}

// Intimidating Shout (caster within 8 yd).
bool CastIntimidatingShoutOnEnemyCasterAction::isUseful()
{
    return CastSpellOnEnemyCasterAction::isUseful() && CasterWithin(botAI, GetTarget(), 8.0f);
}

// Class sheet, protection warrior (BT-125): Cleave (30 rage, relevance ACTION_HIGH) used to fire at 30 rage and kept a
// tank from ever reaching the 60 rage Shield Block needs. While Shield Block is ready (learned, off cooldown, not up) the
// tank cleaves only with the rage for both.
bool CastCleaveAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;

    if (!PlayerBotSpec::IsTank(bot) || !bot->HasSpell(SPELL_SHIELD_BLOCK) || bot->HasSpellCooldown(SPELL_SHIELD_BLOCK) ||
        botAI->HasAura("shield block", bot))
        return true;

    return AI_VALUE2(uint8, "rage", "self target") >= SHIELD_BLOCK_RAGE_COST + CLEAVE_RAGE_COST;
}

bool CastBladestormAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful() || PlayerBotSpec::IsTank(bot))
        return false;
    // Arms sheet: reserve the dungeon talent for a pack of at least three.
    if (botAI->HasStrategy("arms", botAI->GetState()))
    {
        MediumAoeTrigger pack(botAI);
        return pack.IsActive();
    }
    return true;
}

// Class sheet, protection warrior (BT-125): the base action checks the interrupt's own range (melee for Pummel) on the
// caster the task 0 value picked; no PvP map condition.
bool CastPummelOnPackCasterAction::isUseful()
{
    return BotSkill::Uses(bot, BotSkillMath::Tier::GOOD) && CastSpellOnEnemyCasterAction::isUseful();
}

bool CastThunderClapAction::isUseful()
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !bot->IsWithinDistInMap(target, THUNDER_CLAP_RADIUS))
        return false;

    return CastSpellAction::isUseful();
}

bool CastExecuteAction::isUseful()
{
    if (!CastMeleeSpellAction::isUseful())
        return false;
    if (BotGetSpecialization(bot) != SPEC_WARRIOR_ARMS)
        return true;
    bool const expert = BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT);
    uint32 const needed = DpsRotationMath::Warrior::ExecuteRage(bot->getLevel(), expert,
        expert && ArmsColossusSmashWindowUp(botAI));
    return AI_VALUE2(uint8, "rage", "self target") >= needed;
}

// BT-673: see WarriorActions.h.
bool CastArmsPvpStunAction::isUseful()
{
    return PvpArmsGlue::StunDue(botAI, bot);
}

bool CastArmsPvpStunAction::Execute(Event /*event*/)
{
    return PvpArmsGlue::StunCast(botAI, bot);
}
