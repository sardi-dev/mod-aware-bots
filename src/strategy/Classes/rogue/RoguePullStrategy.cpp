/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "RoguePullStrategy.h"

#include "Playerbots.h"

namespace
{
constexpr uint32 SPELL_SHURIKEN_TOSS = 114014;
}  // namespace

std::string RoguePullStrategy::GetPullActionName() const
{
    // The talent is optional: without it an empty name refuses the pull, like the warrior and monk pulls.
    if (!botAI->GetBot()->HasSpell(SPELL_SHURIKEN_TOSS))
        return std::string();

    return PullStrategy::GetPullActionName();
}
