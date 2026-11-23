/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AwareAiRuntime.h"
#include "AwareAiClient.h"
#include "AwareAiConfig.h"
#include "Chat.h"
#include "Config.h"
#include "JsonFields.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include <boost/json.hpp>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace
{
    // Settings are re-sent periodically so a restarted sidecar picks them up without a worldserver restart.
    constexpr uint32 SYNC_INTERVAL_MS = 30000;
    constexpr Milliseconds SYNC_TIMEOUT = 2s;
    constexpr char const* EVENTS_PREFIX = "ai-events";
    constexpr char const* EVENTS_SUBFOLDER = "ai";

    int64 SteadyNowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    constexpr int USD_DECIMALS = 2;

    std::string Usd(double value)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "$%.*f", USD_DECIMALS, value);
        return buffer;
    }

    std::string UtcTimestamp()
    {
        std::time_t const now = std::time(nullptr);
        std::tm utc{};
        gmtime_r(&now, &utc);
        char buffer[32];
        std::size_t const length = std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
        return std::string(buffer, length);
    }
}

AwareAiRuntime& AwareAiRuntime::Instance()
{
    static AwareAiRuntime instance;
    return instance;
}

void AwareAiRuntime::Start()
{
    if (sAwareAiConfig.EventsLogEnabled)
    {
        // LogsDir is the same folder the core's own logs use; the events go in its "ai" subfolder.
        std::string const logsDir = sConfigMgr->GetStringDefault("LogsDir", "");
        _events.Start(std::filesystem::path(logsDir.empty() ? "." : logsDir) / EVENTS_SUBFOLDER, EVENTS_PREFIX);
    }

    if (!sAwareAiConfig.Enabled)
    {
        SF_LOG_INFO("playerbots.ai", "AI layer is off (AwareAi.Enable = 0); .bot ai on turns it on until restart.");
        return;
    }

    if (EnsureClientRunning())
        _syncTimerMs = SYNC_INTERVAL_MS;
}

void AwareAiRuntime::Stop()
{
    sAwareAiClient.Stop();
    _events.Stop();
}

bool AwareAiRuntime::EnsureClientRunning()
{
    return sAwareAiClient.IsRunning() || sAwareAiClient.Start(sAwareAiConfig.SidecarUrl);
}

void AwareAiRuntime::Update(uint32 diff)
{
    if (sAwareAiClient.TakeBreakerTripped())
    {
        SF_LOG_WARN("playerbots.ai", "Sidecar unreachable several times in a row; pausing AI requests for a while.");
        LogEvent("client_breaker", "open");
    }

    if (_statusChanged.exchange(false))
        ObserveStatus();

    if (!sAwareAiConfig.Enabled || !sAwareAiClient.IsRunning())
        return;

    _syncTimerMs += diff;
    if (_syncTimerMs < SYNC_INTERVAL_MS)
        return;

    _syncTimerMs = 0;
    PushSettings();
    PollStatus();
}

void AwareAiRuntime::SyncNow()
{
    if (!EnsureClientRunning())
        return;

    _syncTimerMs = 0;
    PushSettings();
    PollStatus();
}

void AwareAiRuntime::RefreshStatus()
{
    if (EnsureClientRunning())
        PollStatus();
}

void AwareAiRuntime::PushSettings()
{
    AwareAiSettings settings;
    settings.Enabled = sAwareAiConfig.Enabled;
    settings.PvP = sAwareAiConfig.PvPEnabled;
    settings.Chat = sAwareAiConfig.ChatEnabled;
    settings.MonthlyUsd = sAwareAiConfig.BudgetMonthlyUsd;
    settings.DailyUsd = sAwareAiConfig.BudgetDailyUsd;

    bool const sent = sAwareAiClient.Send(AwareAiHttpMethod::Put, "/v1/settings", BuildSettingsBody(settings),
        SYNC_TIMEOUT, [this](AwareAiResponse response)
        {
            if (response.Ok)
                return;

            // The status poll that follows decides reachability; a refused push only matters to the log.
            SF_LOG_DEBUG("playerbots.ai", "Settings push failed: %s", response.Error.c_str());
        });

    if (!sent)
        SF_LOG_DEBUG("playerbots.ai", "Settings push skipped: the sidecar client is stopped or paused.");
}

void AwareAiRuntime::PollStatus()
{
    bool const sent = sAwareAiClient.Send(AwareAiHttpMethod::Get, "/v1/status", "", SYNC_TIMEOUT,
        [this](AwareAiResponse response)
        {
            AwareAiStatus status;
            if (response.Ok)
            {
                if (std::optional<AwareAiStatus> parsed = ParseStatusBody(response.Body))
                {
                    status = std::move(*parsed);
                    status.Reachable = true;
                }
            }

            StoreStatus(std::move(status));
        });

    // A stopped or paused client cannot ask: the layer is unreachable as far as the world is concerned.
    if (!sent)
        StoreStatus(AwareAiStatus{});
}

void AwareAiRuntime::StoreStatus(AwareAiStatus status)
{
    {
        std::lock_guard<std::mutex> guard(_lock);
        _status = std::move(status);
        _statusStampMs = SteadyNowMs();
    }

    _statusChanged = true;
}

AwareAiStatus AwareAiRuntime::Status()
{
    std::lock_guard<std::mutex> guard(_lock);
    return _status;
}

int64 AwareAiRuntime::StatusAgeMs()
{
    std::lock_guard<std::mutex> guard(_lock);
    return _statusStampMs < 0 ? -1 : SteadyNowMs() - _statusStampMs;
}

void AwareAiRuntime::ObserveStatus()
{
    AwareAiStatus const status = Status();
    if (status.Reachable != _lastReachable || status.Mode != _lastMode)
    {
        if (status.Reachable)
        {
            SF_LOG_INFO("playerbots.ai", "Sidecar reachable: mode %s, API key %s, provider breaker %s.",
                status.Mode.c_str(), status.HasApiKey ? "set" : "not in use", status.Breaker.c_str());
            LogEvent("sidecar_up", "mode=" + status.Mode);
        }
        else if (_everReachable)
        {
            SF_LOG_WARN("playerbots.ai", "Sidecar is no longer reachable; bots use their own rules.");
            LogEvent("sidecar_down", "");
        }

        _lastReachable = status.Reachable;
        _lastMode = status.Mode;
    }

    _everReachable = _everReachable || status.Reachable;
    if (!status.Reachable)
        return;

    if (status.Exhausted && !_warnedExhausted)
        TellPlayers("AI layer: budget reached (" + Usd(status.MonthSpentUsd) + " of " + Usd(status.MonthCapUsd) +
            " this month, " + DaySpendText(status.DaySpentUsd, status.DayCapUsd, USD_DECIMALS) + "). Bots use their "
            "own rules until it resets.");
    else if (status.Warning && !status.Exhausted && !_warnedWarning)
        TellPlayers("AI layer: " + Usd(status.MonthSpentUsd) + " of the " + Usd(status.MonthCapUsd) +
            " monthly budget used.");

    if (status.Exhausted != _warnedExhausted || status.Warning != _warnedWarning)
        LogEvent("budget", status.Exhausted ? "exhausted" : (status.Warning ? "warning" : "ok"));

    _warnedExhausted = status.Exhausted;
    _warnedWarning = status.Warning;
}

void AwareAiRuntime::TellPlayers(std::string const& message)
{
    for (auto const& [accountId, session] : sWorld->GetAllSessions())
        if (session && session->GetPlayer())
            ChatHandler(session).SendSysMessage(message.c_str());
}

void AwareAiRuntime::LogEvent(std::string const& kind, std::string const& detail)
{
    if (!_events.IsRunning())
        return;

    boost::json::object line{ { "ts", UtcTimestamp() }, { "kind", kind }, { "detail", detail } };
    _events.Write(boost::json::serialize(line));
}

void AwareAiRuntime::LogChat(std::string const& chatKind, std::string const& source, std::string const& reason,
    std::string const& outcome, uint32 waitMs)
{
    // The writer's queue is thread-safe; bots log from their map threads.
    if (!_events.IsRunning())
        return;

    boost::json::object line{ { "ts", UtcTimestamp() }, { "kind", "chat" }, { "chat_kind", chatKind },
        { "source", source }, { "reason", reason }, { "outcome", outcome }, { "wait_ms", waitMs } };
    _events.Write(boost::json::serialize(line));
}
