/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AwareAiConfig.h"
#include "Config.h"
#include "Log.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr uint32 MIN_DEADLINE_MS = 100;
    constexpr uint32 MAX_DEADLINE_MS = 5000;
    constexpr uint32 DEFAULT_DEADLINE_MS = 600;
    // The sidecar's schema accepts caps up to this; its environment ceiling clamps further.
    constexpr double MAX_BUDGET_USD = 10000.0;
    constexpr double DEFAULT_MONTHLY_USD = 20.0;
    constexpr char const* DEFAULT_SIDECAR_URL = "http://aware-ai:8090";

    // The sidecar accepts chat deadlines from 500 ms; more than 5 s and the moment is gone.
    constexpr uint32 MIN_CHAT_DEADLINE_MS = 500;
    constexpr uint32 MAX_CHAT_DEADLINE_MS = 5000;
    constexpr uint32 DEFAULT_CHAT_DEADLINE_MS = 2500;
    constexpr uint32 DEFAULT_NEARBY_CHANCE_PCT = 25;
    constexpr uint32 DEFAULT_PARTY_CHANCE_PCT = 30;
    constexpr uint32 MAX_PERCENT = 100;
    // The sidecar refuses guild replies to a human line older than its AWARE_AI_GUILD_RECENT_S (60 by default).
    constexpr uint32 MIN_GUILD_WINDOW_S = 5;
    constexpr uint32 MAX_GUILD_WINDOW_S = 60;
    constexpr uint32 DEFAULT_GUILD_WINDOW_S = 60;
    constexpr uint32 MIN_GUILD_BOTS = 1;
    constexpr uint32 MAX_GUILD_BOTS = 2;
    constexpr uint32 DEFAULT_LATE_CHANCE_PCT = 7;
    constexpr uint32 DEFAULT_COMBAT_WAIT_PCT = 60;
    constexpr uint32 DEFAULT_COMBAT_SKIP_PCT = 20;
    constexpr uint32 DEFAULT_OPENER_COOLDOWN_S = 120;
    constexpr uint32 MIN_OPENER_COOLDOWN_S = 10;
    constexpr uint32 MAX_OPENER_COOLDOWN_S = 3600;
    constexpr uint32 DEFAULT_GREET_CHANCE_PCT = 60;
    constexpr uint32 DEFAULT_LEVEL_CHANCE_PCT = 50;
    constexpr uint32 DEFAULT_DUEL_ASK_CHANCE_PCT = 5;
    constexpr uint32 DEFAULT_DUEL_ASK_CHECK_S = 120;
    constexpr uint32 MAX_FRIEND_WHISPERS = 2;
    constexpr uint32 DEFAULT_FRIEND_CHECK_S = 300;
    constexpr uint32 DEFAULT_FRIEND_CHANCE_PCT = 15;
    constexpr uint32 MIN_BG_SPEAKERS = 1;
    constexpr uint32 MAX_BG_SPEAKERS = 2;
    constexpr uint32 MIN_CHECK_S = 30;
    constexpr uint32 MAX_CHECK_S = 3600;

    // The leader's ask interval (design: 15 to 30 s), the confidence an answer needs, and the cap of asks per side and match.
    constexpr uint32 MIN_BG_INTERVAL_S = 15;
    constexpr uint32 MAX_BG_INTERVAL_S = 30;
    constexpr uint32 DEFAULT_BG_INTERVAL_S = 20;
    constexpr double DEFAULT_MIN_CONFIDENCE = 0.6;
    constexpr uint32 DEFAULT_MAX_REQUESTS = 60;
    constexpr uint32 MAX_REQUESTS_CAP = 600;

    uint32 ReadInRange(char const* key, uint32 fallback, uint32 low, uint32 high)
    {
        int const value = sConfigMgr->GetIntDefault(key, int(fallback));
        if (value >= int(low) && value <= int(high))
            return uint32(value);

        SF_LOG_WARN("playerbots.ai", "%s %d is outside %u-%u, using %u.", key, value, low, high, fallback);
        return fallback;
    }
}

AwareAiConfig& AwareAiConfig::Instance()
{
    static AwareAiConfig instance;
    return instance;
}

void AwareAiConfig::Load()
{
    Enabled = sConfigMgr->GetBoolDefault("AwareAi.Enable", false);
    PvPEnabled = sConfigMgr->GetBoolDefault("AwareAi.Pvp.Enable", true);
    EventsLogEnabled = sConfigMgr->GetBoolDefault("AwareAi.Events.Enable", true);
    SidecarUrl = sConfigMgr->GetStringDefault("AwareAi.SidecarUrl", DEFAULT_SIDECAR_URL);
    BudgetMonthlyUsd = sConfigMgr->GetFloatDefault("AwareAi.Budget.MonthlyUsd", float(DEFAULT_MONTHLY_USD));
    BudgetDailyUsd = sConfigMgr->GetFloatDefault("AwareAi.Budget.DailyUsd", 0.0f);

    int const deadline = sConfigMgr->GetIntDefault("AwareAi.DecisionDeadlineMs", int(DEFAULT_DEADLINE_MS));
    if (deadline < int(MIN_DEADLINE_MS) || deadline > int(MAX_DEADLINE_MS))
    {
        SF_LOG_WARN("playerbots.ai", "AwareAi.DecisionDeadlineMs %d is outside %u-%u, using %u.", deadline,
            MIN_DEADLINE_MS, MAX_DEADLINE_MS, DEFAULT_DEADLINE_MS);
        DecisionDeadlineMs = DEFAULT_DEADLINE_MS;
    }
    else
        DecisionDeadlineMs = uint32(deadline);

    // Out of range values are clamped, not replaced: 5 s means the shortest allowed interval, not the default.
    int const interval = sConfigMgr->GetIntDefault("AwareAi.Pvp.BgIntervalSeconds", int(DEFAULT_BG_INTERVAL_S));
    PvpBgIntervalSeconds = uint32(std::clamp(interval, int(MIN_BG_INTERVAL_S), int(MAX_BG_INTERVAL_S)));
    if (interval < int(MIN_BG_INTERVAL_S) || interval > int(MAX_BG_INTERVAL_S))
        SF_LOG_WARN("playerbots.ai", "AwareAi.Pvp.BgIntervalSeconds %d is outside %u-%u, using %u.", interval,
            MIN_BG_INTERVAL_S, MAX_BG_INTERVAL_S, PvpBgIntervalSeconds.load());

    double const confidence = sConfigMgr->GetFloatDefault("AwareAi.Pvp.MinConfidence", float(DEFAULT_MIN_CONFIDENCE));
    if (confidence < 0.0 || confidence > 1.0)
        SF_LOG_WARN("playerbots.ai", "AwareAi.Pvp.MinConfidence %.2f is outside 0-1, clamped.", confidence);
    PvpMinConfidencePct = uint32(std::lround(std::clamp(confidence, 0.0, 1.0) * double(MAX_PERCENT)));
    PvpMaxRequestsPerMatchSide = ReadInRange("AwareAi.Pvp.MaxRequestsPerMatchSide", DEFAULT_MAX_REQUESTS, 0,
        MAX_REQUESTS_CAP);

    ChatEnabled = sConfigMgr->GetBoolDefault("AwareAi.Chat.Enable", false);
    ChatSendLiking = sConfigMgr->GetBoolDefault("AwareAi.Chat.SendLiking", false);
    ChatDenyBeingBot = sConfigMgr->GetBoolDefault("AwareAi.Chat.DenyBeingBot", false);
    ChatDeadlineMs = ReadInRange("AwareAi.Chat.DeadlineMs", DEFAULT_CHAT_DEADLINE_MS, MIN_CHAT_DEADLINE_MS,
        MAX_CHAT_DEADLINE_MS);
    ChatNearbyChancePct = ReadInRange("AwareAi.Chat.NearbyChance", DEFAULT_NEARBY_CHANCE_PCT, 0, MAX_PERCENT);
    ChatPartyChancePct = ReadInRange("AwareAi.Chat.PartyChance", DEFAULT_PARTY_CHANCE_PCT, 0, MAX_PERCENT);
    ChatGuildWindowSeconds = ReadInRange("AwareAi.Chat.Guild.WindowSeconds", DEFAULT_GUILD_WINDOW_S, MIN_GUILD_WINDOW_S,
        MAX_GUILD_WINDOW_S);
    ChatGuildBots = ReadInRange("AwareAi.Chat.Guild.Bots", MIN_GUILD_BOTS, MIN_GUILD_BOTS, MAX_GUILD_BOTS);
    ChatLateChancePct = ReadInRange("AwareAi.Chat.LateChance", DEFAULT_LATE_CHANCE_PCT, 0, MAX_PERCENT);
    ChatCombatWaitPct = ReadInRange("AwareAi.Chat.CombatWaitChance", DEFAULT_COMBAT_WAIT_PCT, 0, MAX_PERCENT);
    ChatCombatSkipPct = ReadInRange("AwareAi.Chat.CombatSkipChance", DEFAULT_COMBAT_SKIP_PCT, 0, MAX_PERCENT);
    ChatOpenersEnabled = sConfigMgr->GetBoolDefault("AwareAi.Chat.Opener.Enable", true);
    ChatOpenerCooldownSeconds = ReadInRange("AwareAi.Chat.Opener.CooldownSeconds", DEFAULT_OPENER_COOLDOWN_S,
        MIN_OPENER_COOLDOWN_S, MAX_OPENER_COOLDOWN_S);
    ChatOpenerGreetChancePct = ReadInRange("AwareAi.Chat.Opener.GreetChance", DEFAULT_GREET_CHANCE_PCT, 0, MAX_PERCENT);
    ChatOpenerLevelChancePct = ReadInRange("AwareAi.Chat.Opener.LevelChance", DEFAULT_LEVEL_CHANCE_PCT, 0, MAX_PERCENT);
    ChatOpenerDuelAskChancePct = ReadInRange("AwareAi.Chat.Opener.DuelAskChance", DEFAULT_DUEL_ASK_CHANCE_PCT, 0,
        MAX_PERCENT);
    ChatOpenerDuelAskCheckSeconds = ReadInRange("AwareAi.Chat.Opener.DuelAskCheckSeconds", DEFAULT_DUEL_ASK_CHECK_S,
        MIN_CHECK_S, MAX_CHECK_S);
    ChatFriendWhisperEnabled = sConfigMgr->GetBoolDefault("AwareAi.Chat.FriendWhisper.Enable", true);
    ChatFriendWhisperMaxPerSession = ReadInRange("AwareAi.Chat.FriendWhisper.MaxPerSession", MAX_FRIEND_WHISPERS, 0,
        MAX_FRIEND_WHISPERS);
    ChatFriendWhisperCheckSeconds = ReadInRange("AwareAi.Chat.FriendWhisper.CheckSeconds", DEFAULT_FRIEND_CHECK_S,
        MIN_CHECK_S, MAX_CHECK_S);
    ChatFriendWhisperChancePct = ReadInRange("AwareAi.Chat.FriendWhisper.Chance", DEFAULT_FRIEND_CHANCE_PCT, 0,
        MAX_PERCENT);
    ChatBattlegroundEnabled = sConfigMgr->GetBoolDefault("AwareAi.Chat.Battleground.Enable", true);
    ChatBattlegroundSpeakers = ReadInRange("AwareAi.Chat.Battleground.Speakers", MAX_BG_SPEAKERS, MIN_BG_SPEAKERS,
        MAX_BG_SPEAKERS);

    if (BudgetMonthlyUsd < 0.0 || BudgetDailyUsd < 0.0 || BudgetMonthlyUsd > MAX_BUDGET_USD ||
        BudgetDailyUsd > MAX_BUDGET_USD)
    {
        SF_LOG_WARN("playerbots.ai", "AwareAi.Budget values must be between 0 and %.0f, using $%.0f a month and the "
            "no daily cap.", MAX_BUDGET_USD, DEFAULT_MONTHLY_USD);
        BudgetMonthlyUsd = DEFAULT_MONTHLY_USD;
        BudgetDailyUsd = 0.0;
    }
}
