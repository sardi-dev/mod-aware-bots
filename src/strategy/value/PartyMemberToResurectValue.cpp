/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "PartyMemberToResurectValue.h"

#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "ResurrectPolicy.h"

class IsTargetOfResurrectSpell : public SpellEntryPredicate
{
public:
    bool Check(SpellInfo const* spellInfo) override
    {
        for (uint8 i = 0; i < 3; ++i)
        {
            if (spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_RESURRECT_NEW ||
                spellInfo->Effects[i].Effect == SPELL_EFFECT_SELF_RESURRECT)
                return true;
        }

        return false;
    }
};

// BT-157: the dead members of the group, raised in the order of ResurrectPolicy: real players first, and the ones the bot
// cannot see yet as well. The old pick took the first member in the order leader, healers, tanks, rest that was within 50
// yards AND in line of sight of the bot, so a player who died around a corner, down a ramp or ahead of the group was never a
// target and nobody walked to him; it also skipped every member with an open request, which the core never closes by
// itself.
Unit* PartyMemberToResurrect::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    IsTargetOfResurrectSpell predicate;
    Player* best = nullptr;
    ResurrectPolicy::Candidate bestCandidate{};
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || member->IsGameMaster() || member->getDeathState() != DeathState::CORPSE ||
            !member->IsInWorld())
            continue;

        // A released player's body is his corpse, wherever the ghost waits (a graveyard, another map): that is what the
        // bot walks to and what it casts at. An unreleased one is the body himself.
        Corpse* corpse = member->GetCorpse();
        WorldObject* body = corpse && corpse->IsInWorld() ? static_cast<WorldObject*>(corpse) : member;
        if (body->FindMap() != bot->FindMap())
            continue;

        ResurrectPolicy::Candidate candidate;
        candidate.human = !GET_PLAYERBOT_AI(member);
        candidate.pendingRequest = member->IsRessurectRequested();
        candidate.distance = bot->GetDistance(body);
        candidate.beingResurrected = false;
        candidate.inSight = false;
        candidate.role = PlayerBotSpec::IsHeal(member) ? ResurrectPolicy::ROLE_HEALER :
            PlayerBotSpec::IsTank(member) ? ResurrectPolicy::ROLE_TANK : ResurrectPolicy::ROLE_OTHER;
        if (!ResurrectPolicy::Eligible(candidate))
            continue;

        // The two tests that cost a trace or a scan of the group come after the cheap ones.
        candidate.beingResurrected = IsTargetOfSpellCast(member, predicate);
        if (candidate.beingResurrected)
            continue;
        candidate.inSight = body == member ? Check(member) : bot->IsWithinLOSInMap(body);

        if (!best || ResurrectPolicy::Prefer(candidate, bestCandidate))
        {
            best = member;
            bestCandidate = candidate;
        }
    }
    return best;
}
