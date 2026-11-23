/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpElemental.h for what this is.
 */

#include "PvpElemental.h"

#include "Action.h"
#include "BotSkill.h"
#include "Group.h"
#include "InterruptGate.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PvpAllyFacts.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpElementalPolicy.h"
#include "PvpInterrupt.h"
#include "PvpSpellCatalog.h"
#include "PvpTeam.h"
#include "Spell.h"
#include "Timer.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PvpElementalGlue
{
namespace
{
using namespace PvpElemental;

constexpr char const* LOG_CATEGORY = "playerbots";
constexpr float WATCH_YD = 40.0f;          // the casters the shaman watches
constexpr float TOTEM_REACH_YD = 30.0f;    // the allies a Grounding Totem covers

bool Active(Player* bot)
{
    return PvpConfig::TeamPlayEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_SHAMAN;
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

struct Incoming
{
    bool control = false;
    bool atShaman = false;
    std::uint32_t leftMs = 0;
};

// The control cast of a hostile player that is aimed at the shaman or at an ally the totem covers, with the most time left.
Incoming ReadIncoming(PlayerbotAI* botAI, Player* bot)
{
    Incoming best;
    for (PvpAllyFacts::ControlCast const& cast : PvpAllyFacts::IncomingControl(botAI, bot, PvpAllyFacts::Reachable(bot, TOTEM_REACH_YD), WATCH_YD))
    {
        bool const atMe = cast.aimed == bot;
        if (!best.control || (atMe && !best.atShaman) || (atMe == best.atShaman && cast.leftMs > best.leftMs))
            best = { true, atMe, cast.leftMs };
    }
    return best;
}

bool NearestEnemyPlayer(PlayerbotAI* botAI, Player* bot, float& distance)
{
    distance = 999.0f;
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (enemy && enemy->IsAlive() && bot->IsValidAttackTarget(enemy))
            distance = std::min(distance, bot->GetExactDist(enemy));
    }
    return distance < 999.0f;
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    Tier const tier = BotSkill::TierOf(bot);
    switch (rule)
    {
        case Rule::SWIFTNESS:
        {
            SwiftnessInputs in;
            in.ready = Ready(bot, SPELL_ANCESTRAL_SWIFTNESS) && !bot->HasAura(SPELL_ANCESTRAL_SWIFTNESS);
            in.hexDue = PvpTeamGlue::ControlPickIs(botAI, bot, SPELL_HEX);
            in.enemyKickNotSpent = PvpTeamGlue::EnemyStopperNotSpent(botAI, bot);
            return SwiftnessForHex(in, tier) == SwiftnessReason::PRESS;
        }
        case Rule::GROUNDING:
        {
            Incoming const incoming = ReadIncoming(botAI, bot);
            GroundInputs in;
            in.ready = Ready(bot, SPELL_GROUNDING_TOTEM);
            in.incomingControl = incoming.control;
            in.atShaman = incoming.atShaman;
            in.leftMs = incoming.leftMs;
            return Grounding(in, tier) == GroundReason::GROUND;
        }
        case Rule::CALL:
        {
            Incoming const incoming = ReadIncoming(botAI, bot);
            bool const spent = bot->HasSpell(SPELL_GROUNDING_TOTEM) && bot->HasSpellCooldown(SPELL_GROUNDING_TOTEM);
            return CallOfTheElementsNow(spent, Ready(bot, SPELL_CALL_OF_THE_ELEMENTS), incoming.control, incoming.leftMs, tier);
        }
        case Rule::GRACE:
        {
            GraceInputs in;
            in.ready = bot->getLevel() >= SPIRITWALKERS_GRACE_LEVEL && Ready(bot, SPELL_SPIRITWALKERS_GRACE) &&
                !bot->HasAura(SPELL_SPIRITWALKERS_GRACE);
            in.moving = bot->isMoving();
            Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
            in.hasTargetInReach = PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target) && bot->GetExactDist(target) <= 40.0f;
            NearestEnemyPlayer(botAI, bot, in.nearestEnemyPlayerYd);
            return SpiritwalkersGrace(in, tier) == GraceReason::PRESS;
        }
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    uint32 spell = 0;
    char const* what = "";
    switch (rule)
    {
        case Rule::SWIFTNESS: spell = SPELL_ANCESTRAL_SWIFTNESS; what = "ANCESTRAL_SWIFTNESS"; break;
        case Rule::GROUNDING: spell = SPELL_GROUNDING_TOTEM; what = "GROUNDING"; break;
        case Rule::CALL: spell = SPELL_CALL_OF_THE_ELEMENTS; what = "CALL_OF_THE_ELEMENTS"; break;
        case Rule::GRACE: spell = SPELL_SPIRITWALKERS_GRACE; what = "SPIRITWALKERS_GRACE"; break;
    }
    if (!spell || !botAI->CastSpell(spell, bot))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPSHAMAN] bot=%u tier=%u kind=%s", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what);
    return true;
}

bool Holds(PlayerbotAI* botAI, Player* bot, Action* action)
{
    if (!action || !Active(bot))
        return false;
    std::string const name = action->getName();
    Tier const tier = BotSkill::TierOf(bot);
    if (name == "ancestral swiftness")
        return SwiftnessHeld(bot->InBattleground(), bot->HasSpell(SPELL_HEX), Ready(bot, SPELL_HEX), tier);
    if (name == "ascendance" || name == "fire elemental totem" || name == "fire elemental totem melee" || name == "elemental mastery")
    {
        Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
        if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
            return false;
        return !BurstAllowed(Burst(true, PvpBurstGlue::Open(botAI, bot, target), tier));
    }
    return false;
}

void Forget(unsigned int guidLow)
{
    PvpAllyFacts::ForgetTracks(guidLow);
}
}  // namespace PvpElementalGlue
