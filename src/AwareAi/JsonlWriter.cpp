/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "JsonlWriter.h"
#include "Log.h"
#include <ctime>
#include <system_error>
#include <utility>

namespace
{
    constexpr std::size_t DAY_BUFFER_SIZE = 16;

    std::string UtcDay()
    {
        std::time_t const now = std::time(nullptr);
        std::tm utc{};
        gmtime_r(&now, &utc);
        char buffer[DAY_BUFFER_SIZE];
        std::size_t const length = std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &utc);
        return std::string(buffer, length);
    }
}

JsonlWriter::~JsonlWriter()
{
    Stop();
}

void JsonlWriter::Start(std::filesystem::path directory, std::string prefix)
{
    if (IsRunning())
        return;

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error)
    {
        SF_LOG_ERROR("playerbots.ai", "Cannot create log directory %s: %s. JSONL logging stays off.",
            directory.string().c_str(), error.message().c_str());
        return;
    }

    _directory = std::move(directory);
    _prefix = std::move(prefix);
    {
        std::lock_guard<std::mutex> guard(_mutex);
        _queue.clear();
        _stopping = false;
    }

    _thread = std::thread(&JsonlWriter::Run, this);
}

void JsonlWriter::Write(std::string line)
{
    if (!IsRunning() || line.empty())
        return;

    {
        std::lock_guard<std::mutex> guard(_mutex);
        _queue.push_back(std::move(line));
    }

    _wake.notify_one();
}

void JsonlWriter::Stop()
{
    if (!IsRunning())
        return;

    {
        std::lock_guard<std::mutex> guard(_mutex);
        _stopping = true;
    }

    _wake.notify_one();
    _thread.join();
    _file.close();
    _openDay.clear();
}

void JsonlWriter::OpenFor(std::string const& day)
{
    if (day == _openDay && _file.is_open())
        return;

    _file.close();
    _file.clear();
    std::filesystem::path const path = _directory / (_prefix + "-" + day + ".jsonl");
    _file.open(path, std::ios::app);
    _openDay = day;
    if (!_file.is_open())
        SF_LOG_ERROR("playerbots.ai", "Cannot open %s for appending.", path.string().c_str());
}

void JsonlWriter::Run()
{
    for (;;)
    {
        std::deque<std::string> batch;
        {
            std::unique_lock<std::mutex> guard(_mutex);
            _wake.wait(guard, [this]() { return _stopping || !_queue.empty(); });
            batch.swap(_queue);
            // Leave only when stopped and everything queued before Stop() has been taken.
            if (batch.empty())
                return;
        }

        OpenFor(UtcDay());
        if (!_file.is_open())
            continue;

        for (std::string const& line : batch)
            _file << line << '\n';

        _file.flush();
    }
}
