/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

// Arathi Basin (map 529) bot tactics - BT-156, docs/bg-sheets/arathi-basin.md. The team plan, the jobs and the option
// scores live in AbPlan.cpp and Utils/AbPlanLogic.h; this file is the part that needs the tactics class: it stores the
// objective the plan chose, reacts when the plan state changes, and hits the enemy that is capturing one of our banners.
// The older objective code of BattleGroundTactics.cpp stays the fallback when the plan cannot say.

#include "BattleGroundTactics.h"

// DatabaseEnv.h must precede the battleground headers (see BattleGroundTactics.cpp).
#include "DatabaseEnv.h"

#include "AbPlan.h"
#include "BgTargetOrderPolicy.h"
#include "BotSkill.h"
#include "Battleground.h"
#include "BattlegroundAB.h"
#include "BattlegroundEY.h"
#include "BgNodeMap.h"
#include "ObjectAccessor.h"
#include "PlayerbotSpec.h"
#include "PositionValue.h"
#include "Playerbots.h"
#include "WorldSession.h"
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{
// An expert stops the enemy's capture cast with an interrupt first, then a stun, whatever the class knows (spell names as
// the class strategies use them; CanCastSpell is false for a spell the bot does not have). Whether an interrupt stops
// spell 21651 is set in SpellInterrupts.dbc (sheet section 3; to check live).
char const* const CAPTURE_STOPPERS[] = {
    "kick", "pummel", "wind shear", "counterspell", "rebuke", "spear hand strike", "mind freeze", "silencing shot",
    "hammer of justice", "kidney shot"
};

// The raider takes its stealth this close to the node (and not closer than the minimum: at the banner the capture starts),
// and an expert opens on the one defender from this range.
float const RAID_STEALTH_RANGE = 70.0f;
float const RAID_STEALTH_MIN = 12.0f;
float const RAID_OPENER_RANGE = 20.0f;
// Eye of the Storm: the approach in stealth starts this far out and ends this far outside the circle's edge.
float const RAID_STEALTH_RANGE_EYE = 130.0f;
float const RAID_CIRCLE_MARGIN = 5.0f;
char const* const RAID_OPENERS[] = { "sap", "cheap shot" };
}  // namespace

bool BGTactics::selectObjectiveAb(Battleground* bg)
{
    if (!AbPlan::Enabled(bg))
        return false;

    uint8 const strategyAlliance = GetBotStrategyForTeam(bg, TEAM_ALLIANCE);
    uint8 const strategyHorde = GetBotStrategyForTeam(bg, TEAM_HORDE);

    AbPlan::Objective objective;
    if (!AbPlan::Decide(bg, bot, strategyAlliance, strategyHorde, objective))
        return false;

    PositionMap& posMap = context->GetValue<PositionMap&>("position")->Get();
    PositionInfo pos = posMap["bg objective"];
    pos.Set(objective.x, objective.y, objective.z, bot->GetMapId());
    posMap["bg objective"] = pos;
    return true;
}

void BGTactics::abTick(Battleground* bg)
{
    if (!AbPlan::Enabled(bg) || !bot->IsAlive())
        return;

    // A call-out that is due (the bot's own delay passed, the team's limiter allows it).
    AbPlan::DeliverCalls(botAI);

    uint8 const strategyAlliance = GetBotStrategyForTeam(bg, TEAM_ALLIANCE);
    uint8 const strategyHorde = GetBotStrategyForTeam(bg, TEAM_HORDE);

    // A changed plan state (a node, a regime, a job, a call): choose again, after the bot's tier lag.
    if (AbPlan::PlanChanged(bg, bot, strategyAlliance, strategyHorde))
    {
        resetObjective();
        return;
    }

    HuntCapper(bg);
    RaiderMoves(bg);
    FightOrder(bg);
}

// G7: an enemy casting the capture within reach of one of our banners is the first target (good rule); an expert tries to
// stop the cast first. The list of such enemies is part of the plan's record, rebuilt every two seconds: no scan here.
void BGTactics::HuntCapper(Battleground* bg)
{
    uint64 const capperGuid = AbPlan::CapperGuid(bg, bot);
    if (!capperGuid)
        return;

    Player* capper = ObjectAccessor::GetPlayer(*bot, capperGuid);
    if (!capper || !capper->IsAlive() || !bot->IsValidAttackTarget(capper))
        return;

    if (BotSkill::Uses(bot, BotSkillMath::Tier::EXPERT))
    {
        for (char const* name : CAPTURE_STOPPERS)
            if (botAI->CanCastSpell(name, capper) && botAI->CastSpell(name, capper))
                return;
    }

    if (AI_VALUE(Unit*, "current target") != capper)
        EngageThreat(bot, capper);
}

// G9: a raider (a rogue or a feral druid) goes to the enemy node in stealth and, for an expert, deals with the one defender
// first (Sap, else Cheap Shot). Starting the capture probably breaks the stealth (sheet section 6; to check live), so
// stealth is only for the approach: it is taken within RAID_STEALTH_RANGE of the node and not at the banner.
void BGTactics::RaiderMoves(Battleground* bg)
{
    AbPlan::Raid raid;
    if (!AbPlan::GetRaid(bg, bot, raid) || bot->IsInCombat())
        return;

    // Eye of the Storm (BT-582): a stealthed player does not count at a base (Player::CanCaptureTowerPoint), so the pair sneaks up to
    // the edge of the 70 yd circle and walks into it visible; there is no cast to break the stealth and no banner to stand at.
    if (bg->GetMapId() == BgNodeMap::EYE.mapId)
    {
        if (raid.distance <= BgNodeMap::EYE.circleRadius + RAID_CIRCLE_MARGIN)
        {
            if (bot->HasStealthAura())
                bot->RemoveAurasByType(SPELL_AURA_MOD_STEALTH);
            return;
        }
        if (raid.distance > RAID_STEALTH_RANGE_EYE)
            return;
    }
    else if (raid.distance > RAID_STEALTH_RANGE || raid.distance < RAID_STEALTH_MIN)
        return;

    if (!bot->HasStealthAura())
    {
        // A mount does not stealth: the raider gets off it on the way in.
        if (bot->IsMounted())
            bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        if (bot->getClass() == CLASS_ROGUE && botAI->CanCastSpell("stealth", bot))
            botAI->CastSpell("stealth", bot);
        else if (bot->getClass() == CLASS_DRUID && botAI->CanCastSpell("prowl", bot))
            botAI->CastSpell("prowl", bot);
        return;
    }

    if (!raid.expert || !raid.lone || raid.distance > RAID_OPENER_RANGE)
        return;

    Player* defender = ObjectAccessor::GetPlayer(*bot, raid.lone);
    if (!defender || !defender->IsAlive() || !bot->IsValidAttackTarget(defender))
        return;

    for (char const* name : RAID_OPENERS)
        if (botAI->CanCastSpell(name, defender) && botAI->CastSpell(name, defender))
            return;
}

// BT-553 (G8): the order of targets in a fight at a node (owner decision 2026-11-18): an expert hits the healer first when the
// team holds a numbers edge and drains the squishy damage dealers first otherwise, a good bot the healer first; a casual bot keeps
// the target of its own rotation (the lowest health). Only a bot already in combat with an enemy player, only among the enemy
// players within 30 yd, and a bot changes its target at most every 3 s. The list is the one the bot's targeting already keeps
// ("possible targets"): no new scan.
namespace
{
std::mutex s_fightOrderLock;
std::unordered_map<uint32, uint32> s_lastSwitch;

bool IsSquishy(Player* p)
{
    switch (p->getClass())
    {
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_DEATH_KNIGHT:
            return true;
        case CLASS_PRIEST:
            return !PlayerBotSpec::IsHeal(p, true);
        default:
            return false;
    }
}
}  // namespace

void BGTactics::FightOrder(Battleground* /*bg*/)
{
    if (!bot->IsInCombat() || !BotSkill::Uses(bot, BotSkillMath::Tier::GOOD))
        return;

    GuidVector const enemies = AI_VALUE(GuidVector, "possible targets");
    std::vector<BgTargetOrder::Foe> foes;
    for (BotGuid const guid : enemies)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || unit->GetTypeId() != TYPEID_PLAYER || !unit->IsAlive() || !bot->IsValidAttackTarget(unit))
            continue;
        Player* player = unit->ToPlayer();
        BgTargetOrder::Foe foe;
        foe.guid = player->GetGUID();
        foe.healer = PlayerBotSpec::IsHeal(player, true);
        foe.squishy = IsSquishy(player);
        foe.carrier = player->HasAura(BG_EY_NETHERSTORM_FLAG_SPELL);
        foe.healthPercent = player->GetHealthPct();
        foe.distance = bot->GetDistance(player);
        foes.push_back(foe);
    }
    if (foes.empty())
        return;

    unsigned inReach = 0;
    for (BgTargetOrder::Foe const& foe : foes)
        if (foe.distance <= BgTargetOrder::REACH_YARDS)
            ++inReach;
    unsigned const friends = 1u + unsigned(AI_VALUE(GuidVector, "closest friendly players").size());
    BotSkillMath::Tier const tier = BotSkill::TierOf(bot);
    bool const edge = BgTargetOrder::HasEdge(friends, inReach);

    uint64 const pick = BgTargetOrder::Pick(tier, edge, foes);
    Unit* const current = AI_VALUE(Unit*, "current target");
    uint64 const currentGuid = current ? current->GetGUID() : 0;

    uint32 const now = getMSTime();
    uint32 since;
    {
        std::lock_guard<std::mutex> guard(s_fightOrderLock);
        auto const itr = s_lastSwitch.find(bot->GetGUIDLow());
        since = itr == s_lastSwitch.end() ? BgTargetOrder::MIN_SWITCH_MS : now - itr->second;
    }
    if (!BgTargetOrder::ShouldSwitch(tier, edge, foes, currentGuid, pick, since))
        return;

    Unit* const target = ObjectAccessor::GetUnit(*bot, pick);
    if (!target || !EngageThreat(bot, target))
        return;

    std::lock_guard<std::mutex> guard(s_fightOrderLock);
    s_lastSwitch[bot->GetGUIDLow()] = now ? now : 1;
}
