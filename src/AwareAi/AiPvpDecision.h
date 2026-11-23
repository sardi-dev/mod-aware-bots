/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#ifndef AI_PVP_DECISION_H
#define AI_PVP_DECISION_H

// BT-502: the battleground leader's asks to the decision layer (design decision 0032 point 9, design decision 0014 and 0026). The pure part (slots,
// pacing, judging, the book) is Utils/AiPvpDecisionPolicy.h; this file is its link to the sidecar, the config and the log.
//
// The plans publish their deterministic plan first and keep playing it. Twice in a refresh they call in here, on the map thread
// of the match:
//  - Receive, from the hook of the leader's tick: the answers that came in are judged against the lists as published now and
//    the accepted ones replace the rule's answer (the leader's stance for D1, the book for the other points).
//  - Ask, at the end of the refresh: when the pacing allows and a point has two materially different legal options, one
//    request goes out. The sidecar client's IO thread only queues the answer (PvpMatch::State::PostAnswer); it judges nothing
//    and touches no game object.
// An answer that is late, stale, from another leader or match, not offered any more, below the confidence or not from a decision
// is refused and the rule plan simply goes on. Every answer prints one line (see Receive).

#include "AwareAiProtocol.h"
#include "BgLeaderPolicy.h"
#include "PvpMatchState.h"

#include <cstdint>
#include <functional>

namespace AiPvpDecision
{
// AwareAi.Enable, AwareAi.Pvp.Enable and the leader on, the sidecar client running with its breaker closed.
bool Enabled();

// Judges and applies the answers queued for this side. Called inside the leader's tick (BgLeader::TickWith), after the lists of
// this refresh are published, so an answer is judged against the list it would change.
void Receive(PvpMatch::State& match, std::uint32_t side, BgLeader::LeaderState& leader, std::uint32_t nowMs);

// What the plan tells the model about the match; called only when a request goes out, so the plan builds it lazily. The glue
// adds the decision, the question, the stance and the candidates.
using DescribeFn = std::function<void(AwareAiBgState&)>;

// Decides whether this side asks now and sends the request. `eventSignature` changes with the flags (or nodes) and the
// captures: a change is a flag or node event. Nothing is asked before `started` (the gates are open).
void Ask(PvpMatch::Handle const& match, std::uint32_t side, BgLeader::LeaderState const& leader, std::uint64_t eventSignature,
         std::uint32_t nowMs, bool started, DescribeFn const& describe);

// The match is over: drops what is awaited, prints the summary of the side once and reports the outcome of its accepted
// decisions. result is 1 for a win of this side, -1 for a loss, 0 for a draw.
void MatchOver(PvpMatch::State& match, std::uint32_t side, int result, std::uint32_t nowMs);
}  // namespace AiPvpDecision

#endif
