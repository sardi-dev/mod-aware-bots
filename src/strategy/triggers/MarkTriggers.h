/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * The group's tank pulls the target its leader marked (design decision 0016): marks lead bots the way they lead players.
 */

#ifndef MOD_AWARE_BOTS_MARKTRIGGERS_H
#define MOD_AWARE_BOTS_MARKTRIGGERS_H

#include "Trigger.h"

#include <ctime>
#include <string>

// Out of combat, the bot's mark (skull by default, the "rti" value) is on a living hostile target that is not fighting
// yet, within AiPlayerbot.MarkPullRange yards, and the whole group is ready: alive, near, out of combat, with health
// and mana recovered.
class MarkedTargetReadyTrigger : public Trigger
{
public:
    // BT-191: `bodyPull` makes the trigger for a marked boss the tank walks up to (IsBodyPullMark) and the plain one
    // ignores such a mark; both share the group's ready test.
    MarkedTargetReadyTrigger(PlayerbotAI* botAI, std::string const name = "marked target ready", bool bodyPull = false)
        : Trigger(botAI, name, 2), bodyPull(bodyPull)
    {
    }

    bool IsActive() override;

    // BT-157: the whole group is ready for `tank` to pull - the test IsActive ends with, so the healers can prepare the
    // tank exactly when the pull is about to start and not while the group still drinks.
    // BT-160: `target` (the mark) decides the healer's mana bar: 70 percent before a boss, 50 before trash.
    static bool GroupReady(Group* group, Player* tank, Unit* target = nullptr);
    // The first reason the group is not ready, or nullptr; the tank says it once per mark.
    // BT-170: `heldSeconds` is how long the tank has held this mark (a damage dealer's mana stops holding it after a
    // while); `blocker` receives the member that holds the pull (BT-169).
    static char const* GroupWaitReason(Group* group, Player* tank, Unit* target, unsigned heldSeconds = 0,
                                       Player** blocker = nullptr);
    static bool IsBossMark(Unit* target);
    // BT-290: the first healer that is caught up and within MarkPullPolicy::HEALER_RANGE of `tank` but out of its sight,
    // whatever else holds the pull.
    static Player* HealerOutOfSight(Group* group, Player* tank);
    // The bot's mark: the "rti target" value, or the group's icon itself when the bot cannot see it yet.
    static Unit* CurrentMark(PlayerbotAI* botAI);
    // BT-191 (MarkPullPolicy::BodyPull): a boss of a five-player dungeon that is immune to players until it aggroes and
    // wakes when someone comes close; nothing else blocks it.
    static bool IsBodyPullMark(Player* bot, Unit* target);

private:
    // BT-169: say once per mark why a mark nobody can pull is not pulled.
    void NoteBlocker(Unit* target, char const* reason, char const* who, bool say);

    uint64 waitingMark = 0;
    time_t waitingSince = 0;
    time_t lastNotice = 0;
    bool announced = false;
    time_t lastBlockerLog = 0;
    // BT-181: since when the group is ready for this mark while it is not fighting yet.
    time_t readySince = 0;
    // Since when the current mark is out of reach and out of sight (MarkPullPolicy::StaleMark).
    time_t unreachableSince = 0;
    bool const bodyPull;
};

// BT-200 (MarkPullPolicy::WalksToLeader): a living mark that is out of reach and out of sight for a while, with the tank
// far from the leader. The tank walks to the leader's side; the pull trigger above still decides every pull.
class MarkedTargetOutOfReachTrigger : public Trigger
{
public:
    MarkedTargetOutOfReachTrigger(PlayerbotAI* botAI) : Trigger(botAI, "marked target out of reach", 2) {}

    bool IsActive() override;

private:
    uint64 markGuid = 0;
    time_t outOfReachSince = 0;
};

// A healer who is the current mark-pull blocker walks to the tank after a short wait.
class MarkHealerOutOfSightTrigger : public Trigger
{
public:
    MarkHealerOutOfSightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mark healer out of sight", 2) {}
    bool IsActive() override;
    static Player* BlockingTank(PlayerbotAI* botAI, Unit** mark = nullptr);
};

// BT-290 (MarkPullPolicy::TankStepsIntoHealerSight): the tank has held a mark for a healer out of its sight long enough
// for the healer's own walk; it steps toward her until they see each other, the spot it then pulls back to.
class MarkHealerOutOfTankSightTrigger : public Trigger
{
public:
    MarkHealerOutOfTankSightTrigger(PlayerbotAI* botAI) : Trigger(botAI, "mark healer out of tank sight", 2) {}
    bool IsActive() override;
    static Player* HiddenHealer(PlayerbotAI* botAI);
};

#endif
