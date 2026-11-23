/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "StuckMember.h"

#include "Creature.h"
#include "GroupRejoinPolicy.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "StuckMemberPolicy.h"

// The living member of the bot's group on its map that stands nearest, or nullptr (a bot alone has none).
Player* StuckMember::NearestMember(Player* bot, float& distance)
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* nearest = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() || member->FindMap() != bot->FindMap())
            continue;
        float const d = bot->GetDistance(member);
        if (!nearest || d < distance)
        {
            nearest = member;
            distance = d;
        }
    }
    return nearest;
}

Player* StuckMember::WalkTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != bot && member->IsAlive() && member->IsInWorld() && member->FindMap() == bot->FindMap() &&
            PlayerBotSpec::IsMainTank(member))
            return member;
    }

    // No living tank on the map: the leader, then whoever stands nearest.
    Player* leader = botAI->GetGroupMaster();
    if (leader && leader != bot && leader->IsAlive() && leader->IsInWorld() && leader->FindMap() == bot->FindMap())
        return leader;

    float distance = 0.0f;
    return NearestMember(bot, distance);
}

bool StuckMember::Observe(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    StuckMemberPolicy::State& state = botAI->stuckMember;
    time_t const now = time(nullptr);
    if (!StuckMemberPolicy::Due(state, now))
        return false;

    StuckMemberPolicy::Sample sample;
    sample.lastDamageAt = botAI->lastDamageTakenAt.load(std::memory_order_relaxed);
    // A hit after a drop ends the post-drop walk: a new fight is the combat code's again.
    GroupRejoinPolicy::EndFollowOnDamage(botAI->rejoin, sample.lastDamageAt);
    // Battlegrounds and arenas have their own movement tactics; a bot out of combat is the follow sampler's, except for
    // the walk after a dropped combat state (BT-202), which needs the distance to the group.
    bool const walking = botAI->rejoin.followUntil != 0 && now < botAI->rejoin.followUntil;
    if (bot->IsAlive() && (bot->IsInCombat() || walking) && bot->GetGroup() && !bot->InBattleground())
    {
        float distance = 0.0f;
        if (NearestMember(bot, distance))
            sample.nearestMember = distance;
        sample.inCombat = bot->IsInCombat();
        sample.x = bot->GetPositionX();
        sample.y = bot->GetPositionY();
        sample.z = bot->GetPositionZ();
    }
    bool const due = StuckMemberPolicy::Read(state, sample, now);
    if (!GroupRejoinPolicy::Wants(state, botAI->rejoin, sPlayerbotAIConfig->sightDistance, now))
        GroupRejoinPolicy::Idle(botAI->rejoin);
    return due;
}

void StuckMember::Report(PlayerbotAI* botAI, std::string const& trigger, std::string const& decision,
                         uint32_t triggersChecked, uint32_t triggersFired, bool minimal)
{
    Player* bot = botAI->GetBot();
    StuckMemberPolicy::State& state = botAI->stuckMember;
    time_t const now = time(nullptr);
    state.loggedAt = now;

    // The bot's own attackers and the creatures that hold it in combat, as the [DEFEND] sample counts them.
    uint32 attackers = 0;
    uint32 hostiles = 0;
    Unit* nearestAttacker = nullptr;
    for (HostileReference* hostile = bot->getHostileRefManager().getFirst(); hostile; hostile = hostile->next())
    {
        Unit* holder = hostile->GetSource()->GetOwner();
        if (holder && holder->ToCreature() && holder->IsAlive())
            ++hostiles;
    }
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->ToCreature() && attacker->IsAlive() && attacker->GetVictim() == bot &&
            bot->IsHostileTo(attacker))
        {
            ++attackers;
            if (!nearestAttacker || bot->GetDistance(attacker) < bot->GetDistance(nearestAttacker))
                nearestAttacker = attacker;
        }

    float memberDistance = 0.0f;
    Player* member = NearestMember(bot, memberDistance);
    Unit* tankTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("tank target")->Get();
    Unit* currentTarget = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    unsigned const stillFor = unsigned(now - state.stillSince);

    SF_LOG_INFO("playerbots", "[DEFEND] bot=%s attackers=%u hostiles=%u nearest=%s los=%s tank_target=%s current_target=%s "
                "trigger=%s checked=%u fired=%u decision=%s stuck=1 still_for=%us nearest_member=%s member_dist=%.1f",
                bot->GetName().c_str(), attackers, hostiles, nearestAttacker ? nearestAttacker->GetName().c_str() : "none",
                nearestAttacker ? (bot->IsWithinLOSInMap(nearestAttacker) ? "yes" : "no") : "n/a",
                tankTarget ? tankTarget->GetName().c_str() : "none", currentTarget ? currentTarget->GetName().c_str() : "none",
                trigger.c_str(), triggersChecked, triggersFired, decision.c_str(), stillFor,
                member ? member->GetName().c_str() : "none", memberDistance);

    Player* leader = botAI->GetGroupMaster();
    bool const leaderHere = leader && leader != bot && leader->IsInWorld() && leader->FindMap() == bot->FindMap();
    Action* follow = botAI->GetAiObjectContext()->GetAction("follow");
    SF_LOG_INFO("playerbots", "[FOLLOW] bot=%s leader=%s dist=%.1f far_for=%us follow=%s stay=%u moving=%u resting=%u "
                "minimal=%u trigger=%s decision=%s stuck=1 combat=1 still_for=%us nearest_member=%s member_dist=%.1f",
                bot->GetName().c_str(), leaderHere ? leader->GetName().c_str() : "none",
                leaderHere ? bot->GetDistance(leader) : 0.0f, unsigned(now - state.farSince),
                !follow ? "none" : (follow->isUseful() ? "useful" : "not useful"),
                uint32(botAI->HasStrategy("stay", BOT_STATE_NON_COMBAT)), uint32(bot->isMoving()),
                uint32(bot->IsSitState()), uint32(minimal), trigger.c_str(), decision.c_str(), stillFor,
                member ? member->GetName().c_str() : "none", memberDistance);
}
