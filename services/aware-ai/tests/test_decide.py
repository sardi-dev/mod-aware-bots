from __future__ import annotations

import json
from collections.abc import Callable

import httpx
from fastapi.testclient import TestClient

from aware_ai.config import Config
from conftest import FakeClock, FakeOpenRouter, enable, engage_request, jev_choice


def test_disabled_by_default_never_calls_provider(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()

    body = client.post("/v1/decide", json=engage_request()).json()

    assert body["source"] == "rules_hint"
    assert body["reason"] == "disabled"
    assert body["choice"] is None
    assert fake_openrouter.requests == []


def test_no_api_key_answers_rules_hint(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None)
    enable(client)

    body = client.post("/v1/decide", json=engage_request()).json()

    assert body["reason"] == "no_api_key"
    assert client.get("/v1/status").json()["has_api_key"] is False


def test_feature_disabled_answers_rules_hint(make_client: Callable[..., TestClient]) -> None:
    client = make_client()
    enable(client, features={"pvp": False})

    assert client.post("/v1/decide", json=engage_request()).json()["reason"] == "feature_disabled"


def test_jev_answer_uses_template_order_and_records_cost(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, tmp_path: object
) -> None:
    client = make_client()
    enable(client)
    fake_openrouter.handler = jev_choice("avoid", cost=0.00003)

    body = client.post("/v1/decide", json=engage_request()).json()

    assert body["source"] == "jev"
    assert body["choice"] == "avoid"
    assert body["template"] == "pvp_engage@1"
    assert body["cost_usd"] == 0.00003
    assert list(body["probabilities"]) == ["attack_now", "avoid", "flee"]
    sent = fake_openrouter.requests[0]
    assert sent["model"] == "typesafe/jev-1.14"
    assert list(sent["questions"]["pvp_engage"]["criteria"]) == ["attack_now", "avoid", "flee"]
    assert "target: {class=mage" in sent["state"]
    assert client.get("/v1/status").json()["budget"]["month_spent_usd"] == 0.00003


def test_decision_is_logged_with_state(
    make_client: Callable[..., TestClient], make_config: Callable[..., Config]
) -> None:
    client = make_client()
    enable(client)
    client.post("/v1/decide", json=engage_request())

    log_dir = make_config().log_dir
    lines = (log_dir / "decisions-2026-10-17.jsonl").read_text().splitlines()
    decisions = [json.loads(line) for line in lines if json.loads(line)["kind"] == "decision"]
    assert decisions[0]["bot_guid"] == 42
    assert decisions[0]["state"]["target"]["class"] == "mage"
    assert decisions[0]["source"] == "jev"


def test_similar_state_hits_cache_within_ttl(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, clock: FakeClock
) -> None:
    client = make_client()
    enable(client)
    first = engage_request()
    nearly_same = engage_request()
    nearly_same["state"]["target"]["hp_pct"] = 62
    nearly_same["state"]["target"]["distance_yd"] = 51

    assert client.post("/v1/decide", json=first).json()["source"] == "jev"
    cached = client.post("/v1/decide", json=nearly_same).json()
    clock.advance(11)
    expired = client.post("/v1/decide", json=first).json()

    assert cached["source"] == "cache"
    assert cached["cost_usd"] == 0
    assert expired["source"] == "jev"
    assert len(fake_openrouter.requests) == 2


def test_budget_exhausted_stops_calls(make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter) -> None:
    client = make_client()
    enable(client, budget={"monthly_usd": 0.00002})
    fake_openrouter.handler = jev_choice("attack_now", cost=0.00002)

    first = client.post("/v1/decide", json=engage_request()).json()
    second = client.post("/v1/decide", json=engage_request(bot_guid=7, state_version=1, options=["flee", "avoid"]))

    assert first["source"] == "jev"
    assert second.json()["reason"] == "budget_exhausted"
    assert len(fake_openrouter.requests) == 1
    assert client.get("/v1/status").json()["budget"]["exhausted"] is True


def test_explicit_daily_cap(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, clock: FakeClock
) -> None:
    client = make_client()
    enable(client, budget={"monthly_usd": 0.0002, "daily_usd": 0.00002})
    fake_openrouter.handler = jev_choice("attack_now", cost=0.00002)

    assert client.post("/v1/decide", json=engage_request()).json()["source"] == "jev"
    blocked = client.post("/v1/decide", json=engage_request(options=["flee", "avoid"])).json()
    clock.advance(24 * 3600)
    fake_openrouter.handler = jev_choice("flee", cost=0.00002)
    next_day = client.post("/v1/decide", json=engage_request(options=["flee", "avoid"])).json()

    assert blocked["reason"] == "budget_exhausted"
    assert next_day["source"] == "jev"


def test_timeouts_open_breaker_then_half_open_trial(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, clock: FakeClock
) -> None:
    client = make_client(breaker_failure_threshold=2, breaker_open_s=60)
    enable(client)

    def timeout(request: httpx.Request) -> httpx.Response:
        raise httpx.ReadTimeout("slow", request=request)

    fake_openrouter.handler = timeout
    reasons = [
        client.post("/v1/decide", json=engage_request(state_version=i, options=["flee", "avoid"])).json()["reason"]
        for i in range(3)
    ]
    assert reasons == ["timeout", "timeout", "breaker_open"]
    assert client.get("/v1/status").json()["breaker"] == "open"

    clock.advance(61)
    fake_openrouter.handler = jev_choice("flee")
    trial = client.post("/v1/decide", json=engage_request(options=["flee", "avoid"])).json()
    assert trial["source"] == "jev"
    assert client.get("/v1/status").json()["breaker"] == "closed"


def test_provider_errors_map_to_reasons(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    cases = {402: "out_of_credit", 401: "auth", 429: "rate_limited", 503: "provider_unavailable"}
    for status, reason in cases.items():
        fake_openrouter.handler = lambda request, status=status: httpx.Response(status, json={"error": "x"})
        body = client.post("/v1/decide", json=engage_request(state_version=status)).json()
        assert (body["source"], body["reason"]) == ("error", reason)


def test_choice_outside_offered_options_is_rejected(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    fake_openrouter.handler = jev_choice("wait_for_opening", probabilities={"wait_for_opening": 1.0})

    body = client.post("/v1/decide", json=engage_request()).json()

    assert (body["source"], body["reason"]) == ("error", "choice_not_offered")


def test_cost_falls_back_to_token_price(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(jev_usd_per_mtok=0.04)
    enable(client)
    fake_openrouter.handler = jev_choice("attack_now", cost=None)

    body = client.post("/v1/decide", json=engage_request()).json()

    assert body["cost_usd"] == 250 * 0.04 / 1_000_000


def test_busy_when_in_flight_limit_reached(make_client: Callable[..., TestClient]) -> None:
    client = make_client(max_in_flight=0)
    enable(client)

    assert client.post("/v1/decide", json=engage_request()).json()["reason"] == "busy"


def test_invalid_requests_are_rejected(make_client: Callable[..., TestClient]) -> None:
    client = make_client()
    enable(client)

    unknown_type = client.post("/v1/decide", json=engage_request(decision_type="pvp_dance"))
    unknown_option = client.post("/v1/decide", json=engage_request(options=["attack_now", "dance"]))
    duplicate = client.post("/v1/decide", json=engage_request(options=["flee", "flee"]))
    huge_state = client.post("/v1/decide", json=engage_request(state={"blob": "x" * 9000}))

    assert unknown_type.status_code == 422
    assert unknown_option.status_code == 422
    assert duplicate.status_code == 422
    assert huge_state.status_code == 422


def test_outcome_is_logged(make_client: Callable[..., TestClient], make_config: Callable[..., Config]) -> None:
    client = make_client()

    response = client.post(
        "/v1/outcome",
        json={"schema_version": 1, "decision_id": "d-abc", "outcome": "won", "details": {"duration_s": 14}},
    )

    assert response.status_code == 204
    log_dir = make_config().log_dir
    records = [json.loads(line) for line in (log_dir / "decisions-2026-10-17.jsonl").read_text().splitlines()]
    assert {"kind": "outcome", "decision_id": "d-abc", "outcome": "won"}.items() <= records[-1].items()


def test_fake_mode_answers_without_key_and_without_spend(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable(client)

    first = client.post("/v1/decide", json=engage_request()).json()
    again = client.post("/v1/decide", json=engage_request(bot_guid=99)).json()
    status = client.get("/v1/status").json()

    assert first["source"] == "fake"
    assert first["choice"] in {"attack_now", "avoid", "flee"}
    assert again["choice"] == first["choice"]
    assert first["cost_usd"] == 0
    assert fake_openrouter.requests == []
    assert status["fake_decisions"] is True
    assert status["budget"]["month_spent_usd"] == 0


def test_fake_mode_never_replaces_a_real_key(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(fake_decisions=True)
    enable(client)

    body = client.post("/v1/decide", json=engage_request()).json()

    assert body["source"] == "jev"
    assert client.get("/v1/status").json()["fake_decisions"] is False


def test_fake_mode_still_respects_disabled(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)

    assert client.post("/v1/decide", json=engage_request()).json()["reason"] == "disabled"
