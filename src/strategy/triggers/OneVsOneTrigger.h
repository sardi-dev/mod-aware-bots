/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ONEVSONETRIGGER_H
#define _PLAYERBOT_ONEVSONETRIGGER_H

#include "Trigger.h"

class PlayerbotAI;

// Active while the 1v1 policy wants the bot's own move (step back, slow, root, escape, gap closer, speed, freedom).
class OneVsOneTrigger : public Trigger
{
public:
    OneVsOneTrigger(PlayerbotAI* botAI) : Trigger(botAI, "1v1", 1) {}

    bool IsActive() override;
};

// A duel request to this bot that is still unanswered. "duel requested" fires once, from the packet; when the bot was in
// its combat engine at that moment and the engine switched (a "drop target" after a fight), the queued answer was lost
// and the request hung until it timed out (duel bench 2026-11-18, twice in a row for one paladin). This asks again.
class DuelPendingTrigger : public Trigger
{
public:
    DuelPendingTrigger(PlayerbotAI* botAI) : Trigger(botAI, "duel pending", 2) {}

    bool IsActive() override;
};

#endif
