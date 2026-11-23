/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPAWARENESSPOLICY_H
#define _PLAYERBOT_PVPAWARENESSPOLICY_H

// BT-520: the per-bot view of enemy state (design decision 0032 point 5, fundamentals sheet topic 14). A skilled player watches the
// enemy trinket, the big defensives, the interrupts and the diminishing returns; this book is what ONE bot has seen of
// them, never the core's hidden state:
//
//   - an observation is a visible event (a cast, a crowd control that landed) that the bot saw within its sight range, in
//     line of sight, of a unit it could detect; the glue (PvP/PvpAwareness.cpp) fills the sight facts, this file decides;
//   - the bot's tier decides what it notices (one stable roll per event, never a fresh one per tick), which enemies it
//     tracks, how long it remembers, and how well it plans around diminishing returns;
//   - what was not seen stays UNKNOWN. A consumer that wants "assume ready" (good and casual bots, the sheet's AW2) asks for
//     it explicitly through AssumeReady();
//   - a cooldown is a window, not a point: a talent or glyph may shorten it, so the reading is DOWN for the first 85 percent
//     of the catalog cooldown, MAYBE up to 100 percent, READY after.
//
// Teammates do not share books: team intel travels only through visible call-outs (owner, 2026-11-18). A bot sees a teammate's
// crowd control land (it is in its own sight) and books it itself, which is what a player's addon does.
//
// Diminishing returns are planned with the 5.4.8 categories and an 18 s reset after the last effect of the category ends
// (the maintainer's default, 2026-11-18): the core resets after 15 s, the retail range is 15 to 20 s.
//
// Free of game types, so tests/ can run it (tests/test_pvp_awareness.cpp).

#include "BotSkillMath.h"
#include "PvpSpellCatalog.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace PvpAware
{
using Tier = BotSkillMath::Tier;
using PvpSpells::DrCategory;
using PvpSpells::Kind;

// The record limits: a battleground side sees at most 15 enemies; the book keeps the most recently touched ones.
constexpr std::size_t MAX_ACTORS = 24;
constexpr std::size_t MAX_COOLDOWNS_PER_ACTOR = 12;
constexpr std::size_t MAX_LOCKS = 24;
constexpr std::size_t MAX_DR = 96;

// The window of a cooldown in which a talent or glyph may already have ended it: DOWN before this share, MAYBE after.
constexpr std::uint32_t COOLDOWN_SURE_PERCENT = 85;

// The diminishing returns reset the bots plan with (the core resets 15 s after the last effect ended).
constexpr std::uint32_t DR_RESET_MS = 18000;
// Steps: full, half, quarter, immune. The fourth effect of a category inside the reset does not land.
constexpr std::uint32_t DR_IMMUNE_STEP = 3;

inline std::uint32_t Elapsed(std::uint32_t nowMs, std::uint32_t thenMs) { return nowMs - thenMs; }

// ---------------------------------------------------------------------------------------------------------------------
// What a tier notices (fundamentals sheet 14.2). The percentages are proposals to tune with the benches, not sourced facts.
// ---------------------------------------------------------------------------------------------------------------------
enum class Scope : std::uint8_t
{
    ALL,   // every enemy it sees
    KEY,   // the enemy healer, its focus target, or the nearest kicker
    NONE   // not tracked: the bot assumes the item is ready
};

struct TierModel
{
    float sightYards;            // a cast farther than this is not noticed
    std::uint32_t trinketPct;    // the trinket and the racials that break control
    Scope trinketScope;
    std::uint32_t defensivePct;  // big defensives and externals
    Scope defensiveScope;
    bool immunityOnly;           // only immunities count (the good tier)
    std::uint32_t kickPct;       // interrupts and silences
    Scope kickScope;
    std::uint32_t ccPct;         // a crowd control that landed (diminishing returns)
    bool ownCcOnly;              // only the bot's own crowd control and the one on its focus target
    std::uint32_t memoryMs;      // how long a record lives (0: the whole match)
    std::uint32_t kickMemoryMs;  // how long a seen kick counts (0: its whole cooldown)
    std::uint32_t lastSeenMs;    // how long an enemy's last position counts after it went out of sight (stealth)
    std::uint32_t drIgnorePct;   // the share of planning decisions in which the bot disregards a diminishing return
    std::uint32_t mistakePct;    // a mistake on purpose (a late defensive, a kick on filler, a trinket at full health)
};

inline TierModel ModelOf(Tier tier)
{
    switch (tier)
    {
        case Tier::EXPERT:
            return { 70.0f, 98, Scope::ALL, 98, Scope::ALL, false, 98, Scope::ALL, 98, false, 0, 0, 8000, 0, 5 };
        case Tier::GOOD:
            return { 50.0f, 90, Scope::KEY, 85, Scope::KEY, true, 80, Scope::KEY, 90, true, 60000, 5000, 5000, 0, 15 };
        case Tier::CASUAL:
            return { 35.0f, 0, Scope::NONE, 50, Scope::ALL, false, 0, Scope::NONE, 60, true, 15000, 0, 2500, 50, 30 };
    }
    return ModelOf(Tier::GOOD);
}

// A stable roll for one event and one observer: the same event always gives the same answer, so a notice is decided once.
inline std::uint32_t Roll(std::uint32_t observerGuid, std::uint32_t key, std::uint32_t salt)
{
    return BotSkillMath::Mix(observerGuid * 31u + BotSkillMath::Mix(key ^ (salt * 2246822519u))) % 100;
}

// One mistake on purpose per key (a go, a cast, a control effect), at the tier's rate.
inline bool MakesMistake(Tier tier, std::uint32_t observerGuid, std::uint32_t key)
{
    return Roll(observerGuid, key, 0x4D15u) < ModelOf(tier).mistakePct;
}

// ---------------------------------------------------------------------------------------------------------------------
// Events and what the observer could see of them.
// ---------------------------------------------------------------------------------------------------------------------
struct Event
{
    std::uint32_t seq = 0;       // unique per event, the key of the notice roll
    std::uint32_t atMs = 0;
    std::uint64_t actor = 0;     // the caster, a raw GUID
    std::uint64_t target = 0;    // the unit it was aimed at (0: none or itself)
    std::uint32_t spellId = 0;
    std::uint8_t actorClass = 0;
    std::uint32_t castSchool = 0;  // an interrupt: the school of the cast it stopped (0: it stopped nothing)
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Sight
{
    float distance = 0.0f;
    bool lineOfSight = false;
    bool detectable = false;  // the actor is not stealthed or invisible to the observer
};

// Facts about the actor that widen or narrow what a tier tracks.
struct Interest
{
    bool actorKey = false;            // the enemy healer, or the observer's focus target
    bool actorNearestKicker = false;  // the enemy with a stopper nearest to the observer
    bool ownCast = false;             // the observer cast it
};

inline bool CanSee(TierModel const& model, Sight const& sight)
{
    return sight.detectable && sight.lineOfSight && sight.distance <= model.sightYards;
}

inline bool ScopeCovers(Scope scope, bool key)
{
    return scope == Scope::ALL || (scope == Scope::KEY && key);
}

// The chance (percent) that the tier notices a cast of this kind from this actor; 0 when it does not track it at all.
inline std::uint32_t NoticePercent(TierModel const& model, Kind kind, Interest const& interest)
{
    switch (kind)
    {
        case Kind::TRINKET:
        case Kind::BREAKER:
            return ScopeCovers(model.trinketScope, interest.actorKey) ? model.trinketPct : 0;
        case Kind::DEFENSIVE_TEMPO:
        case Kind::EXTERNAL:
            return model.immunityOnly ? 0 : (ScopeCovers(model.defensiveScope, interest.actorKey) ? model.defensivePct : 0);
        case Kind::DEFENSIVE_IMMUNITY:
            return ScopeCovers(model.defensiveScope, interest.actorKey) ? model.defensivePct : 0;
        case Kind::INTERRUPT:
        case Kind::SILENCE:
            return ScopeCovers(model.kickScope, interest.actorNearestKicker || interest.actorKey) ? model.kickPct : 0;
        case Kind::CROWD_CONTROL:
            return (!model.ownCcOnly || interest.ownCast || interest.actorKey) ? model.ccPct : 0;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Readings.
// ---------------------------------------------------------------------------------------------------------------------
enum class State : std::uint8_t
{
    UNKNOWN,  // never seen (or forgotten): the book does not know
    READY,    // seen used, and its cooldown is over
    MAYBE,    // seen used; inside the last share of the cooldown, a talent may have ended it
    DOWN      // seen used, still on cooldown
};

struct Reading
{
    State state = State::UNKNOWN;
    std::uint32_t remainingMs = 0;  // until it is surely ready (0 unless DOWN or MAYBE)
};

// Good and casual bots treat an unknown item as ready (sheet AW2); so does anything not seen down.
inline bool AssumeReady(Reading const& r) { return r.state != State::DOWN; }
// The strict reading: seen ready, or seen used long enough ago.
inline bool KnownReady(Reading const& r) { return r.state == State::READY; }

inline Reading CooldownReading(std::uint32_t usedMs, std::uint32_t cooldownMs, std::uint32_t nowMs)
{
    std::uint32_t const elapsed = Elapsed(nowMs, usedMs);
    if (elapsed >= cooldownMs)
        return { State::READY, 0 };
    std::uint32_t const sure = static_cast<std::uint32_t>(std::uint64_t(cooldownMs) * COOLDOWN_SURE_PERCENT / 100);
    return { elapsed >= sure ? State::MAYBE : State::DOWN, cooldownMs - elapsed };
}

// ---------------------------------------------------------------------------------------------------------------------
// Diminishing returns.
// ---------------------------------------------------------------------------------------------------------------------
struct DrRecord
{
    std::uint64_t target = 0;
    DrCategory category = DrCategory::NONE;
    std::uint8_t count = 0;        // effects inside the current reset window
    std::uint32_t lastEndMs = 0;   // when the last effect ends (or ended)
    std::uint32_t touchedMs = 0;
};

// The step a new effect of this category would land at: 0 full, 1 half, 2 quarter, 3 immune.
inline std::uint32_t DrStepOf(DrRecord const& r, std::uint32_t nowMs)
{
    if (r.count == 0)
        return 0;
    // Still running, or inside the reset after it ended: signed difference so a long idle record does not wrap.
    bool const reset = static_cast<std::int32_t>(nowMs - r.lastEndMs) >= static_cast<std::int32_t>(DR_RESET_MS);
    return reset ? 0 : std::min<std::uint32_t>(r.count, DR_IMMUNE_STEP);
}

// Percent of the base duration at a step (the PvP cap of 8 s on long crowd control is applied by the caller).
inline std::uint32_t DrPercent(std::uint32_t step)
{
    switch (step)
    {
        case 0: return 100;
        case 1: return 50;
        case 2: return 25;
        default: return 0;
    }
}

// Milliseconds until the category is back to full for this target (0 when it already is).
inline std::uint32_t DrResetsInMs(DrRecord const& r, std::uint32_t nowMs)
{
    if (DrStepOf(r, nowMs) == 0)
        return 0;
    std::int32_t const left = static_cast<std::int32_t>(r.lastEndMs + DR_RESET_MS - nowMs);
    return left > 0 ? static_cast<std::uint32_t>(left) : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// The book of one observer.
// ---------------------------------------------------------------------------------------------------------------------
struct CooldownRecord
{
    std::uint32_t spellId = 0;
    std::uint32_t usedMs = 0;
    std::uint32_t cooldownMs = 0;
};

struct ActorRecord
{
    std::uint64_t guid = 0;
    std::uint8_t cls = 0;
    std::uint32_t trinketUsedMs = 0;  // 0: not seen
    std::vector<CooldownRecord> cooldowns;
    float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
    std::uint32_t lastSeenMs = 0;  // 0: never seen
    std::uint32_t touchedMs = 0;
};

struct LockRecord
{
    std::uint64_t target = 0;
    std::uint32_t schoolMask = 0;
    std::uint32_t untilMs = 0;
};

struct Position
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    std::uint32_t ageMs = 0;
};

class Book
{
public:
    Book(std::uint32_t observerGuid, Tier tier) : m_guid(observerGuid), m_tier(tier), m_model(ModelOf(tier)) {}

    std::uint32_t ObserverGuid() const { return m_guid; }
    Tier TierOfBook() const { return m_tier; }
    TierModel const& Model() const { return m_model; }

    // --- ingestion ---------------------------------------------------------------------------------------------------

    // A cast of a catalog spell that the glue saw (and, for an interrupt, saw land). Returns true when the bot noticed it.
    bool ObserveCast(Event const& e, Sight const& sight, Interest const& interest, std::uint32_t nowMs)
    {
        PvpSpells::Entry const* entry = PvpSpells::Find(e.spellId);
        if (!entry || !CanSee(m_model, sight))
            return false;

        std::uint32_t const pct = NoticePercent(m_model, entry->kind, interest);
        if (pct == 0 || Roll(m_guid, e.seq, 1) >= pct)
            return false;

        ActorRecord& actor = ActorFor(e.actor, e.actorClass, nowMs);
        NotePosition(actor, e, nowMs);

        switch (entry->kind)
        {
            case Kind::TRINKET:
                actor.trinketUsedMs = Stamp(e.atMs);
                break;
            case Kind::BREAKER:
                // The racial that shares the medallion's cooldown is the medallion for planning; the others keep their own.
                if (PvpSpells::SharesTrinketCooldown(e.spellId))
                    actor.trinketUsedMs = Stamp(e.atMs);
                else
                    NoteCooldown(actor, e.spellId, e.atMs, entry->cooldownMs);
                break;
            case Kind::INTERRUPT:
            case Kind::SILENCE:
                NoteCooldown(actor, e.spellId, e.atMs, entry->cooldownMs);
                NoteStop(*entry, e);
                break;
            default:
                NoteCooldown(actor, e.spellId, e.atMs, entry->cooldownMs);
                break;
        }
        return true;
    }

    // A crowd control that was seen to land on `target` with this duration (the glue read the aura). Returns true when noticed.
    bool ObserveCcLanded(Event const& e, DrCategory category, std::uint32_t durationMs, Sight const& sight,
                         Interest const& interest, std::uint32_t nowMs)
    {
        if (category == DrCategory::NONE || !CanSee(m_model, sight))
            return false;

        std::uint32_t const pct = NoticePercent(m_model, Kind::CROWD_CONTROL, interest);
        if (pct == 0 || Roll(m_guid, e.seq, 2) >= pct)
            return false;

        DrRecord& dr = DrFor(e.target, category, nowMs);
        std::uint32_t const at = e.atMs;
        // A new effect after the reset starts a fresh count; one while another still runs is the next step.
        if (DrStepOf(dr, at) == 0 && dr.count > 0)
            dr.count = 0;
        if (dr.count < 255)
            ++dr.count;
        dr.lastEndMs = Stamp(at + durationMs);
        dr.touchedMs = Stamp(nowMs);
        return true;
    }

    // The effect ended before its time (broken by damage, dispelled, trinketed): the reset clock starts now.
    void NoteCcEnded(std::uint64_t target, DrCategory category, std::uint32_t nowMs)
    {
        for (DrRecord& dr : m_dr)
            if (dr.target == target && dr.category == category && static_cast<std::int32_t>(dr.lastEndMs - nowMs) > 0)
                dr.lastEndMs = Stamp(nowMs);
    }

    // The enemy was in sight at this position (a stealthed enemy that is not detectable is never noted).
    void NoteSeen(std::uint64_t guid, std::uint8_t cls, float x, float y, float z, std::uint32_t nowMs)
    {
        ActorRecord& actor = ActorFor(guid, cls, nowMs);
        actor.lastX = x;
        actor.lastY = y;
        actor.lastZ = z;
        actor.lastSeenMs = Stamp(nowMs);
    }

    // --- readings ----------------------------------------------------------------------------------------------------

    Reading TrinketReading(std::uint64_t guid, std::uint32_t nowMs) const
    {
        ActorRecord const* actor = Find(guid);
        if (!actor || actor->trinketUsedMs == 0 || Forgotten(actor->trinketUsedMs, nowMs))
            return {};
        return CooldownReading(actor->trinketUsedMs, PvpSpells::TRINKET_COOLDOWN_MS, nowMs);
    }

    Reading CooldownOf(std::uint64_t guid, std::uint32_t spellId, std::uint32_t nowMs) const
    {
        ActorRecord const* actor = Find(guid);
        if (!actor)
            return {};
        for (CooldownRecord const& c : actor->cooldowns)
            if (c.spellId == spellId && !Forgotten(c.usedMs, nowMs))
                return CooldownReading(c.usedMs, c.cooldownMs, nowMs);
        return {};
    }

    // Can this enemy stop a cast now? UNKNOWN when it was not seen to use every stopper its class is sure to have;
    // DOWN when all the stoppers it is known to have were seen used. A class with no stopper is READY never: ask HasStopper.
    Reading KickReading(std::uint64_t guid, std::uint8_t cls, std::uint32_t nowMs) const
    {
        ActorRecord const* actor = Find(guid);
        if (!actor)
            return {};
        bool any = false;
        bool unrecorded = false;
        Reading best{ State::DOWN, ~0u };
        for (PvpSpells::Entry const& row : PvpSpells::InterruptsOf(cls))
        {
            Reading r{};
            for (CooldownRecord const& c : actor->cooldowns)
                if (c.spellId == row.spellId && !KickForgotten(c.usedMs, nowMs))
                {
                    r = CooldownReading(c.usedMs, c.cooldownMs, nowMs);
                    break;
                }
            if (r.state == State::UNKNOWN)
            {
                if (row.cost == 0)
                    unrecorded = true;
                continue;  // a secondary stopper never seen is not counted
            }
            any = true;
            if (r.state == State::READY || (r.state == State::MAYBE && best.state == State::DOWN))
                best = r;
            else if (r.state == State::DOWN && best.state == State::DOWN && r.remainingMs < best.remainingMs)
                best = r;
        }
        if (!any || unrecorded)
            return {};
        return best;
    }

    // The enemy's class has some way to stop a cast (whether or not it was seen yet).
    static bool HasStopper(std::uint8_t cls) { return !PvpSpells::InterruptsOf(cls).empty(); }

    // Is this unit's school locked now (a kick the bot or its group saw land)?
    bool SchoolLocked(std::uint64_t target, std::uint32_t schoolMask, std::uint32_t nowMs) const
    {
        for (LockRecord const& l : m_locks)
            if (l.target == target && (l.schoolMask & schoolMask) && static_cast<std::int32_t>(l.untilMs - nowMs) > 0)
                return true;
        return false;
    }

    // The step the next effect of the category would land at on this target, as THIS bot would plan it: 0 full ... 3 immune.
    // A casual bot disregards a diminishing return in half of its decisions (the roll is fixed per target and category).
    std::uint32_t DrStep(std::uint64_t target, DrCategory category, std::uint32_t nowMs) const
    {
        DrRecord const* dr = FindDr(target, category);
        if (!dr)
            return 0;
        std::uint32_t const step = DrStepOf(*dr, nowMs);
        if (step == 0 || m_model.drIgnorePct == 0)
            return step;
        std::uint32_t const key = static_cast<std::uint32_t>(target) ^ (static_cast<std::uint32_t>(category) << 24) ^ (dr->count << 16);
        return Roll(m_guid, key, 3) < m_model.drIgnorePct ? 0 : step;
    }

    // Exact step (what the book holds, whatever the tier would plan): for logs and tests.
    std::uint32_t DrStepExact(std::uint64_t target, DrCategory category, std::uint32_t nowMs) const
    {
        DrRecord const* dr = FindDr(target, category);
        return dr ? DrStepOf(*dr, nowMs) : 0;
    }

    std::uint32_t DrResetsIn(std::uint64_t target, DrCategory category, std::uint32_t nowMs) const
    {
        DrRecord const* dr = FindDr(target, category);
        return dr ? DrResetsInMs(*dr, nowMs) : 0;
    }

    // The enemy's last known position, when it is still worth walking to (a stealth player gone from sight expires).
    bool LastSeen(std::uint64_t guid, std::uint32_t nowMs, Position& out) const
    {
        ActorRecord const* actor = Find(guid);
        if (!actor || actor->lastSeenMs == 0)
            return false;
        std::uint32_t const age = Elapsed(nowMs, actor->lastSeenMs);
        if (age > m_model.lastSeenMs)
            return false;
        out = { actor->lastX, actor->lastY, actor->lastZ, age };
        return true;
    }

    // An enemy of a class that holds a crowd control the bot has not been diminished against: a follow-up that could land
    // on `ourGuid` after an effect of `exceptCategory` ended, and whose cooldown was not seen running (sheet TR6).
    bool FollowUpControlPossible(std::uint64_t enemy, std::uint8_t enemyClass, std::uint64_t ourGuid, DrCategory exceptCategory,
                                 std::uint32_t nowMs) const
    {
        for (PvpSpells::Entry const& row : PvpSpells::RowsFor(enemyClass, Kind::CROWD_CONTROL))
        {
            if (row.dr == exceptCategory || row.dr == DrCategory::SILENCE || row.dr == DrCategory::DISARM ||
                row.dr == DrCategory::ROOT)
                continue;
            DrRecord const* dr = FindDr(ourGuid, row.dr);
            if (dr && DrStepOf(*dr, nowMs) >= DR_IMMUNE_STEP)
                continue;
            if (row.cooldownMs != 0 && !AssumeReady(CooldownOf(enemy, row.spellId, nowMs)))
                continue;
            return true;
        }
        return false;
    }

    // --- upkeep ------------------------------------------------------------------------------------------------------

    // Drops what the tier forgot or what no longer matters. Cheap; the glue calls it when the book is read.
    void Prune(std::uint32_t nowMs)
    {
        for (ActorRecord& a : m_actors)
        {
            a.cooldowns.erase(std::remove_if(a.cooldowns.begin(), a.cooldowns.end(), [&](CooldownRecord const& c)
                { return Elapsed(nowMs, c.usedMs) > c.cooldownMs || Forgotten(c.usedMs, nowMs); }), a.cooldowns.end());
            if (a.trinketUsedMs && (Elapsed(nowMs, a.trinketUsedMs) > PvpSpells::TRINKET_COOLDOWN_MS || Forgotten(a.trinketUsedMs, nowMs)))
                a.trinketUsedMs = 0;
            if (a.lastSeenMs && Elapsed(nowMs, a.lastSeenMs) > std::max<std::uint32_t>(m_model.lastSeenMs, 1))
                a.lastSeenMs = 0;
        }
        m_actors.erase(std::remove_if(m_actors.begin(), m_actors.end(), [&](ActorRecord const& a)
            { return a.cooldowns.empty() && a.trinketUsedMs == 0 && a.lastSeenMs == 0 && Elapsed(nowMs, a.touchedMs) > ACTOR_IDLE_MS; }),
            m_actors.end());
        m_locks.erase(std::remove_if(m_locks.begin(), m_locks.end(), [&](LockRecord const& l)
            { return static_cast<std::int32_t>(l.untilMs - nowMs) <= 0; }), m_locks.end());
        m_dr.erase(std::remove_if(m_dr.begin(), m_dr.end(), [&](DrRecord const& d)
            { return DrStepOf(d, nowMs) == 0 && Elapsed(nowMs, d.touchedMs) > DR_RESET_MS; }), m_dr.end());
    }

    std::size_t ActorCount() const { return m_actors.size(); }
    std::size_t DrCount() const { return m_dr.size(); }
    std::size_t LockCount() const { return m_locks.size(); }

private:
    // A record untouched for this long and empty is dropped.
    static constexpr std::uint32_t ACTOR_IDLE_MS = 60000;

    static std::uint32_t Stamp(std::uint32_t ms) { return ms ? ms : 1; }

    bool Forgotten(std::uint32_t usedMs, std::uint32_t nowMs) const
    {
        return m_model.memoryMs != 0 && Elapsed(nowMs, usedMs) > m_model.memoryMs;
    }

    bool KickForgotten(std::uint32_t usedMs, std::uint32_t nowMs) const
    {
        if (Forgotten(usedMs, nowMs))
            return true;
        return m_model.kickMemoryMs != 0 && Elapsed(nowMs, usedMs) > m_model.kickMemoryMs;
    }

    ActorRecord const* Find(std::uint64_t guid) const
    {
        for (ActorRecord const& a : m_actors)
            if (a.guid == guid)
                return &a;
        return nullptr;
    }

    ActorRecord& ActorFor(std::uint64_t guid, std::uint8_t cls, std::uint32_t nowMs)
    {
        for (ActorRecord& a : m_actors)
            if (a.guid == guid)
            {
                if (cls)
                    a.cls = cls;
                a.touchedMs = Stamp(nowMs);
                return a;
            }
        if (m_actors.size() >= MAX_ACTORS)
        {
            auto oldest = std::min_element(m_actors.begin(), m_actors.end(), [&](ActorRecord const& l, ActorRecord const& r)
                { return Elapsed(nowMs, l.touchedMs) > Elapsed(nowMs, r.touchedMs); });
            m_actors.erase(oldest);
        }
        ActorRecord fresh;
        fresh.guid = guid;
        fresh.cls = cls;
        fresh.touchedMs = Stamp(nowMs);
        m_actors.push_back(std::move(fresh));
        return m_actors.back();
    }

    static void NotePosition(ActorRecord& actor, Event const& e, std::uint32_t nowMs)
    {
        actor.lastX = e.x;
        actor.lastY = e.y;
        actor.lastZ = e.z;
        actor.lastSeenMs = Stamp(nowMs);
    }

    static void NoteCooldown(ActorRecord& actor, std::uint32_t spellId, std::uint32_t atMs, std::uint32_t cooldownMs)
    {
        if (cooldownMs == 0)
            return;
        for (CooldownRecord& c : actor.cooldowns)
            if (c.spellId == spellId)
            {
                c.usedMs = Stamp(atMs);
                c.cooldownMs = cooldownMs;
                return;
            }
        if (actor.cooldowns.size() >= MAX_COOLDOWNS_PER_ACTOR)
            actor.cooldowns.erase(actor.cooldowns.begin());
        actor.cooldowns.push_back({ spellId, Stamp(atMs), cooldownMs });
    }

    // The school closed by a kick that landed on a cast, for the lockout of the spell. A silence closes every school.
    void NoteStop(PvpSpells::Entry const& entry, Event const& e)
    {
        if (e.target == 0 || entry.lockoutMs == 0)
            return;
        std::uint32_t mask = e.castSchool;
        if (entry.kind == Kind::SILENCE)
            mask = 0x7F;
        if (mask == 0)
            return;
        if (m_locks.size() >= MAX_LOCKS)
            m_locks.erase(m_locks.begin());
        m_locks.push_back({ e.target, mask, Stamp(e.atMs + entry.lockoutMs) });
    }

    DrRecord const* FindDr(std::uint64_t target, DrCategory category) const
    {
        for (DrRecord const& d : m_dr)
            if (d.target == target && d.category == category)
                return &d;
        return nullptr;
    }

    DrRecord& DrFor(std::uint64_t target, DrCategory category, std::uint32_t nowMs)
    {
        for (DrRecord& d : m_dr)
            if (d.target == target && d.category == category)
                return d;
        if (m_dr.size() >= MAX_DR)
        {
            auto oldest = std::min_element(m_dr.begin(), m_dr.end(), [&](DrRecord const& l, DrRecord const& r)
                { return Elapsed(nowMs, l.touchedMs) > Elapsed(nowMs, r.touchedMs); });
            m_dr.erase(oldest);
        }
        DrRecord fresh;
        fresh.target = target;
        fresh.category = category;
        fresh.touchedMs = Stamp(nowMs);
        m_dr.push_back(fresh);
        return m_dr.back();
    }

    std::uint32_t m_guid;
    Tier m_tier;
    TierModel m_model;
    std::vector<ActorRecord> m_actors;
    std::vector<LockRecord> m_locks;
    std::vector<DrRecord> m_dr;
};
}  // namespace PvpAware

#endif
