/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TalentSpells.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "SpellInfo.h"

using TalentSpellPolicy::Facts;
using TalentSpellPolicy::Talent;

namespace
{
// Attackers that are near enough to the bot to be caught by its own melee-range peels (Ring of Frost's 10 yd circle,
// Disorienting Roar's 10 yd, a monk's ring).
constexpr float SELF_PEEL_REACH_YD = 8.0f;
// Leg Sweep's radius (SpellRadius of 119381) and the reach of a player's fear (Psychic Scream, Howl of Terror and
// Intimidating Shout are close-range, Fear itself is a 30 yd cast; 20 yd keeps the guard on the players who threaten).
constexpr float LEG_SWEEP_RADIUS_YD = 5.0f;
constexpr float FEAR_CASTER_REACH_YD = 20.0f;
constexpr float MAIN_TANK_WARD_RANGE_YD = 40.0f;

bool InPvp(Player* bot, Unit* target)
{
    if (bot->InBattleground() || bot->InArena())
        return true;
    if (bot->duel && bot->duel->opponent && bot->duel->startTime != 0)
        return true;
    return target && target->GetTypeId() == TYPEID_PLAYER;
}

bool EnemyPlayerWithin(Player* bot, float reach)
{
    std::list<Player*> players;
    bot->GetPlayerListInGrid(players, reach);
    for (Player* player : players)
        if (player != bot && bot->IsValidAttackTarget(player) && bot->CanSeeOrDetect(player))
            return true;
    return false;
}

bool FearCasterWithin(Player* bot, float reach)
{
    std::list<Player*> players;
    bot->GetPlayerListInGrid(players, reach);
    for (Player* player : players)
    {
        if (player == bot || !bot->IsValidAttackTarget(player))
            continue;
        uint8 const cls = player->getClass();
        if (cls == CLASS_PRIEST || cls == CLASS_WARLOCK || cls == CLASS_WARRIOR)
            return true;
    }
    return false;
}

unsigned AttackersOnBotWithin(Player* bot, float reach)
{
    unsigned count = 0;
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsAlive() && attacker->GetVictim() == bot && bot->IsWithinDistInMap(attacker, reach))
            ++count;
    return count;
}

bool IsRooted(Player* bot) { return bot->HasAuraType(SPELL_AURA_MOD_ROOT) || bot->HasUnitState(UNIT_STATE_ROOT); }

bool IsImpaired(Player* bot) { return IsRooted(bot) || bot->GetSpeedRate(MOVE_RUN) < 1.0f; }
}  // namespace

bool TalentSpellTrigger::IsActive()
{
    TalentSpellPolicy::Entry const& entry = TalentSpellPolicy::Of(talent);
    if (!bot->IsAlive() || bot->getClass() != entry.playerClass || !bot->HasSpell(entry.spellId))
        return false;
    if (!bot->IsInCombat() || bot->IsMounted())
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    Facts f;
    f.healthPct = bot->GetHealthPct();
    f.tank = PlayerBotSpec::IsTank(bot);
    f.healer = PlayerBotSpec::IsHeal(bot);
    f.melee = PlayerBotSpec::IsMelee(bot);
    f.grouped = bot->GetGroup() != nullptr;
    if (target && target->IsAlive() && target->IsInWorld() && target->GetMapId() == bot->GetMapId())
    {
        f.targetDistance = std::max(0.01f, bot->GetDistance(target));
        f.targetIsPlayer = target->GetTypeId() == TYPEID_PLAYER;
    }

    switch (talent)
    {
        case Talent::PSYFIEND:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
            return TalentSpellPolicy::WantsPsyfiend(f);
        case Talent::SPECTRAL_GUISE:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
            return TalentSpellPolicy::WantsEscape(f);
        case Talent::ANGELIC_FEATHER:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            return TalentSpellPolicy::WantsKiteSpeed(f) && !botAI->HasAura("angelic feather", bot);
        case Talent::SPEED_OF_LIGHT:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.rooted = IsRooted(bot);
            return TalentSpellPolicy::WantsSpeedOfLight(f);
        case Talent::BURST_OF_SPEED:
        case Talent::WINDWALK_TOTEM:
            f.rooted = IsRooted(bot);
            f.impaired = IsImpaired(bot);
            f.pvp = InPvp(bot, target);
            return TalentSpellPolicy::WantsFreedom(f);
        case Talent::RING_OF_FROST:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
            return TalentSpellPolicy::WantsRingOfFrost(f);
        case Talent::LEG_SWEEP:
        {
            Unit* caster = AI_VALUE(Unit*, "enemy caster target");
            // "enemy caster target" holds only interruptible casts; a stun stops one in any fight.
            f.casterInRadius = caster && caster->IsAlive() && bot->IsWithinDistInMap(caster, LEG_SWEEP_RADIUS_YD);
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
            unsigned inRadius = 0;
            for (BotGuid guid : AI_VALUE(GuidVector, "attackers"))
                if (Unit* unit = botAI->GetUnit(guid))
                    if (unit->IsAlive() && bot->IsWithinDistInMap(unit, LEG_SWEEP_RADIUS_YD))
                        ++inRadius;
            f.enemiesInRadius = inRadius;
            return TalentSpellPolicy::WantsLegSweep(f);
        }
        case Talent::RING_OF_PEACE:
        case Talent::DISORIENTING_ROAR:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
            return TalentSpellPolicy::WantsSelfPeel(f);
        case Talent::DISPLACER_BEAST:
            f.catForm = botAI->HasAura("cat form", bot);
            f.rooted = IsRooted(bot);
            f.facingTarget = target && AI_VALUE2(bool, "facing", "current target");
            return TalentSpellPolicy::WantsLeap(f);
        case Talent::DIVINE_STAR:
        {
            f.facingTarget = target && AI_VALUE2(bool, "facing", "current target");
            if (f.healer)
            {
                Unit* healTarget = AI_VALUE(Unit*, "party member to heal");
                f.healTargetPct = healTarget ? healTarget->GetHealthPct() : 100.0f;
            }
            return target && bot->IsValidAttackTarget(target) && TalentSpellPolicy::WantsDivineStar(f);
        }
        case Talent::HOLY_PRISM_HEAL:
        {
            Unit* healTarget = AI_VALUE(Unit*, "party member to heal");
            f.healTargetPct = healTarget ? healTarget->GetHealthPct() : 100.0f;
            return TalentSpellPolicy::WantsPrismHeal(f);
        }
        case Talent::HOLY_PRISM_DAMAGE:
            return target && bot->IsValidAttackTarget(target) && TalentSpellPolicy::WantsPrismDamage(f);
        case Talent::STAGGERING_SHOUT:
            f.targetSnared = target && (target->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED) ||
                target->GetSpeedRate(MOVE_RUN) < 1.0f);
            return TalentSpellPolicy::WantsStaggeringShout(f);
        case Talent::LICHBORNE:
            f.pvp = InPvp(bot, target);
            f.fearCasterNear = f.pvp && FearCasterWithin(bot, FEAR_CASTER_REACH_YD);
            return TalentSpellPolicy::WantsLichborne(f);
        case Talent::DESECRATED_GROUND:
            f.enemyPlayerClose = EnemyPlayerWithin(bot, NOMINAL_MELEE_RANGE);
            return TalentSpellPolicy::WantsDesecratedGround(f);
        case Talent::CENARION_WARD:
        {
            if (f.healer)
            {
                Unit* tank = AI_VALUE(Unit*, "main tank");
                f.mainTankInCombat = tank && tank != bot && tank->IsAlive() && tank->IsInCombat() &&
                    bot->IsWithinDistInMap(tank, MAIN_TANK_WARD_RANGE_YD);
                f.hasAura = tank && botAI->HasAura("cenarion ward", tank);
            }
            else
            {
                f.attackersOnBot = AttackersOnBotWithin(bot, SELF_PEEL_REACH_YD);
                f.hasAura = botAI->HasAura("cenarion ward", bot);
            }
            return TalentSpellPolicy::WantsCenarionWard(f);
        }
        case Talent::SAFEGUARD:
            // A protection warrior of level 72 and up reaches Safeguard through its own Intervene edge (the swap); below
            // that, and for a damage warrior, this edge protects the member.
            return AI_VALUE(Unit*, "party member to protect") != nullptr;
        case Talent::HEART_OF_THE_WILD:
            // The class boost rule (GenericBoostTrigger): an enemy player, or a fight the group is not winning.
            return (target && target->GetTypeId() == TYPEID_PLAYER) || AI_VALUE(uint8, "balance") <= 50;
        case Talent::COUNT:
            break;
    }
    return false;
}

std::string const TalentSpellAction::GetTargetName()
{
    switch (talent)
    {
        case Talent::HOLY_PRISM_HEAL:
            return "party member to heal";
        case Talent::HOLY_PRISM_DAMAGE:
        case Talent::STAGGERING_SHOUT:
            return "current target";
        case Talent::CENARION_WARD:
            return PlayerBotSpec::IsHeal(bot) ? "main tank" : "self target";
        case Talent::SAFEGUARD:
            return "party member to protect";
        default:
            // Self casts, and the ground spells placed at the bot's feet (Psyfiend, Angelic Feather, Ring of Frost under
            // its attackers): PlayerbotAI::CastSpell sets a destination spell's point on the target unit.
            return "self target";
    }
}

void TalentSpellsStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    uint8 const playerClass = botAI->GetBot()->getClass();
    auto add = [&](Talent talent, float relevance)
    {
        TalentSpellPolicy::Entry const& entry = TalentSpellPolicy::Of(talent);
        if (entry.playerClass != playerClass)
            return;
        triggers.push_back(new TriggerNode(entry.name, NextAction::array(0, new NextAction(entry.name, relevance), nullptr)));
    };

    // Escapes and peels sit above the movement edges (ACTION_MOVE) and the critical heals: a priest at 30 percent
    // under two mobs hides first, then heals. Interrupts stay with the class kicks (ACTION_INTERRUPT and up).
    add(Talent::SPECTRAL_GUISE, ACTION_INTERRUPT - 3);
    add(Talent::DESECRATED_GROUND, ACTION_INTERRUPT - 2);
    add(Talent::LEG_SWEEP, ACTION_INTERRUPT - 1);
    add(Talent::PSYFIEND, ACTION_MOVE + 4);
    add(Talent::RING_OF_FROST, ACTION_MOVE + 3);
    add(Talent::RING_OF_PEACE, ACTION_MOVE + 3);
    add(Talent::DISORIENTING_ROAR, ACTION_MOVE + 3);
    add(Talent::BURST_OF_SPEED, ACTION_MOVE + 2);
    add(Talent::WINDWALK_TOTEM, ACTION_MOVE + 2);
    add(Talent::ANGELIC_FEATHER, ACTION_MOVE + 1);
    add(Talent::SPEED_OF_LIGHT, ACTION_MOVE + 1);
    add(Talent::DISPLACER_BEAST, ACTION_MOVE);
    // Support and throughput: below the emergencies, above the fillers.
    add(Talent::SAFEGUARD, ACTION_HIGH + 4);
    add(Talent::LICHBORNE, ACTION_HIGH + 5);
    add(Talent::HEART_OF_THE_WILD, ACTION_HIGH + 3);
    add(Talent::STAGGERING_SHOUT, ACTION_HIGH + 2);
    add(Talent::CENARION_WARD, ACTION_MEDIUM_HEAL + 3);
    add(Talent::HOLY_PRISM_HEAL, ACTION_MEDIUM_HEAL + 2);
    add(Talent::DIVINE_STAR, ACTION_LIGHT_HEAL + 5);
    add(Talent::HOLY_PRISM_DAMAGE, ACTION_NORMAL + 2);
}
