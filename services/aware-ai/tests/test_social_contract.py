from __future__ import annotations

import logging
from collections.abc import Callable
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from aware_ai.chat import archetype, build_messages, load_chat_template
from aware_ai.schemas import ChatPersonality, ChatRequest
from aware_ai.storage import RELATION_WEIGHTS, Store
from conftest import FakeClock, FakeOpenRouter, chat_request, enable, engage_request, jev_choice, llm_reply


def event(client: TestClient, name: str, detail: str | None = None) -> None:
    assert (
        client.post(
            "/v1/memory/event",
            json={"schema_version": 1, "bot_guid": 42, "player_guid": 7, "event": name, "detail": detail},
        ).status_code
        == 204
    )


def test_events_and_relation(tmp_path: Path, clock: FakeClock) -> None:
    store = Store(tmp_path / "test.sqlite")
    for name in RELATION_WEIGHTS:
        store.record_social_event(1, 2, name, "Keep", clock.now())
        assert store.social_note(1, 2)["counts"][name] == 1
    assert store.social_note(1, 2)["score"] == sum(RELATION_WEIGHTS.values())
    for _ in range(30):
        store.record_social_event(1, 2, "loot_ninja", None, clock.now())
    assert store.social_note(1, 2)["score"] == -100
    assert store.social_note(1, 2)["detail"] == "Keep"
    store.close()


def test_friend_gate_cooldown_and_fake(
    make_client: Callable[..., TestClient], clock: FakeClock, fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable(client, features={"chat": True})
    request = chat_request(kind="friend_whisper")
    result = client.post("/v1/chat", json=request).json()
    assert (result["source"], result["reason"], result["cost_usd"]) == ("rules", "relation_too_low", 0)
    event(client, "helped")
    event(client, "dungeon_done", "Shadowfang Keep")
    assert client.post("/v1/chat", json=request).json()["source"] == "fake"
    assert client.post("/v1/chat", json=request).json()["reason"] == "friend_whisper_cooldown"
    clock.advance(12 * 3600)
    assert client.post("/v1/chat", json=request).json()["source"] == "fake"
    assert not fake_openrouter.requests


def test_conversation_limit(make_client: Callable[..., TestClient], clock: FakeClock) -> None:
    client = make_client(
        openrouter_api_key=None, fake_decisions=True, chat_bot_interval_s=30, chat_human_per_minute=100
    )
    enable(client, features={"chat": True})
    request = chat_request(conversation=True, message="hello / another line")
    for _ in range(4):
        assert client.post("/v1/chat", json=request).json()["source"] == "fake"
    assert client.post("/v1/chat", json=request).json()["reason"] == "rate_limited"
    assert client.post("/v1/chat", json=chat_request(conversation=True, player_guid=8)).json()["source"] == "fake"
    clock.advance(60)
    assert client.post("/v1/chat", json=request).json()["source"] == "fake"


@pytest.mark.parametrize(
    ("aggression", "chattiness", "honor", "expected"),
    [
        (90, 25, 0, "quiet"),
        (70, 80, 40, "rough"),
        (69, 80, 50, "chatty"),
        (40, 50, 50, "friendly"),
    ],
)
def test_archetype(aggression: int, chattiness: int, honor: int, expected: str) -> None:
    personality = ChatPersonality(aggression=aggression, sociability=50, chattiness=chattiness, honor=honor)
    assert archetype(personality) == expected


def test_prompt_note_and_english() -> None:
    template = load_chat_template(Path(__file__).resolve().parents[1] / "templates")
    assert template is not None
    request = ChatRequest.model_validate(chat_request(kind="opener"))
    note: dict[str, object] = {"score": 20, "counts": {"dungeon_done": 3}, "detail": "Shadowfang Keep", "topic": "hi"}
    system = build_messages(template, request, None, [], note)[0]["content"]
    assert "run dungeons with them 3 time(s)" in system and "Shadowfang Keep" in system and "You like them." in system
    assert "Always answer in English" in system


def test_shared_budget_alarm(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter, caplog: pytest.LogCaptureFixture
) -> None:
    client = make_client(chat_model="test/model")
    enable(client, features={"chat": True, "pvp": True}, budget={"monthly_usd": 1, "daily_usd": None})
    fake_openrouter.handler = llm_reply("hi", cost=0.3)
    with caplog.at_level(logging.WARNING):
        assert client.post("/v1/chat", json=chat_request()).json()["source"] == "llm"
        status = client.get("/v1/status").json()["budget"]
        client.get("/v1/status")
    assert status["day_alarm"] and status["day_spent_usd"] == 0.3 and not status["exhausted"]
    assert sum("Daily AI spend alarm" in record.message for record in caplog.records) == 1
    fake_openrouter.handler = llm_reply("hi", cost=0.7)
    assert client.post("/v1/chat", json=chat_request()).json()["source"] == "llm"
    assert client.post("/v1/chat", json=chat_request()).json()["reason"] == "budget_exhausted"


def test_event_validation(make_client: Callable[..., TestClient]) -> None:
    client = make_client()
    body = {"schema_version": 1, "bot_guid": 42, "player_guid": 7}
    assert client.post("/v1/memory/event", json=body).status_code == 422
    assert client.post("/v1/memory/event", json={**body, "event": "helped", "detail": "x" * 65}).status_code == 422


def test_decisions_and_chat_share_budget(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model="test/model")
    enable(client, features={"chat": True, "pvp": True}, budget={"monthly_usd": 1})
    fake_openrouter.handler = jev_choice("attack_now", cost=1)
    assert client.post("/v1/decide", json=engage_request()).json()["source"] == "jev"
    assert client.post("/v1/chat", json=chat_request()).json()["reason"] == "budget_exhausted"
    assert len(fake_openrouter.requests) == 1
