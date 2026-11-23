from __future__ import annotations

import json
from collections.abc import Callable
from typing import Any

from fastapi.testclient import TestClient

from conftest import FakeClock, FakeOpenRouter, enable, engage_request


def fight(client: TestClient, outcome: str, bot: int = 42, player: int = 7) -> None:
    body: dict[str, Any] = {"schema_version": 1, "bot_guid": bot, "player_guid": player, "outcome": outcome}
    response = client.post("/v1/memory/event", json=body)
    assert response.status_code == 204, response.text


def test_decision_state_gets_memory_of_target(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    fight(client, "human_won")
    fight(client, "human_won")
    fight(client, "bot_won")
    fight(client, "disengaged")
    fight(client, "human_won", bot=99)

    client.post("/v1/decide", json=engage_request(target_guid=7))

    sent = fake_openrouter.requests[0]["state"]
    assert "memory_of_target: {fights=4, kills=1, deaths=2, disengages=1, grudge=38, last_seen_min=0}" in sent


def test_no_memory_without_target_or_history(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client()
    enable(client)
    fight(client, "human_won", player=8)

    client.post("/v1/decide", json=engage_request())
    client.post("/v1/decide", json=engage_request(target_guid=7, options=["flee", "avoid"]))

    assert all("memory_of_target" not in request["state"] for request in fake_openrouter.requests)


def test_grudge_fades_after_thirty_days(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, clock: FakeClock
) -> None:
    client = make_client()
    enable(client)
    fight(client, "human_won")
    clock.advance(31 * 24 * 3600)

    client.post("/v1/decide", json=engage_request(target_guid=7))

    state = fake_openrouter.requests[0]["state"]
    assert "deaths=1" in state
    assert "grudge=0" in state


def test_memory_is_logged_with_the_decision(
    make_client: Callable[..., TestClient], make_config: Callable[..., Any]
) -> None:
    client = make_client()
    enable(client)
    fight(client, "bot_won")

    client.post("/v1/decide", json=engage_request(target_guid=7))

    log_dir = make_config().log_dir
    records = [json.loads(line) for line in (log_dir / "decisions-2026-10-17.jsonl").read_text().splitlines()]
    decision = next(record for record in records if record["kind"] == "decision")
    assert decision["target_guid"] == 7
    assert decision["state"]["memory_of_target"]["kills"] == 1


def test_unknown_fight_outcome_is_rejected(make_client: Callable[..., TestClient]) -> None:
    client = make_client()

    response = client.post(
        "/v1/memory/event", json={"schema_version": 1, "bot_guid": 1, "player_guid": 2, "outcome": "draw"}
    )

    assert response.status_code == 422
