/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpAwareness.h for what this is.
 */

#include "PvpAwareness.h"

#include "BotSkill.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpConfig.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Timer.h"

#include <deque>
#include <mutex>
#include <unordered_map>

// The pure catalog numbers the mechanics of the core by hand; a change of the core's enum must stop the build.
static_assert(PvpSpells::MECH_CHARM == MECHANIC_CHARM && PvpSpells::MECH_DISORIENTED == MECHANIC_DISORIENTED &&
              PvpSpells::MECH_DISARM == MECHANIC_DISARM && PvpSpells::MECH_FEAR == MECHANIC_FEAR &&
              PvpSpells::MECH_ROOT == MECHANIC_ROOT && PvpSpells::MECH_SILENCE == MECHANIC_SILENCE &&
              PvpSpells::MECH_SLEEP == MECHANIC_SLEEP && PvpSpells::MECH_SNARE == MECHANIC_SNARE &&
              PvpSpells::MECH_STUN == MECHANIC_STUN && PvpSpells::MECH_FREEZE == MECHANIC_FREEZE &&
              PvpSpells::MECH_KNOCKOUT == MECHANIC_KNOCKOUT && PvpSpells::MECH_POLYMORPH == MECHANIC_POLYMORPH &&
              PvpSpells::MECH_BANISH == MECHANIC_BANISH && PvpSpells::MECH_SHACKLE == MECHANIC_SHACKLE &&
              PvpSpells::MECH_HORROR == MECHANIC_HORROR && PvpSpells::MECH_SAPPED == MECHANIC_SAPPED,
              "PvpSpellCatalog.h mechanic numbers differ from the core's enum Mechanics");
static_assert(PvpSpells::SCHOOL_HOLY == SPELL_SCHOOL_MASK_HOLY && PvpSpells::SCHOOL_NATURE == SPELL_SCHOOL_MASK_NATURE &&
              PvpSpells::SCHOOL_FROST == SPELL_SCHOOL_MASK_FROST && PvpSpells::SCHOOL_SHADOW == SPELL_SCHOOL_MASK_SHADOW &&
              PvpSpells::SCHOOL_FIRE == SPELL_SCHOOL_MASK_FIRE && PvpSpells::SCHOOL_ARCANE == SPELL_SCHOOL_MASK_ARCANE &&
              PvpSpells::SCHOOL_NORMAL == SPELL_SCHOOL_MASK_NORMAL,
              "PvpSpellCatalog.h school masks differ from the core's SpellSchoolMask");

namespace PvpAwareness
{
namespace
{
    // A quarter second for the effect of a cast to show (a cooldown that starts, an aura on the target) before it is read.
    constexpr uint32 CONFIRM_DELAY_MS = 250;
    // Events older than this are not read any more (the bot was busy or away): the fight they belonged to is over.
    constexpr uint32 EVENT_STALE_MS = 15000;
    // The queue keeps this many events and none older than EVENT_LIFE_MS.
    constexpr std::size_t EVENT_RING_MAX = 512;
    constexpr uint32 EVENT_LIFE_MS = 30000;
    // A book is pruned this often when it is read.
    constexpr uint32 PRUNE_EVERY_MS = 2000;
    // Books of bots that were not read for this long are swept when the registry has grown.
    constexpr uint32 BOOK_IDLE_MS = 600000;
    constexpr std::size_t BOOK_SWEEP_SIZE = 400;

    struct RawEvent
    {
        uint32 seq = 0;
        uint32 mapId = 0;
        uint32 instanceId = 0;
        uint32 atMs = 0;
        uint64 actor = 0;
        uint64 target = 0;
        uint32 spellId = 0;
        uint8 actorClass = 0;
        uint32 castSchool = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
    };

    std::mutex g_eventLock;
    std::deque<RawEvent> g_events;
    uint32 g_nextSeq = 0;

    struct Slot
    {
        Slot(uint32 guid, PvpAware::Tier tier) : book(guid, tier) {}
        PvpAware::Book book;
        uint32 cursor = 0;
        uint32 lastPruneMs = 0;
        uint32 lastUseMs = 0;
    };

    std::mutex g_registryLock;
    std::unordered_map<uint32, std::shared_ptr<Slot>> g_books;

    bool PvpContext(Player* caster, Unit* target)
    {
        if (caster->InBattleground() || caster->InArena() || caster->duel || caster->IsPvP())
            return true;
        // A kick, a stun or a polymorph on another player is a PvP event wherever it happens.
        return target && target != caster && target->GetCharmerOrOwnerPlayerOrPlayerItself() != nullptr;
    }

    // The school of the cast an interrupt is about to stop (the lock it leaves is of that school).
    uint32 SchoolOfCurrentCast(Unit* unit)
    {
        if (!unit)
            return 0;
        for (CurrentSpellTypes const type : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
            if (Spell* current = unit->GetCurrentSpell(type))
                if (SpellInfo const* info = current->GetSpellInfo())
                    return info->GetSchoolMask();
        return 0;
    }

    void Record(Player* caster, Spell* spell)
    {
        SpellInfo const* info = spell ? spell->GetSpellInfo() : nullptr;
        if (!info)
            return;
        PvpSpells::Entry const* entry = PvpSpells::Find(info->Id);
        if (!entry)
            return;

        Unit* target = spell->m_targets.GetUnitTarget();
        if (!PvpContext(caster, target))
            return;

        RawEvent ev;
        ev.mapId = caster->GetMapId();
        ev.instanceId = caster->GetInstanceId();
        ev.atMs = getMSTime();
        ev.actor = caster->GetGUID();
        ev.target = target && target != caster ? target->GetGUID() : 0;
        ev.spellId = info->Id;
        ev.actorClass = caster->getClass();
        if (entry->kind == PvpSpells::Kind::INTERRUPT || entry->kind == PvpSpells::Kind::SILENCE)
            ev.castSchool = SchoolOfCurrentCast(target);
        ev.x = caster->GetPositionX();
        ev.y = caster->GetPositionY();
        ev.z = caster->GetPositionZ();

        std::lock_guard<std::mutex> lock(g_eventLock);
        ev.seq = ++g_nextSeq;
        g_events.push_back(ev);
        while (!g_events.empty() &&
               (g_events.size() > EVENT_RING_MAX || getMSTimeDiff(g_events.front().atMs, ev.atMs) > EVENT_LIFE_MS))
            g_events.pop_front();
    }

    // The events of the bot's map instance it has not read, in order, and the newest sequence number at that moment (the
    // cursor may jump to it when every event of the map was handled: the others belong to other instances).
    std::vector<RawEvent> Unread(Player* bot, uint32 cursor, uint32& newest)
    {
        std::vector<RawEvent> out;
        uint32 const map = bot->GetMapId();
        uint32 const instance = bot->GetInstanceId();
        std::lock_guard<std::mutex> lock(g_eventLock);
        newest = g_nextSeq;
        for (RawEvent const& ev : g_events)
            if (static_cast<int32>(ev.seq - cursor) > 0 && ev.mapId == map && ev.instanceId == instance)
                out.push_back(ev);
        return out;
    }

    Unit* UnitOf(Player* bot, uint64 guid)
    {
        return guid ? ObjectAccessor::GetUnit(*bot, guid) : nullptr;
    }

    PvpAware::Sight SightOf(Player* bot, Unit* unit, float x, float y, float z)
    {
        PvpAware::Sight s;
        s.distance = bot->GetExactDist(x, y, z);
        s.lineOfSight = unit == bot || bot->IsWithinLOS(x, y, z);
        s.detectable = unit && (unit == bot || bot->CanSeeOrDetect(unit));
        return s;
    }

    bool ActorIsKey(Player* bot, Unit* unit)
    {
        if (!unit)
            return false;
        if (bot->GetVictim() == unit || bot->GetTarget() == unit->GetGUID())
            return true;
        Player* player = unit->ToPlayer();
        return player && PlayerBotSpec::IsHeal(player, true);
    }

    // The enemy player with a cast stopper that stands nearest to the bot: the one a kick-aware player watches.
    bool IsNearestKicker(Player* bot, Unit* actor)
    {
        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
            return false;
        GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
        Unit* best = nullptr;
        float bestDist = 0.0f;
        for (BotGuid const guid : near)
        {
            Unit* other = botAI->GetUnit(guid);
            Player* player = other ? other->ToPlayer() : nullptr;
            if (!player || !player->IsAlive() || !PvpAware::Book::HasStopper(player->getClass()))
                continue;
            float const dist = bot->GetExactDist(other);
            if (!best || dist < bestDist)
            {
                best = other;
                bestDist = dist;
            }
        }
        return best == actor;
    }

    // What the bot would see of the cast's result: a cooldown that started, or an aura on the target. False when the cast
    // did not go off. `durationMs` is the whole length of a crowd control effect from the moment of the cast.
    bool Confirm(Unit* actor, Unit* target, PvpSpells::Entry const& entry, uint32 atMs, uint32 nowMs, uint32& durationMs)
    {
        durationMs = 0;
        switch (entry.kind)
        {
            case PvpSpells::Kind::TRINKET:
                return true;  // the medallion's own cooldown is the item's: not readable, booked as an attempt
            case PvpSpells::Kind::CROWD_CONTROL:
            {
                if (!target)
                    return false;
                Aura* aura = target->GetAura(entry.spellId, actor->GetGUID());
                if (!aura || aura->GetDuration() <= 0)
                    return false;
                durationMs = static_cast<uint32>(aura->GetDuration()) + getMSTimeDiff(atMs, nowMs);
                return true;
            }
            default:
            {
                if (entry.cooldownMs == 0)
                    return true;
                Player* player = actor->ToPlayer();
                return player && player->HasSpellCooldown(entry.spellId);
            }
        }
    }

    void Drain(Player* bot, Slot& slot, uint32 nowMs)
    {
        uint32 newest = slot.cursor;
        std::vector<RawEvent> const fresh = Unread(bot, slot.cursor, newest);
        uint32 processed = slot.cursor;
        bool waiting = false;
        for (RawEvent const& raw : fresh)
        {
            uint32 const age = getMSTimeDiff(raw.atMs, nowMs);
            PvpSpells::Entry const* entry = PvpSpells::Find(raw.spellId);
            // The effect of a cast needs a moment to show; the later events wait behind it.
            if (entry && entry->kind != PvpSpells::Kind::TRINKET && age < CONFIRM_DELAY_MS)
            {
                waiting = true;
                break;
            }
            processed = raw.seq;
            if (!entry || age > EVENT_STALE_MS)
                continue;

            Unit* actor = UnitOf(bot, raw.actor);
            if (!actor)
                continue;
            Unit* target = UnitOf(bot, raw.target);
            bool const own = actor == bot;
            bool const crowdControl = entry->kind == PvpSpells::Kind::CROWD_CONTROL;
            // Cooldowns, trinkets and stoppers are tracked on enemies; a crowd control counts whoever cast it and whoever
            // it landed on (the bot sees the effect on the frame of an ally as well).
            if (!crowdControl && !own && !bot->IsHostileTo(actor))
                continue;
            if (crowdControl && !target)
                continue;

            uint32 durationMs = 0;
            if (!Confirm(actor, target, *entry, raw.atMs, nowMs, durationMs))
                continue;

            PvpAware::Event ev;
            ev.seq = raw.seq;
            ev.atMs = raw.atMs;
            ev.actor = raw.actor;
            ev.target = raw.target;
            ev.spellId = raw.spellId;
            ev.actorClass = raw.actorClass;
            ev.castSchool = raw.castSchool;
            ev.x = raw.x;
            ev.y = raw.y;
            ev.z = raw.z;

            PvpAware::Interest interest;
            interest.ownCast = own;
            if (crowdControl)
            {
                interest.actorKey = ActorIsKey(bot, target);
                PvpAware::Sight const sight = SightOf(bot, target, target->GetPositionX(), target->GetPositionY(),
                                                      target->GetPositionZ());
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(raw.spellId);
                PvpSpells::DrCategory const category = PvpSpells::DrCategoryFor(
                    raw.spellId, spellInfo ? spellInfo->GetAllEffectsMechanicMask() : 0);
                slot.book.ObserveCcLanded(ev, category, durationMs, sight, interest, nowMs);
            }
            else
            {
                interest.actorKey = ActorIsKey(bot, actor);
                if (entry->kind == PvpSpells::Kind::INTERRUPT || entry->kind == PvpSpells::Kind::SILENCE)
                    interest.actorNearestKicker = IsNearestKicker(bot, actor);
                slot.book.ObserveCast(ev, SightOf(bot, actor, raw.x, raw.y, raw.z), interest, nowMs);
            }
        }
        // Every event of this map was handled: the cursor jumps over the ones of other instances too.
        slot.cursor = waiting ? processed : newest;
    }

    std::shared_ptr<Slot> SlotFor(uint32 guidLow, PvpAware::Tier tier, uint32 nowMs)
    {
        std::lock_guard<std::mutex> lock(g_registryLock);
        auto it = g_books.find(guidLow);
        if (it != g_books.end() && it->second->book.TierOfBook() == tier)
        {
            it->second->lastUseMs = nowMs;
            return it->second;
        }
        if (g_books.size() >= BOOK_SWEEP_SIZE)
            for (auto sweep = g_books.begin(); sweep != g_books.end();)
                sweep = getMSTimeDiff(sweep->second->lastUseMs, nowMs) > BOOK_IDLE_MS ? g_books.erase(sweep) : std::next(sweep);
        auto slot = std::make_shared<Slot>(guidLow, tier);
        slot->lastUseMs = nowMs;
        g_books[guidLow] = slot;
        return slot;
    }
}  // namespace

std::shared_ptr<PvpAware::Book> ViewOf(Player* bot)
{
    if (!PvpConfig::AwarenessEnabled() || !bot || !bot->IsInWorld() || !GET_PLAYERBOT_AI(bot))
        return nullptr;

    uint32 const now = getMSTime();
    std::shared_ptr<Slot> slot = SlotFor(bot->GetGUIDLow(), BotSkill::TierOf(bot), now);
    Drain(bot, *slot, now);
    if (getMSTimeDiff(slot->lastPruneMs, now) >= PRUNE_EVERY_MS)
    {
        slot->book.Prune(now);
        slot->lastPruneMs = now;
    }
    return std::shared_ptr<PvpAware::Book>(slot, &slot->book);
}

void Forget(uint32 guidLow)
{
    std::lock_guard<std::mutex> lock(g_registryLock);
    g_books.erase(guidLow);
}

std::size_t QueuedEvents()
{
    std::lock_guard<std::mutex> lock(g_eventLock);
    return g_events.size();
}

std::size_t BookCount()
{
    std::lock_guard<std::mutex> lock(g_registryLock);
    return g_books.size();
}
}  // namespace PvpAwareness

namespace
{
    class PvpAwarenessScript : public PlayerScript
    {
    public:
        PvpAwarenessScript() : PlayerScript("PvpAwarenessScript") {}

        // The core calls this when the cast completes, before it is checked: a record of an attempt (PvpAwareness.h).
        void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
        {
            if (!player || !spell || !PvpConfig::AwarenessEnabled())
                return;
            PvpAwareness::Record(player, spell);
        }
    };
}  // namespace

void AddSC_pvp_awareness()
{
    new PvpAwarenessScript();
}
