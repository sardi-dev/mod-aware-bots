/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCScripts.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "Creature.h"
#include "Group.h"
#include "ICCTriggers.h"
#include "Player.h"
#include "Playerbots.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "RaidBotHelpers.h"
#include "Timer.h"

// AC Ai/Raid/ICC/ICCScripts.cpp. 5.4.8: AC fills the shared encounter state from spell and creature script hooks
// (AllSpellScript OnSpellCast/OnSpellPrepare, AllCreatureScript, AllMapScript) that this core does not have. The bots
// read the same facts from what a player sees instead (IcecrownHelpers::ObserveEncounter, run by the "icc watch"
// trigger): the Lich King's Defile cast and its target, Putricide's goo casts and the goo and vile gas stalkers the
// 5.4.8 scripts summon at the landing spots.

namespace
{
    RaidBotHelpers::InstanceStateStore<IcecrownHelpers::IccInstanceState> g_state;

    // Retention of a goo impact: the longest consumer window (Festergut avoid: 8 s) plus slack, as in AC
    uint32 constexpr GOO_RETENTION_MS = 9000;
    float constexpr SAME_SPOT_YARDS = 1.0f;

    void AddGooImpact(IcecrownHelpers::IccInstanceState& st, Position const& pos, uint32 now)
    {
        auto& impacts = st.malleableGoo;
        impacts.erase(std::remove_if(impacts.begin(), impacts.end(),
                                     [now](IcecrownHelpers::MalleableGooImpact const& i)
                                     { return getMSTimeDiff(i.castTime, now) > GOO_RETENTION_MS; }),
                      impacts.end());

        for (auto const& impact : impacts)
            if (impact.position.GetExactDist2d(pos.GetPositionX(), pos.GetPositionY()) < SAME_SPOT_YARDS)
                return;

        IcecrownHelpers::MalleableGooImpact impact;
        impact.position = pos;
        impact.castTime = now;
        impacts.push_back(impact);
    }

    Unit* CastTarget(Unit* caster, uint32 spellId)
    {
        if (!caster || !caster->HasUnitState(UNIT_STATE_CASTING))
            return nullptr;

        Spell* spell = caster->FindCurrentSpellBySpellId(spellId);
        return spell ? spell->m_targets.GetUnitTarget() : nullptr;
    }
}

namespace IcecrownHelpers
{
    IccInstanceState& IccState(uint32 instanceId)
    {
        return g_state.Get(instanceId);
    }

    void IccResetInstance(uint32 instanceId)
    {
        g_state.Reset(instanceId);
    }

    std::vector<Position> ActiveGooPositions(uint32 instanceId, uint32 lifetimeMs)
    {
        std::vector<Position> out;
        IccInstanceState& st = IccState(instanceId);

        uint32 const now = getMSTime();
        for (auto const& impact : st.malleableGoo)
            if (getMSTimeDiff(impact.castTime, now) <= lifetimeMs)
                out.push_back(impact.position);

        return out;
    }

    void ObserveEncounter(Player* bot)
    {
        if (bot->GetMapId() != ICC_MAP_ID)
            return;

        uint32 const instanceId = bot->GetInstanceId();
        uint32 const now = getMSTime();

        // AC IccBossStateResetScript: forget the encounter state once nobody of the group has fought for the grace time
        bool groupInCombat = bot->IsInCombat();
        if (Group* group = bot->GetGroup())
            for (GroupReference* ref = group->GetFirstMember(); ref && !groupInCombat; ref = ref->next())
                if (Player* member = ref->GetSource())
                    groupInCombat = member->IsInCombat();

        IccInstanceState& st = IccState(instanceId);
        if (groupInCombat)
            st.lastBossCombatMs = now;
        else
        {
            if (st.lastBossCombatMs != 0 && getMSTimeDiff(st.lastBossCombatMs, now) > ICC_RESET_GRACE_MS)
                IccResetInstance(instanceId);
            return;
        }

        float constexpr SEARCH_RANGE = 150.0f;

        // AC IccLichKingListenerScript: Defile has a 2 s cast, and its target shows while he casts
        if (Creature* lichKing = bot->FindNearestCreature(NPC_THE_LICH_KING, SEARCH_RANGE))
        {
            Unit* target = CastTarget(lichKing, DEFILE_CAST_ID);
            if (target && target->GetTypeId() == TYPEID_PLAYER &&
                (st.defileCast.targetGuid != target->GetGUID() || getMSTimeDiff(st.defileCast.castTime, now) > 3000))
            {
                st.defileCast.targetGuid = target->GetGUID();
                st.defileCast.castTime = now;
            }
        }

        // AC IccPutricideListenerScript: the goo lands where its target stood when it was cast
        if (Creature* putricide = bot->FindNearestCreature(NPC_PROFESSOR_PUTRICIDE, SEARCH_RANGE))
        {
            for (uint32 spellId : {SPELL_MALLEABLE_GOO_10N, SPELL_MALLEABLE_GOO_25N, SPELL_MALLEABLE_GOO_10H,
                                   SPELL_MALLEABLE_GOO_25H, SPELL_MALLEABLE_GOO_BALCONY})
            {
                Unit* target = CastTarget(putricide, spellId);
                if (target && target->GetTypeId() == TYPEID_PLAYER)
                    AddGooImpact(st, target->GetPosition(), now);
            }
        }

        // The 5.4.8 scripts also summon a Malleable Ooze Stalker where a goo lands (boss_professor_putricide.cpp)
        std::list<Creature*> stalkers;
        bot->GetCreatureListWithEntryInGrid(stalkers, NPC_MALLEABLE_OOZE_STALKER, SEARCH_RANGE);
        for (Creature* stalker : stalkers)
            AddGooImpact(st, stalker->GetPosition(), now);

        // AC IccRotfaceListenerScript, for the 5.4.8 Rotface script: the heroic Vile Gas is aimed at a Vile Gas Stalker
        // summoned on its victim (boss_rotface.cpp), so the victim is the member who stands on the newest stalker
        std::list<Creature*> gasStalkers;
        bot->GetCreatureListWithEntryInGrid(gasStalkers, NPC_VILE_GAS_STALKER, SEARCH_RANGE);
        for (Creature* gas : gasStalkers)
        {
            if (getMSTimeDiff(st.rotfaceVileGas.castTime, now) < 8000)
                break;

            Player* victim = nullptr;
            float best = 5.0f;
            if (Group* group = bot->GetGroup())
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* member = ref->GetSource())
                        if (member->IsAlive() && member->GetExactDist2d(gas->GetPositionX(), gas->GetPositionY()) < best)
                        {
                            best = member->GetExactDist2d(gas->GetPositionX(), gas->GetPositionY());
                            victim = member;
                        }

            if (victim)
            {
                st.rotfaceVileGas.victimGuid = victim->GetGUID();
                st.rotfaceVileGas.castTime = now;
            }
        }
    }
}
