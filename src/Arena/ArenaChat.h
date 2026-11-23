/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_ARENACHAT_H
#define _PLAYERBOT_ARENACHAT_H

// BT-154: what the bots say about the ladder, in the General channel of the speaker's zone, through the bot voice
// (Mgr/Text/BotVoice.h). World thread only, with no ladder lock held. The speaker is an online member of the team the
// line is about; the lines are rows of ai_playerbot_texts keyed arn_<moment>[_<personality>], with a default in code.

#include "ArenaChatMath.h"
#include "ArenaState.h"

namespace ArenaLadder
{
namespace Chat
{
// A new team has been founded.
void OnFormed(Team const& team, int64 utcNow);
// `winner` won a match; says its notable moment (a streak, a climb, a rivalry), if any.
void OnWon(Team const& winner, Team const& loser, ArenaChatMath::ResultFacts const& facts, int64 utcNow);
// `winner` ended the streak of `loser`.
void OnStreakBroken(Team const& winner, Team const& loser, int32 loserStreak, int64 utcNow);
}
}  // namespace ArenaLadder

#endif
