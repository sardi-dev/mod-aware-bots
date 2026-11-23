/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 *
 * #793: spell-hit / aura probe for boss-mechanic evidence. Its own file, like BotScenarioDuel.cpp, so a change costs a
 * few files plus a relink. Written to be identical on the MoP fork and the AC test tree - only the core-API spelling
 * (hook signatures, script base classes, log macro) differs, and every [SPELLWATCH] line is byte-identical on both.
 *
 * Verb (no player needed; the name slot is "-"):
 *   `.botscenario - spellwatch <id,id,...> on`    adds the ids to the watch set
 *   `.botscenario - spellwatch <id,id,...> off`   removes those ids (other runs' ids stay watched)
 *   `.botscenario - spellwatch off`               clears the whole watch set
 *   `.botscenario - spellwatch list`              prints the watch set
 * The watch set is server-global. Issue the verb over SOAP / the console (World::ProcessCliCommands), as every harness
 * driver does: the first `on` registers the hook script (see "Hook registration").
 *
 * While a watched id is in the set, one line per event goes to the "playerbots" debug channel (same channel as [PROBE]):
 *   [SPELLWATCH] cast|hit|aura+|aura- spell=<id> caster=<who> target=<who> amount=<n> map=<id>  iid=<instance id> (w106: map-wide lines mixed two SSB runs)
 *   <who> = the player's name, or <entry>:<guidlow> for a creature (player:<guidlow> / - when the unit is not resolvable).
 *   cast   a successful cast finish (end of Spell::cast / AC Spell::_cast); target = the spell's explicit unit target or -;
 *          amount=0. It is logged after the spell's immediate effects, so an instant spell's hit/aura lines come first.
 *   hit    damage or heal done, amount = the value the core hands its script hook: direct spell damage after class
 *          mitigation, before absorb/resist (Unit::CalculateSpellDamageTaken); a periodic damage / leech tick before
 *          bonuses (AuraEffect::HandlePeriodic{Damage,HealthLeech}AurasTick); a heal (Unit::HealBySpell) or a periodic
 *          heal tick (AuraEffect::HandlePeriodicHealAurasTick) before absorb. A periodic heal tick fires two core hooks
 *          on both cores (the periodic tick, then "heal received"); it is logged once.
 *   aura+  an aura applied to a unit, or a stack added / refreshed on an existing one; amount = stack count.
 *   aura-  an aura application removed from a unit; amount = stack count at removal.
 *
 * Thread safety: the hooks run on the map update threads (MapUpdate.Threads > 1). The watch set is an immutable sorted
 * vector published through one std::atomic pointer (release store by the command, acquire load by the hooks). The
 * command builds a new vector under a mutex and swaps the pointer; an old vector is never freed (kept in a retired list
 * until shutdown) so a map thread still holding it can finish its binary_search. A few bytes per command.
 * Cost: with the set empty the pointer is null and every hook returns after one atomic load. Before the first `on` the
 * hook script is not even registered.
 *
 * Hook registration: the script objects are created lazily, once, by the first `spellwatch ... on` (std::call_once), the
 * same contract as EnsureDuelHarnessScript (BotScenarioDuel.cpp): the command runs on the world thread while the map
 * threads are parked (World::Update has returned from MapManager::Update, which waits for them), so the script
 * registry is not being iterated while it grows.
 *
 * Core hooks used (MoP fork, added for #793 as mirrors of AC's): UnitScript::ModifySpellDamageTaken /
 * ModifyPeriodicDamageAurasTick (SpellInfo argument added), ModifyHealReceived, OnAuraApply, OnAuraRemove,
 * OnUnitSpellCast (AC: AllSpellScript::OnSpellCast).
 *
 * PORT: SkyFire has none of those hooks. Its UnitScript offers ModifySpellDamageTaken and ModifyPeriodicDamageAurasTick
 * without a SpellInfo, and OnHeal; its PlayerScript offers OnSpellCast. So here: "hit" lines for direct damage and heals
 * name the spell the dealer is casting at that moment (the current generic, channeled or auto-repeat spell; no line when
 * it is casting none); periodic ticks are not reported (nothing says which aura ticked); "aura+" and "aura-" lines are
 * not available at all; "cast" lines cover players and bots only, and come when the cast starts (PlayerScript::OnSpellCast
 * runs in Spell::Cast). The set handling and the line format are unchanged. Moving these to core hooks that pass the
 * SpellInfo (and aura apply and remove) would restore the full probe.
 */

#include "BotScenario.h"
#include "BotScenarioInternal.h"

#include "Chat.h"
#include "Log.h"
#include "BotGuid.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Unit.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace BotScenario
{
namespace
{
    // One place for the core-specific log macro; every [SPELLWATCH] line is built as a plain string first.
    void SpellWatchLog(std::string const& line)
    {
        SF_LOG_DEBUG("playerbots", "%s", line.c_str());
    }

    // ---- the watch set ------------------------------------------------------------------------
    using WatchSet = std::vector<uint32>;                          // sorted, unique, never modified once published
    std::atomic<WatchSet const*> g_watch{nullptr};                 // null = nothing watched (the fast path)
    std::mutex g_watchMutex;                                       // writers only (the command)
    std::vector<std::unique_ptr<WatchSet const>> g_watchRetired;   // every published set, kept alive (see header)

    bool Watched(uint32 spellId)
    {
        WatchSet const* w = g_watch.load(std::memory_order_acquire);
        return w && std::binary_search(w->begin(), w->end(), spellId);
    }

    std::string SetText(WatchSet const* w)
    {
        if (!w || w->empty())
            return "-";
        std::string out;
        for (uint32 id : *w)
            out += (out.empty() ? "" : ",") + std::to_string(id);
        return out;
    }

    // ---- line formatting ----------------------------------------------------------------------
    std::string GuidText(BotGuid guid)
    {
        if (guid.IsEmpty())
            return "-";
        if ((guid.GetTypeId() == TypeID::TYPEID_PLAYER))
            return "player:" + std::to_string(guid.GetCounter());
        return std::to_string(guid.GetEntry()) + ":" + std::to_string(guid.GetCounter());
    }

    std::string UnitText(Unit const* u)
    {
        if (!u)
            return "-";
        if (u->GetTypeId() == TypeID::TYPEID_PLAYER)
            return u->GetName();
        return GuidText(u->GetGUID());
    }

    void Emit(char const* ev, uint32 spellId, std::string const& caster, std::string const& target, int64 amount,
              WorldObject const* where)
    {
        SpellWatchLog(std::string("[SPELLWATCH] ") + ev + " spell=" + std::to_string(spellId) + " caster=" + caster +
                      " target=" + target + " amount=" + std::to_string(amount) + " map=" + std::to_string(where->GetMapId()) + " iid=" + std::to_string(where->GetInstanceId()));
    }

    // ---- the shared event bodies (called by the core-specific overrides below) ------------------
    void OnDirectDamage(Unit* target, Unit* attacker, int64 damage, SpellInfo const* spellInfo)
    {
        if (!spellInfo || !target || !Watched(spellInfo->Id))
            return;
        Emit("hit", spellInfo->Id, UnitText(attacker), UnitText(target), damage, target);
    }

    // Direct heal (SkyFire's UnitScript::OnHeal): the spell is the one the healer is casting now.
    void OnHealDone(Unit* healer, Unit* target, int64 heal, SpellInfo const* spellInfo)
    {
        if (!spellInfo || !Watched(spellInfo->Id))
            return;
        Unit* where = target ? target : healer;
        if (!where)
            return;
        Emit("hit", spellInfo->Id, UnitText(healer), UnitText(target), heal, where);
    }

    // PORT: stands in for the SpellInfo argument SkyFire's damage and heal hooks lack: the spell the unit is casting.
    SpellInfo const* CurrentSpellInfo(Unit const* unit)
    {
        if (!unit)
            return nullptr;
        for (uint32 type : { uint32(CURRENT_GENERIC_SPELL), uint32(CURRENT_CHANNELED_SPELL), uint32(CURRENT_AUTOREPEAT_SPELL) })
            if (Spell* spell = unit->GetCurrentSpell(type))
                return spell->GetSpellInfo();
        return nullptr;
    }

    void OnCastFinished(Spell* spell, Unit* caster, SpellInfo const* spellInfo)
    {
        if (!spellInfo || !caster || !Watched(spellInfo->Id))
            return;
        Unit* target = spell ? spell->m_targets.GetUnitTarget() : nullptr;
        Emit("cast", spellInfo->Id, UnitText(caster), UnitText(target), 0, caster);
    }

    // ---- the hook scripts (SkyFire: a UnitScript for damage and heals, a PlayerScript for casts) ---
    // PORT: SkyFire's hooks cannot report periodic ticks or aura apply and remove; see the header comment.
    class BotScenarioSpellWatchScript : public UnitScript
    {
    public:
        BotScenarioSpellWatchScript() : UnitScript("BotScenarioSpellWatchScript") {}

        void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage) override
        {
            if (!g_watch.load(std::memory_order_acquire))
                return;
            OnDirectDamage(target, attacker, damage, CurrentSpellInfo(attacker));
        }

        void OnHeal(Unit* healer, Unit* receiver, uint32& gain) override
        {
            if (!g_watch.load(std::memory_order_acquire))
                return;
            OnHealDone(healer, receiver, gain, CurrentSpellInfo(healer));
        }
    };

    class BotScenarioSpellWatchCastScript : public PlayerScript
    {
    public:
        BotScenarioSpellWatchCastScript() : PlayerScript("BotScenarioSpellWatchCastScript") {}

        void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
        {
            if (!g_watch.load(std::memory_order_acquire) || !spell)
                return;
            OnCastFinished(spell, player, spell->GetSpellInfo());
        }
    };

    void EnsureSpellWatchScript()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            new BotScenarioSpellWatchScript();
            new BotScenarioSpellWatchCastScript();
        });
    }

    // "76207,76213" -> ids; false on an empty list or a token that is not a positive number
    bool ParseIds(std::string const& text, std::set<uint32>& ids)
    {
        std::istringstream in(text);
        std::string tok;
        while (std::getline(in, tok, ','))
        {
            if (tok.empty())
                continue;
            char* end = nullptr;
            unsigned long v = std::strtoul(tok.c_str(), &end, 10);
            if (!end || *end != '\0' || v == 0 || v > 0xFFFFFFFFul)
                return false;
            ids.insert(uint32(v));
        }
        return !ids.empty();
    }

    // under g_watchMutex: publish `next` (empty -> null)
    void Publish(std::set<uint32> const& next)
    {
        if (next.empty())
        {
            g_watch.store(nullptr, std::memory_order_release);
            return;
        }
        std::unique_ptr<WatchSet const> snap(new WatchSet(next.begin(), next.end()));
        g_watch.store(snap.get(), std::memory_order_release);
        g_watchRetired.push_back(std::move(snap));
    }
} // anonymous namespace

bool HandleSpellWatchCommand(ChatHandler* handler, std::istringstream& in)
{
    std::string a, b;
    in >> a >> b;

    if (a == "list" || a.empty())
    {
        std::string const now = SetText(g_watch.load(std::memory_order_acquire));
        handler->SendSysMessage(("botscenario: spellwatch ids=" + now).c_str());
        return true;
    }

    std::lock_guard<std::mutex> lock(g_watchMutex);
    WatchSet const* cur = g_watch.load(std::memory_order_acquire);
    std::set<uint32> next;
    if (cur)
        next.insert(cur->begin(), cur->end());

    std::string verb;
    std::set<uint32> ids;
    if (a == "off" && b.empty())
    {
        verb = "off";
        next.clear();
    }
    else if ((b == "on" || b == "off") && ParseIds(a, ids))
    {
        verb = b;
        if (b == "on")
        {
            EnsureSpellWatchScript();
            next.insert(ids.begin(), ids.end());
        }
        else
            for (uint32 id : ids)
                next.erase(id);
    }
    else
    {
        handler->SendSysMessage("botscenario: usage: botscenario - spellwatch <id,id,...> on|off | off | list");
        return false;
    }

    Publish(next);
    std::string const now = SetText(g_watch.load(std::memory_order_acquire));
    std::string reqText;
    for (uint32 id : ids)
        reqText += (reqText.empty() ? "" : ",") + std::to_string(id);
    if (reqText.empty())
        reqText = "all";
    SpellWatchLog("[SCENARIO] spellwatch " + verb + " ids=" + reqText + " watching=" + now);
    // plain-string reply: PSendSysMessage is printf-style on MoP and fmt-style on AC
    handler->SendSysMessage(("botscenario: spellwatch " + verb + " ids=" + reqText + " watching=" + now).c_str());
    return true;
}

} // namespace BotScenario
