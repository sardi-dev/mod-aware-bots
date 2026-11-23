/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpShadowPriest.h for what this is.
 */

#include "PvpShadowPriest.h"

#include "Action.h"
#include "BotSkill.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpAllyFacts.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpShadowPriestPolicy.h"

#include <string>
#include <vector>

namespace PvpShadowPriestGlue
{
namespace
{
using namespace PvpShadowPriest;

constexpr char const* LOG_CATEGORY = "playerbots";
constexpr float REACH_YD = 40.0f;
// The glyph of Mass Dispel exists under two spell ids in the data (the 3.x and the 5.x row); either one on the priest counts.
constexpr uint32 SPELL_GLYPH_OF_MASS_DISPEL_5X = 57190;

bool Active(Player* bot)
{
    return PvpConfig::TeamPlayEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_PRIEST;
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

struct Choice
{
    Player* ally = nullptr;
    std::int32_t score = 0;
};

Choice BestLeap(Player* bot, std::vector<Player*> const& allies)
{
    Choice best;
    if (!Ready(bot, SPELL_LEAP_OF_FAITH))
        return best;
    for (Player* ally : allies)
    {
        bool burst = false;
        LeapInputs in;
        in.ready = true;
        in.allyIsSelf = ally == bot;
        in.allyHeld = ally->HasUnitState(UNIT_STATE_STUNNED | UNIT_STATE_ROOT);
        in.playerAttackers = PvpAllyFacts::PlayerAttackers(bot, ally, burst);
        in.allyHealthPercent = ally->GetHealthPct();
        in.allyDistance = bot->GetExactDist(ally);
        LeapVerdict const verdict = LeapOfFaith(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.ally || verdict.score > best.score))
            best = { ally, verdict.score };
    }
    return best;
}

bool TargetImmune(Unit* target)
{
    return target->HasAura(SPELL_ICE_BLOCK) || target->HasAura(SPELL_DIVINE_SHIELD) || target->HasAura(SPELL_HAND_OF_PROTECTION);
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    // BT-699: no line-of-sight work unless a fight against players is on, and the group read once per tick.
    if (!PvpAllyFacts::PvpFightNear(botAI, bot, REACH_YD))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, REACH_YD);
    if (!PvpAllyFacts::InPvpFight(bot, allies))
        return false;

    switch (rule)
    {
        case Rule::LEAP:
            return BestLeap(bot, allies).ally != nullptr;
        case Rule::DISPEL:
        {
            Unit* target = CurrentTarget(botAI);
            if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
                return false;
            DispelInputs in;
            in.ready = Ready(bot, SPELL_MASS_DISPEL);
            in.glyphed = bot->HasAura(SPELL_GLYPH_OF_MASS_DISPEL) || bot->HasAura(SPELL_GLYPH_OF_MASS_DISPEL_5X);
            in.targetImmune = TargetImmune(target);
            in.silenced = PvpAllyFacts::Silenced(bot);
            in.targetDistance = bot->GetExactDist(target);
            return MassDispel(in, BotSkill::TierOf(bot)) == DispelReason::BREAK_IMMUNITY;
        }
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, REACH_YD);
    Unit* aimed = nullptr;
    uint32 spell = 0;
    char const* what = "";
    switch (rule)
    {
        case Rule::LEAP:
            what = "LEAP_OF_FAITH";
            spell = SPELL_LEAP_OF_FAITH;
            aimed = BestLeap(bot, allies).ally;
            break;
        case Rule::DISPEL:
            what = "MASS_DISPEL";
            spell = SPELL_MASS_DISPEL;
            aimed = CurrentTarget(botAI);
            break;
    }
    if (!spell || !aimed || !botAI->CastSpell(spell, aimed))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPPRIEST] bot=%u tier=%u kind=%s target=%u", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what,
        aimed->GetGUIDLow());
    return true;
}

bool Holds(PlayerbotAI* botAI, Player* bot, Action* action)
{
    if (!action || !Active(bot) || action->getName() != "vampiric embrace")
        return false;
    if (!PvpAllyFacts::PvpFightNear(botAI, bot, REACH_YD))
        return false;
    std::vector<Player*> const& allies = PvpAllyFacts::ReachableCached(botAI, bot, REACH_YD);
    EmbraceInputs in;
    in.pvpFight = PvpAllyFacts::InPvpFight(bot, allies);
    if (!in.pvpFight)
        return false;
    for (Player* ally : allies)
        if (ally->GetHealthPct() < EMBRACE_HURT_PCT)
            ++in.alliesHurt;
    Unit* target = CurrentTarget(botAI);
    in.windowOpen = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target) && PvpBurstGlue::Open(botAI, bot, target);
    in.selfHealthPercent = bot->GetHealthPct();
    return !EmbraceAllowed(VampiricEmbrace(in, BotSkill::TierOf(bot)));
}
}  // namespace PvpShadowPriestGlue
