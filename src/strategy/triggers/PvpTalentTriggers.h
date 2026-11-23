/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef MOD_AWARE_BOTS_PVPTALENTTRIGGERS_H
#define MOD_AWARE_BOTS_PVPTALENTTRIGGERS_H

#include "Trigger.h"

class PlayerbotAI;

// The bot has a step to take toward the talent group it should be on now (Factory/BotPvpTalents.h, BT-320, BT-323).
// Checked every few seconds: the check reads the talent and glyph tables, and a preparation phase lasts a minute or more.
class PvpTalentSetDueTrigger : public Trigger
{
public:
    static constexpr int32 CHECK_INTERVAL_MS = 5000;

    PvpTalentSetDueTrigger(PlayerbotAI* botAI) : Trigger(botAI, "pvp talent set due", CHECK_INTERVAL_MS) {}

    bool IsActive() override;
};

#endif
