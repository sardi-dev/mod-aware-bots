/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The chat layer of the random bots: one place that decides whether a bot speaks (a stable personality per bot, a roll
 * per moment, a per-bot and a group-wide cooldown) and what it says (the bot text table, with a variant per
 * personality and a default in code). Every line a random bot says on its own in a group goes through here, so a group
 * of bots never floods the chat.
 *
 * The stable guid-derived personality is the idea of the personality store in the author's earlier private bot module (GPL-2.0)
 * (GPL-2.0), with its SplitMix64 mixing; the traits and the lines are this module's own.
 *
 * Lines: key "<name>_<personality>" is tried first (quiet, friendly, joker, grumpy), then "<name>", then the default
 * given in code. Nothing here runs on a tick where nothing happens: it is called at moments only.
 *
 * Config: AiPlayerbot.Voice.Enable, .CooldownSeconds, .GroupCooldownSeconds, .JoinChance, .PullChance, .BossChance,
 * .WipeChance, .LeaveChance (see conf/playerbots.conf.dist).
 */

#ifndef _PLAYERBOT_BOTVOICE_H
#define _PLAYERBOT_BOTVOICE_H

#include "Define.h"

#include <map>
#include <string>

class Player;
class PlayerbotAI;

namespace BotVoice
{
    enum class Personality : uint8
    {
        QUIET,
        FRIENDLY,
        JOKER,
        GRUMPY
    };

    // Stable for a character: derived from its guid, nothing is stored. Random bots and a human's alt bots have their
    // own shares (AiChatPolicy::PersonalityIndex).
    Personality PersonalityOf(Player* bot);
    char const* PersonalityName(Personality personality);

    enum class Channel
    {
        PARTY,    // the bot's group
        WHISPER,  // to whisperTo
        SAY,
        LFG,      // the LookingForGroup channel
        GENERAL,  // the General channel of the bot's zone
        GUILD,    // the bot's guild chat
        BATTLEGROUND  // the battleground group (instance chat: the 5.4.8 client shows it as battleground chat)
    };

    enum class Priority
    {
        REPLY,    // an answer to someone: always said, no roll, no cooldown
        NOTICE,   // information the group needs (where the bot is): always said, counts for the cooldowns
        CHATTER   // small talk: rolled, and held back by the per-bot and the group cooldown
    };

    enum class Moment
    {
        JOIN_GROUP,
        BEFORE_PULL,
        BOSS_KILLED,
        WIPE,
        LEAVE_GROUP
    };

    // AiPlayerbot.Voice.Enable. Off: replies and notices go out as plain default lines, chatter stays silent.
    bool Enabled();

    // A moment of a group's life. False when the bot stays quiet (not a random bot with a player in its group, a failed
    // roll, a cooldown) or the line could not be sent.
    bool Say(PlayerbotAI* botAI, Moment moment);

    // Any line by text table key, for other features (the Dungeon Finder fill, travel, invites).
    // chancePct only counts for Priority::CHATTER, after the personality's chattiness. whisperTo only for WHISPER.
    // blendPools: a bot with a personality variant of the line uses it in 6 of 10 cases and the plain pool otherwise, so a
    // bot has the whole pool to draw from and not only its personality's few lines.
    bool Say(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
             std::map<std::string, std::string> const& placeholders = {}, Channel channel = Channel::PARTY,
             Priority priority = Priority::CHATTER, uint32 chancePct = 60, Player* whisperTo = nullptr,
             bool blendPools = false);

    // Say() for a CHATTER line a bot shares with a human of its group (a grats on their roll win): when the voice
    // speaks, its words may come from the AI layer (an opener, AiChat::OnOpener described by aiMoment), with the text
    // line as the fallback.
    bool SayToHuman(PlayerbotAI* botAI, std::string const& key, std::string const& fallback,
                    std::map<std::string, std::string> const& placeholders, Player* human, std::string const& aiMoment,
                    uint32 chancePct);

    // A line straight to the bot's battleground team (instance chat); false outside a battleground group.
    bool SayToBattleground(Player* bot, std::string const& text);

    // How talkative the bot's personality is, in percent (quiet 40, friendly 100, joker 120, grumpy 80). Callers that pick
    // one speaker out of several weigh the choice with it.
    uint32 ChattinessPercent(Player* bot);

    // A bot that releases its spirit after the whole group died notes it; the bot says its wipe line once it is alive
    // and out of combat again (VoiceWipeAction).
    void NoteDeath(Player* bot);
    bool HasPendingWipe(Player* bot);
    // Takes the note and speaks if it is fresh; true when a line was said.
    bool SayAfterWipe(PlayerbotAI* botAI);
}

#endif
