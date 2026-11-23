/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_PVPMATCHSTATE_H
#define _PLAYERBOT_PVPMATCHSTATE_H

// BT-480: the per-match state the PvP layers share (design decision 0032, docs/bg-sheets). One State per battleground instance and
// generation holds, for each side, what that side has learned (observations with provenance and expiry, PvpObservationMath.h)
// and who leads it (BT-481, BgLeaderPolicy.h).
//
// Ownership. A State belongs to the map thread of its battleground: its members are read and written only from the bots'
// ticks and the plans' refresh of that match, which the core runs one at a time per map. The ONLY member another thread may
// touch is the inbox, through Post(), which holds its own short lock; a session thread (a human's chat line) never reads or
// writes anything else. The sidecar client's IO thread does the same with the answers of the leader's asks (BT-502): it only
// queues them through PostAnswer(), the map thread judges them. The table's lock guards the table only and is never held while a plan scans players, calls a plan or
// sends chat; the plans' own locks come first in the lock order (plan lock, then table lock, then inbox lock), and no code
// path takes them the other way round.
//
// Lifecycle. Created by the first Touch() of a match (the plans' refresh, on the map thread), ended by End() when the leave
// path sees the match over (BGStatusAction::LeaveBG), marked ended by Touch(ended = true) when a refresh sees the match over
// (the core has no battleground end hook for modules, docs/core-hooks.md), and swept as a safety net: ended for two minutes,
// idle for ten, or over MAX_MATCHES (PvpObservationMath.h). The state holds GUIDs and value objects only, never a Player*,
// Unit* or Battleground*.
//
// Free of game types (the callers pass the clock), so tests/ run it (tests/test_pvp_match_state.cpp).

#include "AiPvpDecisionPolicy.h"
#include "BgLeaderPolicy.h"
#include "PvpObservationMath.h"
#include "PvpTeamPolicy.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace PvpMatch
{
// What one side knows and who leads it (BT-481).
struct SideState
{
    PvpObs::SideStore observations;
    BgLeader::LeaderState leader;
    AiPvp::SideAi ai;     // BT-502: the leader's asks to the decision layer and the answers that stand
    PvpTeam::TeamState team;  // BT-628: the open kill window and the control offers of the side's bots (R4, R6)
};

class State
{
public:
    State(PvpObs::MatchKey key, std::uint32_t mapId, std::uint32_t nowMs);

    PvpObs::MatchKey const& Key() const { return m_key; }
    std::uint32_t MapId() const { return m_mapId; }

    // --- map thread only ---
    SideState& Side(std::uint32_t side) { return m_side[side % PvpObs::SIDE_COUNT]; }
    SideState const& Side(std::uint32_t side) const { return m_side[side % PvpObs::SIDE_COUNT]; }

    // The messages posted since the last drain, in order of arrival, those of another generation or side left out.
    std::vector<PvpObs::Message> Drain();

    // The drained messages, each put into the store of its side (map thread, once per plan refresh). Returns the ones that were
    // new: not a duplicate of a fresh line of the same sender and not over the sender's rate. The time stamps of an observation
    // are the time of this call, so a line waits at most one refresh before its life starts.
    struct Ingested
    {
        std::uint8_t side = 0;
        PvpObs::Observation obs;
    };
    std::vector<Ingested> IngestInbox(std::uint32_t nowMs, PvpObs::Limits const& limits);

    // The queued answers of one side's asks, in order of arrival (map thread). Those of the other side stay queued.
    std::vector<AiPvp::RawAnswer> TakeAnswers(std::uint32_t side);

    // --- any thread ---
    // Queues one immutable message for the map thread. False when the inbox is full (the line is dropped).
    bool Post(PvpObs::Message message);

    // Queues the answer of an ask for the map thread (the sidecar client's IO thread). False when the queue is full.
    bool PostAnswer(AiPvp::RawAnswer answer);

    // Bookkeeping the table keeps through the handle. Written by the map thread, read by whichever thread sweeps the table.
    std::atomic<std::uint32_t> lastTouchMs{ 0 };
    std::atomic<std::uint32_t> lastElapsedMs{ 0 };
    std::atomic<bool> ended{ false };
    std::atomic<std::uint32_t> endedMs{ 0 };

private:
    PvpObs::MatchKey m_key;
    std::uint32_t m_mapId;
    SideState m_side[PvpObs::SIDE_COUNT];
    std::mutex m_inboxLock;
    PvpObs::Inbox m_inbox;
    std::mutex m_answerLock;
    std::vector<AiPvp::RawAnswer> m_answers;
};

using Handle = std::shared_ptr<State>;

// The state of an instance, created by the first call (map thread). elapsedMs is the instance clock: a reading far below the
// last one is a reused instance and starts a new generation. ended marks a match the caller saw over. Sweeps the table when
// a state is created and at most once a minute otherwise.
Handle Touch(std::uint32_t instanceId, std::uint32_t mapId, std::uint32_t elapsedMs, bool ended, std::uint32_t nowMs);

// The current state of an instance, nullptr when none (any thread). The handle keeps the state alive for the caller.
Handle Find(std::uint32_t instanceId);

// The match is over: drop its state (the leave path). A late Post to the dropped state is harmless.
void End(std::uint32_t instanceId);

// States in the table, and a drop of everything (tests).
std::size_t Size();
void Clear();
}  // namespace PvpMatch

#endif
