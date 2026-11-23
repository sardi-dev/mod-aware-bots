/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "ToCActions.h"

#include <iterator>

#include "EncounterHelpers.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "ToCTriggers.h"

using ToCLogic::Point;

namespace
{
// One heal per class that heals in 5.4.8; the bot casts the first it knows. Any heal drains Incinerate Flesh's healing
// absorb (66237, aura 301), full health or not, so the choice of heal does not matter.
char const* const HEAL_SPELLS[] = {"flash heal", "flash of light", "healing surge", "healing touch", "surging mist"};
}  // namespace

bool ToCMarkPriorityAction::Execute(Event /*event*/)
{
    Unit* target = ToC::MarkTarget(botAI);
    return target && EncounterHelpers::MarkTargetWithSkull(bot, target);
}

bool ToCAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* target = ToC::DpsTarget(botAI);
    return target && Attack(target);
}

bool ToCTankTargetAction::Execute(Event /*event*/)
{
    Unit* target = ToC::TankTarget(botAI, _main);
    return target && Attack(target);
}

bool ToCAvoidHazardAction::Execute(Event /*event*/)
{
    Unit* hazard = ToC::HazardToAvoid(botAI);
    if (!hazard)
        return false;

    float const radius = ToCLogic::HazardRadius(hazard->GetEntry());
    return MoveAway(hazard, radius - bot->GetDistance2d(hazard) + 1.0f);
}

bool ToCDodgeChargeAction::Execute(Event /*event*/)
{
    Point dest;
    if (!ToC::ChargeEscape(botAI, dest))
        return false;

    return MoveTo(ToC::MAP_ID, dest.x, dest.y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED);
}

bool ToCRunFromRaidAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    std::vector<Point> others;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->GetMapId() == bot->GetMapId())
            others.push_back({member->GetPositionX(), member->GetPositionY()});
    }

    Point dest;
    if (!ToCLogic::LegionFlameSpot({bot->GetPositionX(), bot->GetPositionY()}, others, dest))
        return false;

    return MoveTo(ToC::MAP_ID, dest.x, dest.y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED);
}

bool ToCHealIncineratedAction::Execute(Event /*event*/)
{
    Player* member = ToC::IncineratedMember(botAI);
    if (!member)
        return false;

    for (char const* spell : HEAL_SPELLS)
        if (botAI->CanCastSpell(spell, member))
            return botAI->CastSpell(spell, member);
    return false;
}

// Walks to the essence and clicks it as a player does: the click is the spellclick the client sends within
// interaction range (npc_spellclick_spells 34567 and 34568).
bool ToCTakeEssenceAction::Execute(Event /*event*/)
{
    Unit* essence = ToC::EssenceToTake(botAI);
    if (!essence)
        return false;

    if (!essence->IsWithinDistInMap(bot, INTERACTION_DISTANCE))
        return MoveTo(essence, std::max(INTERACTION_DISTANCE - 1.0f, 0.0f), MovementPriority::MOVEMENT_COMBAT);

    return essence->HandleSpellClick(bot);
}

bool ToCHandleOrbsAction::Execute(Event /*event*/)
{
    Point dest;
    if (!ToC::OrbMove(botAI, dest))
        return false;

    return MoveTo(ToC::MAP_ID, dest.x, dest.y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool ToCKiteSpikeAction::Execute(Event /*event*/)
{
    Point dest;
    if (!ToC::SpikeEscape(botAI, dest))
        return false;

    return MoveTo(ToC::MAP_ID, dest.x, dest.y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED);
}

bool ToCPullBurrowerAwayAction::Execute(Event /*event*/)
{
    ToC::Scene const& scene = ToC::Scan(botAI);
    if (!scene.anubarak)
        return false;

    float const missing = ToCLogic::BURROWER_TANK_DISTANCE - bot->GetDistance2d(scene.anubarak);
    return missing > 0.0f && MoveAway(scene.anubarak, missing + 2.0f);
}
