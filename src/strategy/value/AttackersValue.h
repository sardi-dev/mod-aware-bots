#ifndef _PLAYERBOT_ATTACKERSVALUE_H
#define _PLAYERBOT_ATTACKERSVALUE_H

#include "PlayerbotAIConfig.h"
#include "Value.h"

#include <ctime>

class Group;
class Player;
class PlayerbotAI;
class Unit;

class AttackersValue : public ObjectGuidListCalculatedValue
{
public:
    AttackersValue(PlayerbotAI* botAI) : ObjectGuidListCalculatedValue(botAI, "attackers", 1 * 1000) {}

    GuidVector Calculate();
    static bool IsPossibleTarget(Unit* attacker, Player* bot, float range = sPlayerbotAIConfig->sightDistance);
    static bool IsValidTarget(Unit* attacker, Player* bot);
    bool HasReleasedMark(uint64 guid) const { return heldMark == guid && heldMarkReleaseLogged; }
    // BT-187: `target` is this bot's stale combat target (BT-168), chosen less than 20 s ago: it stays the bot's target
    // without line of sight so the bot walks to it (StaleCombatPolicy::KeepWithoutSight).
    static bool KeepsStaleTargetWithoutSight(PlayerbotAI* botAI, Unit* target);

private:
    void AddAttackersOf(Group* group, std::unordered_set<Unit*>& targets);
    void AddAttackersOf(Player* player, std::unordered_set<Unit*>& targets);
    void RemoveNonThreating(std::unordered_set<Unit*>& targets);
    bool hasRealThreat(Unit* attacker);
    bool HoldMarkForTank(Unit* marked);
    void AddStaleCombatTarget(GuidVector& result);

    // BT-141: the untouched mark this bot left to the tank, and since when (the fallback opens it after a while).
    uint64 heldMark = 0;
    time_t heldMarkSince = 0;
    bool heldMarkReleaseLogged = false;

    // BT-168: since when the bot is flagged in combat with nothing attacking it, and the stale target already logged.
    time_t quietSince = 0;
    uint64 staleLoggedFor = 0;
    // BT-187: the stale target handed out in this quiet spell, and since when (0 = none).
    uint64 staleTarget = 0;
    time_t staleTargetSince = 0;
};

class PossibleAddsValue : public BoolCalculatedValue
{
public:
    PossibleAddsValue(PlayerbotAI* botAI, std::string const name = "possible adds") : BoolCalculatedValue(botAI, name)
    {
    }

    bool Calculate() override;
};

class PrioritizedTargetsValue : public ManualSetValue<GuidVector>
{
public:
    PrioritizedTargetsValue(PlayerbotAI* botAI, std::string const name = "prioritized targets")
        : ManualSetValue(botAI, GuidVector(), name)
    {
    }
};

#endif
