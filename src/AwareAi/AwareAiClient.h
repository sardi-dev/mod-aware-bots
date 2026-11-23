/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_CLIENT_H
#define AWARE_AI_CLIENT_H

#include "Define.h"
#include "Duration.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>

enum class AwareAiHttpMethod
{
    Get,
    Post,
    Put
};

struct AwareAiResponse
{
    bool Ok = false;
    unsigned Status = 0;
    std::string Body;
    std::string Error;
};

// Called on the client's IO thread: keep it short and thread-safe, never touch game objects from it.
using AwareAiCallback = std::function<void(AwareAiResponse)>;

// Async HTTP/1.1 client for the aware-ai sidecar on its own thread. One short connection per request (the sidecar is
// on the local Compose network), a cap on requests in flight, and a breaker that stops trying for a while when the
// sidecar is down, so a dead sidecar never piles up work.
class AwareAiClient
{
public:
    static AwareAiClient& Instance();

    // Returns false when the URL is not "http://host[:port]".
    bool Start(std::string const& baseUrl);
    void Stop();

    // Returns false without calling back when not running, over the in-flight cap, or while the breaker is open.
    bool Send(AwareAiHttpMethod method, std::string target, std::string body, Milliseconds timeout,
        AwareAiCallback callback);

    [[nodiscard]] bool IsRunning() const { return _running; }
    [[nodiscard]] bool IsBreakerOpen() const;
    [[nodiscard]] uint32 InFlight() const { return _inFlight; }
    // True once per breaker trip, for the world thread to log: the IO thread itself never logs.
    [[nodiscard]] bool TakeBreakerTripped() { return _breakerTripped.exchange(false); }

private:
    AwareAiClient();
    ~AwareAiClient();

    void OnFinished(bool transportOk);

    struct Impl;
    std::unique_ptr<Impl> _impl;
    std::atomic<bool> _running{false};
    std::atomic<uint32> _inFlight{0};
    std::atomic<uint32> _consecutiveFailures{0};
    std::atomic<int64> _breakerOpenUntilMs{0};
    std::atomic<bool> _breakerTripped{false};
};

#define sAwareAiClient AwareAiClient::Instance()

#endif
