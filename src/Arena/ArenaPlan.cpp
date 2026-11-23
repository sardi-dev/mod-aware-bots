/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ArenaPlan.h"

#include "ArenaLadder.h"
#include "ArenaPlanMath.h"
#include "Battleground.h"
#include "IVMapManager.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpDefense.h"
#include "PvpTeam.h"
#include "PvpTeamPolicy.h"
#include "Timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ArenaPlan
{
namespace
{
using namespace ArenaPlanMath;

constexpr float RANGE_OF_BOTH = 35.0f;          // yards: a member "can reach" the target within this
constexpr float MELEE_REACH = 8.0f;             // a cover point must lie farther than this from the melee enemies
constexpr float HEALER_REACH = 40.0f;           // the partner healer can heal within this
constexpr float CAUGHT_Z_ABOVE = 2.0f;          // the eye height added to a point for sight checks (as WorldObject::IsWithinLOS)
constexpr float OUTWARD_PROBE = 6.0f;           // dead end probe: a point this far beyond the candidate
constexpr uint32 OUTWARD_PROBES_NEEDED = 2;
constexpr uint32 COVER_SEARCH_GAP_MS = 1500;    // one search per bot per 1.5 s
constexpr uint32 COVER_KEEP_MS = 2000;          // a spot that was found stays valid this long (trigger and action share it)
constexpr uint32 SAMPLE_GAP_MS = 100;           // health samples are at least this far apart
constexpr uint32 LETHAL_WINDOW_MS = 3000;
constexpr uint32 PLAN_ENTRIES_MAX = 16;
constexpr uint32 PLAN_ENTRY_AGE_MS = 3600000;
constexpr uint32 LOG_TOP = 3;

// Auras of a big defensive on a friend of the enemy (the focus term "heavy defensive"): Pain Suppression, Guardian Spirit,
// Life Cocoon, Hand of Protection, Hand of Sacrifice, Ice Block's cousin Dispersion.
constexpr uint32 HEAVY_DEFENSIVE_AURAS[] = { 33206, 47788, 116849, 1022, 6940, 47585 };
// Auras that make an enemy untouchable even though the school immunity type does not show it (Deterrence deflects).
// BT-676: with Touch of Karma (122470), as in PvpUnitFacts.h.
constexpr uint32 EXTRA_IMMUNE_AURAS[] = { 19263, 31224, 122470 };
// A crowd control that a hit breaks (polymorph, sap, fear, sleep, banish, disorient).
constexpr uint32 BREAKS_ON_DAMAGE_MECHANICS = (1u << MECHANIC_POLYMORPH) | (1u << MECHANIC_SAPPED) | (1u << MECHANIC_DISORIENTED) |
    (1u << MECHANIC_FEAR) | (1u << MECHANIC_SLEEP) | (1u << MECHANIC_BANISH) | (1u << MECHANIC_HORROR);

// The self defensives by class, strongest first. Ids are the spells the player casts (5.4.8 Spell.dbc names checked); a
// spell the bot has not learned (a talent, a higher level) is skipped by HasSpell.
struct DefensiveList
{
    uint8 classId;
    uint32 spells[3];
};
constexpr DefensiveList DEFENSIVES[] = {
    { CLASS_WARRIOR, { 871, 118038, 55694 } },          // Shield Wall, Die by the Sword, Enraged Regeneration
    { CLASS_PALADIN, { 642, 498, 0 } },                 // Divine Shield, Divine Protection
    { CLASS_HUNTER, { 19263, 0, 0 } },                  // Deterrence
    { CLASS_ROGUE, { 31224, 5277, 0 } },                // Cloak of Shadows, Evasion
    { CLASS_PRIEST, { 47585, 19236, 0 } },              // Dispersion, Desperate Prayer
    { CLASS_DEATH_KNIGHT, { 48792, 48707, 0 } },        // Icebound Fortitude, Anti-Magic Shell
    { CLASS_SHAMAN, { 108271, 0, 0 } },                 // Astral Shift
    { CLASS_MAGE, { 45438, 0, 0 } },                    // Ice Block
    { CLASS_WARLOCK, { 104773, 0, 0 } },                // Unending Resolve
    { CLASS_MONK, { 115203, 0, 0 } },                   // Fortifying Brew
    { CLASS_DRUID, { 22812, 61336, 0 } },               // Barkskin, Survival Instincts
};

struct TeamPlan
{
    uint32 focusLow = 0;
    uint32 focusSinceMs = 0;
    uint32 lastPlanMs = 0;
    bool planned = false;
    uint32 touchedMs = 0;
};

struct BotMemory
{
    uint32 lastDefensiveMs = 0;
    bool usedDefensive = false;
    uint32 lastCoverSearchMs = 0;
    bool hasCover = false;
    uint32 coverFoundMs = 0;
    float coverX = 0.0f;
    float coverY = 0.0f;
    float coverZ = 0.0f;
    std::deque<std::pair<uint32, uint32>> healthSamples;  // (ms, hp)
};

std::mutex gLock;
std::map<std::pair<uint32, uint32>, TeamPlan> gPlans;  // (arena instance, side) -> plan
std::unordered_map<uint32, BotMemory> gBots;

struct Match
{
    Battleground* bg = nullptr;
    uint32 side = 0;
    BotSkillMath::Tier tier = BotSkillMath::Tier::GOOD;
    TierProfile profile{};
    std::vector<Player*> mates;    // alive, the bot excluded
    std::vector<Player*> enemies;  // alive
};

bool Gather(Player* bot, Match& match)
{
    if (!bot || !bot->IsInWorld() || !bot->IsAlive() || !bot->InArena())
        return false;

    Battleground* bg = bot->GetBattleground();
    if (!bg || bg->GetStatus() != STATUS_IN_PROGRESS || bg->GetStartDelayTime() > 0)
        return false;

    if (!ArenaLadder::TeamTierOf(bot->GetGUIDLow(), match.tier))
        return false;

    match.bg = bg;
    match.side = bot->GetBGTeam();
    match.profile = ProfileOf(match.tier);
    for (auto const& entry : bg->GetPlayers())
    {
        Player* other = ObjectAccessor::FindPlayer(entry.first);
        if (!other || other == bot || !other->IsInWorld() || !other->IsAlive())
            continue;
        (entry.second.Team == match.side ? match.mates : match.enemies).push_back(other);
    }
    return true;
}

bool IsImmune(Unit* unit)
{
    if (unit->HasAuraType(SPELL_AURA_SCHOOL_IMMUNITY) || unit->HasAuraType(SPELL_AURA_DAMAGE_IMMUNITY))
        return true;
    for (uint32 spell : EXTRA_IMMUNE_AURAS)
        if (unit->HasAura(spell))
            return true;
    return false;
}

bool HasHeavyDefensive(Unit* unit)
{
    for (uint32 spell : HEAVY_DEFENSIVE_AURAS)
        if (unit->HasAura(spell))
            return true;
    return false;
}

FocusInput InputFor(Player* bot, Match const& match, Player* enemy, uint32 currentFocus)
{
    FocusInput in;
    in.id = enemy->GetGUIDLow();
    in.hpPercent = int32(enemy->GetHealthPct());
    in.healer = PlayerBotSpec::IsHeal(enemy, true);
    in.casterDamage = !in.healer && PlayerBotSpec::IsCaster(enemy, true);
    in.meleeDamage = !in.healer && !in.casterDamage;
    in.isCurrentFocus = currentFocus == in.id;

    uint32 sees = 0;
    uint32 total = 0;
    bool inRange = true;
    std::vector<Player*> team = match.mates;
    team.push_back(bot);
    for (Player* member : team)
    {
        ++total;
        if (member->IsWithinLOSInMap(enemy))
            ++sees;
        if (!PlayerBotSpec::IsHeal(member, true) && !member->IsWithinDist(enemy, RANGE_OF_BOTH))
            inRange = false;
    }
    in.inRangeOfBoth = inRange;
    in.losSharePercent = total ? int32(100 * sees / total) : 0;
    in.immune = IsImmune(enemy);
    in.heavyDefensive = HasHeavyDefensive(enemy);
    in.ccBreaksOnDamage = enemy->HasAuraWithMechanic(BREAKS_ON_DAMAGE_MECHANICS);
    // BT-628 (play v2): the shared kill window and the held enemy healer come from the team state (PvP/PvpTeam.h). A held healer
    // raises the other targets (the healer is out of the fight for a while); an open go raises a hurt target. The team's own
    // cooldown readiness stays unknown to the plan: each member goes as soon as its own burst is ready.
    in.burstWindow = PvpTeamGlue::GoOpenOn(bot, enemy);
    for (Player* other : match.enemies)
        if (PlayerBotSpec::IsHeal(other, true) && PvpTeam::HealerHeld(PvpTeamGlue::LockedMs(other)))
            in.healerCcd = true;
    return in;
}

void PruneLocked(uint32 nowMs)
{
    if (gPlans.size() <= PLAN_ENTRIES_MAX)
        return;
    for (auto it = gPlans.begin(); it != gPlans.end();)
        it = getMSTimeDiff(it->second.touchedMs, nowMs) > PLAN_ENTRY_AGE_MS ? gPlans.erase(it) : std::next(it);
}
}  // namespace

bool Active(Player* bot)
{
    Match match;
    return Gather(bot, match);
}

Unit* FocusTarget(Player* bot)
{
    Match match;
    if (!Gather(bot, match) || match.enemies.empty())
        return nullptr;

    // A healer heals: the focus is for the damage dealers.
    if (PlayerBotSpec::IsHeal(bot, true))
        return nullptr;

    uint32 const nowMs = getMSTime();
    std::lock_guard<std::mutex> guard(gLock);
    TeamPlan& plan = gPlans[{ match.bg->GetInstanceID(), match.side }];
    plan.touchedMs = nowMs;

    auto resolve = [&](uint32 low) -> Unit*
    {
        for (Player* enemy : match.enemies)
            if (enemy->GetGUIDLow() == low && bot->IsValidAttackTarget(enemy))
                return enemy;
        return nullptr;
    };

    bool const due = !plan.planned || getMSTimeDiff(plan.lastPlanMs, nowMs) >= match.profile.planIntervalMs || !resolve(plan.focusLow);
    if (due)
    {
        std::vector<FocusInput> inputs;
        for (Player* enemy : match.enemies)
            if (bot->IsValidAttackTarget(enemy) && bot->CanSeeOrDetect(enemy))
                inputs.push_back(InputFor(bot, match, enemy, plan.focusLow));

        FocusDecision const decision = ChooseFocus(inputs, plan.focusLow, getMSTimeDiff(plan.focusSinceMs, nowMs), match.profile);
        plan.lastPlanMs = nowMs;
        plan.planned = true;
        if (decision.hasTarget && decision.changed)
        {
            plan.focusLow = decision.id;
            plan.focusSinceMs = nowMs;
        }
        else if (!decision.hasTarget)
            plan.focusLow = 0;

        if (decision.hasTarget && decision.changed)
        {
            // The top candidates, for the log and for a later selector that may choose among them (the score is deterministic).
            std::vector<ArenaOption> options;
            for (FocusInput const& in : inputs)
                options.push_back({ OptionKind::FOCUS, bot->GetGUIDLow(), in.id, 0, FocusScore(in, match.profile) });
            std::sort(options.begin(), options.end(), OptionLess);
            for (size_t i = 0; i < options.size() && i < LOG_TOP; ++i)
                SF_LOG_DEBUG("playerbots", "[ARENAPLAN] side=%u tier=%u kind=FOCUS target=%u score=%d%s", match.side,
                    uint32(match.tier), options[i].target, options[i].score, options[i].target == decision.id ? " (taken)" : "");
        }
    }

    PruneLocked(nowMs);
    return resolve(plan.focusLow);
}

uint32 DefensiveDue(Player* bot)
{
    Match match;
    if (!Gather(bot, match))
        return 0;

    uint32 const nowMs = getMSTime();
    float const hp = bot->GetHealthPct();
    uint32 const health = bot->GetHealth();

    bool recentlyUsed;
    bool predictedLethal = false;
    {
        std::lock_guard<std::mutex> guard(gLock);
        BotMemory& memory = gBots[bot->GetGUIDLow()];
        if (memory.healthSamples.empty() || getMSTimeDiff(memory.healthSamples.back().first, nowMs) >= SAMPLE_GAP_MS)
            memory.healthSamples.emplace_back(nowMs, health);
        while (!memory.healthSamples.empty() && getMSTimeDiff(memory.healthSamples.front().first, nowMs) > LETHAL_WINDOW_MS)
            memory.healthSamples.pop_front();

        uint32 highest = health;
        for (auto const& sample : memory.healthSamples)
            highest = std::max(highest, sample.second);
        predictedLethal = PredictedLethal(health, highest > health ? highest - health : 0);

        recentlyUsed = memory.usedDefensive && getMSTimeDiff(memory.lastDefensiveMs, nowMs) < DEFENSIVE_RECENT_MS;
    }

    bool underPressure = !bot->getAttackers().empty();
    bool healerCanReach = false;
    for (Player* mate : match.mates)
        if (PlayerBotSpec::IsHeal(mate, true) && mate->IsWithinDist(bot, HEALER_REACH) && mate->IsWithinLOSInMap(bot))
            healerCanReach = true;

    int32 const score = DefensiveScore(int32(hp), match.profile.defensiveThreshold, underPressure, !healerCanReach, recentlyUsed);
    if (!ShouldUseDefensive(score, predictedLethal && underPressure, match.profile))
        return 0;

    // BT-522: the plan says WHEN; the shared defensive rules say WHICH (the smaller one first, one per go, the immunity last).
    if (PvpConfig::DefensiveRulesEnabled())
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        PvpDefenseGlue::DefensiveVerdict const verdict = botAI ? PvpDefenseGlue::Defensive(botAI, bot, true)
                                                               : PvpDefenseGlue::DefensiveVerdict{};
        if (verdict.use)
            SF_LOG_DEBUG("playerbots", "[ARENAPLAN] side=%u tier=%u kind=DEFENSIVE actor=%u spell=%u score=%d hp=%.0f why=%s", match.side,
                uint32(match.tier), bot->GetGUIDLow(), verdict.spellId, score, hp, verdict.why);
        return verdict.use ? verdict.spellId : 0;
    }

    for (DefensiveList const& list : DEFENSIVES)
    {
        if (list.classId != bot->getClass())
            continue;
        for (uint32 spell : list.spells)
            if (spell && bot->HasSpell(spell) && !bot->HasSpellCooldown(spell))
            {
                PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
                if (botAI && botAI->CanCastSpell(spell, bot))
                {
                    SF_LOG_DEBUG("playerbots", "[ARENAPLAN] side=%u tier=%u kind=DEFENSIVE actor=%u spell=%u score=%d hp=%.0f", match.side,
                        uint32(match.tier), bot->GetGUIDLow(), spell, score, hp);
                    return spell;
                }
            }
    }
    return 0;
}

void NoteDefensiveCast(Player* bot)
{
    std::lock_guard<std::mutex> guard(gLock);
    BotMemory& memory = gBots[bot->GetGUIDLow()];
    memory.lastDefensiveMs = getMSTime();
    memory.usedDefensive = true;
}

void NoteCoverTaken(Player* bot)
{
    std::lock_guard<std::mutex> guard(gLock);
    gBots[bot->GetGUIDLow()].hasCover = false;
}

bool CoverDue(Player* bot, float& x, float& y, float& z)
{
    Match match;
    if (!Gather(bot, match) || !match.profile.losRetreat)
        return false;

    float const hp = bot->GetHealthPct();
    bool const isHealer = PlayerBotSpec::IsHeal(bot, true);

    // Enemy casters (and healers) that have this bot as their target: those are the ones whose sight is worth breaking.
    std::vector<Player*> casters;
    bool focusedByCasters = false;
    for (Player* enemy : match.enemies)
    {
        bool const caster = PlayerBotSpec::IsCaster(enemy, true) || PlayerBotSpec::IsHeal(enemy, true);
        if (!caster)
            continue;
        casters.push_back(enemy);
        if (enemy->GetVictim() == bot || enemy->GetTarget() == bot->GetGUID())
            focusedByCasters = true;
    }

    if (casters.empty() || !WantsCover(match.profile, int32(hp), focusedByCasters, isHealer))
        return false;

    uint32 const nowMs = getMSTime();
    {
        std::lock_guard<std::mutex> guard(gLock);
        BotMemory& memory = gBots[bot->GetGUIDLow()];
        if (memory.hasCover && getMSTimeDiff(memory.coverFoundMs, nowMs) < COVER_KEEP_MS)
        {
            x = memory.coverX;
            y = memory.coverY;
            z = memory.coverZ;
            return true;
        }
        memory.hasCover = false;
        if (getMSTimeDiff(memory.lastCoverSearchMs, nowMs) < COVER_SEARCH_GAP_MS)
            return false;
        memory.lastCoverSearchMs = nowMs;
    }

    Map* map = bot->GetMap();
    if (!map)
        return false;

    Player* partner = match.mates.empty() ? nullptr : match.mates.front();
    uint32 const phase = bot->GetPhaseMask();
    float const bx = bot->GetPositionX();
    float const by = bot->GetPositionY();
    float const bz = bot->GetPositionZ();

    std::vector<LosCandidate> candidates;
    std::vector<std::array<float, 3>> points;
    for (uint32 ring = 0; ring < LOS_RING_COUNT; ++ring)
    {
        for (uint32 index = 0; index < LOS_RING_POINTS; ++index)
        {
            float px, py;
            RingPoint(bx, by, ring, index, px, py);
            float const pz = map->GetHeight(phase, px, py, bz + CAUGHT_Z_ABOVE, true);
            if (pz <= INVALID_HEIGHT)
                continue;

            PathGenerator path(bot);
            path.CalculatePath(px, py, pz, false);
            if (uint32(path.GetPathType()) & (uint32(PATHFIND_NOPATH) | uint32(PATHFIND_INCOMPLETE)))
                continue;

            float length = 0.0f;
            Movement::PointsArray const& pts = path.GetPath();
            for (size_t i = 1; i < pts.size(); ++i)
                length += (pts[i] - pts[i - 1]).length();
            float const straight = std::sqrt((px - bx) * (px - bx) + (py - by) * (py - by));
            if (!PathAcceptable(length, straight))
                continue;

            LosCandidate c;
            c.x = px;
            c.y = py;
            c.pathLength = length;
            c.breaksEnemySight = true;
            for (Player* caster : casters)
                if (map->isInLineOfSight(px, py, pz + CAUGHT_Z_ABOVE, caster->GetPositionX(), caster->GetPositionY(),
                        caster->GetPositionZ() + CAUGHT_Z_ABOVE, phase, VMAP::ModelIgnoreFlags::Nothing))
                    c.breaksEnemySight = false;
            c.keepsPartnerSight = partner && map->isInLineOfSight(px, py, pz + CAUGHT_Z_ABOVE, partner->GetPositionX(),
                partner->GetPositionY(), partner->GetPositionZ() + CAUGHT_Z_ABOVE, phase, VMAP::ModelIgnoreFlags::Nothing);
            c.outOfMeleeReach = true;
            for (Player* enemy : match.enemies)
                if (!PlayerBotSpec::IsCaster(enemy, true) && !PlayerBotSpec::IsHeal(enemy, true) &&
                    enemy->GetDistance(px, py, pz) < MELEE_REACH)
                    c.outOfMeleeReach = false;

            // A dead end: fewer than two of three probes beyond the point (away from the bot) have ground.
            uint32 open = 0;
            float const away = std::atan2(py - by, px - bx);
            for (float turn : { -0.6f, 0.0f, 0.6f })
            {
                float const qx = px + OUTWARD_PROBE * std::cos(away + turn);
                float const qy = py + OUTWARD_PROBE * std::sin(away + turn);
                if (map->GetHeight(phase, qx, qy, pz + CAUGHT_Z_ABOVE, true) > INVALID_HEIGHT)
                    ++open;
            }
            c.deadEnd = open < OUTWARD_PROBES_NEEDED;
            // The hazards of the arenas (the Dalaran Sewers waterfall, the Ring of Valor pillars) are play v3.

            candidates.push_back(c);
            points.push_back({ px, py, pz });
        }
    }

    int const best = BestCover(candidates);
    if (best < 0)
        return false;

    x = points[size_t(best)][0];
    y = points[size_t(best)][1];
    z = points[size_t(best)][2];
    {
        std::lock_guard<std::mutex> guard(gLock);
        BotMemory& memory = gBots[bot->GetGUIDLow()];
        memory.hasCover = true;
        memory.coverFoundMs = nowMs;
        memory.coverX = x;
        memory.coverY = y;
        memory.coverZ = z;
    }
    SF_LOG_DEBUG("playerbots", "[ARENAPLAN] side=%u tier=%u kind=LOS actor=%u score=%d to=%.1f,%.1f hp=%.0f", match.side,
        uint32(match.tier), bot->GetGUIDLow(), LosScore(candidates[size_t(best)]), x, y, hp);
    return true;
}
}  // namespace ArenaPlan
