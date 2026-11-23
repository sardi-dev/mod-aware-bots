/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpSubtlety.h for what this is.
 */

#include "PvpSubtlety.h"

#include "Action.h"
#include "BotSkill.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "PvpAwareness.h"
#include "PvpBurst.h"
#include "PvpConfig.h"
#include "PvpSubtletyPolicy.h"
#include "PvpTeam.h"
#include "PvpUnitFacts.h"
#include "Timer.h"

#include <string>
#include <vector>

namespace PvpSubtletyGlue
{
namespace
{
using namespace PvpSubtlety;

constexpr char const* LOG_CATEGORY = "playerbots";
constexpr float PARTNER_RANGE_YD = 60.0f;

bool Active(Player* bot)
{
    return PvpConfig::BurstRulesEnabled() && bot && bot->IsAlive() && bot->getClass() == CLASS_ROGUE;
}

bool Ready(Player* bot, uint32 spellId)
{
    return bot->HasSpell(spellId) && !bot->HasSpellCooldown(spellId);
}

Unit* CurrentTarget(PlayerbotAI* botAI)
{
    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
}

struct SapChoice
{
    Player* target = nullptr;
    std::int32_t score = 0;
};

SapChoice BestSap(PlayerbotAI* botAI, Player* bot)
{
    SapChoice best;
    Unit* killTarget = CurrentTarget(botAI);
    if (!killTarget || !Ready(bot, SPELL_SAP) || !bot->HasAura(SPELL_STEALTH))
        return best;
    std::uint32_t const now = getMSTime();
    std::shared_ptr<PvpAware::Book> const book = PvpAwareness::ViewOf(bot);
    GuidVector const near = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest enemy players")->Get();
    for (BotGuid const guid : near)
    {
        Unit* unit = botAI->GetUnit(guid);
        Player* enemy = unit ? unit->ToPlayer() : nullptr;
        if (!enemy || !enemy->IsAlive() || !bot->IsValidAttackTarget(enemy) || !bot->CanSeeOrDetect(enemy) || !bot->IsWithinLOSInMap(enemy))
            continue;
        SapInputs in;
        in.ready = true;
        in.botInCombat = bot->IsInCombat();
        in.candidateInCombat = enemy->IsInCombat();
        in.candidateIsKillTarget = enemy == killTarget;
        in.candidateDistance = bot->GetExactDist(enemy);
        in.candidateHeld = enemy->HasAura(SPELL_SAP) || PvpUnitFacts::HasBreakableCc(enemy) || PvpUnitFacts::IsHeldStill(enemy);
        in.drStep = book ? book->DrStep(enemy->GetGUID(), PvpSpells::DrCategory::INCAPACITATE, now) : 0;
        in.candidateIsHealer = PlayerBotSpec::IsHeal(enemy, true);
        in.candidateIsCaster = PlayerBotSpec::IsCaster(enemy, true);
        SapVerdict const verdict = Sap(in, BotSkill::TierOf(bot));
        if (verdict.cast && (!best.target || verdict.score > best.score))
            best = { enemy, verdict.score };
    }
    return best;
}

struct TricksChoice
{
    Player* partner = nullptr;
    TricksReason reason = TricksReason::NO;
};

TricksChoice BestTricks(PlayerbotAI* botAI, Player* bot)
{
    TricksChoice best;
    Unit* target = CurrentTarget(botAI);
    Group* group = bot->GetGroup();
    TricksInputs in;
    in.ready = Ready(bot, SPELL_TRICKS_OF_THE_TRADE);
    in.teamGoOpen = target && PvpTeamGlue::GoOpenOn(bot, target);
    if (!group || !in.ready || !in.teamGoOpen)
    {
        best.reason = Tricks(in, false, TricksCandidate(), BotSkill::TierOf(bot));
        return best;
    }
    std::int32_t bestScore = -1;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || member->GetMap() != bot->GetMap() ||
            bot->GetExactDist(member) > PARTNER_RANGE_YD || !bot->IsWithinLOSInMap(member))
            continue;
        TricksCandidate c;
        c.damageDealer = !PlayerBotSpec::IsHeal(member, true) && !PlayerBotSpec::IsTank(member, true);
        c.bursting = PvpTeamGlue::HasBurstAura(member);
        c.healthPercent = member->GetHealthPct();
        TricksReason const reason = Tricks(in, true, c, BotSkill::TierOf(bot));
        if (TricksDue(reason) && TricksScore(c) > bestScore)
        {
            bestScore = TricksScore(c);
            best = { member, reason };
        }
    }
    if (!best.partner)
        best.reason = TricksReason::NO_PARTNER;
    return best;
}
}  // namespace

bool Due(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Active(bot))
        return false;
    switch (rule)
    {
        case Rule::SAP:
            return BestSap(botAI, bot).target != nullptr;
        case Rule::TRICKS:
            return BestTricks(botAI, bot).partner != nullptr;
    }
    return false;
}

bool Cast(PlayerbotAI* botAI, Player* bot, Rule rule)
{
    if (!Due(botAI, bot, rule))
        return false;
    Player* aimed = nullptr;
    uint32 spell = 0;
    char const* what = "";
    switch (rule)
    {
        case Rule::SAP:
            what = "SAP";
            spell = SPELL_SAP;
            aimed = BestSap(botAI, bot).target;
            break;
        case Rule::TRICKS:
            what = "TRICKS_OF_THE_TRADE";
            spell = SPELL_TRICKS_OF_THE_TRADE;
            aimed = BestTricks(botAI, bot).partner;
            break;
    }
    if (!spell || !aimed || !botAI->CastSpell(spell, aimed))
        return false;
    SF_LOG_DEBUG(LOG_CATEGORY, "[PVPROGUE] bot=%u tier=%u kind=%s target=%u", bot->GetGUIDLow(), uint32(BotSkill::TierOf(bot)), what,
        aimed->GetGUIDLow());
    return true;
}

bool Holds(PlayerbotAI* botAI, Player* bot, Action* action)
{
    if (!action || !Active(bot))
        return false;
    std::string const name = action->getName();
    if (name != "shadow dance" && name != "shadow blades" && name != "marked for death")
        return false;
    Unit* target = CurrentTarget(botAI);
    if (!PvpBurstGlue::TargetIsEnemyPlayer(botAI, bot, target))
        return false;
    return !GoAllowed(Go(true, PvpBurstGlue::Open(botAI, bot, target), BotSkill::TierOf(bot)));
}
}  // namespace PvpSubtletyGlue
