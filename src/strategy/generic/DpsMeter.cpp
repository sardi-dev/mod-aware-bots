/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "BotSkill.h"
#include "DpsMeterPolicy.h"
#include "DpsSheetPolicy.h"
#include "Playerbots.h"

// The [DPSFIGHT] line: one per fight of a damage dealer in a group, written when the bot leaves combat. The damage comes
// from the core's damage hook (BotDamageWatch.cpp), which sees the hit before absorbs and overkill are taken off, so a
// number is not the combat log's, but two specialisations of the same level on the same pack compare fairly. The
// cooldowns are the spells with at least a minute of recovery the bot cast, how often, and the fight weight is the
// heaviest picture seen when one was cast (attackers on the group, elites among them, a boss). The proof run reads the
// lines with tools/dps_by_spec.py.

namespace
{
char const* TierName(BotSkillMath::Tier tier)
{
    switch (tier)
    {
        case BotSkillMath::Tier::EXPERT: return "expert";
        case BotSkillMath::Tier::GOOD: return "good";
        default: return "casual";
    }
}

// Only a damage dealer in a group is measured: a random bot's fights on its own way to a quest would fill the log with
// lines nobody reads, and tanks and healers have no damage line to compare. Battlegrounds and arenas are left out: their
// fights are against players and a match holds hundreds of them.
bool IsMeasured(Player* bot)
{
    return bot->GetGroup() && !bot->InBattleground() && DpsSheetPolicy::IsDamageSpec(BotGetSpecialization(bot));
}
}  // namespace

void PlayerbotAI::BeginDpsFight()
{
    if (!IsMeasured(bot))
        return;
    DpsMeterPolicy::Begin(dpsFight, getMSTime(), damageDealtTotal.load(std::memory_order_relaxed),
                          petDamageTotal.load(std::memory_order_relaxed));
}

void PlayerbotAI::NoteDpsCast(uint32 spellId, SpellInfo const* spellInfo)
{
    if (!dpsFight.active || !spellInfo)
        return;
    if (!DpsMeterPolicy::NoteCast(dpsFight, spellId, spellInfo->GetRecoveryTime()))
        return;

    // The first cast of a cooldown in this fight: read the fight's weight now, while its attackers are alive. One read per
    // cooldown per fight, never per global cooldown.
    unsigned attackers = 0;
    unsigned elites = 0;
    bool boss = false;
    for (BotGuid const guid : _aiObjectContext->GetValue<GuidVector>("attackers")->Get())
    {
        Unit* unit = GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;
        ++attackers;
        if (Creature* creature = unit->ToCreature())
        {
            if (creature->isElite())
                ++elites;
            CreatureTemplate const* info = creature->GetCreatureTemplate();
            boss = boss || creature->IsDungeonBoss() || (info && info->rank == CREATURE_ELITE_WORLDBOSS);
        }
    }
    DpsMeterPolicy::NoteSituation(dpsFight, attackers, elites, boss);
}

void PlayerbotAI::EndDpsFight()
{
    if (!dpsFight.active)
        return;

    // Copy first: the fight record is reset by End, and its cooldown list is read below.
    DpsMeterPolicy::Fight const fight = dpsFight;
    DpsMeterPolicy::Summary const summary = DpsMeterPolicy::End(dpsFight, getMSTime(),
        damageDealtTotal.load(std::memory_order_relaxed), petDamageTotal.load(std::memory_order_relaxed));
    if (!summary.write)
        return;

    std::string cooldowns;
    for (std::size_t i = 0; i < fight.cooldownKinds; ++i)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(fight.cooldowns[i].spell);
        if (!cooldowns.empty())
            cooldowns += ",";
        cooldowns += (info && info->SpellName ? info->SpellName : std::to_string(fight.cooldowns[i].spell)) + "=" +
                     std::to_string(fight.cooldowns[i].casts);
    }

    Group const* group = bot->GetGroup();
    SF_LOG_INFO("playerbots",
                "[DPSFIGHT] %s class=%u spec=%u level=%u tier=%s secs=%u damage=%llu dps=%u pet=%u casts=%u attackers=%u "
                "elites=%u boss=%u group=%u map=%u cooldowns=%s",
                bot->GetName().c_str(), unsigned(bot->getClass()), unsigned(BotGetSpecialization(bot)),
                unsigned(bot->getLevel()), TierName(BotSkill::TierOf(bot)), summary.seconds,
                static_cast<unsigned long long>(summary.damage), summary.dps, summary.petDps, unsigned(fight.casts),
                unsigned(fight.attackers), unsigned(fight.elites), fight.boss ? 1u : 0u,
                group ? unsigned(group->GetMembersCount()) : 0u, bot->GetMapId(), cooldowns.empty() ? "-" : cooldowns.c_str());
}
