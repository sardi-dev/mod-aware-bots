/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TankPackStart.h"

#include "Event.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "TankPackStartPolicy.h"
#include "Timer.h"

bool TankPackStartTrigger::IsActive()
{
    if (!bot->IsAlive() || !bot->IsInCombat())
    {
        combatSinceMs = 0;
        return false;
    }
    uint32 const now = getMSTime();
    if (!combatSinceMs)
        combatSinceMs = now;

    unsigned onTank = 0;
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsAlive() && attacker->GetVictim() == bot)
            ++onTank;
    return TankPackStartPolicy::PackStart(onTank, getMSTimeDiff(combatSinceMs, now));
}

std::vector<std::string> const& TankPackStartAction::Cooldowns(uint8 playerClass)
{
    // Role sheet tank.md section 3: the pack-start cooldowns that exist as class actions. The monk has none in the list.
    static std::vector<std::string> const none;
    static std::vector<std::string> const warrior = { "demoralizing shout" };
    static std::vector<std::string> const paladin = { "avenging wrath" };
    static std::vector<std::string> const deathKnight = { "dancing rune weapon" };
    static std::vector<std::string> const druid = { "berserk" };
    switch (playerClass)
    {
        case CLASS_WARRIOR:
            return warrior;
        case CLASS_PALADIN:
            return paladin;
        case CLASS_DEATH_KNIGHT:
            return deathKnight;
        case CLASS_DRUID:
            return druid;
        default:
            return none;
    }
}

bool TankPackStartAction::isUseful()
{
    return PlayerBotSpec::IsTank(bot) && !Cooldowns(bot->getClass()).empty();
}

bool TankPackStartAction::Execute(Event event)
{
    for (std::string const& name : Cooldowns(bot->getClass()))
    {
        Action* action = context->GetAction(name);
        if (!action || !action->isPossible() || !action->isUseful() || !action->Execute(event))
            continue;
        SF_LOG_INFO("playerbots", "[TANK] %s pack start casts %s", bot->GetName().c_str(), name.c_str());
        return true;
    }
    return false;
}

void TankPackStartStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // With the small danger defensive (ACTION_MOVE + 1): above facing and threat fillers, below the emergencies.
    triggers.push_back(new TriggerNode("tank pack start",
        NextAction::array(0, new NextAction("tank pack start", ACTION_MOVE + 2), nullptr)));
}
