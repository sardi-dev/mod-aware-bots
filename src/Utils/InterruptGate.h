/* BT-164: shared game-side interrupt readiness and stagger gate. */
#ifndef PLAYERBOTS_INTERRUPT_GATE_H
#define PLAYERBOTS_INTERRUPT_GATE_H

#include "Group.h"
#include "Log.h"
#include "Pet.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "InterruptPolicy.h"
#include <algorithm>
#include <vector>

namespace MopDungeon
{
// How long the next ready interrupter waits behind the previous one (a kick lands within one server tick, a ranged
// interrupt within ~200 ms; 500 ms leaves room for a bot that is mid-GCD).
constexpr uint32 INTERRUPT_STAGGER_MS = InterruptPolicy::STAGGER_MS;
// A cast the gate has not looked at for this long counts as a new cast when it shows up again.
constexpr uint32 INTERRUPT_GATE_FORGET_MS = 1500;
// Members further than this from the caster are not counted as interrupters.
constexpr float INTERRUPT_COUNT_RANGE = 45.0f;
// BT-174: a bot logs at most one interrupt line per second.
constexpr uint32 INTERRUPT_LOG_GAP_MS = 1000;

// The dedicated interrupts of the 5.4.8 classes that the dungeon strategies cast by name ("kick", "pummel", ...).
// Ids and names checked in the 5.4.8 Spell.dbc on 2026-10-11. The felhunter's Spell Lock is a pet spell: see
// PetInterruptReady() below.
inline std::vector<uint32> const& InterruptSpellIds()
{
    static std::vector<uint32> const ids = {
        1766,    // Kick
        6552,    // Pummel
        57994,   // Wind Shear
        47528,   // Mind Freeze
        2139,    // Counterspell
        96231,   // Rebuke
        116705,  // Spear Hand Strike
        106839,  // Skull Bash
        80964,   // Skull Bash (bear id kept in the 5.4.8 DBC)
        80965,   // Skull Bash (cat id kept in the 5.4.8 DBC)
        147362,  // Counter Shot
        34490,   // Silencing Shot
        15487,   // Silence
        132409,  // Spell Lock (the warlock's own copy after Grimoire of Sacrifice)
    };
    return ids;
}

// The felhunter's Spell Lock (19647). AC counts it as a dedicated interrupt (MgTShared.cpp:546 "spell lock") and the
// "spell id" value resolves it from the pet's spell book (AC and fork SpellIdValue.cpp, "Pet* pet = bot->GetPet()"),
// so a warlock whose felhunter has it ready is an interrupter too.
constexpr uint32 SPELL_FELHUNTER_SPELL_LOCK = 19647;
constexpr uint32 SPELL_OBSERVER_OPTICAL_BLAST = 115781;

inline bool PetInterruptReady(Player* player, Unit* caster)
{
    Pet* pet = player ? player->GetPet() : nullptr;
    if (!pet || !pet->IsAlive() || !pet->IsInWorld() || !caster || pet->GetMap() != caster->GetMap() )
        return false;

    uint32 const spellId = pet->HasSpell(SPELL_FELHUNTER_SPELL_LOCK) ? SPELL_FELHUNTER_SPELL_LOCK :
        (pet->HasSpell(SPELL_OBSERVER_OPTICAL_BLAST) ? SPELL_OBSERVER_OPTICAL_BLAST : 0);
    SpellInfo const* info = spellId ? sSpellMgr->GetSpellInfo(spellId) : nullptr;
    // SpellHistory::HasCooldown is private in the fork (SpellHistory.h:123); IsReady is the public test (as OCActions.cpp:38).
    if (!info || !BotSpellHistory(pet) || !BotSpellHistory(pet)->IsReady(spellId))
        return false;

    return pet->GetDistance(caster) <= std::max(info->GetMaxRange(false), 5.0f) + 1.0f;
}

// Longest range of an interrupt `player` knows and has off cooldown; 0 when it has none ready.
inline float ReadyInterruptRange(Player* player)
{
    if (!player || !player->IsAlive())
        return 0.0f;

    float best = 0.0f;
    for (uint32 const id : InterruptSpellIds())
    {
        if (!player->HasSpell(id) || player->HasSpellCooldown(id))
            continue;

        float range = 5.0f;
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(id))
            range = std::max(info->GetMaxRange(false), 5.0f);

        best = std::max(best, range);
    }

    return best;
}

inline bool HasReadyInterrupt(Player* player) { return ReadyInterruptRange(player) > 0.0f; }

// BT-164: use the spell instance to distinguish consecutive casts of the same spell.
inline Spell* CurrentInterruptCast(Unit* caster)
{
    if (!caster)
        return nullptr;

    for (CurrentSpellTypes const slot : {CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL})
    {
        if (Spell* spell = caster->GetCurrentSpell(slot))
        {
            if (spell->m_spellInfo)
                return spell;
        }
    }

    return nullptr;
}

inline uint32 CurrentCastSpellId(Unit* caster)
{
    Spell* cast = CurrentInterruptCast(caster);
    return cast && cast->m_spellInfo ? cast->m_spellInfo->Id : 0;
}

// Can `member` interrupt `caster` right now: alive, on the bot's map, not channelling, an interrupt off cooldown and
// the caster within its range (+1 yd).
inline bool CanInterruptNow(Player* member, Unit* caster)
{
    if (!member || !caster || !member->IsAlive() || !member->IsInWorld() || member->GetMap() != caster->GetMap())
        return false;

    if (member->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
        return false;

    float const range = ReadyInterruptRange(member);
    if (range > 0.0f && member->GetDistance(caster) <= range + 1.0f)
        return true;

    return PetInterruptReady(member, caster);
}

// BT-164: rank ready bots by role and GUID, independent of group list order.
inline InterruptPolicy::Role InterruptRole(Player* player)
{
    if (PlayerBotSpec::IsTank(player))
        return InterruptPolicy::Role::TANK;
    return PlayerBotSpec::IsMelee(player) ? InterruptPolicy::Role::MELEE : InterruptPolicy::Role::RANGED;
}

inline uint32 InterrupterRank(Player* bot, Unit* caster)
{
    Group* group = bot->GetGroup();
    if (!group)
        return 0;

    uint32 rank = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !GET_PLAYERBOT_AI(member) ||
            member->GetExactDist(caster) > INTERRUPT_COUNT_RANGE || !CanInterruptNow(member, caster))
            continue;
        if (InterruptPolicy::Before(InterruptRole(member), member->GetGUIDLow(),
                                    InterruptRole(bot), bot->GetGUIDLow()))
            ++rank;
    }
    return rank;
}

// Bots of the group (this one included) that can interrupt `caster` right now.
inline uint32 CountInterrupters(Player* bot, Unit* caster)
{
    Group* group = bot->GetGroup();
    if (!group)
        return CanInterruptNow(bot, caster) ? 1 : 0;

    uint32 count = 0;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && GET_PLAYERBOT_AI(member) && CanInterruptNow(member, caster))
            ++count;
    }

    return count;
}

// BT-164: leave solo and PvP interrupts on their existing paths.
inline bool GroupPveInterrupt(Player* bot, Unit* target)
{
    return bot && target && bot->GetGroup() && !bot->InBattleground() && !bot->InArena() &&
           !target->GetCharmerOrOwnerPlayerOrPlayerItself();
}

// BT-164: per-bot gate (one instance per interrupt trigger): true when this bot may spend its interrupt on `caster`'s current
// cast now. The first ready interrupter fires at once, the second 500 ms later if the cast is still running, ...
class InterruptGate
{
public:
    bool Allow(Player* bot, Unit* caster)
    {
        if (!bot || !caster || !CanInterruptNow(bot, caster))
            return false;

        Spell* const cast = CurrentInterruptCast(caster);
        if (!cast)
            return false;

        uint32 const now = getMSTime();
        if (caster->GetGUID() != _caster || cast != _cast || getMSTimeDiff(_lastSeen, now) > INTERRUPT_GATE_FORGET_MS)
        {
            _caster = caster->GetGUID();
            _cast = cast;
            _firstSeen = now;
        }

        _lastSeen = now;
        uint32 const rank = InterrupterRank(bot, caster);
        bool const open = getMSTimeDiff(_firstSeen, now) >= rank * INTERRUPT_STAGGER_MS;
        // BT-174: one info line per cast this bot is let to interrupt, so a run shows whether the interrupts fired and
        // against what (Deadmines run 3 had no Rebuke or Pummel in 34 minutes and no way to tell why). The cast itself
        // is the next action; its result shows in the client's spell stream.
        if (open && _logged != cast && getMSTimeDiff(_loggedAt, now) >= INTERRUPT_LOG_GAP_MS)
        {
            _logged = cast;
            _loggedAt = now;
            SF_LOG_INFO("playerbots", "[INTERRUPT] bot=%s target=%s entry=%u cast=%u rank=%u dist=%.1f",
                        bot->GetName().c_str(), caster->GetName().c_str(), caster->GetEntry(), CurrentCastSpellId(caster),
                        rank, bot->GetDistance(caster));
        }
        return open;
    }

private:
    BotGuid _caster;
    Spell const* _cast = nullptr;
    uint32 _firstSeen = 0;
    uint32 _lastSeen = 0;
    Spell const* _logged = nullptr;
    uint32 _loggedAt = 0;
};

} // namespace MopDungeon
#endif
