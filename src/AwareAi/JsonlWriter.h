/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AWARE_AI_JSONL_WRITER_H
#define AWARE_AI_JSONL_WRITER_H

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

// Appends one JSON object per line to <dir>/<prefix>-YYYY-MM-DD.jsonl (UTC day) on its own thread, so callers on map
// threads never wait for disk I/O. SkyFire has no producer/consumer queue in its shared library, so the queue is
// local to this class.
class JsonlWriter
{
public:
    JsonlWriter() = default;
    ~JsonlWriter();

    JsonlWriter(JsonlWriter const&) = delete;
    JsonlWriter& operator=(JsonlWriter const&) = delete;

    void Start(std::filesystem::path directory, std::string prefix);
    // Thread-safe and non-blocking; dropped silently when the writer is not running.
    void Write(std::string line);
    // Writes everything still queued, then stops the thread.
    void Stop();

    [[nodiscard]] bool IsRunning() const { return _thread.joinable(); }

private:
    void Run();
    void OpenFor(std::string const& day);

    std::mutex _mutex;
    std::condition_variable _wake;
    std::deque<std::string> _queue;
    bool _stopping = false;
    std::thread _thread;
    std::filesystem::path _directory;
    std::string _prefix;
    std::string _openDay;
    std::ofstream _file;
};

#endif
