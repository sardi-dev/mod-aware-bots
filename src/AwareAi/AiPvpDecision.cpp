/*
 * This file is part of mod-aware-bots-548, a playerbots module for SkyFire 5.4.8. Released under the GNU GPL
 * v2 license; you can redistribute it and/or modify it under version 2 of the License, or (at your option) any
 * later version. See LICENSE in the module folder.
 */

#include "AiPvpDecision.h"

#include "AiPvpDecisionPolicy.h"
#include "AwareAiClient.h"
#include "AwareAiConfig.h"
#include "BgLeaderAdapters.h"
#include "Log.h"
#include "PvpConfig.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr char const* DECIDE_TARGET = "/v1/decide";
constexpr char const* OUTCOME_TARGET = "/v1/outcome";
// The client waits this much longer than the sidecar's deadline for the sidecar's own answer (a rules hint) to arrive.
constexpr Milliseconds HTTP_MARGIN = 300ms;
constexpr Milliseconds OUTCOME_TIMEOUT = 2000ms;
// Outcomes are a courtesy: they never take more than this many of the client's requests in flight.
constexpr uint32 OUTCOME_IN_FLIGHT_LIMIT = 8;
// The state of a request shows the time left in steps of this many seconds, so near-equal states share a cached answer.
constexpr int32 TIME_STEP_S = 10;

char const* SideName(std::uint32_t side)
{
    return side == 0 ? "A" : "H";
}

std::string OfferedText(AiPvp::PendingAsk const& pending)
{
    std::string text;
    for (AiPvp::Slot const& slot : pending.slots.slots)
    {
        if (!text.empty())
            text += ",";
        text += BgLeader::CandidateName(pending.point, slot.optionId);
    }
    return text.empty() ? "-" : text;
}

// The word of the slot the answer named, "-" when it named none of the slots.
std::string ChoseText(AiPvp::PendingAsk const& pending, std::string const& choice)
{
    std::optional<std::uint8_t> const option = pending.slots.OptionOf(choice);
    return option ? BgLeader::CandidateName(pending.point, *option) : "-";
}

void PostOutcome(std::string const& decisionId, std::string const& outcome,
                 std::vector<std::pair<std::string, std::string>> details)
{
    if (decisionId.empty() || !sAwareAiClient.IsRunning() || sAwareAiClient.InFlight() >= OUTCOME_IN_FLIGHT_LIMIT)
        return;

    AwareAiOutcome body;
    body.DecisionId = decisionId;
    body.Outcome = outcome;
    body.Details = std::move(details);
    std::string const text = BuildOutcomeBody(body);
    if (text.empty())
        return;

    sAwareAiClient.Send(AwareAiHttpMethod::Post, OUTCOME_TARGET, text, OUTCOME_TIMEOUT, [](AwareAiResponse) {});
}

// The reason a refused answer printed: the reason and, for the two that come from outside, what was said.
std::string ReasonText(AiPvp::Reject reason, AiPvp::RawAnswer const& answer)
{
    std::string text = AiPvp::RejectName(reason);
    if (reason == AiPvp::Reject::TRANSPORT && !answer.error.empty())
        text += "(" + answer.error + ")";
    else if (reason == AiPvp::Reject::SOURCE && !answer.reason.empty())
        text += "(" + answer.reason + ")";
    return text;
}

}  // namespace

namespace AiPvpDecision
{
bool Enabled()
{
    AwareAiConfig const& config = sAwareAiConfig;
    return config.Enabled && config.PvPEnabled && PvpConfig::LeaderEnabled() && sAwareAiClient.IsRunning() &&
           !sAwareAiClient.IsBreakerOpen();
}

void Receive(PvpMatch::State& match, std::uint32_t side, BgLeader::LeaderState& leader, std::uint32_t nowMs)
{
    AiPvp::SideAi& ai = match.Side(side).ai;
    std::vector<AiPvp::RawAnswer> const answers = match.TakeAnswers(side);
    AwareAiConfig const& config = sAwareAiConfig;

    AiPvp::ReceiveInput in;
    in.key = match.Key();
    in.side = static_cast<std::uint8_t>(side);
    in.nowMs = nowMs;
    in.minConfidencePct = config.PvpMinConfidencePct;
    in.lifeMs = AiPvp::AnswerLifeMs(config.PvpBgIntervalSeconds);
    in.matchOver = match.ended;

    AiPvp::ProcessAnswers(ai, leader, in, answers,
        [side, nowMs](AiPvp::PendingAsk const& pending, AiPvp::RawAnswer const& answer, AiPvp::Judgement const& verdict)
        {
            // One line per answer: what was offered, what the model said, whether the plan took it and why not.
            bool const same = pending.requestId == answer.requestId;
            bool const awaited = same && pending.active;
            std::string const source = answer.transportOk && !answer.source.empty() ? answer.source : "none";
            SF_LOG_INFO("playerbots.ai",
                        "[BGAI] side=%s point=%s offered=%s chose=%s source=%s latency=%u cost=%.6f accepted=%s reason=%s conf=%.2f "
                        "wait=%u",
                        SideName(side), AiPvp::PointName(answer.point), same ? OfferedText(pending).c_str() : "-",
                        same ? ChoseText(pending, answer.choice).c_str() : "-", source.c_str(), answer.latencyMs,
                        answer.costUsd, verdict.accept ? "yes" : "no", ReasonText(verdict.reason, answer).c_str(),
                        std::max(0.0, answer.confidence), awaited ? nowMs - pending.sentMs : 0u);

            // The outcome of the decision as the plan took it. A refusal before a decision exists (no reply) has no id.
            std::string const outcome =
                verdict.accept ? "accepted" : std::string("rejected_") + AiPvp::RejectName(verdict.reason);
            PostOutcome(answer.decisionId, outcome, { { "point", AiPvp::PointName(answer.point) }, { "side", SideName(side) } });
        });
}

void Ask(PvpMatch::Handle const& match, std::uint32_t side, BgLeader::LeaderState const& leader, std::uint64_t eventSignature,
         std::uint32_t nowMs, bool started, DescribeFn const& describe)
{
    AiPvp::SideAi& ai = match->Side(side).ai;
    AiPvp::ObserveEvents(ai, eventSignature, nowMs);

    AwareAiConfig const& config = sAwareAiConfig;
    AiPvp::GateInput gate;
    gate.enabled = Enabled();
    gate.started = started;
    gate.matchOver = match->ended;
    gate.leaderPresent = leader.election.leader != 0;
    gate.nowMs = nowMs;
    gate.intervalMs = AiPvp::ClampIntervalS(config.PvpBgIntervalSeconds) * 1000u;
    gate.deadlineMs = config.DecisionDeadlineMs;
    gate.maxRequests = config.PvpMaxRequestsPerMatchSide;

    AiPvp::AskPlan const plan = AiPvp::PlanAsk(ai, gate, leader.offer);
    if (plan.gate == AiPvp::Gate::CAP && !ai.capLogged)
    {
        ai.capLogged = true;
        SF_LOG_INFO("playerbots.ai", "[BGAI] side=%s point=- offered=- chose=- source=none latency=0 cost=0.000000 accepted=no "
                    "reason=cap(%u asks, AwareAi.Pvp.MaxRequestsPerMatchSide)", SideName(side), ai.pacing.requests);
    }
    if (!plan.ask)
        return;

    AwareAiDecideRequest request;
    request.BotGuid = leader.election.leader;
    request.StateVersion = plan.stateVersion;
    request.DeadlineMs = gate.deadlineMs;
    describe(request.State);
    request.State.Decision = AiPvp::PointName(plan.point);
    request.State.Question = AiPvp::PointQuestion(plan.point);
    request.State.Stance = BgLeader::StanceName(BgLeader::Effective(leader));
    if (request.State.TimeLeftS >= 0)
        request.State.TimeLeftS = request.State.TimeLeftS / TIME_STEP_S * TIME_STEP_S;
    if (plan.point != BgLeader::DecisionPoint::D1_STANCE)
        for (std::string const& fact : ai.book.At(plan.point).facts)
            request.State.Facts.push_back(fact);
    for (std::size_t i = 0; i < plan.slots.slots.size(); ++i)
        request.State.Candidates.emplace_back(AiPvp::SlotName(i), BgLeader::CandidateName(plan.point, plan.slots.slots[i].optionId));

    std::string const body = BuildDecideBody(request);
    if (body.empty())
        return;

    // What the IO thread needs to tag the answer with; it queues it and returns (no judging, no game object).
    AiPvp::RawAnswer tag;
    tag.key = match->Key();
    tag.side = static_cast<std::uint8_t>(side);
    tag.leaderEpoch = leader.election.epoch;
    tag.requestId = ai.pacing.requestId + 1;
    tag.stateVersion = plan.stateVersion;
    tag.point = plan.point;

    bool const sent = sAwareAiClient.Send(AwareAiHttpMethod::Post, DECIDE_TARGET, body,
        Milliseconds(gate.deadlineMs) + HTTP_MARGIN, [match, tag](AwareAiResponse response)
        {
            AiPvp::RawAnswer answer = tag;
            answer.transportOk = response.Ok;
            answer.error = response.Error;
            if (response.Ok)
            {
                std::optional<AwareAiDecideResponse> const parsed = ParseDecideBody(response.Body);
                if (!parsed)
                {
                    answer.transportOk = false;
                    answer.error = "malformed";
                }
                else
                {
                    answer.source = parsed->Source;
                    answer.reason = parsed->Reason;
                    answer.choice = parsed->Choice;
                    answer.decisionId = parsed->DecisionId;
                    answer.confidence = parsed->Confidence;
                    answer.latencyMs = parsed->LatencyMs;
                    answer.costUsd = parsed->CostUsd;
                }
            }
            match->PostAnswer(std::move(answer));
        });

    if (!sent)
    {
        // Busy, paused or stopped: not counted, and the side waits for its next interval.
        AiPvp::AskRefused(ai, nowMs);
        SF_LOG_DEBUG("playerbots.ai", "[BGAI-ASK] side=%s point=%s refused by the sidecar client", SideName(side),
                     AiPvp::PointName(plan.point));
        return;
    }

    AiPvp::BeginAsk(ai, plan, leader.election.epoch, gate.deadlineMs, nowMs);
    SF_LOG_DEBUG("playerbots.ai", "[BGAI-ASK] side=%s point=%s request=%u version=%u slots=%u", SideName(side),
                 AiPvp::PointName(plan.point), tag.requestId, plan.stateVersion, uint32(plan.slots.slots.size()));
}

void MatchOver(PvpMatch::State& match, std::uint32_t side, int result, std::uint32_t /*nowMs*/)
{
    AiPvp::SideAi& ai = match.Side(side).ai;
    if (ai.summarised)
        return;
    ai.summarised = true;
    AiPvp::OnMatchOver(ai);

    if (!ai.pacing.requests && !ai.refused)
        return;

    SF_LOG_INFO("playerbots.ai", "[BGAI] summary side=%s asks=%u accepted=%u rejected=%u refused=%u cost=%.6f result=%s",
                SideName(side), ai.pacing.requests, ai.accepted, ai.rejected, ai.refused, ai.costUsd,
                result > 0 ? "won" : result < 0 ? "lost" : "draw");

    // What became of the decisions the plan took: the sidecar's log joins them to the match result.
    char const* const outcome = result > 0 ? "match_won" : result < 0 ? "match_lost" : "match_draw";
    for (std::string const& id : ai.acceptedIds)
        PostOutcome(id, outcome, { { "side", SideName(side) } });
}
}  // namespace AiPvpDecision
