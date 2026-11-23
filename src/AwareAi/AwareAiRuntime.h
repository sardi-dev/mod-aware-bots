/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_RUNTIME_H
#define AWARE_AI_RUNTIME_H

#include "AwareAiProtocol.h"
#include "Define.h"
#include "JsonlWriter.h"
#include <atomic>
#include <mutex>
#include <string>

// Shared AI-layer state: the link to the sidecar (settings push and status polling every 30 s), the cached status,
// the events log and the budget notices to players. Nothing here changes what a bot does; the chat moments live in
// AiChat.h.
class AwareAiRuntime
{
public:
    static AwareAiRuntime& Instance();

    // World thread. Start reads nothing from the config file itself: AwareAiConfig::Load runs first.
    void Start();
    void Stop();
    // World thread, every world tick.
    void Update(uint32 diff);

    // Starts the client if it is not running, sends the effective settings to the sidecar and asks for its status.
    // Used after a command changed something.
    void SyncNow();
    // Asks for the status only.
    void RefreshStatus();

    [[nodiscard]] AwareAiStatus Status();
    // Milliseconds since the cached status last changed (a poll answered or failed), or -1 when never polled.
    [[nodiscard]] int64 StatusAgeMs();
    // One JSON line in the events log: {"ts":...,"kind":kind,"detail":detail}.
    void LogEvent(std::string const& kind, std::string const& detail);
    // One chat request (AiChat.cpp, any thread): {"ts":...,"kind":"chat","chat_kind":...,"source":"ai|fake|rules",
    // "reason":...,"outcome":...,"wait_ms":...}. Never the chat text.
    void LogChat(std::string const& chatKind, std::string const& source, std::string const& reason,
        std::string const& outcome, uint32 waitMs);

private:
    AwareAiRuntime() = default;

    bool EnsureClientRunning();
    void PushSettings();
    void PollStatus();
    void StoreStatus(AwareAiStatus status);
    // World thread: logs link and budget changes, once per change, and tells the players about the budget.
    void ObserveStatus();
    void TellPlayers(std::string const& message);

    std::mutex _lock;
    AwareAiStatus _status;
    int64 _statusStampMs = -1;
    std::atomic<bool> _statusChanged{false};
    // World thread only.
    bool _everReachable = false;
    bool _lastReachable = false;
    std::string _lastMode;
    bool _warnedWarning = false;
    bool _warnedExhausted = false;
    uint32 _syncTimerMs = 0;
    JsonlWriter _events;
};

#define sAwareAiRuntime AwareAiRuntime::Instance()

#endif
