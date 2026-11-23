/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_CHAT_H
#define AWARE_AI_CHAT_H

#include "Define.h"
#include <optional>
#include <string>

class Creature;
class Group;
class Player;
class PlayerbotAI;
struct ItemTemplate;
struct Roll;

// Player-to-player chat through the AI layer (design decision 0029, phase 1; AwareAi.Chat.* in playerbots.conf). A bot asks
// the sidecar for its line in seven moments with a human: a duel challenge, a duel won or lost, a whisper, a /say line
// near it, its party or raid (a human's line or a bot voice moment) and its guild chat; phase 2 adds its team's
// battleground chat at the end of a match with a human on the team. Never for a bot's line: bots do not talk to each
// other through the API.
//
// The queue: a hook (world or map thread) files a ticket under the bot's guid and sends the request; the HTTP answer
// arrives on the client's IO thread, which only stores it in the ticket under a lock. The bot's own update (Deliver,
// at the top of PlayerbotAI::UpdateAI, so a resting bot still answers on time) takes its tickets once the answer is in
// (or the deadline passed) and the reading and typing delay is over, checks that the human is still there (online for
// a whisper, in say range, still in the group, the guild line still fresh) and says the line, or drops it. One open
// ticket per bot. Every ticket writes one "chat" event to the events log: kind, source (ai, fake, rules, template,
// canned), reason.
//
// Conversation mode (design decision 0031 item 12): a human's line waits a short reading time before its request goes out, and
// more lines from that human to that bot in the meantime join the same request (" / "); lines that come after it went
// out wait as follow-ups and get one more answer, together, once the first is said. Requests say "conversation" while
// the human's last line to the bot is under 2 minutes old.
//
// Design decision 0031 adds the free layers around the sidecar: a trivial human line ("hi", "ty", "gg") is answered from the
// template layer (AiChatLines.h) without a request, and when the sidecar has no line (timeout, error, a rate limit,
// the budget, the link down) a moment without a rule line gets a short canned line of the bot's archetype.
//
// With the layer or the chat switch off, every hook returns false at once and the bots behave as before.
namespace AiChat
{
    // A human challenged the bot and it answered (AcceptDuelAction).
    void OnDuelAnswered(Player* bot, Player* challenger, bool accepted);

    // The core's duel end: decided is false for an interrupted duel, fled when the loser ran out of the duel area.
    void OnDuelEnd(Player* winner, Player* loser, bool decided, bool fled);

    // A whisper to a bot. True when it is small talk the layer took: the caller then skips the command handling. A line
    // the module reads as a command returns false and is handled as before, without a chat reply.
    bool OnWhisper(Player* human, Player* bot, std::string const& message);

    // A human's /say line (Player::Say): one nearby bot may answer.
    void OnSay(Player* human, std::string const& message);

    // A human's party or raid line that no rule reply took: one bot of the group may answer.
    void OnGroupChat(Group* group, Player* human, uint32 chatType, std::string const& message);

    // A bot voice moment that passed its own roll and cooldowns. True when the layer took it; it says fallback itself
    // when the sidecar has no line. False: the caller says fallback now.
    bool OnPartyMoment(PlayerbotAI* botAI, std::string const& moment, std::string const& fallback);

    // A human's guild line: one or two online bots of that guild may answer.
    void OnGuildChat(Player* human, std::string const& message);

    // The bot's own update: says the lines that are due. One atomic load when no ticket is open anywhere.
    void Deliver(PlayerbotAI* botAI);

    // Memory events (design decision 0031 items 5 and 6): moments a bot shares with a human go to the sidecar's per-pair
    // memory, fire and forget, only while the layer and chat are on and the link is up. Duels, thanks and insults are
    // sent from the chat hooks above; these are the other moments.
    // GroupScript::OnAddMember: a human joined bots, or a bot joined humans ("grouped").
    void OnGroupMemberAdded(Group* group, uint64 guid);
    // PlayerScript::OnCreatureKill: a dungeon or world boss died to a group with humans and bots in it.
    void OnBossKilled(Player* killer, Creature* boss);
    // A bot released its spirit after its whole group died (BotVoice::NoteDeath).
    void OnWipe(Player* bot);
    // A bot saw its finder or party run finished (LfgMannersAction).
    void OnDungeonDone(Player* bot);
    // A bot voted on a group roll: wantedNeed when it would need the item, needs when it finally rolled Need (a human
    // who passed on it while able to use it is noted for "loot_passed").
    void NoteLootRoll(Player* bot, Roll const* roll, ItemTemplate const* proto, bool wantedNeed, bool needs);
    // Openers (design decision 0031 item 3): small moments where a bot speaks first, rare and rule-gated (one per human per
    // AwareAi.Chat.Opener.CooldownSeconds). The greeting when a human joins bots comes from OnGroupMemberAdded.
    // A bot voice line shared with a human of the group (a rare or epic roll win): true when the layer took it; it says
    // fallback itself when the sidecar has no line. False: the caller says fallback now.
    bool OnOpener(PlayerbotAI* botAI, Player* human, std::string const& moment, std::string const& fallback);
    // PlayerScript::OnLevelChanged: a bot of the group near, or else a bot standing near, may say gz.
    void OnLevelUp(Player* player, uint8 oldLevel);
    // A human's own update (map thread): the rare duel ask by a rough or chatty bot near an idle human, and the
    // friendly whisper timer (design decision 0031 item 14): now and then a bot the human grouped or duelled with since the
    // server started whispers them first (kind friend_whisper), at most 1 to 2 times per session, never in combat or an
    // instance; the sidecar may refuse it by relation, and a refusal or failure says nothing.
    void OnHumanUpdate(Player* player, uint32 diff);
    // A human's play session starts and ends here (the friendly whispers count per session).
    void OnHumanLogin(Player* player);
    void OnHumanLogout(Player* player);

    // Battleground chat (phase 2). Called where a bot leaves its ended match on its own (BGTactics, BGStatusAction): the
    // first bot of each team to get here files one or two end-of-match lines of the team (bg_win or bg_loss, only with
    // a human of that team in the match, never in an arena or after a draw). True while this bot still has its line to
    // say: it stays in the match until then, at most AiChatPolicy::BG_END_HOLD_MS, and leaves afterwards.
    bool HoldForMatchEnd(Player* bot);

    // Observe flag scores on the bot's battleground map update.
    void OnBattlegroundUpdate(Player* bot);

    // The GM wipe command (".playerbots social wipe"): asks the sidecar to delete what it keeps about one human (their full
    // guid) or about every human (no guid), fire and forget, the counts go to the server log ([AIWIPE]). Works with the chat
    // switch off, needs only AwareAi.Enable. Returns the line the console prints: what was asked, or why nothing was (layer
    // off, link down, client busy) so the operator can run it again.
    std::string WipeSidecar(std::optional<uint64> playerGuid);

    // SMSG_LOOT_ROLL_WON as a bot reads it: the bot won what a human passed ("loot_passed"), or a human won with Need a
    // lighter armor than their own that the bot wanted ("loot_ninja").
    void OnLootRollWon(Player* bot, uint64 winnerGuid, uint32 itemId, uint8 rollType);
}

#endif
