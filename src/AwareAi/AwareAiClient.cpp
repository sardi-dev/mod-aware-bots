/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AwareAiClient.h"
#include "Log.h"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <optional>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace
{
    constexpr uint32 MAX_IN_FLIGHT = 16;
    constexpr uint32 BREAKER_FAILURE_THRESHOLD = 5;
    constexpr int64 BREAKER_OPEN_MS = 30000;
    constexpr int HTTP_VERSION_1_1 = 11;
    // A status or decision answer is a few hundred bytes; anything near this is a bug on the other side.
    constexpr std::uint64_t MAX_RESPONSE_BYTES = 256 * 1024;
    constexpr std::string_view HTTP_SCHEME = "http://";

    int64 SteadyNowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    http::verb ToVerb(AwareAiHttpMethod method)
    {
        switch (method)
        {
            case AwareAiHttpMethod::Post:
                return http::verb::post;
            case AwareAiHttpMethod::Put:
                return http::verb::put;
            default:
                return http::verb::get;
        }
    }

    // One request on its own connection; keeps itself alive through the async chain.
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        Session(asio::io_context& io, std::function<void(bool)> onFinished, AwareAiCallback callback)
            : _resolver(io), _stream(io), _onFinished(std::move(onFinished)), _callback(std::move(callback))
        {
            _parser.body_limit(MAX_RESPONSE_BYTES);
        }

        void Run(std::string const& host, std::string const& port, http::request<http::string_body> request,
            Milliseconds timeout)
        {
            _request = std::move(request);
            _stream.expires_after(timeout);
            _resolver.async_resolve(host, port, beast::bind_front_handler(&Session::OnResolve, shared_from_this()));
        }

    private:
        void OnResolve(beast::error_code error, tcp::resolver::results_type results)
        {
            if (error)
                return Fail("resolve", error);

            _stream.async_connect(results, beast::bind_front_handler(&Session::OnConnect, shared_from_this()));
        }

        void OnConnect(beast::error_code error, tcp::resolver::results_type::endpoint_type /*endpoint*/)
        {
            if (error)
                return Fail("connect", error);

            http::async_write(_stream, _request, beast::bind_front_handler(&Session::OnWrite, shared_from_this()));
        }

        void OnWrite(beast::error_code error, std::size_t /*bytes*/)
        {
            if (error)
                return Fail("write", error);

            http::async_read(_stream, _buffer, _parser, beast::bind_front_handler(&Session::OnRead, shared_from_this()));
        }

        void OnRead(beast::error_code error, std::size_t /*bytes*/)
        {
            if (error)
                return Fail("read", error);

            beast::error_code ignored;
            _stream.socket().shutdown(tcp::socket::shutdown_both, ignored);

            http::response<http::string_body> message = _parser.release();
            AwareAiResponse response;
            response.Status = message.result_int();
            response.Ok = response.Status >= 200 && response.Status < 300;
            response.Body = std::move(message.body());
            if (!response.Ok)
                response.Error = "http_" + std::to_string(response.Status);

            // An HTTP error still proves the sidecar is up, so only transport errors count toward the breaker.
            _onFinished(true);
            _callback(std::move(response));
        }

        void Fail(std::string_view stage, beast::error_code const& error)
        {
            AwareAiResponse response;
            response.Error = error == beast::error::timeout ? "timeout" : std::string(stage) + ": " + error.message();
            _onFinished(false);
            _callback(std::move(response));
        }

        tcp::resolver _resolver;
        beast::tcp_stream _stream;
        beast::flat_buffer _buffer;
        http::request<http::string_body> _request;
        http::response_parser<http::string_body> _parser;
        std::function<void(bool)> _onFinished;
        AwareAiCallback _callback;
    };
}

struct AwareAiClient::Impl
{
    asio::io_context Io;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> Work;
    std::thread Thread;
    std::string Host;
    std::string Port;
};

AwareAiClient::AwareAiClient() : _impl(std::make_unique<Impl>()) { }

AwareAiClient::~AwareAiClient()
{
    Stop();
}

AwareAiClient& AwareAiClient::Instance()
{
    static AwareAiClient instance;
    return instance;
}

bool AwareAiClient::Start(std::string const& baseUrl)
{
    if (_running)
        return true;

    std::string_view rest(baseUrl);
    if (!rest.starts_with(HTTP_SCHEME))
    {
        SF_LOG_ERROR("playerbots.ai", "AwareAi.SidecarUrl must start with http://, got '%s'.", baseUrl.c_str());
        return false;
    }

    rest.remove_prefix(HTTP_SCHEME.size());
    if (std::size_t const slash = rest.find('/'); slash != std::string_view::npos)
        rest = rest.substr(0, slash);

    std::size_t const colon = rest.find(':');
    _impl->Host = std::string(rest.substr(0, colon));
    _impl->Port = colon == std::string_view::npos ? "80" : std::string(rest.substr(colon + 1));
    if (_impl->Host.empty() || _impl->Port.empty())
    {
        SF_LOG_ERROR("playerbots.ai", "AwareAi.SidecarUrl '%s' has no host or port.", baseUrl.c_str());
        return false;
    }

    _consecutiveFailures = 0;
    _breakerOpenUntilMs = 0;
    _impl->Io.restart();
    _impl->Work.emplace(asio::make_work_guard(_impl->Io));
    _impl->Thread = std::thread([this]() { _impl->Io.run(); });
    _running = true;
    SF_LOG_INFO("playerbots.ai", "Sidecar client started for %s:%s", _impl->Host.c_str(), _impl->Port.c_str());
    return true;
}

void AwareAiClient::Stop()
{
    if (!_running)
        return;

    _running = false;
    _impl->Work.reset();
    _impl->Io.stop();
    if (_impl->Thread.joinable())
        _impl->Thread.join();
    _inFlight = 0;
}

bool AwareAiClient::IsBreakerOpen() const
{
    return SteadyNowMs() < _breakerOpenUntilMs;
}

bool AwareAiClient::Send(AwareAiHttpMethod method, std::string target, std::string body, Milliseconds timeout,
    AwareAiCallback callback)
{
    if (!_running || IsBreakerOpen())
        return false;

    if (++_inFlight > MAX_IN_FLIGHT)
    {
        --_inFlight;
        return false;
    }

    http::request<http::string_body> request{ ToVerb(method), target, HTTP_VERSION_1_1 };
    request.set(http::field::host, _impl->Host);
    request.set(http::field::user_agent, "aware-bots-worldserver");
    request.keep_alive(false);
    if (method != AwareAiHttpMethod::Get)
    {
        request.set(http::field::content_type, "application/json");
        request.body() = std::move(body);
        request.prepare_payload();
    }

    auto session = std::make_shared<Session>(_impl->Io, [this](bool ok) { OnFinished(ok); }, std::move(callback));
    // Hand the request to the IO thread; Session objects are only touched there.
    asio::post(_impl->Io, [session, host = _impl->Host, port = _impl->Port, request = std::move(request),
        timeout]() mutable { session->Run(host, port, std::move(request), timeout); });
    return true;
}

void AwareAiClient::OnFinished(bool transportOk)
{
    --_inFlight;
    if (transportOk)
    {
        _consecutiveFailures = 0;
        return;
    }

    if (++_consecutiveFailures >= BREAKER_FAILURE_THRESHOLD)
    {
        _breakerOpenUntilMs = SteadyNowMs() + BREAKER_OPEN_MS;
        _consecutiveFailures = 0;
        _breakerTripped = true;
    }
}
