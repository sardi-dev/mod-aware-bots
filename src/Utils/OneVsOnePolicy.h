/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ONEVSONEPOLICY_H
#define _PLAYERBOT_ONEVSONEPOLICY_H

// Movement and crowd control of a bot fighting exactly one enemy player: a duel, an open-world fight, or an isolated
// skirmish in a battleground (docs/duels/one-vs-one.md). Pure logic, free of game types, so that tests/ can run it.
//
// Ranged bots keep their distance from a melee enemy: slow or root it, then step back to range, and use an escape
// spell when it catches them. Melee bots close the gap: gap closers, slows on a fleeing or ranged enemy, speed and
// freedom spells. Every decision is a list of candidate options with a deterministic score, highest first, so a later
// decision layer (design decision 0014, 0015) can pick among the same options; the rules stay the fallback.
//
// Skill tiers (owner, 2026-11-17): expert kites and chases cleanly with good timing, good kites with slows but slips now
// and then, casual keeps some distance and often lets the melee catch up. Reaction limits per tier: expert 250 to 350 ms,
// good 350 to 500 ms, casual 600 to 900 ms. Only what a player sees: distances, movement, the debuffs on the enemy, its
// class and spec, its own cooldowns.
//
// Round 2 adds the tiered defensives and crowd control breakers (a class defensive at a health line, a racial or trinket
// when stunned or feared), and cover: a hurt ranged bot takes a point out of a caster's line of sight and waits there.

#include "BotSkillMath.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace OneVsOne
{
using Tier = BotSkillMath::Tier;

enum class Role : std::uint8_t
{
    RANGED,
    MELEE
};

enum class Choice : std::uint8_t
{
    HOLD,        // fight where it stands (the class rotation runs)
    STEP_BACK,   // ranged: move away from a slowed or held melee enemy, back toward its keep distance
    SLOW,        // cast a slow on the enemy
    ROOT,        // ranged: root, stun or fear the enemy (then step back on the next decision)
    ESCAPE,      // ranged: escape spell when caught (Disengage, Blink, Demonic Circle, ...)
    CLOSE,       // melee: walk into melee range (the rotation's reach action does it)
    GAP_CLOSER,  // melee: Charge, Shadowstep, Death Grip, Roll, Wild Charge, ...
    SPEED,       // melee: a run speed spell while the gap stays open
    FREEDOM,     // melee: remove its own slow or root while the gap is open
    DEFENSIVE,   // class defensive (Ice Block, Evasion, Die by the Sword, ...) when hurt
    CC_BREAK,    // racial or trinket that frees the bot from a stun, fear or root
    COVER,       // ranged: walk to a point out of the enemy caster's line of sight
    COVER_HOLD   // ranged: stay out of the line of sight for a few seconds (nothing else moves the bot)
};

struct Facts
{
    Tier tier = Tier::GOOD;
    std::uint32_t guidLow = 0;
    std::uint32_t nowMs = 0;

    Role self = Role::RANGED;
    Role enemy = Role::MELEE;

    float dist = 30.0f;        // yards, centre to centre
    float meleeReach = 5.0f;   // yards at which a melee swing connects between these two bodies
    float hpPct = 100.0f;

    bool enemyApproaching = false;  // the gap shrank since the last look
    bool enemyMovingAway = false;   // the gap grew since the last look while the bot stood or chased
    bool enemySlowed = false;       // its run speed is under 85 percent of the bot's
    bool enemySnared = false;       // a snare (slow debuff) sits on it, whatever its speed now: do not stack another
    bool enemyHeld = false;         // rooted, stunned, feared or incapacitated
    bool selfSlowed = false;
    bool selfFast = false;          // already well ahead of the enemy's run speed (a speed spell is running)
    bool selfHeld = false;          // rooted or stunned: no movement possible
    bool selfCasting = false;
    bool roomToBackOff = true;      // stepping back keeps the bot inside the duel area and on walkable ground

    std::uint32_t threatForMs = 0;   // how long the melee enemy has been inside ThreatRadius (ranged side)
    std::uint32_t contactForMs = 0;  // how long the enemy has been inside melee reach plus a step
    std::uint32_t apartForMs = 0;    // how long the gap has been open beyond melee reach (melee side)

    bool slowReady = false;
    float slowRange = 0.0f;
    bool rootReady = false;
    float rootRange = 0.0f;
    bool escapeReady = false;
    bool closerReady = false;
    float closerMin = 0.0f;
    float closerMax = 0.0f;
    bool speedReady = false;
    bool freedomReady = false;

    // Round 2.
    bool selfControlLost = false;          // stunned, feared, confused, rooted or asleep: it cannot act freely
    std::uint32_t controlLostForMs = 0;    // how long
    std::uint32_t hurtForMs = 0;           // how long its health has been at or under the tier's cover line
    std::uint32_t lowHpForMs = 0;          // how long its health has been at or under the tier's defensive line
    std::uint32_t coverForMs = 0;          // how long it has been taking cover (0: not taking cover)
    bool defensiveReady = false;
    bool defensivePolicy = false;          // defensiveReady already says "now" by the shared defensive rules (BT-522): no health line here
    bool ccBreakReady = false;
    bool coverReady = false;               // a walkable point out of the enemy's line of sight is near and not on cooldown
    bool inCover = false;                  // no line of sight to the enemy now
};

struct Option
{
    Choice choice;
    std::uint32_t score;
    char const* why;
};

// How a tier plays a 1v1. Distances in yards.
struct TierPlay
{
    std::uint32_t reactMinMs;
    std::uint32_t reactMaxMs;
    std::uint32_t slipPercent;    // share of decision windows in which the bot fumbles its movement
    float threatRadius;           // a melee enemy this close starts the ranged bot's reactions (and its slows)
    float keepAt;                 // the distance a ranged bot backs off to from a slowed or held melee enemy
    float escapeBelowHp;          // escape spells only at or below this health (100: whenever caught)
    float defensiveBelowHp;       // class defensives at or below this health
    float coverBelowHp;           // a ranged bot takes cover from a caster at or below this health (0: never)
    bool cancelsCasts;            // steps away from a caught melee enemy in the middle of a cast
};

inline TierPlay PlayOf(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT: return { 250, 350, 0, 25.0f, 25.0f, 100.0f, 55.0f, 65.0f, true };
        case Tier::GOOD: return { 350, 500, 20, 20.0f, 20.0f, 100.0f, 40.0f, 50.0f, true };
        case Tier::CASUAL: return { 600, 900, 50, 10.0f, 15.0f, 60.0f, 25.0f, 0.0f, false };
    }
    return { 350, 500, 20, 20.0f, 20.0f, 100.0f, 40.0f, 50.0f, true };
}

// The role a bot plays in a 1v1, for itself and for the enemy it sees. Warriors, rogues, death knights, paladins and
// monks fight in melee in every spec (a holy paladin or a mistweaver still hits from melee range, which is what a
// player standing next to one sees); a hunter with no bow, gun or crossbow in hand is a melee fighter; the rest follow
// the spec.
inline Role RoleOfClass(std::uint8_t cls, bool specRanged, bool specTank, bool hunterHasRangedWeapon)
{
    switch (cls)
    {
        case 1: case 2: case 4: case 6: case 10:  // warrior, paladin, rogue, death knight, monk
            return Role::MELEE;
        case 3:
            return hunterHasRangedWeapon ? Role::RANGED : Role::MELEE;
        default:
            return specRanged && !specTank ? Role::RANGED : Role::MELEE;
    }
}

inline float ThreatRadius(Tier tier) { return PlayOf(tier).threatRadius; }

// One reaction time per bot, fixed inside its tier's window, so the same bot always reacts alike.
inline std::uint32_t ReactionMs(Tier tier, std::uint32_t guidLow)
{
    TierPlay const p = PlayOf(tier);
    std::uint32_t const span = p.reactMaxMs - p.reactMinMs + 1;
    return p.reactMinMs + BotSkillMath::Mix(guidLow ^ 0x5D1Eu) % span;
}

// A decision window is 3 seconds: inside one window a bot either fumbles or plays clean, so a slip shows as a short
// stretch of standing in melee rather than a flicker, and the same moment replays the same way.
constexpr std::uint32_t SLIP_WINDOW_MS = 3000;

inline bool Slips(Tier tier, std::uint32_t guidLow, std::uint32_t nowMs)
{
    std::uint32_t const pct = PlayOf(tier).slipPercent;
    if (pct == 0)
        return false;
    std::uint32_t const window = nowMs / SLIP_WINDOW_MS;
    return BotSkillMath::Mix(guidLow * 31u + window * 2654435761u + 7u) % 100 < pct;
}

// Extra yards beyond melee reach that still count as "caught" (a melee player lunges the last step).
constexpr float CAUGHT_MARGIN = 1.5f;
// A melee bot treats a gap beyond its reach plus this as open (worth a gap closer or a speed spell).
constexpr float OPEN_GAP_MARGIN = 2.0f;
// The bot counts as already fast when its run speed is this much above the enemy's.
constexpr float SELF_FAST_SHARE = 1.15f;
// A speed spell is not cast again within this long of the last cast (it lasts at least this long).
constexpr std::uint32_t SPEED_RECAST_MS = 3000;
// A speed spell only after the gap has stayed open this long beyond the reaction (a gap closer comes first).
constexpr std::uint32_t SPEED_AFTER_MS = 2500;
// A ranged bot starts a step back only this far inside its keep distance (yards), so it casts between the steps.
constexpr float STEP_BACK_BAND = 7.0f;
// Shortest and longest step back, yards: shorter is not worth the stop in casting, longer leaves the fight.
constexpr float MIN_STEP_BACK = 6.0f;
constexpr float MAX_STEP_BACK = 20.0f;

// How long a ranged bot stays out of the line of sight, and how long it waits before it takes cover again.
constexpr std::uint32_t COVER_MAX_MS = 6000;
constexpr std::uint32_t COVER_COOLDOWN_MS = 15000;
// Inside this distance the enemy is on the bot and hiding is over (the rotation fights).
constexpr float COVER_HOLD_MIN_YARDS = 8.0f;
// A class defensive is worth casting against a melee enemy this far beyond its reach, or against a ranged enemy in range.
constexpr float DEFENSIVE_MELEE_MARGIN = 4.0f;
constexpr float DEFENSIVE_RANGED_YARDS = 40.0f;
// A stun breaker is only worth it with the enemy this close (it would not hit the bot from further away anyway).
constexpr float CC_BREAK_YARDS = 40.0f;

inline bool Caught(Facts const& f) { return f.dist <= f.meleeReach + CAUGHT_MARGIN; }

// Options both sides share: the breaker for a stun or fear, and the class defensive at a health line. Both wait for the
// tier's reaction time after the trigger (the bot has to notice), and a slipping window makes a good or casual bot miss it.
inline void CommonOptions(Facts const& f, std::vector<Option>& out)
{
    TierPlay const play = PlayOf(f.tier);
    std::uint32_t const react = ReactionMs(f.tier, f.guidLow);
    bool const slip = Slips(f.tier, f.guidLow, f.nowMs);

    if (f.selfControlLost && f.ccBreakReady && f.controlLostForMs >= react && !slip && f.dist <= CC_BREAK_YARDS)
        out.push_back({ Choice::CC_BREAK, 950, "cc-break" });

    float const reach = f.enemy == Role::MELEE ? f.meleeReach + CAUGHT_MARGIN + DEFENSIVE_MELEE_MARGIN : DEFENSIVE_RANGED_YARDS;
    bool const lineReached = f.defensivePolicy || f.hpPct <= play.defensiveBelowHp;
    if (f.defensiveReady && !f.selfControlLost && !f.enemyHeld && lineReached && f.lowHpForMs >= react && !slip &&
        f.dist <= reach)
        out.push_back({ Choice::DEFENSIVE, 880, "defensive" });
}

inline std::vector<Option> RangedOptions(Facts const& f)
{
    std::vector<Option> out;
    out.push_back({ Choice::HOLD, 100, f.enemy == Role::RANGED ? "ranged-enemy" : "hold" });
    TierPlay const play = PlayOf(f.tier);
    std::uint32_t const react = ReactionMs(f.tier, f.guidLow);
    bool const slip = Slips(f.tier, f.guidLow, f.nowMs);

    CommonOptions(f, out);

    // Against a caster the ranged bot hides: out of its line of sight the spells cannot be cast. Only when hurt, only for
    // a few seconds, and not for casual bots.
    if (f.enemy == Role::RANGED)
    {
        bool const hurt = play.coverBelowHp > 0.0f && f.hpPct <= play.coverBelowHp && f.hurtForMs >= react && !slip;
        if (f.coverForMs > 0 && f.coverForMs < COVER_MAX_MS && f.inCover && !f.selfHeld && f.dist > COVER_HOLD_MIN_YARDS)
            out.push_back({ Choice::COVER_HOLD, 830, "cover-hold" });
        else if (hurt && f.coverReady && !f.inCover && f.coverForMs < COVER_MAX_MS && !f.selfHeld && !f.selfCasting)
            out.push_back({ Choice::COVER, 820, "cover" });
        return out;
    }

    if (f.enemy != Role::MELEE)
        return out;

    bool const caught = Caught(f);
    bool const seenThreat = f.threatForMs >= react;
    bool const seenContact = f.contactForMs >= react;

    // Caught by a free melee enemy: the escape spell (Disengage, Blink) is the clean answer. Casual bots keep it for
    // when they are hurt, and a fumbling window lets the melee stay on them.
    if (caught && !f.enemyHeld && f.escapeReady && !f.selfHeld && seenContact && !slip && f.hpPct <= play.escapeBelowHp)
        out.push_back({ Choice::ESCAPE, 900, "escape" });

    // Root, stun or fear a free melee enemy inside the spell's reach (Frost Nova, Mortal Coil, Psychic Scream).
    if (!f.enemyHeld && f.rootReady && f.dist <= f.rootRange && (caught ? seenContact : seenThreat) &&
        (caught || f.enemyApproaching))
        out.push_back({ Choice::ROOT, 850, caught ? "root-caught" : "root-approach" });

    // A slowed or held melee enemy cannot keep up: once it is inside the keep distance less a casting band, step back
    // toward the keep distance. The band leaves time to shoot between steps. Never while held or out of the duel area.
    // A bot of the good and expert tiers walks away from a caught melee enemy in the middle of a cast (the move cancels
    // it, as a player's does); casual bots finish the cast.
    bool const mayMove = !f.selfCasting || (play.cancelsCasts && (caught || f.enemyHeld));
    if ((f.enemyHeld || f.enemySlowed) && f.dist < play.keepAt - STEP_BACK_BAND && f.roomToBackOff && !f.selfHeld &&
        mayMove && seenThreat && !slip)
        out.push_back({ Choice::STEP_BACK, 800, f.enemyHeld ? "step-back-held" : "step-back-slowed" });

    // Slow a free melee enemy that is coming in (or already on the bot). Casual bots fumble this too.
    bool const slowSlip = slip && f.tier == Tier::CASUAL;
    if (!f.enemySlowed && !f.enemySnared && !f.enemyHeld && f.slowReady && f.dist <= std::min(play.threatRadius, f.slowRange) &&
        (f.enemyApproaching || caught) && seenThreat && !slowSlip)
        out.push_back({ Choice::SLOW, 700, "slow" });

    return out;
}

inline std::vector<Option> MeleeOptions(Facts const& f)
{
    std::vector<Option> out;
    bool const open = f.dist > f.meleeReach + OPEN_GAP_MARGIN;
    out.push_back(f.dist > f.meleeReach ? Option{ Choice::CLOSE, 300, "close" } : Option{ Choice::HOLD, 100, "in-melee" });

    std::uint32_t const react = ReactionMs(f.tier, f.guidLow);
    bool const slip = Slips(f.tier, f.guidLow, f.nowMs);
    bool const seenGap = f.apartForMs >= react;
    CommonOptions(f, out);
    // Casual bots fumble their gap closers and slows in a slipping window; good and expert bots chase cleanly (their
    // slips are movement mistakes, which the melee side does not have: it just runs at the enemy).
    bool const fumble = slip && f.tier == Tier::CASUAL;

    if (open && f.closerReady && !f.selfHeld && f.dist >= f.closerMin && f.dist <= f.closerMax && seenGap && !fumble)
        out.push_back({ Choice::GAP_CLOSER, 900, "gap-closer" });

    if (open && (f.selfSlowed || f.selfHeld) && f.freedomReady && seenGap)
        out.push_back({ Choice::FREEDOM, 750, "freedom" });

    if (!f.enemySlowed && !f.enemySnared && !f.enemyHeld && f.slowReady && f.dist <= f.slowRange &&
        (f.enemy == Role::RANGED || f.enemyMovingAway) && (open ? seenGap : true) && !fumble)
        out.push_back({ Choice::SLOW, 700, f.enemy == Role::RANGED ? "slow-ranged" : "slow-runner" });

    // Never while a speed spell is already running: the cast would be repeated every tick (Burst of Speed has no cooldown)
    // and, being the engine's own action, it would keep the bot from walking (live 2026-11-18: 92 casts, no step).
    if (open && f.speedReady && !f.selfFast && !f.selfHeld && f.apartForMs >= react + SPEED_AFTER_MS)
        out.push_back({ Choice::SPEED, 600, "speed" });

    return out;
}

inline std::vector<Option> Options(Facts const& f)
{
    std::vector<Option> out = f.self == Role::RANGED ? RangedOptions(f) : MeleeOptions(f);
    std::stable_sort(out.begin(), out.end(), [](Option const& a, Option const& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        return static_cast<std::uint8_t>(a.choice) < static_cast<std::uint8_t>(b.choice);
    });
    return out;
}

inline Option Choose(Facts const& f) { return Options(f).front(); }

// Whether the choice is the bot's own move (this engine acts) or the rotation's business (HOLD, CLOSE).
inline bool Acts(Choice c) { return c != Choice::HOLD && c != Choice::CLOSE; }

// Whether the choice casts a kit spell (the glue picks the row), moves the bot, or only holds it still in cover.
inline bool CastsKitSpell(Choice c)
{
    return c == Choice::SLOW || c == Choice::ROOT || c == Choice::ESCAPE || c == Choice::GAP_CLOSER ||
           c == Choice::SPEED || c == Choice::FREEDOM || c == Choice::DEFENSIVE || c == Choice::CC_BREAK;
}

// How far a ranged bot steps back from where it stands: to its keep distance, within sensible limits.
inline float StepBackDistance(Tier tier, float dist)
{
    float const want = PlayOf(tier).keepAt - dist;
    return std::clamp(want, MIN_STEP_BACK, MAX_STEP_BACK);
}

inline char const* ChoiceName(Choice c)
{
    switch (c)
    {
        case Choice::HOLD: return "hold";
        case Choice::STEP_BACK: return "step-back";
        case Choice::SLOW: return "slow";
        case Choice::ROOT: return "root";
        case Choice::ESCAPE: return "escape";
        case Choice::CLOSE: return "close";
        case Choice::GAP_CLOSER: return "gap-closer";
        case Choice::SPEED: return "speed";
        case Choice::FREEDOM: return "freedom";
        case Choice::DEFENSIVE: return "defensive";
        case Choice::CC_BREAK: return "cc-break";
        case Choice::COVER: return "cover";
        case Choice::COVER_HOLD: return "cover-hold";
    }
    return "?";
}

// ---------------------------------------------------------------------------------------------------------------------
// The class kits: which spell answers which choice. Spell ids are checked against the 5.4.8 Spell.dbc names (comment per
// row); the game glue only uses a row the bot knows (HasSpell) whose cooldown is ready and whose cast the core accepts.
// Rows of one kind are tried in order. Ground-targeted spells (Binding Shot, Shadowfury, Heroic Leap's landing point) are
// marked; the glue casts them at the enemy's position.
// ---------------------------------------------------------------------------------------------------------------------

enum class Kind : std::uint8_t
{
    SLOW,
    ROOT,
    ESCAPE,
    GAP_CLOSER,
    SPEED,
    FREEDOM,
    DEFENSIVE,
    CC_BREAK
};

enum class Aim : std::uint8_t
{
    ENEMY,       // cast at the enemy
    SELF,        // self cast (an area around the bot, a leap, a buff)
    SELF_AWAY,   // self cast after turning the back to the enemy (Blink)
    SELF_FACING, // self cast after facing the enemy (Disengage leaps backward, Roll forward)
    GROUND       // cast at the enemy's position
};

struct KitSpell
{
    std::uint8_t cls;       // class id (1 warrior ... 11 druid)
    Kind kind;
    std::uint32_t spellId;
    Aim aim;
    float minRange;         // yards to the enemy, centre to centre
    float maxRange;
};

constexpr std::uint8_t CLS_ANY = 0;  // a row every class tries (racials)
constexpr std::uint8_t CLS_WARRIOR = 1;
constexpr std::uint8_t CLS_PALADIN = 2;
constexpr std::uint8_t CLS_HUNTER = 3;
constexpr std::uint8_t CLS_ROGUE = 4;
constexpr std::uint8_t CLS_PRIEST = 5;
constexpr std::uint8_t CLS_DEATH_KNIGHT = 6;
constexpr std::uint8_t CLS_SHAMAN = 7;
constexpr std::uint8_t CLS_MAGE = 8;
constexpr std::uint8_t CLS_WARLOCK = 9;
constexpr std::uint8_t CLS_MONK = 10;
constexpr std::uint8_t CLS_DRUID = 11;

inline std::vector<KitSpell> const& Kit()
{
    static std::vector<KitSpell> const kit = {
        // Warrior
        { CLS_WARRIOR, Kind::GAP_CLOSER, 100, Aim::ENEMY, 8.0f, 25.0f },        // Charge
        { CLS_WARRIOR, Kind::GAP_CLOSER, 6544, Aim::GROUND, 8.0f, 40.0f },      // Heroic Leap
        { CLS_WARRIOR, Kind::SLOW, 1715, Aim::ENEMY, 0.0f, 5.0f },              // Hamstring
        { CLS_WARRIOR, Kind::SLOW, 12323, Aim::SELF, 0.0f, 15.0f },             // Piercing Howl
        { CLS_WARRIOR, Kind::DEFENSIVE, 118038, Aim::SELF, 0.0f, 100.0f },      // Die by the Sword
        { CLS_WARRIOR, Kind::DEFENSIVE, 871, Aim::SELF, 0.0f, 100.0f },         // Shield Wall
        { CLS_WARRIOR, Kind::DEFENSIVE, 55694, Aim::SELF, 0.0f, 100.0f },       // Enraged Regeneration
        { CLS_WARRIOR, Kind::CC_BREAK, 18499, Aim::SELF, 0.0f, 100.0f },        // Berserker Rage
        // Paladin
        { CLS_PALADIN, Kind::FREEDOM, 1044, Aim::SELF, 0.0f, 100.0f },          // Hand of Freedom
        { CLS_PALADIN, Kind::SPEED, 85499, Aim::SELF, 0.0f, 100.0f },           // Speed of Light
        { CLS_PALADIN, Kind::DEFENSIVE, 498, Aim::SELF, 0.0f, 100.0f },         // Divine Protection
        { CLS_PALADIN, Kind::DEFENSIVE, 642, Aim::SELF, 0.0f, 100.0f },         // Divine Shield
        // Hunter
        { CLS_HUNTER, Kind::SLOW, 5116, Aim::ENEMY, 0.0f, 40.0f },              // Concussive Shot
        { CLS_HUNTER, Kind::ROOT, 19386, Aim::ENEMY, 0.0f, 35.0f },             // Wyvern Sting
        { CLS_HUNTER, Kind::ROOT, 19503, Aim::ENEMY, 0.0f, 20.0f },             // Scatter Shot
        { CLS_HUNTER, Kind::ROOT, 109248, Aim::GROUND, 0.0f, 30.0f },           // Binding Shot
        { CLS_HUNTER, Kind::ESCAPE, 781, Aim::SELF_FACING, 0.0f, 100.0f },      // Disengage
        { CLS_HUNTER, Kind::DEFENSIVE, 19263, Aim::SELF, 0.0f, 100.0f },        // Deterrence
        // Rogue
        { CLS_ROGUE, Kind::GAP_CLOSER, 36554, Aim::ENEMY, 0.0f, 25.0f },        // Shadowstep
        { CLS_ROGUE, Kind::SPEED, 108212, Aim::SELF, 0.0f, 100.0f },            // Burst of Speed
        { CLS_ROGUE, Kind::SPEED, 2983, Aim::SELF, 0.0f, 100.0f },              // Sprint
        { CLS_ROGUE, Kind::DEFENSIVE, 5277, Aim::SELF, 0.0f, 100.0f },          // Evasion
        { CLS_ROGUE, Kind::DEFENSIVE, 31224, Aim::SELF, 0.0f, 100.0f },         // Cloak of Shadows
        // Priest
        { CLS_PRIEST, Kind::ROOT, 8122, Aim::SELF, 0.0f, 8.0f },                // Psychic Scream
        { CLS_PRIEST, Kind::ROOT, 108920, Aim::SELF, 0.0f, 8.0f },              // Void Tendrils
        { CLS_PRIEST, Kind::ROOT, 64044, Aim::ENEMY, 0.0f, 30.0f },             // Psychic Horror
        { CLS_PRIEST, Kind::DEFENSIVE, 47585, Aim::SELF, 0.0f, 100.0f },        // Dispersion
        { CLS_PRIEST, Kind::DEFENSIVE, 19236, Aim::SELF, 0.0f, 100.0f },        // Desperate Prayer
        { CLS_PRIEST, Kind::DEFENSIVE, 17, Aim::SELF, 0.0f, 100.0f },           // Power Word: Shield
        // Death knight
        { CLS_DEATH_KNIGHT, Kind::GAP_CLOSER, 49576, Aim::ENEMY, 5.0f, 30.0f }, // Death Grip
        { CLS_DEATH_KNIGHT, Kind::SLOW, 45524, Aim::ENEMY, 0.0f, 30.0f },       // Chains of Ice
        { CLS_DEATH_KNIGHT, Kind::SPEED, 96268, Aim::SELF, 0.0f, 100.0f },      // Death's Advance
        { CLS_DEATH_KNIGHT, Kind::DEFENSIVE, 48792, Aim::SELF, 0.0f, 100.0f },  // Icebound Fortitude
        { CLS_DEATH_KNIGHT, Kind::DEFENSIVE, 48707, Aim::SELF, 0.0f, 100.0f },  // Anti-Magic Shell
        { CLS_DEATH_KNIGHT, Kind::DEFENSIVE, 55233, Aim::SELF, 0.0f, 100.0f },  // Vampiric Blood
        { CLS_DEATH_KNIGHT, Kind::CC_BREAK, 49039, Aim::SELF, 0.0f, 100.0f },   // Lichborne
        // Shaman
        { CLS_SHAMAN, Kind::SLOW, 8056, Aim::ENEMY, 0.0f, 25.0f },              // Frost Shock
        { CLS_SHAMAN, Kind::SLOW, 2484, Aim::SELF, 0.0f, 10.0f },               // Earthbind Totem
        { CLS_SHAMAN, Kind::ROOT, 51485, Aim::SELF, 0.0f, 10.0f },              // Earthgrab Totem
        { CLS_SHAMAN, Kind::ESCAPE, 51490, Aim::SELF, 0.0f, 10.0f },            // Thunderstorm
        { CLS_SHAMAN, Kind::DEFENSIVE, 108271, Aim::SELF, 0.0f, 100.0f },       // Astral Shift
        // Mage
        { CLS_MAGE, Kind::ROOT, 122, Aim::SELF, 0.0f, 10.0f },                  // Frost Nova
        { CLS_MAGE, Kind::ROOT, 102051, Aim::ENEMY, 0.0f, 40.0f },              // Frostjaw
        { CLS_MAGE, Kind::SLOW, 120, Aim::ENEMY, 0.0f, 10.0f },                 // Cone of Cold
        { CLS_MAGE, Kind::SLOW, 116, Aim::ENEMY, 12.0f, 40.0f },                // Frostbolt (cast time: only with room)
        { CLS_MAGE, Kind::ESCAPE, 1953, Aim::SELF_AWAY, 0.0f, 100.0f },         // Blink
        { CLS_MAGE, Kind::DEFENSIVE, 11426, Aim::SELF, 0.0f, 100.0f },          // Ice Barrier
        { CLS_MAGE, Kind::DEFENSIVE, 45438, Aim::SELF, 0.0f, 100.0f },          // Ice Block
        // Warlock
        { CLS_WARLOCK, Kind::ROOT, 6789, Aim::ENEMY, 0.0f, 30.0f },             // Mortal Coil
        { CLS_WARLOCK, Kind::ROOT, 5484, Aim::SELF, 0.0f, 10.0f },              // Howl of Terror
        { CLS_WARLOCK, Kind::ROOT, 5782, Aim::ENEMY, 0.0f, 30.0f },             // Fear
        { CLS_WARLOCK, Kind::SLOW, 18223, Aim::ENEMY, 0.0f, 40.0f },            // Curse of Exhaustion
        { CLS_WARLOCK, Kind::ESCAPE, 48020, Aim::SELF, 0.0f, 100.0f },          // Demonic Circle: Teleport
        { CLS_WARLOCK, Kind::DEFENSIVE, 104773, Aim::SELF, 0.0f, 100.0f },      // Unending Resolve
        { CLS_WARLOCK, Kind::DEFENSIVE, 108359, Aim::SELF, 0.0f, 100.0f },      // Dark Regeneration
        { CLS_WARLOCK, Kind::CC_BREAK, 108482, Aim::SELF, 0.0f, 100.0f },       // Unbound Will (ladder tier 60 pick)
        { CLS_WARLOCK, Kind::DEFENSIVE, 108416, Aim::SELF, 0.0f, 100.0f },      // Sacrificial Pact
        // Monk
        { CLS_MONK, Kind::GAP_CLOSER, 101545, Aim::SELF_FACING, 8.0f, 40.0f },  // Flying Serpent Kick
        { CLS_MONK, Kind::GAP_CLOSER, 115008, Aim::SELF_FACING, 6.0f, 20.0f },  // Chi Torpedo
        { CLS_MONK, Kind::GAP_CLOSER, 109132, Aim::SELF_FACING, 6.0f, 20.0f },  // Roll
        { CLS_MONK, Kind::GAP_CLOSER, 122057, Aim::ENEMY, 8.0f, 40.0f },        // Clash
        { CLS_MONK, Kind::SLOW, 116095, Aim::ENEMY, 0.0f, 5.0f },               // Disable
        { CLS_MONK, Kind::FREEDOM, 116841, Aim::SELF, 0.0f, 100.0f },           // Tiger's Lust
        { CLS_MONK, Kind::DEFENSIVE, 122278, Aim::SELF, 0.0f, 100.0f },         // Dampen Harm
        { CLS_MONK, Kind::DEFENSIVE, 115203, Aim::SELF, 0.0f, 100.0f },         // Fortifying Brew
        // Druid
        { CLS_DRUID, Kind::GAP_CLOSER, 102401, Aim::ENEMY, 8.0f, 25.0f },       // Wild Charge (the form picks the leap)
        { CLS_DRUID, Kind::ROOT, 339, Aim::ENEMY, 0.0f, 35.0f },                // Entangling Roots
        { CLS_DRUID, Kind::ROOT, 102359, Aim::ENEMY, 0.0f, 30.0f },             // Mass Entanglement
        { CLS_DRUID, Kind::ROOT, 5211, Aim::ENEMY, 0.0f, 5.0f },                // Mighty Bash (ladder tier 75 pick)
        { CLS_DRUID, Kind::SLOW, 102355, Aim::ENEMY, 0.0f, 35.0f },             // Faerie Swarm
        { CLS_DRUID, Kind::ESCAPE, 102280, Aim::SELF_AWAY, 0.0f, 100.0f },      // Displacer Beast
        { CLS_DRUID, Kind::SPEED, 1850, Aim::SELF, 0.0f, 100.0f },              // Dash
        { CLS_DRUID, Kind::DEFENSIVE, 22812, Aim::SELF, 0.0f, 100.0f },         // Barkskin
        { CLS_DRUID, Kind::DEFENSIVE, 61336, Aim::SELF, 0.0f, 100.0f },         // Survival Instincts
        // Racials, every class (the game glue uses a row only when the bot knows it)
        { CLS_ANY, Kind::CC_BREAK, 59752, Aim::SELF, 0.0f, 100.0f },            // Every Man for Himself
        { CLS_ANY, Kind::CC_BREAK, 7744, Aim::SELF, 0.0f, 100.0f },             // Will of the Forsaken
        { CLS_ANY, Kind::CC_BREAK, 20589, Aim::SELF, 0.0f, 100.0f },            // Escape Artist
    };
    return kit;
}

// Some defensives only answer one kind of enemy: Evasion stops melee hits, Cloak of Shadows and
// Anti-Magic Shell stop spells. A player picks by the enemy's class; so does the glue.
inline bool DefensiveFits(std::uint32_t spellId, Role enemy)
{
    switch (spellId)
    {
        case 5277:   // Evasion
            return enemy == Role::MELEE;
        case 31224:  // Cloak of Shadows
        case 48707:  // Anti-Magic Shell
            return enemy == Role::RANGED;
        default:
            return true;
    }
}

// The rows of one class and kind, in the order they are tried.
inline std::vector<KitSpell> KitFor(std::uint8_t cls, Kind kind)
{
    std::vector<KitSpell> out;
    for (KitSpell const& k : Kit())
        if ((k.cls == cls || k.cls == CLS_ANY) && k.kind == kind)
            out.push_back(k);
    return out;
}
}  // namespace OneVsOne

#endif
