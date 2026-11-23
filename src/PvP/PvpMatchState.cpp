/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE). See PvpMatchState.h for what this is.
 */

#include "PvpMatchState.h"

#include <unordered_map>

namespace PvpMatch
{
namespace
{
struct Table
{
    std::mutex lock;
    std::unordered_map<std::uint32_t, Handle> states;
    std::uint32_t nextGeneration = 1;
    std::uint32_t lastSweepMs = 0;
};

Table& Instance()
{
    static Table table;
    return table;
}

// Called with the table lock held.
void SweepLocked(Table& table, std::uint32_t nowMs)
{
    table.lastSweepMs = PvpObs::Stamp(nowMs);

    std::vector<std::uint32_t> ids;
    std::vector<PvpObs::MatchStamp> stamps;
    ids.reserve(table.states.size());
    stamps.reserve(table.states.size());
    for (auto const& entry : table.states)
    {
        ids.push_back(entry.first);
        stamps.push_back({ entry.second->lastTouchMs, entry.second->ended, entry.second->endedMs });
    }

    for (std::size_t index : PvpObs::SweepSelect(stamps, nowMs))
        table.states.erase(ids[index]);
}
}  // namespace

State::State(PvpObs::MatchKey key, std::uint32_t mapId, std::uint32_t nowMs)
    : m_key(key), m_mapId(mapId)
{
    lastTouchMs = PvpObs::Stamp(nowMs);
}

std::vector<PvpObs::Message> State::Drain()
{
    std::vector<PvpObs::Message> all;
    {
        std::lock_guard<std::mutex> guard(m_inboxLock);
        all = m_inbox.Drain();
    }

    std::vector<PvpObs::Message> valid;
    valid.reserve(all.size());
    for (PvpObs::Message const& m : all)
        if (PvpObs::MessageValid(m, m_key.generation))
            valid.push_back(m);
    return valid;
}

std::vector<State::Ingested> State::IngestInbox(std::uint32_t nowMs, PvpObs::Limits const& limits)
{
    std::vector<Ingested> accepted;
    for (PvpObs::Message const& m : Drain())
        if (Side(m.side).observations.Add(m.obs, nowMs, limits) == PvpObs::AddResult::ADDED)
            accepted.push_back({ m.side, Side(m.side).observations.items.back() });
    return accepted;
}

std::vector<AiPvp::RawAnswer> State::TakeAnswers(std::uint32_t side)
{
    std::vector<AiPvp::RawAnswer> mine;
    std::lock_guard<std::mutex> guard(m_answerLock);
    for (auto itr = m_answers.begin(); itr != m_answers.end();)
    {
        if (itr->side == side % PvpObs::SIDE_COUNT)
        {
            mine.push_back(std::move(*itr));
            itr = m_answers.erase(itr);
        }
        else
            ++itr;
    }
    return mine;
}

bool State::PostAnswer(AiPvp::RawAnswer answer)
{
    std::lock_guard<std::mutex> guard(m_answerLock);
    if (m_answers.size() >= AiPvp::MAX_QUEUED_ANSWERS)
        return false;
    m_answers.push_back(std::move(answer));
    return true;
}

bool State::Post(PvpObs::Message message)
{
    std::lock_guard<std::mutex> guard(m_inboxLock);
    return m_inbox.Push(message);
}

Handle Touch(std::uint32_t instanceId, std::uint32_t mapId, std::uint32_t elapsedMs, bool ended, std::uint32_t nowMs)
{
    Table& table = Instance();
    Handle handle;
    {
        std::lock_guard<std::mutex> guard(table.lock);
        auto itr = table.states.find(instanceId);
        bool const restart = itr != table.states.end() && PvpObs::IsRestart(itr->second->lastElapsedMs, elapsedMs);
        if (itr == table.states.end() || restart)
        {
            PvpObs::MatchKey const key{ instanceId, table.nextGeneration++ };
            Handle fresh = std::make_shared<State>(key, mapId, nowMs);
            if (restart)
                itr->second = fresh;
            else
                itr = table.states.emplace(instanceId, fresh).first;
            SweepLocked(table, nowMs);
            // The sweep may have dropped nothing but this state is the table's own: look it up again, it is never swept
            // in the call that created it (its touch is now).
            itr = table.states.find(instanceId);
        }
        else if (PvpObs::Reached(nowMs, table.lastSweepMs + PvpObs::SWEEP_INTERVAL_MS))
            SweepLocked(table, nowMs);

        handle = itr->second;
    }

    handle->lastTouchMs = PvpObs::Stamp(nowMs);
    handle->lastElapsedMs = elapsedMs;
    if (ended && !handle->ended)
    {
        handle->ended = true;
        handle->endedMs = PvpObs::Stamp(nowMs);
    }
    return handle;
}

Handle Find(std::uint32_t instanceId)
{
    Table& table = Instance();
    std::lock_guard<std::mutex> guard(table.lock);
    auto const itr = table.states.find(instanceId);
    return itr == table.states.end() ? Handle() : itr->second;
}

void End(std::uint32_t instanceId)
{
    Table& table = Instance();
    std::lock_guard<std::mutex> guard(table.lock);
    table.states.erase(instanceId);
}

std::size_t Size()
{
    Table& table = Instance();
    std::lock_guard<std::mutex> guard(table.lock);
    return table.states.size();
}

void Clear()
{
    Table& table = Instance();
    std::lock_guard<std::mutex> guard(table.lock);
    table.states.clear();
    table.lastSweepMs = 0;
}
}  // namespace PvpMatch
