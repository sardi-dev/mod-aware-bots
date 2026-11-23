/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpAffliction.h for what this is.
 */

#include "PvpAffliction.h"

#include "BotSkill.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAfflictionPolicy.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpUnitFacts.h"

#include <vector>

namespace PvpAfflictionGlue
{
namespace
{
using namespace PvpAffliction;

constexpr char const* LOG_CATEGORY = "playerbots";

bool Active(Player* bot)
{
    return PvpConfig::BurstRulesEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_WARLOCK;
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

// The enemy players within the reach of Howl of Terror around the warlock.
std::vector<Player*> EnemiesAround(PlayerbotAI* botAI, Player* bot, float radius)
{
    std::vector<Player*> out;
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (enemy && enemy->IsAlive() && bot->IsValidAttackTarget(enemy) && bot->GetExactDist(enemy) <= radius)
            out.push_back(enemy);
    }
    return out;
}

HowlInputs HowlFacts(PlayerbotAI* botAI, Player* bot)
{
    HowlInputs in;
    in.ready = Ready(bot, SPELL_HOWL_OF_TERROR);
    in.healthPercent = bot->GetHealthPct();
    for (Player* enemy : EnemiesAround(botAI, bot, AREA_RADIUS_YD))
    {
        if (PvpUnitFacts::HasBreakableCc(enemy))
            ++in.breakableHeldInRadius;
        bool const attackingBot = enemy->GetVictim() == bot || bot->getAttackers().count(enemy) > 0;
        if (attackingBot && PlayerBotSpec::IsMelee(enemy))
            ++in.meleePlayersInRadius;
    }
    return in;
}

bool HealthstoneUsable(PlayerbotAI* botAI, Player* bot)
{
    (void)botAI;
    std::vector<Item*> const items = botAI->GetAiObjectContext()->GetValue<std::vector<Item*>>("inventory items", "healthstone")->Get();
    if (items.empty() || !items.front())
        return false;
    ItemTemplate const* proto = items.front()->GetTemplate();
    if (!proto)
        return false;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        if (proto->Spells[i].SpellId && bot->HasSpellCooldown(proto->Spells[i].SpellId))
            return false;
    return true;
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    Tier const tier = BotSkill::TierOf(bot);
    switch (rule)
    {
        case Rule::HOWL:
            return HowlOfTerror(HowlFacts(botAI, bot), tier).cast;
        case Rule::FEL_FLAME:
        {
            Unit* target = CurrentTarget(botAI);
            FelFlameInputs in;
            in.known = Ready(bot, SPELL_FEL_FLAME);
            // Corruption has no cooldown of its own, so a cooldown on it is the lockout of an interrupt on the shadow school.
            in.shadowLocked = bot->HasSpell(SPELL_CORRUPTION) && bot->HasSpellCooldown(SPELL_CORRUPTION);
            in.targetIsPlayer = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target);
            return FelFlameDue(FelFlame(in, tier));
        }
        case Rule::STONE:
        {
            StoneInputs in;
            in.regenerationRunning = bot->HasAura(SPELL_DARK_REGENERATION);
            in.healthPercent = bot->GetHealthPct();
            in.stoneUsable = in.regenerationRunning && HealthstoneUsable(botAI, bot);
            return StoneDue(RegenerationStone(in, tier));
        }
    }
    return false;
}

bool HowlCast(PlayerbotAI* botAI, Player* bot)
{
    if (!Due(botAI, bot, Rule::HOWL) || !botAI->CastSpell(SPELL_HOWL_OF_TERROR, bot))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPLOCK] bot=%u tier=%u kind=HOWL", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)));
    return true;
}

bool HauntHeld(PlayerbotAI* botAI, Player* bot, Unit* target)
{
    if (!Active(bot) || !PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;
    HauntInputs in;
    in.targetIsPlayer = true;
    in.windowOpen = PvpBurstGlue::Open(botAI, bot, target);
    in.targetBreakableCc = PvpUnitFacts::HasBreakableCc(target);
    in.targetHeld = PvpUnitFacts::IsHeldStill(target);
    in.targetHealthPercent = target->GetHealthPct();
    return !HauntAllowed(Haunt(in, BotSkill::TierOf(bot)));
}
}  // namespace PvpAfflictionGlue
