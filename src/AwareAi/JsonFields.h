/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_JSON_FIELDS_H
#define AWARE_AI_JSON_FIELDS_H

#include <boost/json.hpp>
#include <optional>
#include <string>
#include <string_view>

// Lenient readers for sidecar responses: a missing or mistyped field yields the default instead of throwing.
namespace JsonFields
{
    inline std::optional<boost::json::object> ParseObject(std::string_view body)
    {
        boost::system::error_code error;
        boost::json::value value = boost::json::parse(body, error);
        if (error || !value.is_object())
            return std::nullopt;

        return std::move(value.as_object());
    }

    inline boost::json::object const* Object(boost::json::object const& parent, std::string_view key)
    {
        boost::json::value const* value = parent.if_contains(key);
        return value ? value->if_object() : nullptr;
    }

    inline bool Bool(boost::json::object const& parent, std::string_view key, bool fallback = false)
    {
        boost::json::value const* value = parent.if_contains(key);
        return value && value->is_bool() ? value->as_bool() : fallback;
    }

    inline double Number(boost::json::object const& parent, std::string_view key, double fallback = 0.0)
    {
        boost::json::value const* value = parent.if_contains(key);
        if (!value)
            return fallback;

        if (value->is_double())
            return value->as_double();

        if (value->is_int64())
            return static_cast<double>(value->as_int64());

        if (value->is_uint64())
            return static_cast<double>(value->as_uint64());

        return fallback;
    }

    inline std::string String(boost::json::object const& parent, std::string_view key, std::string fallback = {})
    {
        boost::json::value const* value = parent.if_contains(key);
        return value && value->is_string() ? std::string(value->as_string()) : fallback;
    }
}

#endif
