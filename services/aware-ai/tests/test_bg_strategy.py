"""The battleground strategy ask (design decision 0032 point 9): fixed slots, the state as the model reads it, the
margin reason, the cache that never mixes candidate sets, the cost of one ask, and the reservation that keeps concurrent
asks inside the pool."""

from __future__ import annotations

import asyncio
import json
from collections.abc import Callable
from typing import Any

import httpx
import pytest
from fastapi.testclient import TestClient

from aware_ai.schemas import DecideRequest
from aware_ai.service import DecisionService
from aware_ai.templates import load_templates
from conftest import TEMPLATE_DIR, FakeClock, FakeOpenRouter, enable, jev_choice

SLOTS = ["option_0", "option_1", "option_2", "option_3", "option_4", "option_5"]
JEV_USD_PER_MTOK = 0.042
CHARS_PER_TOKEN = 3


def bg_request(**overrides: Any) -> dict[str, Any]:
    """A request the way the worldserver builds it for an Arathi Basin stance ask (the biggest state it sends)."""
    body: dict[str, Any] = {
        "schema_version": 1,
        "decision_type": "bg_strategy",
        "bot_guid": 1234,
        "state_version": 7,
        "deadline_ms": 600,
        "state": {
            "map": "ab",
            "decision": "stance",
            "question": "Which stance should our team play for the next half minute?",
            "score": [820, 780],
            "score_limit": 1600,
            "stance": "balanced",
            "nodes": ["stables:ours", "blacksmith:theirs", "farm:capping", "lumber_mill:ours", "gold_mine:neutral"],
            "friendly_groups": ["guards_stables:1", "guards_lumber_mill:2", "assault:4", "raider:1", "mobile:1"],
            "observed_pressure": ["blacksmith:3", "farm:2"],
            "facts": [
                "race_regime:even",
                "nodes_held:2 against 1",
                "eta_to_win_s:ours=312,theirs=466",
                "our_bots_alive:9/10",
            ],
            "candidates": {
                "option_0": "balanced",
                "option_1": "hold",
                "option_2": "push",
                "option_3": "offense",
                "option_4": "defense",
            },
        },
        "options": SLOTS[:5],
    }
    body.update(overrides)
    return body


def test_template_has_six_fixed_slots_in_order() -> None:
    template = load_templates(TEMPLATE_DIR)["bg_strategy"]

    assert template.template_id == "bg_strategy@1"
    assert template.feature == "pvp"
    assert list(template.options) == SLOTS
    assert template.reason_from_probabilities is True


def test_a_slot_answer_carries_the_margin_as_its_reason(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    probabilities = {"option_0": 0.2, "option_1": 0.7, "option_2": 0.1, "option_3": 0.0, "option_4": 0.0}
    fake_openrouter.handler = jev_choice("option_1", probabilities=probabilities)

    body = client.post("/v1/decide", json=bg_request()).json()

    assert body["source"] == "jev"
    assert body["choice"] == "option_1"
    assert body["confidence"] == 0.7
    assert body["reason"] == "option_1 0.70, next option_0 0.20"
    assert body["template"] == "bg_strategy@1"
    assert list(body["probabilities"]) == SLOTS[:5]


def test_templates_without_the_flag_keep_no_reason(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    fake_openrouter.handler = jev_choice("avoid")
    request = {
        "schema_version": 1,
        "decision_type": "pvp_engage",
        "bot_guid": 1,
        "state_version": 1,
        "deadline_ms": 600,
        "state": {"self": {"class": "rogue"}},
        "options": ["avoid", "flee"],
    }

    assert client.post("/v1/decide", json=request).json()["reason"] is None


def test_the_model_reads_the_question_the_scores_and_what_each_slot_means(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)

    client.post("/v1/decide", json=bg_request(options=["option_0", "option_1", "option_2"]))

    sent = fake_openrouter.requests[0]
    state = sent["state"]
    assert "decision: stance" in state
    assert "question: Which stance should our team play for the next half minute?" in state
    assert "score: [820; 780]" in state
    assert "nodes: [stables:ours; blacksmith:theirs; farm:capping; lumber_mill:ours; gold_mine:neutral]" in state
    assert "candidates: {option_0=balanced, option_1=hold, option_2=push, option_3=offense, option_4=defense}" in state
    # The vocabulary of the candidates is in the prompt once, so the slots can stay bare words.
    assert "all_in = behind late" in state
    assert "defend_first = run home" in state
    # Only the slots that were offered are options of the question.
    assert list(sent["questions"]["bg_strategy"]["criteria"]) == ["option_0", "option_1", "option_2"]


def test_slots_outside_the_template_and_a_single_slot_are_refused(make_client: Callable[..., TestClient]) -> None:
    client = make_client()
    enable(client)

    seventh = client.post("/v1/decide", json=bg_request(options=["option_0", "option_6"]))
    named = client.post("/v1/decide", json=bg_request(options=["balanced", "hold"]))
    single = client.post("/v1/decide", json=bg_request(options=["option_0"]))

    assert seventh.status_code == 422
    assert named.status_code == 422
    assert single.status_code == 422


def test_an_answer_for_a_slot_that_was_not_offered_is_an_error(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    fake_openrouter.handler = jev_choice("option_4", probabilities={"option_4": 1.0})

    body = client.post("/v1/decide", json=bg_request(options=["option_0", "option_1"])).json()

    assert (body["source"], body["reason"]) == ("error", "choice_not_offered")
    assert body["choice"] is None


def test_the_cache_never_answers_for_another_set_of_candidates(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, clock: FakeClock
) -> None:
    client = make_client()
    enable(client)
    same = bg_request()
    other = bg_request()
    other["state"]["candidates"] = {**same["state"]["candidates"], "option_1": "panic"}

    first = client.post("/v1/decide", json=same).json()
    repeated = client.post("/v1/decide", json=bg_request(state_version=8)).json()
    different = client.post("/v1/decide", json=other).json()
    clock.advance(13)
    expired = client.post("/v1/decide", json=same).json()

    assert first["source"] == "jev"
    # The same state (the version is not part of it) is the same question: answered from the cache, with its reason.
    assert repeated["source"] == "cache"
    assert repeated["reason"] == first["reason"]
    assert repeated["cost_usd"] == 0
    # Slot one now means another candidate: a different question, asked of the model.
    assert different["source"] == "jev"
    assert expired["source"] == "jev"
    assert len(fake_openrouter.requests) == 3


def test_a_stance_ask_costs_a_few_thousandths_of_a_cent(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)

    client.post("/v1/decide", json=bg_request())

    sent = fake_openrouter.requests[0]
    characters = len(sent["state"]) + len(json.dumps(sent["questions"]))
    tokens = characters / CHARS_PER_TOKEN
    cost = tokens * JEV_USD_PER_MTOK / 1_000_000
    # About 1150 input tokens counted at three characters each (a high estimate; at four it is about 860), so about
    # $0.00005 an ask. A 20 minute match has at most 120 asks (60 for each side): well under a cent.
    assert tokens < 1300
    assert cost < 0.00006
    assert 120 * cost < 0.0075


def test_the_worst_case_state_stays_far_below_the_sidecar_cap() -> None:
    state = bg_request()["state"]
    state["nodes"] = ["x" * 64] * 12
    state["friendly_groups"] = ["y" * 64] * 12
    state["observed_pressure"] = ["z" * 64] * 12
    state["facts"] = ["w" * 64] * 12
    state["question"] = "q" * 200

    request = DecideRequest.model_validate(bg_request(state=state))

    assert len(json.dumps(request.state, separators=(",", ":"))) < 4096


@pytest.mark.parametrize(
    ("settings", "reason"),
    [
        (None, "disabled"),
        ({"features": {"pvp": False}}, "feature_disabled"),
        ({"budget": {"monthly_usd": 0}}, "budget_exhausted"),
    ],
)
def test_the_rules_decide_when_the_layer_is_off_or_the_pool_is_spent(
    make_client: Callable[..., TestClient],
    fake_openrouter: FakeOpenRouter,
    settings: dict[str, Any] | None,
    reason: str,
) -> None:
    client = make_client()
    if settings is not None:
        enable(client, **settings)

    body = client.post("/v1/decide", json=bg_request()).json()

    assert (body["source"], body["reason"], body["choice"]) == ("rules_hint", reason, None)
    assert fake_openrouter.requests == []


def test_fake_mode_picks_a_slot_without_a_key(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable(client)

    body = client.post("/v1/decide", json=bg_request()).json()

    assert body["source"] == "fake"
    assert body["choice"] in SLOTS[:5]
    assert body["cost_usd"] == 0


def test_concurrent_asks_see_each_others_reservation(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    """Without a reservation both asks pass the budget check before either has spent anything."""
    client = make_client()
    # A pool a little under one reservation: the first ask may start (nothing is spent or reserved), a second may not.
    enable(client, budget={"monthly_usd": 0.00008})

    async def slow_answer(request: httpx.Request) -> httpx.Response:
        await asyncio.sleep(0.05)
        return jev_choice("option_0", cost=0.00002)(request)

    fake_openrouter.handler = slow_answer
    service: DecisionService = client.app.state.service  # type: ignore[attr-defined]

    async def both() -> list[Any]:
        first = DecideRequest.model_validate(bg_request())
        second = DecideRequest.model_validate(bg_request(bot_guid=99, state_version=8))
        second.state["score"] = [1, 2]
        return await asyncio.gather(service.decide(first), service.decide(second))

    answers = asyncio.run(both())

    outcomes = sorted((a.source.value, a.reason) for a in answers)
    assert outcomes == [("jev", "option_0 0.60, next option_1 0.10"), ("rules_hint", "budget_exhausted")]
    assert len(fake_openrouter.requests) == 1
    # The reservation is released: nothing is held back after the calls, and the spend is the real price.
    assert service._reserved_usd == 0
    status = client.get("/v1/status").json()["budget"]
    assert status["month_spent_usd"] == 0.00002
    assert status["exhausted"] is False


def test_an_ask_that_fails_releases_its_reservation(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)

    def timeout(request: httpx.Request) -> httpx.Response:
        raise httpx.ReadTimeout("slow", request=request)

    fake_openrouter.handler = timeout
    body = client.post("/v1/decide", json=bg_request()).json()
    service: DecisionService = client.app.state.service  # type: ignore[attr-defined]

    assert (body["source"], body["reason"]) == ("error", "timeout")
    assert service._reserved_usd == 0


def test_chat_asks_reserve_too(make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter) -> None:
    """A chat reply and a decision share the pool: while a reply runs, its estimate counts against the cap."""
    client = make_client(chat_model="test/chat-model")
    enable(client, features={"chat": True}, budget={"monthly_usd": 0.0001})
    service: DecisionService = client.app.state.service  # type: ignore[attr-defined]
    seen: list[float] = []

    async def reply(request: httpx.Request) -> httpx.Response:
        seen.append(service._reserved_usd)
        await asyncio.sleep(0)
        from conftest import llm_reply

        return llm_reply("gg", cost=0.00001)(request)

    fake_openrouter.handler = reply
    from conftest import chat_request

    body = client.post("/v1/chat", json=chat_request(kind="bg_win", channel="battleground", message="we won")).json()

    assert body["source"] == "llm"
    assert seen and seen[0] > 0
    assert service._reserved_usd == 0
