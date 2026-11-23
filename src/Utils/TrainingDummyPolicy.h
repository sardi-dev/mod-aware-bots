/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef MOD_AWARE_BOTS_TRAINING_DUMMY_POLICY_H
#define MOD_AWARE_BOTS_TRAINING_DUMMY_POLICY_H

#include <cstring>

// BT-163: training dummies never die, so a bot that picks one as a target fights it forever (live 2026-11-12 in
// Scarlet Monastery, which has seven of them along the way). A dummy is a target only when the bot's human leader has
// selected it, so a player can still train with bots on purpose.
namespace TrainingDummyPolicy
{
inline bool IsDummyScript(char const* scriptName)
{
    return scriptName && std::strstr(scriptName, "training_dummy") != nullptr;
}

inline bool Allowed(bool isDummy, bool leaderTargetsIt)
{
    return !isDummy || leaderTargetsIt;
}
}

#endif
