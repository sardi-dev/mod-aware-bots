/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_CONFIG_H
#define AWARE_AI_CONFIG_H

#include "Define.h"
#include <atomic>
#include <string>

// Settings for the optional AI layer (docs/decisions/0014 and 0026), read once at startup from the AwareAi.* keys of
// playerbots.conf. SkyFire's ConfigMgr::Reload() drops the module's extra config file, so there is no reload path:
// the GM commands change the live values until the next restart.
// Flags that bot AI will read from map threads are atomic; the strings and numbers are only touched on the world thread
// (this class, the runtime and the GM commands all run there).
class AwareAiConfig
{
public:
    static AwareAiConfig& Instance();

    void Load();

    std::atomic<bool> Enabled{false};
    std::atomic<bool> PvPEnabled{true};
    std::atomic<bool> EventsLogEnabled{true};
    // Shown by the status command now; the PvP decisions of the next phase wait this long before the rules decide.
    std::atomic<uint32> DecisionDeadlineMs{600};

    // Battleground strategy asks (design decision 0032 point 9, src/AwareAi/AiPvpDecision.h): the leader of a side asks every
    // PvpBgIntervalSeconds (15 to 30) and after a flag or node event; an answer below PvpMinConfidencePct is not used; one
    // side asks at most PvpMaxRequestsPerMatchSide times in a match. Read by bots on map threads, hence atomic.
    std::atomic<uint32> PvpBgIntervalSeconds{20};
    std::atomic<uint32> PvpMinConfidencePct{60};
    std::atomic<uint32> PvpMaxRequestsPerMatchSide{60};

    // Player-to-player chat (design decision 0029, src/AwareAi/AiChat.h). Read by bots on map threads, hence atomic.
    std::atomic<bool> ChatEnabled{false};
    // Send the liking score and its band word with chat requests (design decision 0033 point 14). Off until the sidecar accepts the
    // two fields (its schema forbids unknown ones); AiChat::Send reads it (FillLiking).
    std::atomic<bool> ChatSendLiking{false};
    // Tell the sidecar (deny_being_bot) that the bots keep up the pretence and deny being bots when asked. Off: the prompt
    // lets a bot admit it. AiChat::Send reads it.
    std::atomic<bool> ChatDenyBeingBot{false};
    std::atomic<uint32> ChatDeadlineMs{1500};
    std::atomic<uint32> ChatNearbyChancePct{25};
    std::atomic<uint32> ChatPartyChancePct{30};
    std::atomic<uint32> ChatGuildWindowSeconds{60};
    std::atomic<uint32> ChatGuildBots{1};
    // Reply timing (design decision 0031 item 11).
    std::atomic<uint32> ChatLateChancePct{7};
    std::atomic<uint32> ChatCombatWaitPct{60};
    std::atomic<uint32> ChatCombatSkipPct{20};
    // Openers: a bot speaks first at a small moment (design decision 0031 item 3).
    std::atomic<bool> ChatOpenersEnabled{true};
    std::atomic<uint32> ChatOpenerCooldownSeconds{120};
    std::atomic<uint32> ChatOpenerGreetChancePct{60};
    std::atomic<uint32> ChatOpenerLevelChancePct{50};
    std::atomic<uint32> ChatOpenerDuelAskChancePct{5};
    std::atomic<uint32> ChatOpenerDuelAskCheckSeconds{120};
    // Friendly whispers: a bot that knows a human whispers first (design decision 0031 item 14).
    std::atomic<bool> ChatFriendWhisperEnabled{true};
    std::atomic<uint32> ChatFriendWhisperMaxPerSession{2};
    std::atomic<uint32> ChatFriendWhisperCheckSeconds{300};
    std::atomic<uint32> ChatFriendWhisperChancePct{15};
    // Battleground chat: the end-of-match line of one or two bots per team (design decision 0029, phase 2).
    std::atomic<bool> ChatBattlegroundEnabled{true};
    std::atomic<uint32> ChatBattlegroundSpeakers{2};

    std::string SidecarUrl;
    double BudgetMonthlyUsd = 20.0;
    // 0 means no daily cap (design decision 0031: one monthly pool for decisions and chat).
    double BudgetDailyUsd = 0.0;

private:
    AwareAiConfig() = default;
};

#define sAwareAiConfig AwareAiConfig::Instance()

#endif
