/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

// Core-free tests of the AI layer plumbing: the JSON helpers, the two protocol bodies, the JSONL writer and the HTTP
// client against an in-process server. Built by tests/aware_ai/run.sh with stub core headers (see stubs/).

#include "AwareAiClient.h"
#include "AwareAiProtocol.h"
#include "JsonFields.h"
#include "JsonlWriter.h"
#include "TestHarness.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <sstream>
#include <thread>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace
{
    // A tiny blocking HTTP server on 127.0.0.1: one thread per connection, behaviour chosen by the request path.
    class FakeSidecar
    {
    public:
        FakeSidecar() : _acceptor(_io, tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0))
        {
            _thread = std::thread([this]() { AcceptLoop(); });
        }

        ~FakeSidecar()
        {
            _stopping = true;
            _release = true;
            // A blocking accept cannot be cancelled from another thread, so wake it with a connection of our own.
            beast::error_code ignored;
            tcp::socket poke(_io);
            poke.connect(_acceptor.local_endpoint(), ignored);
            _thread.join();
            for (std::thread& worker : _workers)
                worker.join();
        }

        std::string Url() const { return "http://127.0.0.1:" + std::to_string(_acceptor.local_endpoint().port()); }
        std::atomic<int> Requests{0};
        std::atomic<bool> SawConnectionClose{false};
        std::string LastBody;
        std::string LastMethod;
        std::atomic<bool> _release{false};

    private:
        void AcceptLoop()
        {
            while (!_stopping)
            {
                beast::error_code error;
                tcp::socket socket(_io);
                _acceptor.accept(socket, error);
                if (error || _stopping)
                    return;

                _workers.emplace_back([this, s = std::make_shared<tcp::socket>(std::move(socket))]() { Serve(*s); });
            }
        }

        void Serve(tcp::socket& socket)
        {
            beast::flat_buffer buffer;
            http::request<http::string_body> request;
            beast::error_code error;
            http::read(socket, buffer, request, error);
            if (error)
                return;

            ++Requests;
            {
                std::lock_guard<std::mutex> guard(_lock);
                LastBody = request.body();
                LastMethod = std::string(request.method_string());
            }
            if (request[http::field::connection] == "close")
                SawConnectionClose = true;

            std::string const target(request.target());
            if (target == "/slow")
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            if (target == "/hold")
                while (!_release)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));

            http::response<http::string_body> response{ target == "/err" ? http::status::service_unavailable
                                                                          : http::status::ok, request.version() };
            response.set(http::field::content_type, "application/json");
            response.body() = target == "/echo" ? request.body() : std::string("{\"ok\":true}");
            response.prepare_payload();
            http::write(socket, response, error);
            socket.shutdown(tcp::socket::shutdown_send, error);
        }

        asio::io_context _io;
        tcp::acceptor _acceptor;
        std::thread _thread;
        std::vector<std::thread> _workers;
        std::atomic<bool> _stopping{false};
        std::mutex _lock;
    };

    AwareAiResponse SendAndWait(AwareAiHttpMethod method, std::string target, std::string body,
        Milliseconds timeout = 2000ms)
    {
        auto promise = std::make_shared<std::promise<AwareAiResponse>>();
        std::future<AwareAiResponse> future = promise->get_future();
        bool const sent = sAwareAiClient.Send(method, std::move(target), std::move(body), timeout,
            [promise](AwareAiResponse response) { promise->set_value(std::move(response)); });
        if (!sent)
        {
            AwareAiResponse refused;
            refused.Error = "not sent";
            return refused;
        }

        if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
        {
            AwareAiResponse lost;
            lost.Error = "no callback";
            return lost;
        }

        return future.get();
    }

    std::string ReadAll(std::filesystem::path const& path)
    {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}

TEST(json_fields_are_lenient)
{
    auto const object = JsonFields::ParseObject(R"({"a":true,"n":3,"d":1.5,"s":"x","o":{"k":1}})");
    CHECK(object.has_value());
    CHECK(JsonFields::Bool(*object, "a"));
    CHECK(!JsonFields::Bool(*object, "missing"));
    CHECK(JsonFields::Bool(*object, "missing", true));
    CHECK(JsonFields::Number(*object, "n") == 3.0);
    CHECK(JsonFields::Number(*object, "d") == 1.5);
    CHECK(JsonFields::Number(*object, "s", -1.0) == -1.0);
    CHECK(JsonFields::String(*object, "s") == "x");
    CHECK(JsonFields::String(*object, "n", "fallback") == "fallback");
    CHECK(JsonFields::Object(*object, "o") != nullptr);
    CHECK(JsonFields::Object(*object, "a") == nullptr);
    CHECK(!JsonFields::ParseObject("[1,2]").has_value());
    CHECK(!JsonFields::ParseObject("not json").has_value());
}

TEST(settings_body_matches_the_sidecar_contract)
{
    AwareAiSettings settings;
    settings.Enabled = true;
    settings.PvP = true;
    settings.MonthlyUsd = 20.0;
    settings.DailyUsd = 0.0;
    auto const body = JsonFields::ParseObject(BuildSettingsBody(settings));
    CHECK(body.has_value());
    CHECK(JsonFields::Number(*body, "schema_version") == 1.0);
    CHECK(JsonFields::Bool(*body, "enabled"));
    CHECK(JsonFields::Bool(*JsonFields::Object(*body, "features"), "pvp"));
    // The sidecar's chat feature defaults to off, so the push always states it.
    CHECK(JsonFields::Object(*body, "features")->contains("chat"));
    CHECK(!JsonFields::Bool(*JsonFields::Object(*body, "features"), "chat", true));
    boost::json::object const* budget = JsonFields::Object(*body, "budget");
    CHECK(budget != nullptr);
    CHECK(JsonFields::Number(*budget, "monthly_usd") == 20.0);
    CHECK(budget->at("daily_usd").is_null());

    settings.Enabled = false;
    settings.PvP = false;
    settings.Chat = true;
    settings.MonthlyUsd = static_cast<double>(0.1f) * 3.0;
    settings.DailyUsd = static_cast<double>(2.5f);
    auto const second = JsonFields::ParseObject(BuildSettingsBody(settings));
    CHECK(second.has_value());
    CHECK(!JsonFields::Bool(*second, "enabled", true));
    CHECK(!JsonFields::Bool(*JsonFields::Object(*second, "features"), "pvp", true));
    CHECK(JsonFields::Bool(*JsonFields::Object(*second, "features"), "chat"));
    boost::json::object const* secondBudget = JsonFields::Object(*second, "budget");
    CHECK(JsonFields::Number(*secondBudget, "monthly_usd") == 0.3);
    CHECK(JsonFields::Number(*secondBudget, "daily_usd") == 2.5);
}

TEST(chat_body_matches_the_sidecar_contract)
{
    AwareAiChatRequest request;
    request.BotGuid = 42;
    request.PlayerGuid = 7;
    request.Bot = { "Taran", "monk", "pandaren", 45 };
    request.Player = { "Aleria", "paladin", "human", 44 };
    request.Aggression = 30;
    request.Sociability = 85;
    request.Chattiness = 60;
    request.Honor = 70;
    request.SameFaction = true;
    request.Zone = "Elwynn Forest";
    request.Kind = "whisper_reply";
    request.Channel = "whisper";
    request.Message = "hey, nice cloak";
    request.DeadlineMs = 1500;
    auto const body = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(body.has_value());
    // Exactly the schema's fields: the sidecar refuses unknown ones, and nothing outside the game is sent.
    std::set<std::string> keys;
    for (auto const& field : *body)
        keys.insert(std::string(field.key()));
    CHECK(keys == (std::set<std::string>{ "schema_version", "bot_guid", "player_guid", "bot", "player", "personality",
        "same_faction", "zone", "kind", "channel", "message", "deadline_ms" }));
    CHECK(JsonFields::Number(*body, "schema_version") == 1.0);
    CHECK(JsonFields::Number(*body, "bot_guid") == 42.0);
    boost::json::object const* bot = JsonFields::Object(*body, "bot");
    CHECK(JsonFields::String(*bot, "name") == "Taran");
    CHECK(JsonFields::String(*bot, "class_name") == "monk");
    CHECK(JsonFields::String(*bot, "race") == "pandaren");
    CHECK(JsonFields::Number(*bot, "level") == 45.0);
    CHECK(bot->size() == 4);
    CHECK(JsonFields::Number(*JsonFields::Object(*body, "personality"), "sociability") == 85.0);
    CHECK(JsonFields::Bool(*body, "same_faction"));
    CHECK(JsonFields::String(*body, "kind") == "whisper_reply");
    CHECK(JsonFields::String(*body, "message") == "hey, nice cloak");
    CHECK(JsonFields::Number(*body, "deadline_ms") == 1500.0);

    request.Kind = "guild_reply";
    request.GuildHumanMessageAgeS = 2.5;
    auto const guild = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(JsonFields::Number(*guild, "guild_human_message_age_s") == 2.5);
    CHECK(!guild->contains("conversation"));

    // Conversation mode: the flag, and the merged lines as one message.
    request.Kind = "whisper_reply";
    request.GuildHumanMessageAgeS.reset();
    request.Conversation = true;
    request.Message = "hey / u there?";
    auto const talk = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(JsonFields::Bool(*talk, "conversation"));
    CHECK(JsonFields::String(*talk, "message") == "hey / u there?");
}

TEST(chat_body_carries_the_liking_fields_only_when_set)
{
    AwareAiChatRequest request;
    request.Bot = { "Taran", "monk", "pandaren", 45 };
    request.Player = { "Aleria", "paladin", "human", 44 };
    request.Kind = "whisper_reply";
    request.Channel = "whisper";
    request.Message = "hey";
    auto const plain = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(!plain->contains("liking"));
    CHECK(!plain->contains("relation"));

    // The schema's liking is 0 to 1 and relation one of four words.
    request.LikingPercent = 62;
    request.Relation = "friendly";
    auto const both = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(JsonFields::Number(*both, "liking") == 0.62);
    CHECK(JsonFields::String(*both, "relation") == "friendly");

    request.LikingPercent = 0;
    request.Relation = "cold";
    CHECK(JsonFields::Number(*JsonFields::ParseObject(BuildChatBody(request)), "liking") == 0.0);
    request.LikingPercent = 400;
    request.Relation = "close";
    auto const clamped = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(JsonFields::Number(*clamped, "liking") == 1.0);

    // A word the sidecar would refuse never goes out; the percent alone is fine.
    request.Relation = "adored";
    auto const unknown = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(!unknown->contains("relation"));
    CHECK(unknown->contains("liking"));
    request.LikingPercent.reset();
    request.Relation = "Friendly";
    CHECK(!JsonFields::ParseObject(BuildChatBody(request))->contains("relation"));
}

TEST(chat_body_carries_the_bot_denial_only_when_set)
{
    AwareAiChatRequest request;
    request.Bot = { "Taran", "monk", "pandaren", 45 };
    request.Player = { "Aleria", "paladin", "human", 44 };
    request.Kind = "whisper_reply";
    request.Channel = "whisper";
    request.Message = "are you a bot?";
    CHECK(!JsonFields::ParseObject(BuildChatBody(request))->contains("deny_being_bot"));

    request.DenyBeingBot = true;
    auto const denies = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(JsonFields::Bool(*denies, "deny_being_bot"));
}

TEST(wipe_body_names_one_human_or_everyone)
{
    AwareAiWipe one;
    one.PlayerGuid = 7;
    auto const named = JsonFields::ParseObject(BuildWipeBody(one));
    std::set<std::string> keys;
    for (auto const& field : *named)
        keys.insert(std::string(field.key()));
    CHECK(keys == (std::set<std::string>{ "schema_version", "player_guid" }));
    CHECK(JsonFields::Number(*named, "player_guid") == 7.0);

    auto const all = JsonFields::ParseObject(BuildWipeBody(AwareAiWipe{}));
    keys.clear();
    for (auto const& field : *all)
        keys.insert(std::string(field.key()));
    CHECK(keys == (std::set<std::string>{ "schema_version", "all" }));
    CHECK(JsonFields::Bool(*all, "all"));
}

TEST(wipe_answer_is_read_and_a_bad_body_is_refused)
{
    auto const result = ParseWipeBody(R"({"schema_version":1,"memory_events":2,"chat_turns":14,"social_events":3,"pair_notes":1,"log_lines":9})");
    CHECK(result.has_value());
    CHECK(result->MemoryEvents == 2);
    CHECK(result->ChatTurns == 14);
    CHECK(result->SocialEvents == 3);
    CHECK(result->PairNotes == 1);
    CHECK(result->LogLines == 9);
    CHECK(!ParseWipeBody("not json").has_value());
    CHECK(ParseWipeBody("{}")->ChatTurns == 0);
}

TEST(memory_event_body_matches_the_sidecar_contract)
{
    AwareAiMemoryEvent event;
    event.BotGuid = 42;
    event.PlayerGuid = 7;
    event.Event = "dungeon_done";
    event.Detail = "The Deadmines";
    auto const body = JsonFields::ParseObject(BuildMemoryEventBody(event));
    CHECK(body.has_value());
    std::set<std::string> keys;
    for (auto const& field : *body)
        keys.insert(std::string(field.key()));
    // No outcome when an event is given, and nothing else.
    CHECK(keys == (std::set<std::string>{ "schema_version", "bot_guid", "player_guid", "event", "detail" }));
    CHECK(JsonFields::Number(*body, "schema_version") == 1.0);
    CHECK(JsonFields::Number(*body, "bot_guid") == 42.0);
    CHECK(JsonFields::Number(*body, "player_guid") == 7.0);
    CHECK(JsonFields::String(*body, "event") == "dungeon_done");
    CHECK(JsonFields::String(*body, "detail") == "The Deadmines");

    // A duel carries the older outcome too; no detail when empty.
    event.Event = "duel_won";
    event.Detail.clear();
    event.Outcome = "bot_won";
    auto const duel = JsonFields::ParseObject(BuildMemoryEventBody(event));
    CHECK(!duel->contains("detail"));
    CHECK(JsonFields::String(*duel, "outcome") == "bot_won");

    // The detail is cut to 64 bytes, never inside a UTF-8 character.
    event.Detail = std::string(63, 'a') + "\xC3\xA9";
    auto const cut = JsonFields::ParseObject(BuildMemoryEventBody(event));
    CHECK(JsonFields::String(*cut, "detail") == std::string(63, 'a'));
}

TEST(chat_body_is_clamped_to_the_schema_limits)
{
    AwareAiChatRequest request;
    request.Bot = { std::string(30, 'x'), "mage", "gnome", 0 };
    request.Player = { "", "rogue", "orc", 300 };
    request.Aggression = 250;
    request.Zone = std::string(70, 'z');
    request.Kind = "nearby_reply";
    request.Channel = "say";
    // 401 two-byte characters: 802 bytes.
    for (int i = 0; i < 401; ++i)
        request.Message += "\xC3\xA9";
    request.DeadlineMs = 100;
    auto const body = JsonFields::ParseObject(BuildChatBody(request));
    CHECK(body.has_value());
    CHECK(JsonFields::String(*JsonFields::Object(*body, "bot"), "name").size() == 24);
    CHECK(JsonFields::Number(*JsonFields::Object(*body, "bot"), "level") == 1.0);
    CHECK(!JsonFields::String(*JsonFields::Object(*body, "player"), "name").empty());
    CHECK(JsonFields::Number(*JsonFields::Object(*body, "player"), "level") == 255.0);
    CHECK(JsonFields::Number(*JsonFields::Object(*body, "personality"), "aggression") == 100.0);
    CHECK(JsonFields::String(*body, "zone").size() == 64);
    CHECK(JsonFields::String(*body, "message").size() == 800);
    CHECK(JsonFields::Number(*body, "deadline_ms") == 500.0);
    request.Message.clear();
    CHECK(!JsonFields::String(*JsonFields::ParseObject(BuildChatBody(request)), "message").empty());
}

namespace
{
    AwareAiDecideRequest SampleDecide()
    {
        AwareAiDecideRequest request;
        request.BotGuid = 123;
        request.StateVersion = 42;
        request.DeadlineMs = 600;
        request.State.Map = "ab";
        request.State.Decision = "stance";
        request.State.Question = "Which stance should the team play now?";
        request.State.ScoreOurs = 820;
        request.State.ScoreTheirs = 780;
        request.State.ScoreLimit = 1600;
        request.State.TimeLeftS = 420;
        request.State.Stance = "balanced";
        request.State.Nodes = { "st:ours", "bs:theirs", "farm:contested" };
        request.State.FriendlyGroups = { "farm:2", "bs:4" };
        request.State.ObservedPressure = { "bs:3" };
        request.State.Facts = { "alive:9" };
        request.State.Candidates = { { "option_0", "balanced" }, { "option_1", "push" } };
        return request;
    }
}

TEST(decide_body_matches_the_sidecar_contract)
{
    auto const body = JsonFields::ParseObject(BuildDecideBody(SampleDecide()));
    CHECK(body.has_value());
    CHECK(JsonFields::Number(*body, "schema_version") == 1.0);
    CHECK(JsonFields::String(*body, "decision_type") == "bg_strategy");
    CHECK(JsonFields::Number(*body, "bot_guid") == 123.0);
    CHECK(JsonFields::Number(*body, "state_version") == 42.0);
    CHECK(JsonFields::Number(*body, "deadline_ms") == 600.0);
    CHECK(body->contains("options") && body->at("options").as_array().size() == 2);
    CHECK(body->at("options").as_array()[0].as_string() == "option_0");
    CHECK(body->at("options").as_array()[1].as_string() == "option_1");
    // Nothing else the schema forbids: the sidecar's model refuses unknown top-level fields.
    CHECK(body->size() == 7);

    boost::json::object const& state = *JsonFields::Object(*body, "state");
    CHECK(JsonFields::String(state, "map") == "ab");
    CHECK(JsonFields::String(state, "decision") == "stance");
    CHECK(state.at("score").as_array()[0].as_int64() == 820);
    CHECK(state.at("score").as_array()[1].as_int64() == 780);
    CHECK(JsonFields::Number(state, "score_limit") == 1600.0);
    CHECK(JsonFields::Number(state, "time_left_s") == 420.0);
    CHECK(JsonFields::String(state, "stance") == "balanced");
    CHECK(state.at("nodes").as_array().size() == 3);
    CHECK(state.at("friendly_groups").as_array()[1].as_string() == "bs:4");
    CHECK(state.at("observed_pressure").as_array()[0].as_string() == "bs:3");
    boost::json::object const& candidates = *JsonFields::Object(state, "candidates");
    CHECK(JsonFields::String(candidates, "option_0") == "balanced");
    CHECK(JsonFields::String(candidates, "option_1") == "push");
    // The state is under the sidecar's 8192 byte cap by a wide margin.
    CHECK(boost::json::serialize(state).size() < 2048);
}

TEST(decide_body_leaves_out_empty_lists_and_keeps_the_order_of_the_template)
{
    AwareAiDecideRequest request = SampleDecide();
    request.State.Nodes.clear();
    request.State.ObservedPressure.clear();
    auto const body = JsonFields::ParseObject(BuildDecideBody(request));
    boost::json::object const& state = *JsonFields::Object(*body, "state");
    CHECK(!state.contains("nodes"));
    CHECK(!state.contains("observed_pressure"));
    CHECK(state.contains("friendly_groups") && state.contains("facts"));

    std::vector<std::string> keys;
    for (auto const& entry : state)
        keys.push_back(std::string(entry.key()));
    std::vector<std::string> const expected = { "map", "decision", "question", "score", "score_limit", "time_left_s", "stance",
        "friendly_groups", "facts", "candidates" };
    CHECK(keys == expected);
}

TEST(decide_body_carries_the_decision_type_of_the_later_arena_asks)
{
    AwareAiDecideRequest request = SampleDecide();
    request.DecisionType = "arena_kill_target";
    CHECK(JsonFields::String(*JsonFields::ParseObject(BuildDecideBody(request)), "decision_type") == "arena_kill_target");
    request.DecisionType = "1 not a type";
    CHECK(JsonFields::String(*JsonFields::ParseObject(BuildDecideBody(request)), "decision_type") == "not_a_type");
    request.DecisionType = "123";
    CHECK(BuildDecideBody(request).empty());
}

TEST(decide_body_leaves_out_unknown_time_and_limit_and_needs_two_slots)
{
    AwareAiDecideRequest request = SampleDecide();
    request.State.TimeLeftS = -1;
    request.State.ScoreLimit = 0;
    auto const body = JsonFields::ParseObject(BuildDecideBody(request));
    boost::json::object const& state = *JsonFields::Object(*body, "state");
    CHECK(!state.contains("time_left_s"));
    CHECK(!state.contains("score_limit"));

    request.State.Candidates.pop_back();
    CHECK(BuildDecideBody(request).empty());
    request.State.Candidates.clear();
    CHECK(BuildDecideBody(request).empty());
    // A duplicate slot or a slot with no meaning is not a second option.
    request.State.Candidates = { { "option_0", "balanced" }, { "option_0", "push" }, { "option_1", "" } };
    CHECK(BuildDecideBody(request).empty());
    CHECK(DecideOptions(SampleDecide()).size() == 2);
}

TEST(decide_body_is_clamped_to_the_schema_limits)
{
    AwareAiDecideRequest request = SampleDecide();
    request.DeadlineMs = 10;
    auto low = JsonFields::ParseObject(BuildDecideBody(request));
    CHECK(JsonFields::Number(*low, "deadline_ms") == 50.0);
    request.DeadlineMs = 90000;
    auto high = JsonFields::ParseObject(BuildDecideBody(request));
    CHECK(JsonFields::Number(*high, "deadline_ms") == 5000.0);

    // Long lists and long texts are cut, at a character boundary; a slot name is folded into the identifier pattern.
    request.State.Nodes.assign(40, std::string(63, 'n') + "\xC3\xA9");
    request.State.Question = std::string(300, 'q');
    request.State.Candidates = { { "Option 0", std::string(63, 'a') + "\xC3\xA9" }, { "option_1", "push" },
        { "option_2", "x" }, { "option_3", "y" }, { "option_4", "z" }, { "option_5", "w" } };
    auto const body = JsonFields::ParseObject(BuildDecideBody(request));
    boost::json::object const& state = *JsonFields::Object(*body, "state");
    CHECK(state.at("nodes").as_array().size() == 12);
    CHECK(std::string(state.at("nodes").as_array()[0].as_string()).size() == 63);
    CHECK(JsonFields::String(state, "question").size() == 200);
    boost::json::object const& candidates = *JsonFields::Object(state, "candidates");
    CHECK(candidates.contains("option_0"));
    CHECK(JsonFields::String(candidates, "option_0") == std::string(63, 'a'));
    CHECK(body->at("options").as_array().size() == 6);
    CHECK(boost::json::serialize(state).size() < 8192);
}

TEST(decide_response_is_parsed_and_garbage_is_refused)
{
    auto const answer = ParseDecideBody(R"({"schema_version":1,"decision_id":"d-1","source":"cache","reason":null,)"
        R"("choice":"option_1","probabilities":{"option_0":0.3,"option_1":0.7},"confidence":0.7,)"
        R"("template":"bg_strategy@1","latency_ms":412,"cost_usd":0.00002})");
    CHECK(answer.has_value());
    CHECK(answer->DecisionId == "d-1");
    CHECK(answer->Source == "cache");
    CHECK(answer->Choice == "option_1");
    CHECK(answer->Template == "bg_strategy@1");
    CHECK_NEAR(answer->Confidence, 0.7, 1e-9);
    CHECK(answer->LatencyMs == 412);
    CHECK_NEAR(answer->CostUsd, 0.00002, 1e-12);

    auto const hint = ParseDecideBody(R"({"decision_id":"d-2","source":"rules_hint","reason":"budget_exhausted"})");
    CHECK(hint->Source == "rules_hint");
    CHECK(hint->Reason == "budget_exhausted");
    CHECK(hint->Choice.empty());
    CHECK(hint->Confidence < 0.0);
    CHECK(!ParseDecideBody("Internal Server Error").has_value());
}

TEST(outcome_body_matches_the_sidecar_contract)
{
    AwareAiOutcome outcome;
    outcome.DecisionId = "d-abc123";
    outcome.Outcome = "Rejected stale-state";
    outcome.Details = { { "point", "stance" }, { "Latency Ms", "412" } };
    auto const body = JsonFields::ParseObject(BuildOutcomeBody(outcome));
    CHECK(body.has_value());
    CHECK(JsonFields::Number(*body, "schema_version") == 1.0);
    CHECK(JsonFields::String(*body, "decision_id") == "d-abc123");
    CHECK(JsonFields::String(*body, "outcome") == "rejected_stale_state");
    boost::json::object const& details = *JsonFields::Object(*body, "details");
    CHECK(JsonFields::String(details, "point") == "stance");
    CHECK(JsonFields::String(details, "latency_ms") == "412");

    outcome.DecisionId.clear();
    CHECK(BuildOutcomeBody(outcome).empty());
    outcome.DecisionId = "d-1";
    outcome.Outcome = "123";
    CHECK(BuildOutcomeBody(outcome).empty());
}

TEST(chat_response_is_parsed_and_garbage_is_refused)
{
    auto const reply = ParseChatBody(R"({"schema_version":1,"decision_id":"c-1","source":"llm","reason":null,)"
        R"("reply":"gg, again?","template":"chat@1","latency_ms":640,"cost_usd":0.0001})");
    CHECK(reply.has_value());
    CHECK(reply->Source == "llm");
    CHECK(reply->Reason.empty());
    CHECK(reply->Reply == "gg, again?");
    auto const hint = ParseChatBody(R"({"decision_id":"c-2","source":"rules_hint","reason":"rate_limited"})");
    CHECK(hint->Source == "rules_hint");
    CHECK(hint->Reason == "rate_limited");
    CHECK(hint->Reply.empty());
    CHECK(!ParseChatBody("Internal Server Error").has_value());
}

TEST(status_body_is_parsed_and_garbage_is_refused)
{
    std::string const body = R"({"schema_version":1,"version":"0.1.0","mode":"fake","enabled":true,"has_api_key":false,)"
        R"("features":{"pvp":true},"budget":{"month_spent_usd":0.5,"month_cap_usd":20.0,"day_spent_usd":0.1,)"
        R"("day_cap_usd":2.0,"month_cap_ceiling_usd":20.0,"cap_clamped":true,"warning":false,"exhausted":false},)"
        R"("breaker":"closed","in_flight":3,"templates":["pvp_engage@2","pvp_reassess@2"]})";
    auto const status = ParseStatusBody(body);
    CHECK(status.has_value());
    CHECK(status->Version == "0.1.0");
    CHECK(status->Mode == "fake");
    CHECK(status->Enabled);
    CHECK(!status->HasApiKey);
    CHECK(status->Breaker == "closed");
    CHECK(status->InFlight == 3);
    CHECK(status->MonthSpentUsd == 0.5);
    CHECK(status->MonthCapUsd == 20.0);
    CHECK(status->DaySpentUsd == 0.1);
    CHECK(status->DayCapUsd == 2.0);
    CHECK(status->CeilingUsd == 20.0);
    CHECK(status->CapClamped);
    CHECK(!status->Warning);
    CHECK(!status->Exhausted);
    CHECK(!status->Reachable);

    CHECK(!ParseStatusBody("<html>bad gateway</html>").has_value());
    auto const empty = ParseStatusBody("{}");
    CHECK(empty.has_value());
    CHECK(empty->Mode.empty());
    CHECK(empty->MonthCapUsd == 0.0);
}

TEST(day_spend_text_says_no_daily_cap_when_the_sidecar_has_none)
{
    auto const status = ParseStatusBody(R"({"budget":{"month_spent_usd":1.5,"month_cap_usd":20.0,)"
        R"("day_spent_usd":0.0123,"day_cap_usd":null}})");
    CHECK(status.has_value());
    CHECK(status->DayCapUsd == 0.0);
    CHECK(DaySpendText(status->DaySpentUsd, status->DayCapUsd, 4) == "$0.0123 today, no daily cap");
    CHECK(DaySpendText(0.5, 0.0, 2) == "$0.50 today, no daily cap");
    CHECK(DaySpendText(0.5, -1.0, 2) == "$0.50 today, no daily cap");
    CHECK(DaySpendText(0.0123, 2.0, 4) == "$0.0123 of $2.00 today");
    CHECK(DaySpendText(0.5, 2.0, 2) == "$0.50 of $2.00 today");
}

TEST(jsonl_writer_appends_lines_and_flushes_on_stop)
{
    std::filesystem::path const dir = std::filesystem::temp_directory_path() / ("aware_ai_jsonl_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    JsonlWriter writer;
    CHECK(!writer.IsRunning());
    writer.Write("{\"dropped\":true}");
    writer.Start(dir / "nested", "events");
    CHECK(writer.IsRunning());
    for (int i = 0; i < 100; ++i)
        writer.Write("{\"n\":" + std::to_string(i) + "}");
    writer.Write("");
    writer.Stop();
    CHECK(!writer.IsRunning());

    std::string content;
    for (auto const& entry : std::filesystem::directory_iterator(dir / "nested"))
    {
        CHECK(entry.path().filename().string().rfind("events-", 0) == 0);
        CHECK(entry.path().extension() == ".jsonl");
        content += ReadAll(entry.path());
    }

    std::size_t lines = 0;
    for (char const c : content)
        lines += c == '\n';
    CHECK(lines == 100);
    CHECK(content.find("dropped") == std::string::npos);
    CHECK(content.rfind("{\"n\":99}\n") == content.size() - 9);
    std::filesystem::remove_all(dir);
}

TEST(client_refuses_bad_urls_and_stays_stopped)
{
    CHECK(!sAwareAiClient.Start("https://example.test:8090"));
    CHECK(!sAwareAiClient.Start("http://:8090"));
    CHECK(!sAwareAiClient.IsRunning());
    CHECK(!sAwareAiClient.Send(AwareAiHttpMethod::Get, "/v1/status", "", 100ms, [](AwareAiResponse) { }));
}

TEST(client_talks_to_a_server_and_counts_only_transport_failures)
{
    FakeSidecar server;
    CHECK(sAwareAiClient.Start(server.Url()));

    AwareAiResponse const status = SendAndWait(AwareAiHttpMethod::Get, "/ok", "");
    CHECK(status.Ok);
    CHECK(status.Status == 200);
    CHECK(status.Body == "{\"ok\":true}");

    AwareAiResponse const echo = SendAndWait(AwareAiHttpMethod::Put, "/echo", "{\"enabled\":true}");
    CHECK(echo.Ok);
    CHECK(echo.Body == "{\"enabled\":true}");
    CHECK(server.LastMethod == "PUT");
    CHECK(server.SawConnectionClose);

    for (int i = 0; i < 7; ++i)
    {
        AwareAiResponse const failed = SendAndWait(AwareAiHttpMethod::Get, "/err", "");
        CHECK(!failed.Ok);
        CHECK(failed.Status == 503);
        CHECK(failed.Error == "http_503");
    }

    // An HTTP error still proves the server is up, so the breaker stays closed after more than five of them.
    CHECK(!sAwareAiClient.IsBreakerOpen());
    CHECK(!sAwareAiClient.TakeBreakerTripped());
    CHECK(sAwareAiClient.InFlight() == 0);
    sAwareAiClient.Stop();
    CHECK(!sAwareAiClient.IsRunning());
}

TEST(client_times_out_slow_answers)
{
    FakeSidecar server;
    CHECK(sAwareAiClient.Start(server.Url()));

    AwareAiResponse const slow = SendAndWait(AwareAiHttpMethod::Get, "/slow", "", 200ms);
    CHECK(!slow.Ok);
    CHECK(slow.Error == "timeout");
    sAwareAiClient.Stop();
}

TEST(client_breaker_opens_after_five_refused_connections_and_pauses_sends)
{
    std::string url;
    {
        // A port that was just open and is now closed: connections are refused.
        FakeSidecar server;
        url = server.Url();
    }

    CHECK(sAwareAiClient.Start(url));
    for (int i = 0; i < 5; ++i)
    {
        AwareAiResponse const refused = SendAndWait(AwareAiHttpMethod::Get, "/v1/status", "", 500ms);
        CHECK(!refused.Ok);
        CHECK(refused.Status == 0);
    }

    CHECK(sAwareAiClient.IsBreakerOpen());
    CHECK(sAwareAiClient.TakeBreakerTripped());
    CHECK(!sAwareAiClient.TakeBreakerTripped());
    CHECK(!sAwareAiClient.Send(AwareAiHttpMethod::Get, "/v1/status", "", 100ms, [](AwareAiResponse) { }));

    // Restarting the client clears the breaker, as a changed sidecar URL or a restart of the layer should.
    sAwareAiClient.Stop();
    FakeSidecar fresh;
    CHECK(sAwareAiClient.Start(fresh.Url()));
    CHECK(!sAwareAiClient.IsBreakerOpen());
    CHECK(SendAndWait(AwareAiHttpMethod::Get, "/ok", "").Ok);
    sAwareAiClient.Stop();
}

TEST(client_caps_requests_in_flight)
{
    FakeSidecar server;
    CHECK(sAwareAiClient.Start(server.Url()));

    std::atomic<int> answered{0};
    int accepted = 0;
    for (int i = 0; i < 20; ++i)
        accepted += sAwareAiClient.Send(AwareAiHttpMethod::Get, "/hold", "", 5000ms,
            [&answered](AwareAiResponse) { ++answered; });

    CHECK(accepted == 16);
    CHECK(sAwareAiClient.InFlight() == 16);
    server._release = true;
    for (int waited = 0; waited < 500 && answered < 16; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    CHECK(answered == 16);
    CHECK(sAwareAiClient.InFlight() == 0);
    sAwareAiClient.Stop();
}
