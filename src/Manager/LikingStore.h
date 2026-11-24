/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 *
 * The liking score of the random bots for human players (design decision 0033 point 14, docs/social/social.md): the store, the game
 * hooks that move the score and the questions the invite and guild-offer code asks. Everything is behind
 * AiPlayerbot.Social.Liking.Enable (default 0): with it off every function here returns at once and nothing is read from or
 * written to the database.
 *
 * Threads: the book is guarded by one lock, so the hooks may be called from the world thread, the map threads and the
 * session threads. The database is only touched from the world thread (the load queries and the flush, in Update) and
 * from the logout, shutdown, wipe and delete paths; nothing runs a query on a bot's tick. The pure memory side is
 * Utils/LikingBook.h; the decisions are Utils/LikingPolicy.h.
 *
 * Only pairs of a random bot and a human player count (LikingPolicy::PairEligible); never two bots, never a player's own
 * alt bot.
 */

#ifndef _PLAYERBOT_LIKINGSTORE_H
#define _PLAYERBOT_LIKINGSTORE_H

#include "Define.h"
#include "LikingPolicy.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

class Group;
class Player;

namespace LikingStore
{
    // The switch (AiPlayerbot.Social.Liking.Enable).
    bool Enabled();
    std::array<uint64_t, 4> TierCounts(uint32 now);

    // World thread, from RandomPlayerbotMgr::UpdateAIInternal next to BotGuilds::Update; throttles itself. Takes in the
    // loads that came back, runs the group-time and guild-time pass once a minute, flushes changed rows and runs the
    // daily retention cleanup.
    void Update();

    // A human logged in or out (PlayerbotsPlayerScript). Login asks for the human's rows; logout writes the changed ones.
    void OnHumanLogin(Player* player);
    void OnHumanLogout(Player* player);

    // The server stops: every changed row is written before the database closes.
    void Shutdown();

    // A character was deleted (PlayerScript::OnDelete): its rows go, as a human and as a bot.
    void OnPlayerDeleted(uint64 guid);

    // One event for a pair, if the pair is a random bot and a human (either order); ignored otherwise.
    void NoteEvent(Player* first, Player* second, LikingPolicy::Event event, uint32 steps = 1);

    // The run of this bot's group is finished: every human of the group counts it.
    void OnDungeonDone(Player* bot);

    // A trade between the two completed with something moved.
    void OnTraded(Player* first, Player* second);

    // A duel between the two was fought to its end (not interrupted).
    void OnDuelEnded(Player* winner, Player* loser);

    // A human's line: a whisper to a bot, a group line, or a line to the bot the human has targeted. A clear thanks or
    // insult to a random bot moves the score. Works with the AI layer off. `group` may be null.
    void OnHumanLine(Player* human, Player* whisperTarget, Group* group, std::string const& message);

    // A member joined or is about to be removed from a group (GroupScript): the join time starts the kick grace, a kick by
    // a human lowers the score of the bot.
    void OnGroupMemberAdded(Group* group, uint64 guid);
    void OnGroupMemberRemoved(Group* group, uint64 guid, uint8 method, uint64 kicker);

    // Percent points the score adds to the chance that this bot accepts this human's group invite (0 when off, for a pair
    // that is not eligible, or while the human's rows are on their way).
    int32 InviteDelta(Player* bot, Player* human);

    // The guild offer gate of BotGuilds::OfferToHumans: the leader likes the human enough, the persistent cooldown ran out
    // and the hour window has room. True takes the offer (starts the cooldown and counts it); call it only when the offer is
    // made right after. True as well when the switch is off, so the existing behaviour stays.
    bool MayOfferGuild(Player* leader, Player* human, uint32 cooldownSeconds);

    // The score (0 to 1, decayed to now) of this random bot for this human, for the sidecar request. Empty when the switch is
    // off, the pair is not a random bot and a human, or the human's rows have not been loaded yet (the request then leaves the
    // fields out and the sidecar uses its own score).
    std::optional<double> ValueOf(Player* bot, Player* human);

    // The shared-quest offer gate (docs/social/social.md section 12): the bot likes the human enough, did not offer them lately
    // and the human's hour window has room. True takes the offer (counts it and starts the pair cooldown); call it only
    // when the offer is made right after. False when the switch is off or the human's rows are not loaded.
    bool MayOfferQuestGroup(Player* bot, Player* human, uint32 pairCooldownSeconds);

    // The wipe command (GM console): ".playerbots social wipe <character name>|all". Returns the lines to print.
    std::vector<std::string> Console(std::string const& args);
}

#endif
