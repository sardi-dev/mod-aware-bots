/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AwareAiProtocol.h"
#include "JsonFields.h"
#include <boost/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace
{
    constexpr int SCHEMA_VERSION = 1;
    // Dollar amounts are sent with four decimals: float config values would otherwise print as 0.10000000149.
    constexpr double USD_PRECISION = 10000.0;

    double RoundUsd(double value)
    {
        return std::round(value * USD_PRECISION) / USD_PRECISION;
    }

    // The chat schema's limits (services/aware-ai/schemas/v1/chat_request.json). Its lengths count characters, so a
    // cut in bytes always fits.
    constexpr std::size_t MAX_NAME_BYTES = 24;
    constexpr std::size_t MAX_ZONE_BYTES = 64;
    constexpr std::size_t MAX_MESSAGE_BYTES = 800;
    constexpr uint32 MIN_LEVEL = 1;
    constexpr uint32 MAX_LEVEL = 255;
    constexpr uint32 MAX_TRAIT = 100;
    constexpr uint32 MIN_CHAT_DEADLINE_MS = 500;
    constexpr uint32 MAX_CHAT_DEADLINE_MS = 20000;
    constexpr std::size_t MAX_MEMORY_DETAIL_BYTES = 64;
    // The chat schema's `liking` is 0 to 1; the module counts whole percent.
    constexpr uint32 MAX_LIKING_PERCENT = 100;
    constexpr std::array<std::string_view, 4> RELATION_BANDS = { "cold", "stranger", "friendly", "close" };
    // The decide schema (services/aware-ai/schemas/v1/decide_request.json): 2 to 16 options, a state of at most 8192 bytes,
    // a deadline of 50 to 5000 ms. The text and list limits here keep a request far below that.
    constexpr std::size_t MAX_DECIDE_OPTIONS = 16;
    constexpr uint32 MIN_DECIDE_DEADLINE_MS = 50;
    constexpr uint32 MAX_DECIDE_DEADLINE_MS = 5000;
    constexpr std::size_t MAX_STATE_ITEM_BYTES = 64;
    constexpr std::size_t MAX_STATE_ITEMS = 12;
    constexpr std::size_t MAX_QUESTION_BYTES = 200;
    constexpr std::size_t MAX_IDENTIFIER_BYTES = 64;
    constexpr std::size_t MAX_DECISION_ID_BYTES = 64;
    constexpr char const* FALLBACK_NAME = "Someone";
    constexpr char const* FALLBACK_MESSAGE = "...";

    std::string Utf8Prefix(std::string const& text, std::size_t maxBytes)
    {
        if (text.size() <= maxBytes)
            return text;

        std::size_t cut = maxBytes;
        while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
            --cut;

        return text.substr(0, cut);
    }

    // The sidecar's identifier pattern: ^[a-z][a-z0-9_]{0,63}$. Upper case is folded, other characters become underscores.
    std::string Identifier(std::string const& text)
    {
        std::string out;
        for (char c : text)
        {
            if (out.size() >= MAX_IDENTIFIER_BYTES)
                break;
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            bool const letter = c >= 'a' && c <= 'z';
            bool const digit = c >= '0' && c <= '9';
            if (out.empty() && !letter)
                continue;
            out.push_back(letter || digit ? c : '_');
        }
        return out;
    }

    boost::json::array TextList(std::vector<std::string> const& items)
    {
        boost::json::array list;
        for (std::string const& item : items)
        {
            if (list.size() >= MAX_STATE_ITEMS)
                break;
            std::string const text = Utf8Prefix(item, MAX_STATE_ITEM_BYTES);
            if (!text.empty())
                list.emplace_back(text);
        }
        return list;
    }

    boost::json::object Character(AwareAiChatCharacter const& character)
    {
        std::string name = Utf8Prefix(character.Name, MAX_NAME_BYTES);
        return boost::json::object{
            { "name", name.empty() ? std::string(FALLBACK_NAME) : name },
            { "class_name", character.ClassName },
            { "race", character.Race },
            { "level", std::clamp(character.Level, MIN_LEVEL, MAX_LEVEL) }
        };
    }
}

std::string BuildSettingsBody(AwareAiSettings const& settings)
{
    boost::json::object budget{ { "monthly_usd", RoundUsd(settings.MonthlyUsd) } };
    if (settings.DailyUsd > 0.0)
        budget["daily_usd"] = RoundUsd(settings.DailyUsd);
    else
        budget["daily_usd"] = nullptr;

    boost::json::object body{
        { "schema_version", SCHEMA_VERSION },
        { "enabled", settings.Enabled },
        { "features", boost::json::object{ { "pvp", settings.PvP }, { "chat", settings.Chat } } },
        { "budget", budget }
    };

    return boost::json::serialize(body);
}

std::string BuildMemoryEventBody(AwareAiMemoryEvent const& event)
{
    boost::json::object body{
        { "schema_version", SCHEMA_VERSION },
        { "bot_guid", event.BotGuid },
        { "player_guid", event.PlayerGuid }
    };
    if (!event.Event.empty())
        body["event"] = event.Event;
    if (std::string const detail = Utf8Prefix(event.Detail, MAX_MEMORY_DETAIL_BYTES); !detail.empty())
        body["detail"] = detail;
    if (!event.Outcome.empty())
        body["outcome"] = event.Outcome;

    return boost::json::serialize(body);
}

std::optional<AwareAiStatus> ParseStatusBody(std::string const& body)
{
    std::optional<boost::json::object> const parsed = JsonFields::ParseObject(body);
    if (!parsed)
        return std::nullopt;

    boost::json::object const& root = *parsed;
    AwareAiStatus status;
    status.Version = JsonFields::String(root, "version");
    status.Mode = JsonFields::String(root, "mode");
    status.Enabled = JsonFields::Bool(root, "enabled");
    status.HasApiKey = JsonFields::Bool(root, "has_api_key");
    status.Breaker = JsonFields::String(root, "breaker");
    status.InFlight = static_cast<uint32>(std::max(0.0, JsonFields::Number(root, "in_flight")));
    if (boost::json::object const* budget = JsonFields::Object(root, "budget"))
    {
        status.MonthSpentUsd = JsonFields::Number(*budget, "month_spent_usd");
        status.MonthCapUsd = JsonFields::Number(*budget, "month_cap_usd");
        status.DaySpentUsd = JsonFields::Number(*budget, "day_spent_usd");
        status.DayCapUsd = JsonFields::Number(*budget, "day_cap_usd");
        status.CeilingUsd = JsonFields::Number(*budget, "month_cap_ceiling_usd");
        status.CapClamped = JsonFields::Bool(*budget, "cap_clamped");
        status.Warning = JsonFields::Bool(*budget, "warning");
        status.Exhausted = JsonFields::Bool(*budget, "exhausted");
    }

    return status;
}

std::string BuildChatBody(AwareAiChatRequest const& request)
{
    std::string message = Utf8Prefix(request.Message, MAX_MESSAGE_BYTES);
    boost::json::object body{
        { "schema_version", SCHEMA_VERSION },
        { "bot_guid", request.BotGuid },
        { "player_guid", request.PlayerGuid },
        { "bot", Character(request.Bot) },
        { "player", Character(request.Player) },
        { "personality", boost::json::object{
            { "aggression", std::min(request.Aggression, MAX_TRAIT) },
            { "sociability", std::min(request.Sociability, MAX_TRAIT) },
            { "chattiness", std::min(request.Chattiness, MAX_TRAIT) },
            { "honor", std::min(request.Honor, MAX_TRAIT) } } },
        { "same_faction", request.SameFaction },
        { "zone", Utf8Prefix(request.Zone, MAX_ZONE_BYTES) },
        { "kind", request.Kind },
        { "channel", request.Channel },
        { "message", message.empty() ? std::string(FALLBACK_MESSAGE) : message },
        { "deadline_ms", std::clamp(request.DeadlineMs, MIN_CHAT_DEADLINE_MS, MAX_CHAT_DEADLINE_MS) }
    };
    if (request.GuildHumanMessageAgeS)
        body["guild_human_message_age_s"] = std::max(0.0, *request.GuildHumanMessageAgeS);
    if (request.Conversation)
        body["conversation"] = true;
    if (request.LikingPercent)
        body["liking"] = std::min(*request.LikingPercent, MAX_LIKING_PERCENT) / double(MAX_LIKING_PERCENT);
    if (std::find(RELATION_BANDS.begin(), RELATION_BANDS.end(), request.Relation) != RELATION_BANDS.end())
        body["relation"] = request.Relation;
    if (request.DenyBeingBot)
        body["deny_being_bot"] = true;

    return boost::json::serialize(body);
}

std::string BuildWipeBody(AwareAiWipe const& wipe)
{
    boost::json::object body{ { "schema_version", SCHEMA_VERSION } };
    if (wipe.PlayerGuid)
        body["player_guid"] = *wipe.PlayerGuid;
    else
        body["all"] = true;

    return boost::json::serialize(body);
}

std::optional<AwareAiWipeResult> ParseWipeBody(std::string const& body)
{
    auto const root = JsonFields::ParseObject(body);
    if (!root)
        return std::nullopt;

    auto const count = [&root](char const* key) { return static_cast<uint32>(std::max(0.0, JsonFields::Number(*root, key))); };
    AwareAiWipeResult result;
    result.MemoryEvents = count("memory_events");
    result.ChatTurns = count("chat_turns");
    result.SocialEvents = count("social_events");
    result.PairNotes = count("pair_notes");
    result.LogLines = count("log_lines");
    return result;
}

std::vector<std::string> DecideOptions(AwareAiDecideRequest const& request)
{
    std::vector<std::string> options;
    for (auto const& candidate : request.State.Candidates)
    {
        std::string const slot = Identifier(candidate.first);
        if (options.size() >= MAX_DECIDE_OPTIONS || slot.empty() || candidate.second.empty())
            continue;
        if (std::find(options.begin(), options.end(), slot) == options.end())
            options.push_back(slot);
    }
    return options;
}

std::string BuildDecideBody(AwareAiDecideRequest const& request)
{
    std::vector<std::string> const options = DecideOptions(request);
    std::string const type = Identifier(request.DecisionType);
    if (options.size() < 2 || type.empty())
        return std::string();

    AwareAiBgState const& state = request.State;
    boost::json::object candidates;
    for (std::string const& slot : options)
        for (auto const& candidate : state.Candidates)
            if (Identifier(candidate.first) == slot)
            {
                candidates[slot] = Utf8Prefix(candidate.second, MAX_STATE_ITEM_BYTES);
                break;
            }

    // The order is the order the sidecar's template prints the state in: the question first, the numbers, then the lists, the
    // candidates last. Empty lists are left out (the model reads less and the request is cheaper).
    boost::json::object content{
        { "map", Utf8Prefix(state.Map, MAX_STATE_ITEM_BYTES) },
        { "decision", Utf8Prefix(state.Decision, MAX_STATE_ITEM_BYTES) },
        { "question", Utf8Prefix(state.Question, MAX_QUESTION_BYTES) },
        { "score", boost::json::array{ state.ScoreOurs, state.ScoreTheirs } }
    };
    if (state.ScoreLimit)
        content["score_limit"] = state.ScoreLimit;
    if (state.TimeLeftS >= 0)
        content["time_left_s"] = state.TimeLeftS;
    content["stance"] = Utf8Prefix(state.Stance, MAX_STATE_ITEM_BYTES);
    for (auto const& list : { std::make_pair("nodes", &state.Nodes), std::make_pair("friendly_groups", &state.FriendlyGroups),
                              std::make_pair("observed_pressure", &state.ObservedPressure), std::make_pair("facts", &state.Facts) })
    {
        boost::json::array items = TextList(*list.second);
        if (!items.empty())
            content[list.first] = std::move(items);
    }
    content["candidates"] = candidates;

    boost::json::array optionList;
    for (std::string const& slot : options)
        optionList.emplace_back(slot);

    boost::json::object body{
        { "schema_version", SCHEMA_VERSION },
        { "decision_type", type },
        { "bot_guid", request.BotGuid },
        { "state_version", request.StateVersion },
        { "deadline_ms", std::clamp(request.DeadlineMs, MIN_DECIDE_DEADLINE_MS, MAX_DECIDE_DEADLINE_MS) },
        { "state", content },
        { "options", optionList }
    };

    return boost::json::serialize(body);
}

std::optional<AwareAiDecideResponse> ParseDecideBody(std::string const& body)
{
    std::optional<boost::json::object> const parsed = JsonFields::ParseObject(body);
    if (!parsed)
        return std::nullopt;

    AwareAiDecideResponse response;
    response.DecisionId = JsonFields::String(*parsed, "decision_id");
    response.Source = JsonFields::String(*parsed, "source");
    response.Reason = JsonFields::String(*parsed, "reason");
    response.Choice = JsonFields::String(*parsed, "choice");
    response.Template = JsonFields::String(*parsed, "template");
    response.Confidence = JsonFields::Number(*parsed, "confidence", -1.0);
    response.LatencyMs = static_cast<uint32>(std::max(0.0, JsonFields::Number(*parsed, "latency_ms")));
    response.CostUsd = std::max(0.0, JsonFields::Number(*parsed, "cost_usd"));
    return response;
}

std::string BuildOutcomeBody(AwareAiOutcome const& outcome)
{
    std::string const decisionId = Utf8Prefix(outcome.DecisionId, MAX_DECISION_ID_BYTES);
    std::string const name = Identifier(outcome.Outcome);
    if (decisionId.empty() || name.empty())
        return std::string();

    boost::json::object details;
    for (auto const& detail : outcome.Details)
    {
        std::string const key = Identifier(detail.first);
        if (!key.empty() && details.size() < MAX_STATE_ITEMS)
            details[key] = Utf8Prefix(detail.second, MAX_STATE_ITEM_BYTES);
    }

    boost::json::object body{
        { "schema_version", SCHEMA_VERSION },
        { "decision_id", decisionId },
        { "outcome", name },
        { "details", details }
    };

    return boost::json::serialize(body);
}

std::optional<AwareAiChatResponse> ParseChatBody(std::string const& body)
{
    std::optional<boost::json::object> const parsed = JsonFields::ParseObject(body);
    if (!parsed)
        return std::nullopt;

    AwareAiChatResponse response;
    response.Source = JsonFields::String(*parsed, "source");
    response.Reason = JsonFields::String(*parsed, "reason");
    response.Reply = JsonFields::String(*parsed, "reply");
    return response;
}

std::string DaySpendText(double spentUsd, double capUsd, int spentDecimals)
{
    char spent[32];
    std::snprintf(spent, sizeof(spent), "$%.*f", std::clamp(spentDecimals, 0, 6), spentUsd);
    if (!(capUsd > 0.0))
        return std::string(spent) + " today, no daily cap";

    char cap[32];
    std::snprintf(cap, sizeof(cap), "$%.2f", capUsd);
    return std::string(spent) + " of " + cap + " today";
}
