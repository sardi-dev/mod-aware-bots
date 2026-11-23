/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_PROTOCOL_H
#define AWARE_AI_PROTOCOL_H

#include "Define.h"
#include <optional>
#include <string>
#include <utility>
#include <vector>

// The sidecar API v1 bodies the module uses (PUT /v1/settings, GET /v1/status, POST /v1/chat, POST /v1/decide, POST
// /v1/outcome), kept free of game-core types so they can be unit-tested without the core. The contract is services/aware-ai/schemas/v1/.

// What worldserver pushes: the sidecar clamps the budget to its own environment ceiling.
struct AwareAiSettings
{
    bool Enabled = false;
    bool PvP = true;
    // The sidecar's chat feature is off unless worldserver sends it on (AwareAi.Chat.Enable).
    bool Chat = false;
    double MonthlyUsd = 20.0;
    // 0 means no daily cap (design decision 0031: one monthly pool), sent as null.
    double DailyUsd = 0.0;
};

// What the sidecar last reported in GET /v1/status.
struct AwareAiStatus
{
    bool Reachable = false;
    std::string Version;
    // off, fake or live.
    std::string Mode;
    bool Enabled = false;
    // Only true in live mode with a key present.
    bool HasApiKey = false;
    // The sidecar's provider-side breaker, not the client link.
    std::string Breaker;
    uint32 InFlight = 0;
    double MonthSpentUsd = 0.0;
    double MonthCapUsd = 0.0;
    double DaySpentUsd = 0.0;
    double DayCapUsd = 0.0;
    double CeilingUsd = 0.0;
    bool CapClamped = false;
    bool Warning = false;
    bool Exhausted = false;
};

// One side of a chat request: game context only (character name, class and race tokens, level), never an account.
struct AwareAiChatCharacter
{
    std::string Name;
    std::string ClassName;
    std::string Race;
    uint32 Level = 1;
};

// POST /v1/chat. Kind, channel, class and race are the sidecar's lower-case identifiers (AiChatPolicy.h names them).
struct AwareAiChatRequest
{
    uint64 BotGuid = 0;
    uint64 PlayerGuid = 0;
    AwareAiChatCharacter Bot;
    AwareAiChatCharacter Player;
    uint32 Aggression = 50;
    uint32 Sociability = 50;
    uint32 Chattiness = 50;
    uint32 Honor = 50;
    bool SameFaction = true;
    std::string Zone;
    std::string Kind;
    std::string Channel;
    // The human's line, or a short description of the moment when there is none.
    std::string Message;
    uint32 DeadlineMs = 1500;
    // Only for guild_reply: how long ago the human wrote, in seconds.
    std::optional<double> GuildHumanMessageAgeS;
    // Sent (as true) only while the human is in a conversation with the bot: their last line to it is under 2 minutes
    // old (design decision 0031 item 12). Message may then hold several of their lines joined by " / ".
    bool Conversation = false;
    // The module's liking score of this bot for this human in whole percent (0 to 100) and its band word (cold, stranger,
    // friendly or close; LikingPolicy::BandName), sent only while AwareAi.Chat.SendLiking is on and the pair is known (design
    // decision 0033 point 14). The body carries the percent as 0 to 1, which is the sidecar's `liking`. A band word that is not
    // one of the four is left out, so the sidecar never refuses the body.
    std::optional<uint32> LikingPercent;
    std::string Relation;
    // Sent (as true) only while AwareAi.Chat.DenyBeingBot is on: the bot then denies being a bot when asked. Absent, the
    // sidecar lets it admit it.
    bool DenyBeingBot = false;
};

// What came back: Source is "llm" or "fake" with a reply, "rules_hint" or "error" with a reason.
struct AwareAiChatResponse
{
    std::string Source;
    std::string Reason;
    std::string Reply;
};

// POST /v1/memory/event: a moment a bot shared with a human, for the sidecar's per-pair memory and relation (design
// decision 0031 items 5 and 6). Event is one of grouped, dungeon_done, boss_killed_together, wipe_together, duel_won (the bot won),
// duel_lost (the bot lost), loot_ninja, loot_passed, helped, insulted, thanked; Detail is optional (64 characters at
// most, the dungeon's name for dungeon_done); Outcome is the older fight outcome, optional when Event is given.
struct AwareAiMemoryEvent
{
    uint64 BotGuid = 0;
    uint64 PlayerGuid = 0;
    std::string Event;
    std::string Detail;
    std::string Outcome;
};

// POST /v1/decide for a battleground strategy question (decision type bg_strategy, design decision 0032 point 9). The leader of a
// side asks which of a few candidates the team should play. The candidates travel as FIXED slots: the sidecar's template
// knows option_0 to option_5 only, and the state names what each slot means for this request, so a cached answer can never
// point at another set of candidates and no GUID or free coordinate is ever an option. Game context only: no names, no account.
// POST /v1/memory/wipe: the sidecar forgets one human (their guid) or every human (no guid). The answer holds the counts of
// what it removed.
struct AwareAiWipe
{
    std::optional<uint64> PlayerGuid;
};

struct AwareAiWipeResult
{
    uint32 MemoryEvents = 0;
    uint32 ChatTurns = 0;
    uint32 SocialEvents = 0;
    uint32 PairNotes = 0;
    uint32 LogLines = 0;
};

struct AwareAiBgState
{
    // "wsg", "tp" or "ab".
    std::string Map;
    // What the slots choose between: stance, carrier_lane, floater, wipe_reply, target_node.
    std::string Decision;
    // One line that says it in words for the model.
    std::string Question;
    uint32 ScoreOurs = 0;
    uint32 ScoreTheirs = 0;
    // The score that ends the match (captures or resources); 0 leaves it out.
    uint32 ScoreLimit = 0;
    // Negative leaves it out.
    int32 TimeLeftS = -1;
    // What the team plays now.
    std::string Stance;
    // Flags or nodes as "name:state" in the map's fixed order.
    std::vector<std::string> Nodes;
    // Our groups as "place:count".
    std::vector<std::string> FriendlyGroups;
    // What call-outs and scouts reported, as "place:count".
    std::vector<std::string> ObservedPressure;
    // Short "key:value" facts of the decision (alive counts, flag carrier class, minutes of a stalemate).
    std::vector<std::string> Facts;
    // Slot name -> what it means ("option_0" -> "balanced"), best rule score first. The slot names are the request's options.
    std::vector<std::pair<std::string, std::string>> Candidates;
};

struct AwareAiDecideRequest
{
    // The sidecar's template: bg_strategy now; arena_kill_target and arena_cc_target are the later arena asks, which use the
    // same envelope (fixed slots, a local slot-to-GUID table kept on the map thread).
    std::string DecisionType = "bg_strategy";
    uint64 BotGuid = 0;
    uint32 StateVersion = 0;
    uint32 DeadlineMs = 600;
    AwareAiBgState State;
};

// What came back from /v1/decide. A decision has a Choice and a Source (the decision model, cache or fake); rules_hint or error
// come with a Reason and no Choice.
struct AwareAiDecideResponse
{
    std::string DecisionId;
    std::string Source;
    std::string Reason;
    std::string Choice;
    std::string Template;
    // Negative when the response had none.
    double Confidence = -1.0;
    uint32 LatencyMs = 0;
    double CostUsd = 0.0;
};

// POST /v1/outcome: what became of a decision. Outcome is a lower-case identifier; Details are short strings.
struct AwareAiOutcome
{
    std::string DecisionId;
    std::string Outcome;
    std::vector<std::pair<std::string, std::string>> Details;
};

std::string BuildSettingsBody(AwareAiSettings const& settings);
// Leaves out the empty optional fields and cuts the detail to 64 bytes at a UTF-8 character boundary.
std::string BuildMemoryEventBody(AwareAiMemoryEvent const& event);
// Clamps every field to the schema's limits (name 24, zone 64 and message 800 characters, level 1-255, traits 0-100,
// deadline 500-20000 ms), cutting text at a UTF-8 character boundary, so the sidecar never refuses the body.
std::string BuildChatBody(AwareAiChatRequest const& request);
std::string BuildWipeBody(AwareAiWipe const& wipe);
// Empty when the body is not a JSON object.
std::optional<AwareAiWipeResult> ParseWipeBody(std::string const& body);
// Empty (no request to send) when fewer than two slots are offered. Clamps the deadline to the schema's 50-5000 ms, cuts
// every text at a UTF-8 character boundary and keeps the lists short, so the sidecar never refuses the body.
std::string BuildDecideBody(AwareAiDecideRequest const& request);
// The slot names of a request, in order: the options of the body.
std::vector<std::string> DecideOptions(AwareAiDecideRequest const& request);
// Empty when the body is not a JSON object.
std::optional<AwareAiDecideResponse> ParseDecideBody(std::string const& body);
// Empty when the decision id is missing. The outcome is made a valid identifier (lower case, digits, underscore).
std::string BuildOutcomeBody(AwareAiOutcome const& outcome);
// Empty when the body is not a JSON object.
std::optional<AwareAiChatResponse> ParseChatBody(std::string const& body);
// Empty when the body is not a JSON object; Reachable is left for the caller to set.
std::optional<AwareAiStatus> ParseStatusBody(std::string const& body);
// The day's spend for the status and budget lines: "$0.0123 of $2.00 today", or "$0.0123 today, no daily cap" when
// the sidecar has no daily cap (day_cap_usd null or 0 since design decision 0031). spentDecimals: digits of the spend.
std::string DaySpendText(double spentUsd, double capUsd, int spentDecimals);

#endif
