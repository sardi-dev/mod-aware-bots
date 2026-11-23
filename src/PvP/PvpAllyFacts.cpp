/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpAllyFacts.h for what this is.
 */

#include "PvpAllyFacts.h"

#include "Group.h"
#include "InterruptGate.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpInterrupt.h"
#include "PvpSpellCatalog.h"
#include "PvpTeam.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "TickStamp.h"
#include "Timer.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace PvpAllyFacts
{
namespace
{
constexpr std::uint32_t TRACK_FORGET_MS = 1500;
constexpr std::size_t TRACK_MAX = 256;

struct Track
{
    Spell const* cast = nullptr;
    std::uint32_t firstSeenMs = 0;
    std::uint32_t lastSeenMs = 0;
};

std::mutex g_lock;
std::unordered_map<std::uint64_t, Track> g_tracks;   // (bot low guid << 32) | caster low guid

// What is left of the cast: the cast time less the time since the bot first looked at this cast.
uint32 CastLeftMs(Player* bot, Unit* caster, Spell* cast, int32 castMs)
{
    std::uint32_t const now = getMSTime();
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_tracks.size() > TRACK_MAX)
        g_tracks.clear();
    Track& t = g_tracks[(std::uint64_t(bot->GetGUIDLow()) << 32) | std::uint32_t(caster->GetGUID())];
    if (t.cast != cast || !t.firstSeenMs || getMSTimeDiff(t.lastSeenMs, now) > TRACK_FORGET_MS)
    {
        t.cast = cast;
        t.firstSeenMs = now;
    }
    t.lastSeenMs = now;
    std::uint32_t const since = getMSTimeDiff(t.firstSeenMs, now);
    return castMs > int32(since) ? uint32(castMs) - since : 0;
}

// BT-699: what one bot read of its group in one AI tick, per range. Thread local: a bot ticks on its map's thread, and the
// serial is unique per tick across threads, so an entry is only ever used in the tick that built it.
constexpr std::size_t FACTS_MAX = 1024;

struct Facts
{
    TickStamp stamp;
    bool fightKnown = false;
    bool fight = false;
    bool alliesBuilt = false;
    std::vector<Player*> allies;
};

thread_local std::unordered_map<std::uint64_t, Facts> t_facts;

std::uint64_t FactsKey(Player* bot, float range)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &range, sizeof(bits));
    return (std::uint64_t(bot->GetGUIDLow()) << 32) | bits;
}

Facts& FactsFor(PlayerbotAI* botAI, Player* bot, float range)
{
    if (t_facts.size() > FACTS_MAX)
        t_facts.clear();
    Facts& facts = t_facts[FactsKey(bot, range)];
    if (facts.stamp.Renew(botAI->aiTickSerial))
    {
        facts.fightKnown = false;
        facts.fight = false;
        facts.alliesBuilt = false;
        facts.allies.clear();
    }
    return facts;
}

bool GroupMemberNear(Player* bot, Player* member, float range)
{
    return member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() && bot->GetExactDist(member) <= range;
}
}  // namespace

bool PvpFightNear(PlayerbotAI* botAI, Player* bot, float range)
{
    Facts& facts = FactsFor(botAI, bot, range);
    if (facts.fightKnown)
        return facts.fight;
    facts.fightKnown = true;
    facts.fight = true;
    if (bot->InBattleground() || bot->duel)
        return true;
    bool burst = false;
    if (PlayerAttackers(bot, bot, burst) > 0)
        return true;
    if (Group* group = bot->GetGroup())
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (GroupMemberNear(bot, member, range) && PlayerAttackers(bot, member, burst) > 0)
                return true;
        }
    facts.fight = false;
    return false;
}

std::vector<Player*> const& ReachableCached(PlayerbotAI* botAI, Player* bot, float range)
{
    Facts& facts = FactsFor(botAI, bot, range);
    if (!facts.alliesBuilt)
    {
        facts.allies = Reachable(bot, range);
        facts.alliesBuilt = true;
    }
    return facts.allies;
}

std::vector<ControlCast> IncomingControl(PlayerbotAI* botAI, Player* bot, std::vector<Player*> const& allies, float watchYards)
{
    std::vector<ControlCast> out;
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* caster = unit ? unit->ToPlayer() : nullptr;
        if (!caster || !caster->IsAlive() || !bot->IsValidAttackTarget(caster) || bot->GetExactDist(caster) > watchYards)
            continue;
        Spell* cast = MopDungeon::CurrentInterruptCast(caster);
        if (!cast || !cast->GetSpellInfo())
            continue;
        PvpInterrupt::CastShape const shape = PvpInterruptGlue::ShapeOf(cast);
        if (shape.positive || !(PvpSpells::CcKindsOfMechanics(shape.mechanicMask) & PvpSpells::CC_HARD) || shape.castMs == 0)
            continue;
        Unit* aimedUnit = cast->m_targets.GetUnitTarget();
        Player* aimed = aimedUnit ? aimedUnit->ToPlayer() : nullptr;
        if (!aimed || std::find(allies.begin(), allies.end(), aimed) == allies.end())
            continue;
        out.push_back({ caster, aimed, CastLeftMs(bot, caster, cast, int32(shape.castMs)) });
    }
    return out;
}

void ForgetTracks(uint32 guidLow)
{
    for (auto it = t_facts.begin(); it != t_facts.end();)
        it = (it->first >> 32) == guidLow ? t_facts.erase(it) : std::next(it);
    std::lock_guard<std::mutex> guard(g_lock);
    for (auto it = g_tracks.begin(); it != g_tracks.end();)
        it = (it->first >> 32) == guidLow ? g_tracks.erase(it) : std::next(it);
}

std::vector<Player*> Reachable(Player* bot, float range)
{
    std::vector<Player*> out;
    out.push_back(bot);
    Group* group = bot->GetGroup();
    if (!group)
        return out;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->GetMap() == bot->GetMap() &&
            bot->GetExactDist(member) <= range && bot->IsWithinLOSInMap(member))
            out.push_back(member);
    }
    return out;
}

int32 PlayerAttackers(Player* bot, Unit* ally, bool& burstSeen)
{
    int32 count = 0;
    for (Unit* attacker : ally->getAttackers())
    {
        Player* enemy = attacker ? attacker->ToPlayer() : nullptr;
        if (!enemy || !enemy->IsAlive() || !bot->IsValidAttackTarget(enemy))
            continue;
        ++count;
        burstSeen = burstSeen || PvpTeamGlue::HasBurstAura(enemy);
    }
    return count;
}

bool InPvpFight(Player* bot, std::vector<Player*> const& allies)
{
    if (bot->InBattleground() || bot->duel)
        return true;
    bool burst = false;
    for (Player* ally : allies)
        if (PlayerAttackers(bot, ally, burst) > 0)
            return true;
    return false;
}

float LowestPercent(std::vector<Player*> const& allies)
{
    float lowest = 100.0f;
    for (Player* ally : allies)
        lowest = std::min(lowest, float(ally->GetHealthPct()));
    return lowest;
}

Player* Weakest(std::vector<Player*> const& allies)
{
    Player* weakest = nullptr;
    for (Player* ally : allies)
        if (!weakest || ally->GetHealthPct() < weakest->GetHealthPct())
            weakest = ally;
    return weakest;
}

bool Silenced(Unit* unit)
{
    return unit->HasAuraType(SPELL_AURA_MOD_SILENCE) || unit->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE);
}
}  // namespace PvpAllyFacts
