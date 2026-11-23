/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "RejoinGroup.h"

#include "Creature.h"
#include "GroupRejoinPolicy.h"
#include "Log.h"
#include "Player.h"
#include "Playerbots.h"
#include "StuckMember.h"

namespace
{
bool Wanted(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    // A bot told to stay stays; a bot in a vehicle or in transit is not left behind.
    if (!bot->IsAlive() || bot->GetVehicle() || bot->IsBeingTeleported() || botAI->HasStrategy("stay", botAI->GetState()))
        return false;
    return GroupRejoinPolicy::Wants(botAI->stuckMember, botAI->rejoin, sPlayerbotAIConfig->sightDistance, time(nullptr));
}
}  // namespace

bool FarFromGroupInCombatTrigger::IsActive()
{
    return Wanted(botAI);
}

bool RejoinGroupAction::isUseful()
{
    return Wanted(botAI);
}

bool RejoinGroupAction::DropStaleCombat(time_t now)
{
    StuckMemberPolicy::State const& member = botAI->stuckMember;
    // A creature with the bot as its victim that stands at the bot is a fight; one that has not come in 30 s is not. A
    // player or a player's pet that attacks the bot is a fight whatever its distance (PvP, a duel, a mind-controlled
    // member).
    bool attackerAtHand = false;
    bool playerAttacker = false;
    uint32 attackers = 0;
    for (Unit* attacker : bot->getAttackers())
    {
        if (!attacker || !attacker->IsAlive())
            continue;
        if (attacker->IsControlledByPlayer())
            playerAttacker = true;
        if (!attacker->ToCreature() || attacker->GetVictim() != bot)
            continue;
        ++attackers;
        if (bot->GetDistance(attacker) <= GroupRejoinPolicy::ATTACKER_REACH_YARDS)
            attackerAtHand = true;
    }

    if (!GroupRejoinPolicy::FlagIsStale(member, attackerAtHand, playerAttacker, now))
    {
        if (GroupRejoinPolicy::ShouldLog(botAI->rejoin, now))
        {
            botAI->rejoin.loggedAt = now;
            SF_LOG_INFO("playerbots", "[REJOIN] %s keeps its combat state: %s (attackers=%u)", bot->GetName().c_str(),
                        playerAttacker ? "a player or a player's pet attacks it" :
                        (attackerAtHand ? "a creature that attacks it stands at it" : "it took damage in the last 30 s"),
                        attackers);
        }
        return false;
    }

    SF_LOG_INFO("playerbots", "[REJOIN] %s drops a stale combat state: %u failed walks, no damage for %u s, attackers=%u "
                "none within %.0f yd, nearest member %.1f yd", bot->GetName().c_str(), botAI->rejoin.walkFails,
                member.hurtAt ? unsigned(now - member.hurtAt) : unsigned(now - member.farSince), attackers,
                GroupRejoinPolicy::ATTACKER_REACH_YARDS, member.nearestMember);
    botAI->rejoin.loggedAt = now;

    // The core's own way to end a fight (the immunity aura and the charm code do the same): the engine's drop target,
    // CombatStop (stops the swing, removes every attacker from the bot, clears the flag) and then deleteReferences, which
    // takes the bot off every creature's threat list. ClearInCombat alone would leave those references: the instance
    // rule of HostileRefManager::deleteStaleReferences keeps them for living creatures, the creatures would still hold
    // the bot as their victim and the bot's attacker list would pull it into the combat engine again. Without the
    // references the Officers have no victim left and evade. No teleport. includingCast stays false (the core's own
    // callers note that interrupting spells here may crash).
    botAI->DoSpecificAction("drop target", Event(), true);
    bot->CombatStop();
    bot->getHostileRefManager().deleteReferences();
    botAI->rejoin.droppedAt = now;
    botAI->rejoin.followUntil = now + GroupRejoinPolicy::FOLLOW_AFTER_DROP_SECONDS;
    GroupRejoinPolicy::Idle(botAI->rejoin);
    return true;
}

bool RejoinGroupAction::Execute(Event /*event*/)
{
    Player* target = StuckMember::WalkTarget(botAI);
    if (!target)
        return false;

    time_t const now = time(nullptr);
    GroupRejoinPolicy::State& state = botAI->rejoin;
    if (GroupRejoinPolicy::NextStep(state, bot->GetDistance(target), bot->GetPositionX(), bot->GetPositionY(),
                                    bot->GetPositionZ(), bot->IsInCombat(), now) == GroupRejoinPolicy::Step::Drop)
    {
        if (DropStaleCombat(now))
            return true;
        // Refused: the flag is real. Walk on, and judge again after two more windows.
        GroupRejoinPolicy::Idle(state);
    }

    if (GroupRejoinPolicy::ShouldLog(state, now))
    {
        state.loggedAt = now;
        SF_LOG_INFO("playerbots", "[REJOIN] %s walks to %s at %.1f yd (nearest member %.1f yd, flagged=%u, failed walks=%u)",
                    bot->GetName().c_str(), target->GetName().c_str(), bot->GetDistance(target),
                    botAI->stuckMember.nearestMember, uint32(bot->IsInCombat()), state.walkFails);
    }

    // BT-698: no new path request while the last order gained no ground (backoff 2 s to 30 s, at once the longest beyond
    // 500 yd); a bot that walks is ordered as often as before.
    if (!GroupRejoinPolicy::MoveDue(state, bot->GetDistance(target), now))
        return false;

    // The full path to the tank's own spot, ahead of the combat engine's own moves (MOVEMENT_COMBAT outranks MOVEMENT_NORMAL
    // and is not held by a wait for an older move of lower priority). A failed move is judged by the yards it closes.
    return MoveTo(target->GetMapId(), target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false, false,
                  false, false, MovementPriority::MOVEMENT_COMBAT);
}

void RejoinGroupStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("mark healer out of sight",
        NextAction::array(0, new NextAction("mark healer move into sight", 50.0f), nullptr)));
    triggers.push_back(new TriggerNode("far from group in combat",
        NextAction::array(0, new NextAction("rejoin group", ACTION_INTERRUPT), nullptr)));
}
