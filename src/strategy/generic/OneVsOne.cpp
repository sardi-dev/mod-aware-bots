/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "OneVsOne.h"

#include "BotSkill.h"
#include "Config.h"
#include "GameObject.h"
#include "Item.h"
#include "Map.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "PvpDefense.h"
#include "PvpRotationValues.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <cmath>
#include <mutex>
#include <unordered_map>

namespace OneVsOneGlue
{
namespace
{
    using namespace OneVsOne;

    // Another enemy player this close makes the fight more than a 1v1 (the team tactics take over).
    constexpr float ENEMY_SCAN_YARDS = 30.0f;
    // The core warns at 50 yd from the duel flag and clears the warning within 40 (Player::CheckDuelDistance).
    constexpr float DUEL_SAFE_RADIUS = 40.0f;
    // Under this share of the bot's run speed the enemy counts as slowed (same rule as the old kite, BT-145).
    constexpr float SLOWED_SPEED_SHARE = 0.85f;
    // The gap must change by more than this between two looks to count as closing or opening.
    constexpr float MOVE_EPSILON = 0.3f;
    // Looks closer together than this keep the previous closing / opening reading (trigger and action share a tick).
    constexpr std::uint32_t MIN_LOOK_MS = 200;
    // A look older than this is stale: the fight paused or the bot was busy, so the timers restart.
    constexpr std::uint32_t STALE_LOOK_MS = 3000;
    // A decision line at most this often per bot unless the choice changes.
    constexpr std::uint32_t LOG_EVERY_MS = 2000;
    // Escapes leap or blink about 20 yd: the end point must stay inside the duel area.
    constexpr float ESCAPE_YARDS = 20.0f;
    // The step back looks at these turns away from the straight line (degrees), so that a bot at the edge of the duel
    // area circles instead of standing against the boundary; the endpoint has to gain at least MIN_GAIN yards on the enemy.
    constexpr int BACK_OFF_TURNS[] = { 0, 30, -30, 60, -60, 90, -90, 120, -120 };
    constexpr float BACK_OFF_MIN_GAIN = 2.0f;
    constexpr float DUEL_AREA_MARGIN = 1.5f;
    // Cover: points on rings around the bot, out of the enemy's line of sight, found at most once a second.
    constexpr float COVER_RINGS[] = { 5.0f, 8.0f, 11.0f };
    constexpr int COVER_DIRECTIONS = 16;
    constexpr std::uint32_t COVER_LOOK_EVERY_MS = 1000;
    // A line of sight check looks from eye height, as the core does for spells.
    constexpr float LOS_EYE_HEIGHT = 2.0f;
    // A gap this long in the racial triggers asking means the bot was free in between.
    constexpr std::uint32_t GATE_GAP_MS = 1500;
    // The item spell of every PvP trinket (Medallion of the Alliance and Horde, gladiator medallions).
    constexpr uint32 SPELL_PVP_TRINKET = 42292;

    struct Track
    {
        std::uint64_t enemy = 0;
        float lastDist = 0.0f;
        std::uint32_t lastMs = 0;
        std::uint32_t threatSince = 0;
        std::uint32_t contactSince = 0;
        std::uint32_t apartSince = 0;
        bool approaching = false;
        bool movingAway = false;
        std::uint32_t controlSince = 0;
        std::uint32_t speedCastMs = 0;
        std::uint32_t gateSince = 0;
        std::uint32_t gateLastMs = 0;
        std::uint32_t hurtSince = 0;
        std::uint32_t lowSince = 0;
        std::uint32_t coverSince = 0;
        std::uint32_t coverLookMs = 0;
        bool coverFound = false;
        float coverX = 0.0f, coverY = 0.0f, coverZ = 0.0f;
        Choice lastLogged = Choice::HOLD;
        std::uint32_t lastLogMs = 0;
    };

    std::mutex g_trackMutex;
    std::unordered_map<std::uint32_t, Track> g_tracks;

    bool Enabled()
    {
        static bool const enabled = sConfigMgr->GetBoolDefault("AiPlayerbot.OneVsOne.Enable", true);
        return enabled;
    }

    std::uint32_t Since(std::uint32_t since, std::uint32_t now) { return since ? getMSTimeDiff(since, now) : 0; }

    // Whether moving `yards` straight away from the enemy keeps the bot inside its duel area (always true outside duels;
    // MoveAway checks the ground and walls itself).
    bool RoomToBackOff(Player* bot, Player* enemy, float yards)
    {
        if (!bot->duel)
            return true;
        GameObject* flag = bot->GetMap()->GetGameObject(bot->GetUInt64Value(PLAYER_FIELD_DUEL_ARBITER));
        if (!flag)
            return false;
        float const angle = enemy->GetAngle(bot);
        float const x = bot->GetPositionX() + std::cos(angle) * yards;
        float const y = bot->GetPositionY() + std::sin(angle) * yards;
        return flag->GetExactDist2d(x, y) <= DUEL_SAFE_RADIUS;
    }

    // The first row of this kind the bot can use now at this distance, or false. A defensive is skipped when it does not
    // fit the enemy's kind (Evasion against a caster) or is already running.
    bool PickSpell(PlayerbotAI* botAI, Player* bot, Player* enemy, Role enemyRole, Kind kind, float dist, KitSpell& out)
    {
        for (KitSpell const& k : KitFor(bot->getClass(), kind))
        {
            if (dist < k.minRange || dist > k.maxRange || !bot->HasSpell(k.spellId) || bot->HasSpellCooldown(k.spellId))
                continue;
            if (kind == Kind::DEFENSIVE && (!DefensiveFits(k.spellId, enemyRole) || bot->HasAura(k.spellId)))
                continue;
            bool ok = true;
            switch (k.aim)
            {
                case Aim::ENEMY:
                    ok = botAI->CanCastSpell(k.spellId, enemy, true);
                    break;
                case Aim::SELF:
                case Aim::SELF_AWAY:
                case Aim::SELF_FACING:
                    ok = botAI->CanCastSpell(k.spellId, bot, true);
                    break;
                case Aim::GROUND:
                    break;  // range and cooldown were checked above; the cast itself answers the rest
            }
            if (ok)
            {
                out = k;
                return true;
            }
        }
        return false;
    }

    // An equipped PvP trinket that is ready, or nullptr.
    Item* ReadyPvpTrinket(Player* bot) { return PvpDefenseGlue::ReadyTrinket(bot); }

    // A point `yards` away from the bot that gains distance on the enemy, is walkable in a straight line and stays inside
    // the duel area, trying turns away from the straight back-off line (a bot against the boundary circles instead of
    // standing there). The best point is the one farthest from the enemy, the straight one on a tie.
    bool FindBackOffPoint(Player* bot, Player* enemy, float yards, float& outX, float& outY, float& outZ)
    {
        GameObject* flag = nullptr;
        if (bot->duel)
        {
            flag = bot->GetMap()->GetGameObject(bot->GetUInt64Value(PLAYER_FIELD_DUEL_ARBITER));
            if (!flag)
                return false;
        }
        float const away = enemy->GetAngle(bot);
        float const now = bot->GetExactDist2d(enemy);
        float bestGap = now + BACK_OFF_MIN_GAIN;
        bool found = false;
        for (int turn : BACK_OFF_TURNS)
        {
            float const angle = away + float(turn) * float(M_PI) / 180.0f;
            float x = bot->GetPositionX() + std::cos(angle) * yards;
            float y = bot->GetPositionY() + std::sin(angle) * yards;
            float z = bot->GetPositionZ();
            if (flag && flag->GetExactDist2d(x, y) > DUEL_SAFE_RADIUS - DUEL_AREA_MARGIN)
                continue;
            if (!BotCheckCollisionAndGetValidCoords(bot->GetMap(), bot, bot->GetPositionX(), bot->GetPositionY(),
                    bot->GetPositionZ(), x, y, z))
                continue;
            float const gap = enemy->GetExactDist2d(x, y);
            if (gap > bestGap + 0.01f)
            {
                bestGap = gap;
                outX = x;
                outY = y;
                outZ = z;
                found = true;
            }
        }
        return found;
    }

    // The nearest point out of the enemy's line of sight (a pillar, a wall, a tree) that the bot can walk to in a straight
    // line and that stays inside the duel area. Terrain a player sees, nothing hidden.
    bool FindCoverPoint(Player* bot, Player* enemy, float& outX, float& outY, float& outZ)
    {
        GameObject* flag = nullptr;
        if (bot->duel)
        {
            flag = bot->GetMap()->GetGameObject(bot->GetUInt64Value(PLAYER_FIELD_DUEL_ARBITER));
            if (!flag)
                return false;
        }
        Map* map = bot->GetMap();
        for (float ring : COVER_RINGS)
        {
            float bestGap = -1.0f;
            bool found = false;
            for (int i = 0; i < COVER_DIRECTIONS; ++i)
            {
                float const angle = float(i) * 2.0f * float(M_PI) / float(COVER_DIRECTIONS);
                float x = bot->GetPositionX() + std::cos(angle) * ring;
                float y = bot->GetPositionY() + std::sin(angle) * ring;
                float z = bot->GetPositionZ();
                if (flag && flag->GetExactDist2d(x, y) > DUEL_SAFE_RADIUS - DUEL_AREA_MARGIN)
                    continue;
                if (!BotCheckCollisionAndGetValidCoords(map, bot, bot->GetPositionX(), bot->GetPositionY(),
                        bot->GetPositionZ(), x, y, z))
                    continue;
                if (map->isInLineOfSight(enemy->GetPositionX(), enemy->GetPositionY(), enemy->GetPositionZ() + LOS_EYE_HEIGHT, x,
                        y, z + LOS_EYE_HEIGHT, bot->GetPhaseMask(), VMAP::ModelIgnoreFlags::Nothing))
                    continue;
                float const gap = enemy->GetExactDist2d(x, y);
                if (gap > bestGap)
                {
                    bestGap = gap;
                    outX = x;
                    outY = y;
                    outZ = z;
                    found = true;
                }
            }
            if (found)
                return true;  // the nearest ring that has cover
        }
        return false;
    }

    // Cannot walk after the bot: rooted, stunned, feared, polymorphed or asleep. A silence or a disarm does not count, the
    // enemy still runs.
    bool Held(Unit* u)
    {
        return !u->CanFreeMove() || u->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED | UNIT_STATE_ROOT) ||
               u->HasAuraType(SPELL_AURA_MOD_STUN) || u->HasAuraType(SPELL_AURA_MOD_ROOT) ||
               u->HasAuraType(SPELL_AURA_MOD_FEAR) || u->HasAuraType(SPELL_AURA_MOD_FEAR_2) ||
               u->HasAuraType(SPELL_AURA_MOD_CONFUSE);
    }

    // A hunter fights at range only with a bow, gun or crossbow in hand (MoP keeps them in the main hand); one without
    // plays the melee side (bench 2026-11-18). Paladins and monks fight in melee in every spec (RoleOfClass).
    Role RoleOf(Player* p)
    {
        bool hunterRanged = false;
        if (p->getClass() == CLASS_HUNTER)
        {
            Item* weapon = p->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            ItemTemplate const* proto = weapon ? weapon->GetTemplate() : nullptr;
            hunterRanged = proto && proto->IsRangedInventoryType();
        }
        return RoleOfClass(p->getClass(), PlayerBotSpec::IsRanged(p, true), PlayerBotSpec::IsTank(p, true), hunterRanged);
    }

    Kind KindOf(Choice c)
    {
        switch (c)
        {
            case Choice::SLOW: return Kind::SLOW;
            case Choice::ROOT: return Kind::ROOT;
            case Choice::ESCAPE: return Kind::ESCAPE;
            case Choice::GAP_CLOSER: return Kind::GAP_CLOSER;
            case Choice::SPEED: return Kind::SPEED;
            case Choice::FREEDOM: return Kind::FREEDOM;
            case Choice::DEFENSIVE: return Kind::DEFENSIVE;
            case Choice::CC_BREAK: return Kind::CC_BREAK;
            default: return Kind::SLOW;
        }
    }
}  // namespace

Player* LoneEnemy(PlayerbotAI* botAI, Player* bot)
{
    if (!Enabled() || !bot->IsAlive() || bot->InArena() || bot->IsMounted() || bot->IsInFlight())
        return nullptr;

    // Cheap test first (every combat tick of every bot asks): a started duel, or a player as the current target.
    Player* enemy = nullptr;
    bool const duel = bot->duel && bot->duel->opponent && bot->duel->startTime != 0;
    if (duel)
        enemy = bot->duel->opponent;
    else if (Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get())
        enemy = target->ToPlayer();
    // An enemy the bot cannot see (stealth, invisibility) gives it nothing to react to: a player sees no position either.
    if (!enemy || !enemy->IsAlive() || !bot->CanSeeOrDetect(enemy))
        return nullptr;

    if ((bot->GetGroup() && PlayerBotSpec::IsHeal(bot, true)) || CarryingObjectiveValue::IsCarrying(bot))
        return nullptr;

    // A started duel: the opponent, whatever else stands around (nobody else can hit a duellist).
    if (duel)
        return enemy;
    if (!bot->IsValidAttackTarget(enemy))
        return nullptr;

    // Open world and battlegrounds: exactly one enemy player near the bot, and that is the one it fights.
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* other = botAI->GetUnit(guid);
        if (other && other != enemy && other->IsAlive() && bot->GetExactDist(other) <= ENEMY_SCAN_YARDS)
            return nullptr;
    }
    return enemy;
}

bool Owns(PlayerbotAI* botAI, Player* bot)
{
    return botAI->HasStrategy("1v1", BOT_STATE_COMBAT) && LoneEnemy(botAI, bot) != nullptr;
}

void NoteSpeedCast(Player* bot)
{
    std::lock_guard<std::mutex> lock(g_trackMutex);
    g_tracks[bot->GetGUIDLow()].speedCastMs = getMSTime();
}

bool CcBreakAllowed(PlayerbotAI* botAI, Player* bot)
{
    // BT-522: in a battleground, an arena or a 1v1 the shared rules decide how to get out of control (the cheapest break,
    // the medallion by role) and the 1v1 engine or the "pvp break" action casts it; the racial triggers stand down.
    if (!PvpDefenseGlue::RacialTriggerMayFire(botAI, bot))
        return false;
    if (!Owns(botAI, bot))
        return true;
    std::uint32_t const now = getMSTime();
    BotSkillMath::Tier const tier = BotSkill::TierOf(bot);
    std::uint32_t const guid = bot->GetGUIDLow();
    std::lock_guard<std::mutex> lock(g_trackMutex);
    Track& t = g_tracks[guid];
    // The triggers ask only while the bot is held, slowed or feared; a gap in the asking restarts the clock.
    if (!t.gateSince || getMSTimeDiff(t.gateLastMs, now) > GATE_GAP_MS)
        t.gateSince = now;
    t.gateLastMs = now;
    return Since(t.gateSince, now) >= ReactionMs(tier, guid) && !Slips(tier, guid, now);
}

bool Decide(PlayerbotAI* botAI, Player* bot, Decision& out)
{
    Player* enemy = LoneEnemy(botAI, bot);
    if (!enemy)
        return false;

    Facts f;
    f.tier = BotSkill::TierOf(bot);
    f.guidLow = bot->GetGUIDLow();
    f.nowMs = getMSTime();
    f.self = RoleOf(bot);
    f.enemy = RoleOf(enemy);
    f.dist = bot->GetExactDist(enemy);
    f.meleeReach = MELEE_RANGE + bot->GetMeleeReach() + enemy->GetMeleeReach();
    f.hpPct = bot->GetHealthPct();
    f.enemySlowed = enemy->GetSpeed(MOVE_RUN) < bot->GetSpeed(MOVE_RUN) * SLOWED_SPEED_SHARE;
    f.enemySnared = enemy->HasAuraType(SPELL_AURA_MOD_DECREASE_SPEED);
    f.enemyHeld = Held(enemy);
    f.selfSlowed = bot->GetSpeedRate(MOVE_RUN) < SLOWED_SPEED_SHARE;
    f.selfFast = bot->GetSpeed(MOVE_RUN) > enemy->GetSpeed(MOVE_RUN) * SELF_FAST_SHARE;
    f.selfHeld = !bot->CanFreeMove();
    f.selfControlLost = f.selfHeld || Held(bot);
    // Auto Shot (an autorepeat spell) runs the whole fight: it does not count as casting, or a hunter never steps back
    // (live 2026-11-18: "choice=hold dist=4.0 slowed=1" over and over).
    f.selfCasting = bot->IsNonMeleeSpellCasted(false, false, true) || bot->GetCurrentSpell(CURRENT_CHANNELED_SPELL);

    TierPlay const play = PlayOf(f.tier);
    bool const caught = Caught(f);
    bool const threat = f.enemy == Role::MELEE && f.dist <= ThreatRadius(f.tier);
    bool const open = f.dist > f.meleeReach + OPEN_GAP_MARGIN;
    bool const hurt = play.coverBelowHp > 0.0f && f.hpPct <= play.coverBelowHp;
    // BT-522: the shared defensive rules say when and which; the old health line stays when they are switched off.
    bool const rules = PvpConfig::DefensiveRulesEnabled();
    PvpDefenseGlue::DefensiveVerdict defensiveVerdict;
    if (rules)
        defensiveVerdict = PvpDefenseGlue::Defensive(botAI, bot, false);
    bool const low = rules ? defensiveVerdict.use : f.hpPct <= play.defensiveBelowHp;
    f.defensivePolicy = rules;
    f.inCover = f.self == Role::RANGED && f.enemy == Role::RANGED && !bot->IsWithinLOSInMap(enemy);
    bool logIt = false;
    bool lookForCover = false;
    {
        std::lock_guard<std::mutex> lock(g_trackMutex);
        Track& t = g_tracks[f.guidLow];
        bool const stale = t.enemy != enemy->GetGUID() || !t.lastMs || getMSTimeDiff(t.lastMs, f.nowMs) > STALE_LOOK_MS;
        if (stale)
        {
            t = Track();
            t.enemy = enemy->GetGUID();
            t.lastDist = f.dist;
            t.lastMs = f.nowMs;
        }
        else if (getMSTimeDiff(t.lastMs, f.nowMs) >= MIN_LOOK_MS)
        {
            t.approaching = f.dist < t.lastDist - MOVE_EPSILON;
            t.movingAway = f.dist > t.lastDist + MOVE_EPSILON;
            t.lastDist = f.dist;
            t.lastMs = f.nowMs;
        }
        t.threatSince = threat ? (t.threatSince ? t.threatSince : f.nowMs) : 0;
        t.contactSince = caught ? (t.contactSince ? t.contactSince : f.nowMs) : 0;
        t.apartSince = open ? (t.apartSince ? t.apartSince : f.nowMs) : 0;
        t.controlSince = f.selfControlLost ? (t.controlSince ? t.controlSince : f.nowMs) : 0;
        t.hurtSince = hurt ? (t.hurtSince ? t.hurtSince : f.nowMs) : 0;
        t.lowSince = low ? (t.lowSince ? t.lowSince : f.nowMs) : 0;
        // The cover window ends after COVER_MAX_MS and the next one may start COVER_COOLDOWN_MS later.
        if (t.coverSince && Since(t.coverSince, f.nowMs) > COVER_MAX_MS + COVER_COOLDOWN_MS)
            t.coverSince = 0;
        f.enemyApproaching = t.approaching;
        f.enemyMovingAway = t.movingAway;
        f.threatForMs = Since(t.threatSince, f.nowMs);
        f.contactForMs = Since(t.contactSince, f.nowMs);
        f.apartForMs = Since(t.apartSince, f.nowMs);
        f.controlLostForMs = Since(t.controlSince, f.nowMs);
        f.hurtForMs = Since(t.hurtSince, f.nowMs);
        f.lowHpForMs = Since(t.lowSince, f.nowMs);
        f.coverForMs = Since(t.coverSince, f.nowMs);
        lookForCover = f.self == Role::RANGED && f.enemy == Role::RANGED && hurt && !f.inCover && !t.coverSince &&
                       (!t.coverLookMs || getMSTimeDiff(t.coverLookMs, f.nowMs) >= COVER_LOOK_EVERY_MS);
    }

    // The cover search costs a few line of sight rays: only a hurt ranged bot against a caster, once a second.
    if (lookForCover)
    {
        float cx = 0.0f, cy = 0.0f, cz = 0.0f;
        bool const found = FindCoverPoint(bot, enemy, cx, cy, cz);
        std::lock_guard<std::mutex> lock(g_trackMutex);
        Track& t = g_tracks[f.guidLow];
        t.coverLookMs = f.nowMs;
        t.coverFound = found;
        t.coverX = cx;
        t.coverY = cy;
        t.coverZ = cz;
    }
    {
        std::lock_guard<std::mutex> lock(g_trackMutex);
        Track const& t = g_tracks[f.guidLow];
        // The point stays valid while the bot walks to it (the window started), else for two looks.
        f.coverReady = t.coverFound && t.coverLookMs &&
                       (t.coverSince || getMSTimeDiff(t.coverLookMs, f.nowMs) < 2 * COVER_LOOK_EVERY_MS);
    }

    KitSpell slow{}, root{}, escape{}, closer{}, speed{}, freedom{}, defensive{}, breaker{};
    f.slowReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::SLOW, f.dist, slow);
    f.slowRange = f.slowReady ? slow.maxRange : 0.0f;
    if (f.self == Role::RANGED)
    {
        f.rootReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::ROOT, f.dist, root);
        f.rootRange = f.rootReady ? root.maxRange : 0.0f;
        f.escapeReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::ESCAPE, f.dist, escape) &&
                        RoomToBackOff(bot, enemy, ESCAPE_YARDS);
    }
    else
    {
        f.closerReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::GAP_CLOSER, f.dist, closer);
        f.closerMin = f.closerReady ? closer.minRange : 0.0f;
        f.closerMax = f.closerReady ? closer.maxRange : 0.0f;
        bool recentSpeed;
        {
            std::lock_guard<std::mutex> lock(g_trackMutex);
            Track const& t = g_tracks[f.guidLow];
            recentSpeed = t.speedCastMs && getMSTimeDiff(t.speedCastMs, f.nowMs) < SPEED_RECAST_MS;
        }
        f.speedReady = !recentSpeed && PickSpell(botAI, bot, enemy, f.enemy, Kind::SPEED, f.dist, speed);
        f.freedomReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::FREEDOM, f.dist, freedom);
    }
    // The two kinds below are only looked up at the moment they could matter (a bot in trouble), not every tick.
    if (rules)
    {
        if (defensiveVerdict.use)
        {
            defensive = KitSpell{ bot->getClass(), Kind::DEFENSIVE, defensiveVerdict.spellId, Aim::SELF, 0.0f, 100.0f };
            f.defensiveReady = true;
        }
    }
    else if (low)
        f.defensiveReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::DEFENSIVE, f.dist, defensive);
    Item* trinket = nullptr;
    if (f.selfControlLost)
    {
        if (rules)
        {
            // A melee bot already on the enemy has nowhere to go; every other held bot may need its legs.
            PvpDefenseGlue::BreakVerdict const breakVerdict = PvpDefenseGlue::Break(botAI, bot, !(f.self == Role::MELEE && Caught(f)));
            if (breakVerdict.use)
            {
                f.ccBreakReady = true;
                if (breakVerdict.trinket)
                    trinket = breakVerdict.item;
                else
                    breaker = KitSpell{ CLS_ANY, Kind::CC_BREAK, breakVerdict.spellId, Aim::SELF, 0.0f, 100.0f };
            }
        }
        else
        {
            f.ccBreakReady = PickSpell(botAI, bot, enemy, f.enemy, Kind::CC_BREAK, f.dist, breaker);
            if (!f.ccBreakReady && (trinket = ReadyPvpTrinket(bot)))
                f.ccBreakReady = true;
        }
    }

    float const stepBack = StepBackDistance(f.tier, f.dist);
    float stepX = 0.0f, stepY = 0.0f, stepZ = 0.0f;
    bool stepPoint = false;
    if (f.self == Role::RANGED && f.enemy == Role::MELEE && (f.enemySlowed || f.enemyHeld))
    {
        stepPoint = FindBackOffPoint(bot, enemy, stepBack, stepX, stepY, stepZ);
        f.roomToBackOff = stepPoint;
    }

    out.option = Choose(f);
    out.enemy = enemy;
    out.stepBack = stepBack;
    out.hasSpell = false;
    out.hasPoint = false;
    out.trinket = false;
    switch (out.option.choice)
    {
        case Choice::SLOW: out.spell = slow; out.hasSpell = true; break;
        case Choice::ROOT: out.spell = root; out.hasSpell = true; break;
        case Choice::ESCAPE: out.spell = escape; out.hasSpell = true; break;
        case Choice::GAP_CLOSER: out.spell = closer; out.hasSpell = true; break;
        case Choice::SPEED: out.spell = speed; out.hasSpell = true; break;
        case Choice::FREEDOM: out.spell = freedom; out.hasSpell = true; break;
        case Choice::DEFENSIVE: out.spell = defensive; out.hasSpell = true; break;
        case Choice::CC_BREAK:
            if (trinket)
            {
                out.trinket = true;
                out.trinketItem = trinket;
                out.spell = KitSpell{ 0, Kind::CC_BREAK, SPELL_PVP_TRINKET, Aim::SELF, 0.0f, 100.0f };
            }
            else
                out.spell = breaker;
            out.hasSpell = true;
            break;
        case Choice::STEP_BACK:
            out.hasPoint = stepPoint;
            out.pointX = stepX;
            out.pointY = stepY;
            out.pointZ = stepZ;
            break;
        case Choice::COVER:
        {
            std::lock_guard<std::mutex> lock(g_trackMutex);
            Track& t = g_tracks[f.guidLow];
            out.hasPoint = t.coverFound;
            out.pointX = t.coverX;
            out.pointY = t.coverY;
            out.pointZ = t.coverZ;
            if (!t.coverSince)
                t.coverSince = f.nowMs;
            break;
        }
        default: break;
    }
    if (out.hasSpell && !out.trinket && out.spell.kind != KindOf(out.option.choice))
        out.hasSpell = false;

    {
        std::lock_guard<std::mutex> lock(g_trackMutex);
        Track& t = g_tracks[f.guidLow];
        if (out.option.choice != t.lastLogged || getMSTimeDiff(t.lastLogMs, f.nowMs) >= LOG_EVERY_MS)
        {
            // In a duel a held choice is written too (every LOG_EVERY_MS), so a bench run shows why nothing happens.
            logIt = Acts(out.option.choice) || out.option.choice != t.lastLogged || bot->duel;
            t.lastLogged = out.option.choice;
            t.lastLogMs = f.nowMs;
        }
    }
    if (logIt)
        SF_LOG_DEBUG("playerbots", "[1V1] %s vs %s choice=%s why=%s spell=%u dist=%.1f tier=%u self=%s enemy=%s slowed=%d held=%d "
                     "casting=%d selfheld=%d room=%d near=%u contact=%u apart=%u ready=%c%c%c%c%c%c%c%c cover=%d hp=%.0f slip=%d",
                     bot->GetName().c_str(), enemy->GetName().c_str(), ChoiceName(out.option.choice), out.option.why,
                     out.hasSpell ? out.spell.spellId : 0u, f.dist, uint32(f.tier),
                     f.self == Role::RANGED ? "ranged" : "melee", f.enemy == Role::RANGED ? "ranged" : "melee",
                     int(f.enemySlowed), int(f.enemyHeld), int(f.selfCasting), int(f.selfHeld), int(f.roomToBackOff),
                     f.threatForMs, f.contactForMs, f.apartForMs, f.slowReady ? 'S' : '-', f.rootReady ? 'R' : '-',
                     f.escapeReady ? 'E' : '-', f.closerReady ? 'G' : '-', f.speedReady ? 'V' : '-',
                     f.freedomReady ? 'F' : '-', f.defensiveReady ? 'D' : '-', f.ccBreakReady ? 'B' : '-',
                     int(f.coverReady) + 2 * int(f.inCover), f.hpPct, int(Slips(f.tier, f.guidLow, f.nowMs)));

    switch (out.option.choice)
    {
        case Choice::STEP_BACK: return true;  // MoveAway answers when no point was found
        case Choice::COVER: return out.hasPoint;
        case Choice::COVER_HOLD: return true;
        default: return Acts(out.option.choice) && out.hasSpell;
    }
}
}  // namespace OneVsOneGlue
