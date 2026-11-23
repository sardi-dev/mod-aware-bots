/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TankDanger.h"

#include "Creature.h"
#include "Group.h"
#include "Log.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotSpec.h"
#include "Playerbots.h"
#include "Timer.h"

namespace
{
// A living healer of the group on the tank's map, in heal range and in sight of it.
bool HealerCanReach(Player* tank)
{
    Group* group = tank->GetGroup();
    if (!group)
        return false;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (member && member != tank && member->IsAlive() && member->GetMapId() == tank->GetMapId() &&
            PlayerBotSpec::IsHeal(member, true) && member->IsWithinDistInMap(tank, sPlayerbotAIConfig->healDistance) &&
            member->IsWithinLOSInMap(tank))
            return true;
    }
    return false;
}

bool IsBoss(Unit* unit)
{
    Creature* creature = unit ? unit->ToCreature() : nullptr;
    return creature && (creature->IsDungeonBoss() || creature->isWorldBoss());
}
}  // namespace

uint8 TankDangerValue::Calculate()
{
    uint32 const now = getMSTime();
    if (!bot->IsAlive() || !bot->IsInCombat())
    {
        drop.Reset();
        botAI->tankDanger = 0;
        botAI->tankDangerAtMs = now;
        return 0;
    }

    TankDangerPolicy::Reading reading;
    // The attackers list the engine already keeps; only the ones that hit the tank count.
    for (BotGuid const& guid : AI_VALUE(GuidVector, "attackers"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetVictim() != bot)
            continue;
        ++reading.attackersOnTank;
        reading.bossOnTank = reading.bossOnTank || IsBoss(unit);
    }
    reading.healthDropPct = drop.Observe(bot->GetHealthPct(), now);
    reading.healerAvailable = HealerCanReach(bot);

    uint8 const level = uint8(TankDangerPolicy::Level(reading));
    botAI->tankDanger = level;
    botAI->tankDangerAtMs = now;
    return level;
}

unsigned TankDanger::LevelFor(Player* tank)
{
    PlayerbotAI* tankAI = tank ? GET_PLAYERBOT_AI(tank) : nullptr;
    if (!tankAI || tankAI->IsRealPlayer() || !tank->IsInCombat() ||
        !TankDangerPolicy::ReadingFresh(tankAI->tankDangerAtMs, getMSTime()))
        return 0;
    return tankAI->tankDanger;
}

bool TankDangerDefensiveTrigger::IsActive()
{
    if (!bot->IsAlive() || !bot->IsInCombat())
        return false;
    unsigned const level = AI_VALUE(uint8, "tank danger");
    float const health = bot->GetHealthPct();
    return big ? TankDangerPolicy::BigDefensive(health, level) : TankDangerPolicy::SmallDefensive(health, level);
}

std::vector<std::string> const& TankDangerDefensiveAction::Defensives(uint8 playerClass, bool big)
{
    // The tank defensives of the role sheet (tank.md, section 3) that already exist as class actions.
    static std::vector<std::string> const none;
    static std::vector<std::string> const warriorSmall = { "shield barrier", "demoralizing shout" };
    static std::vector<std::string> const warriorBig = { "shield wall", "last stand" };
    static std::vector<std::string> const paladinSmall = { "divine protection" };
    static std::vector<std::string> const paladinBig = { "ardent defender", "guardian of ancient kings" };
    static std::vector<std::string> const deathKnightSmall = { "rune tap" };
    static std::vector<std::string> const deathKnightBig = { "icebound fortitude", "vampiric blood" };
    static std::vector<std::string> const druidSmall = { "barkskin", "savage defense" };
    static std::vector<std::string> const druidBig = { "survival instincts", "might of ursoc" };
    static std::vector<std::string> const monkSmall = { "elusive brew", "dampen harm" };
    static std::vector<std::string> const monkBig = { "fortifying brew" };
    switch (playerClass)
    {
        case CLASS_WARRIOR:
            return big ? warriorBig : warriorSmall;
        case CLASS_PALADIN:
            return big ? paladinBig : paladinSmall;
        case CLASS_DEATH_KNIGHT:
            return big ? deathKnightBig : deathKnightSmall;
        case CLASS_DRUID:
            return big ? druidBig : druidSmall;
        case CLASS_MONK:
            return big ? monkBig : monkSmall;
        default:
            return none;
    }
}

bool TankDangerDefensiveAction::isUseful()
{
    return PlayerBotSpec::IsTank(bot, true) && !Defensives(bot->getClass(), big).empty() &&
        (!big || TankDangerPolicy::BigAllowed(lastBigMs, getMSTime(), AI_VALUE(uint8, "tank danger")));
}

bool TankDangerDefensiveAction::Execute(Event event)
{
    for (std::string const& name : Defensives(bot->getClass(), big))
    {
        Action* action = context->GetAction(name);
        if (!action || !action->isPossible() || !action->isUseful() || !action->Execute(event))
            continue;

        if (big)
            lastBigMs = getMSTime();
        // One line per defensive at most every few seconds: enough to see the rule work in a run log.
        constexpr time_t LOG_GAP_SECONDS = 5;
        time_t const now = time(nullptr);
        if (now - lastLog >= LOG_GAP_SECONDS)
        {
            lastLog = now;
            SF_LOG_INFO("playerbots", "[TANK] %s danger=%u health=%.0f casts %s", bot->GetName().c_str(),
                        unsigned(botAI->tankDanger), bot->GetHealthPct(), name.c_str());
        }
        return true;
    }
    return false;
}

void TankDangerStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // Below interrupts, taunts and the health-based emergencies of the class rotations (ACTION_INTERRUPT and up), above
    // facing the mob and the threat fillers: a cooldown the danger calls for comes before the next repositioning.
    triggers.push_back(new TriggerNode("tank danger big defensive",
        NextAction::array(0, new NextAction("tank danger big defensive", ACTION_INTERRUPT - 1), nullptr)));
    triggers.push_back(new TriggerNode("tank danger small defensive",
        NextAction::array(0, new NextAction("tank danger small defensive", ACTION_MOVE + 1), nullptr)));
}

bool MainTankInDangerTrigger::IsActive()
{
    if (!bot->IsInCombat())
        return false;
    Unit* unit = AI_VALUE(Unit*, "main tank");
    Player* tank = unit ? unit->ToPlayer() : nullptr;
    if (!tank || tank == bot || !tank->IsAlive())
        return false;
    return TankDangerPolicy::ExternalOnTank(tank->GetHealthPct(), TankDanger::LevelFor(tank));
}
