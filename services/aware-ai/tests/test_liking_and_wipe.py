from __future__ import annotations

import json
from collections.abc import Callable
from datetime import UTC, datetime
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from aware_ai.chat import (
    FRIENDLY_BANDS,
    band_of,
    build_messages,
    describe_feeling,
    feeling_band,
    is_friend,
    load_chat_template,
)
from aware_ai.config import Config
from aware_ai.decision_log import DecisionLog
from aware_ai.export_schemas import SCHEMA_DIR, render_schemas
from aware_ai.schemas import ChatRequest, WipeRequest
from conftest import FakeClock, FakeOpenRouter, chat_request, enable, engage_request, llm_reply

TEMPLATES = Path(__file__).resolve().parents[1] / "templates"


def request_with(**overrides: object) -> ChatRequest:
    return ChatRequest.model_validate(chat_request(**overrides))


def system_prompt(**overrides: object) -> str:
    template = load_chat_template(TEMPLATES)
    assert template is not None
    note: dict[str, object] = {"score": 0, "counts": {}}
    return build_messages(template, request_with(**overrides), None, [], note)[0]["content"]


def event(client: TestClient, name: str, bot: int = 42, player: int = 7) -> None:
    body = {"schema_version": 1, "bot_guid": bot, "player_guid": player, "event": name}
    assert client.post("/v1/memory/event", json=body).status_code == 204


# --- the request fields -----------------------------------------------------------------------------------------


def test_fields_are_optional_and_validated() -> None:
    assert request_with().liking is None and request_with().relation is None
    assert request_with(liking=0.0, relation="cold").relation == "cold"
    assert request_with(liking=1.0).liking == 1.0
    for bad in ({"liking": -0.01}, {"liking": 1.01}, {"relation": "adored"}, {"liking": float("nan")}):
        with pytest.raises(ValueError):
            request_with(**bad)


def test_http_accepts_the_fields_and_still_forbids_unknown_ones(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable(client, features={"chat": True})
    ok = client.post("/v1/chat", json=chat_request(liking=0.62, relation="friendly"))
    assert ok.status_code == 200 and ok.json()["source"] == "fake"
    assert client.post("/v1/chat", json=chat_request(liking=62)).status_code == 422
    assert client.post("/v1/chat", json=chat_request(affinity=0.5)).status_code == 422


# --- one score in the end ---------------------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("liking", "band"),
    [
        (0.0, "cold"),
        (0.19, "cold"),
        (0.2, "stranger"),
        (0.54, "stranger"),
        (0.55, "friendly"),
        (0.74, "friendly"),
        (0.75, "close"),
        (1.0, "close"),
    ],
)
def test_band_edges_match_worldserver_defaults(liking: float, band: str) -> None:
    assert band_of(liking) == band
    assert feeling_band(request_with(liking=liking)) == band


def test_relation_wins_over_liking_and_both_win_over_nothing() -> None:
    # Worldserver may retune its edges, so the band it names is the truth; liking alone is derived with the defaults.
    assert feeling_band(request_with(liking=0.9, relation="stranger")) == "stranger"
    assert feeling_band(request_with(relation="close")) == "close"
    assert feeling_band(request_with()) is None


def test_prompt_uses_worldserver_score_not_the_sidecar_score() -> None:
    template = load_chat_template(TEMPLATES)
    assert template is not None
    # The sidecar's own score says friend (30) but worldserver says cold: the cold band is what the prompt carries.
    note: dict[str, object] = {"score": 30, "counts": {"dungeon_done": 2}}
    system = build_messages(template, request_with(liking=0.12, relation="cold"), None, [], note)[0]["content"]
    assert "You do not like them" in system and "Your feeling for them: cold, 12 of 100." in system
    assert "You like them." not in system
    # And the other way round: the sidecar holds a grudge, worldserver says close.
    note = {"score": -50, "counts": {}}
    system = build_messages(template, request_with(liking=0.8, relation="close"), None, [], note)[0]["content"]
    assert "close friend" in system and "grudge" not in system


def test_prompt_falls_back_to_the_sidecar_score_without_the_fields() -> None:
    template = load_chat_template(TEMPLATES)
    assert template is not None
    liked = build_messages(template, request_with(), None, [], {"score": 20, "counts": {}})[0]["content"]
    grudge = build_messages(template, request_with(), None, [], {"score": -5, "counts": {}})[0]["content"]
    assert "You like them." in liked and "grudge" in grudge and "Your feeling" not in liked


def test_stranger_band_adds_no_warmth_and_no_grudge() -> None:
    stranger = describe_feeling(request_with(liking=0.3, relation="stranger"))
    assert stranger == "(Your feeling for them: stranger, 30 of 100.)"
    assert "feeling for them: friendly." in (describe_feeling(request_with(relation="friendly")) or "")
    assert describe_feeling(request_with()) is None


def test_prompt_carries_the_band_and_percent_only() -> None:
    system = system_prompt(liking=0.62, relation="friendly")
    assert "friendly, 62 of 100" in system
    for leaked in ("0.62", "pair_note", "event"):
        assert leaked not in system


def test_friend_gate_uses_the_band_when_sent() -> None:
    assert not is_friend(request_with(relation="stranger"), own_score=90)
    assert not is_friend(request_with(liking=0.54), own_score=90)
    assert is_friend(request_with(relation="friendly"), own_score=-10)
    assert is_friend(request_with(liking=0.8), own_score=0)
    # Nothing sent: the sidecar's own friendly threshold of 20, as before.
    assert is_friend(request_with(), own_score=20) and not is_friend(request_with(), own_score=19)
    assert frozenset({"friendly", "close"}) == FRIENDLY_BANDS


def test_friend_whisper_follows_worldserver_over_its_own_score(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable(client, features={"chat": True})
    # The sidecar's own pair score is high (friend), worldserver says stranger: no whisper.
    event(client, "dungeon_done")
    event(client, "helped")
    result = client.post("/v1/chat", json=chat_request(kind="friend_whisper", liking=0.3, relation="stranger")).json()
    assert (result["source"], result["reason"]) == ("rules", "relation_too_low")
    # Worldserver says friendly for a pair the sidecar has never seen: the whisper goes ahead.
    fresh = chat_request(kind="friend_whisper", bot_guid=99, liking=0.62, relation="friendly")
    assert client.post("/v1/chat", json=fresh).json()["source"] == "fake"
    # The cooldown still applies on top of the score.
    assert client.post("/v1/chat", json=fresh).json()["reason"] == "friend_whisper_cooldown"
    assert not fake_openrouter.requests


def test_live_prompt_reaches_the_model_with_the_feeling(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model="test/chat-model")
    enable(client, features={"chat": True})
    fake_openrouter.handler = llm_reply("hey", cost=0.0001)
    assert client.post("/v1/chat", json=chat_request(liking=0.8, relation="close")).json()["source"] == "llm"
    system = fake_openrouter.requests[0]["messages"][0]["content"]
    assert "close friend" in system and "close, 80 of 100" in system


# --- the wipe endpoint ------------------------------------------------------------------------------------------


def seed(client: TestClient) -> None:
    """Two humans (7 and 8) with every kind of row, through the public endpoints."""
    for player in (7, 8):
        event(client, "dungeon_done", player=player)
        client.post(
            "/v1/memory/event",
            json={"schema_version": 1, "bot_guid": 42, "player_guid": player, "outcome": "human_won"},
        )
        assert client.post("/v1/chat", json=chat_request(player_guid=player)).json()["source"] == "llm"


def counts(config: Config) -> dict[str, dict[int, int]]:
    import sqlite3

    db = sqlite3.connect(config.data_dir / "aware-ai.sqlite3")
    try:
        return {
            table: dict(db.execute(f"SELECT player_guid, COUNT(*) FROM {table} GROUP BY player_guid").fetchall())
            for table in ("memory_event", "chat_turn", "social_event", "pair_note")
        }
    finally:
        db.close()


def test_wipe_one_human_deletes_all_their_rows_and_nothing_else(
    make_client: Callable[..., TestClient],
    make_config: Callable[..., Config],
    fake_openrouter: FakeOpenRouter,
    clock: FakeClock,
    tmp_path: Path,
) -> None:
    config = make_config(chat_model="test/chat-model")
    client = make_client(chat_model="test/chat-model")
    enable(client, features={"chat": True})
    fake_openrouter.handler = llm_reply("hey there", cost=0.0001)
    seed(client)
    before = counts(config)
    assert all(set(rows) == {7, 8} for rows in before.values())

    response = client.post("/v1/memory/wipe", json={"schema_version": 1, "player_guid": 7})

    assert response.status_code == 200
    body = response.json()
    assert body["memory_events"] == 1 and body["social_events"] == 1 and body["chat_turns"] == 2
    assert body["pair_notes"] == 1 and body["log_lines"] == 1
    after = counts(config)
    assert all(set(rows) == {8} for rows in after.values())
    # The other human's memory still answers, the wiped one has none.
    assert client.get("/v1/status").json()["budget"]["month_spent_usd"] > 0
    again = client.post("/v1/memory/wipe", json={"schema_version": 1, "player_guid": 7}).json()
    assert again == {
        "schema_version": 1,
        "memory_events": 0,
        "chat_turns": 0,
        "social_events": 0,
        "pair_notes": 0,
        "log_lines": 0,
    }


def test_wipe_resets_the_friend_gate_and_the_rate_books(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True, chat_human_per_minute=1)
    enable(client, features={"chat": True})
    assert client.post("/v1/chat", json=chat_request()).json()["source"] == "fake"
    assert client.post("/v1/chat", json=chat_request(bot_guid=43)).json()["reason"] == "rate_limited"
    client.post("/v1/memory/wipe", json={"schema_version": 1, "player_guid": 7})
    assert client.post("/v1/chat", json=chat_request(bot_guid=44)).json()["source"] == "fake"


def test_wipe_all_keeps_the_spend_ledger_and_settings(
    make_client: Callable[..., TestClient],
    make_config: Callable[..., Config],
    fake_openrouter: FakeOpenRouter,
    clock: FakeClock,
    tmp_path: Path,
) -> None:
    config = make_config(chat_model="test/chat-model")
    client = make_client(chat_model="test/chat-model")
    enable(client, features={"chat": True})
    fake_openrouter.handler = llm_reply("hey", cost=0.0004)
    seed(client)
    spent = client.get("/v1/status").json()["budget"]["month_spent_usd"]

    body = client.post("/v1/memory/wipe", json={"schema_version": 1, "all": True}).json()

    assert body["chat_turns"] == 4 and body["social_events"] == 2 and body["memory_events"] == 2
    assert all(not rows for rows in counts(config).values())
    status = client.get("/v1/status").json()
    assert status["budget"]["month_spent_usd"] == spent and status["enabled"] and status["features"]["chat"]


@pytest.mark.parametrize(
    "body",
    [
        {"schema_version": 1},
        {"schema_version": 1, "player_guid": 7, "all": True},
        {"schema_version": 1, "player_guid": -1},
        {"schema_version": 1, "player_guid": 7, "bot_guid": 1},
        {"schema_version": 2, "player_guid": 7},
        {"player_guid": 7},
    ],
)
def test_wipe_request_is_strict(make_client: Callable[..., TestClient], body: dict[str, object]) -> None:
    assert make_client().post("/v1/memory/wipe", json=body).status_code == 422


def test_wipe_request_model() -> None:
    assert WipeRequest.model_validate({"schema_version": 1, "player_guid": 0}).player_guid == 0
    assert WipeRequest.model_validate({"schema_version": 1, "all": True}).all is True


# --- the log files ----------------------------------------------------------------------------------------------


def test_wipe_scrubs_the_jsonl_logs_of_that_human_only(
    make_client: Callable[..., TestClient],
    make_config: Callable[..., Config],
    fake_openrouter: FakeOpenRouter,
    clock: FakeClock,
) -> None:
    config = make_config(chat_model="test/chat-model")
    client = make_client(chat_model="test/chat-model")
    enable(client, features={"chat": True, "pvp": True})
    fake_openrouter.handler = llm_reply("hey", cost=0.0001)
    client.post("/v1/chat", json=chat_request(player_guid=7))
    client.post("/v1/chat", json=chat_request(player_guid=8, bot_guid=43))
    client.post("/v1/decide", json={**engage_request(), "target_guid": 7})
    log_file = next(config.log_dir.glob("decisions-*.jsonl"))
    before = [json.loads(line) for line in log_file.read_text(encoding="utf-8").splitlines()]
    assert sum(1 for r in before if r.get("player_guid") == 7 or r.get("target_guid") == 7) == 2

    body = client.post("/v1/memory/wipe", json={"schema_version": 1, "player_guid": 7}).json()

    assert body["log_lines"] == 2
    after = [json.loads(line) for line in log_file.read_text(encoding="utf-8").splitlines()]
    assert not any(r.get("player_guid") == 7 or r.get("target_guid") == 7 for r in after)
    assert any(r.get("player_guid") == 8 for r in after)
    # The settings line has no human and stays.
    assert any(r.get("kind") == "settings" for r in after)
    assert not list(config.log_dir.glob("*.tmp"))


def test_scrub_all_removes_every_human_line_and_tolerates_junk(tmp_path: Path) -> None:
    log = DecisionLog(tmp_path, 90)
    now = datetime(2026, 11, 22, 12, 0, tzinfo=UTC)
    log.write({"kind": "chat", "player_guid": 5}, now)
    log.write({"kind": "decision", "target_guid": 5}, now)
    log.write({"kind": "decision", "target_guid": None, "bot_guid": 1}, now)
    log.write({"kind": "outcome"}, now)
    path = log.path_for(now)
    with path.open("a", encoding="utf-8") as handle:
        handle.write("not json\n")
    assert log.scrub(None) == 2
    kept = path.read_text(encoding="utf-8").splitlines()
    assert len(kept) == 3 and "not json" in kept
    assert log.scrub(5) == 0


# --- the contract files -----------------------------------------------------------------------------------------


def test_exported_schemas_match_the_models_and_name_the_new_fields() -> None:
    for name, text in render_schemas().items():
        assert (SCHEMA_DIR / f"{name}.json").read_text(encoding="utf-8") == text
    chat = json.loads((SCHEMA_DIR / "chat_request.json").read_text(encoding="utf-8"))
    assert chat["properties"]["liking"]["anyOf"][0] == {"maximum": 1.0, "minimum": 0.0, "type": "number"}
    assert "relation" in chat["properties"] and "liking" not in chat["required"]
    assert chat["properties"]["deny_being_bot"]["default"] is False and "deny_being_bot" not in chat["required"]
    assert chat["additionalProperties"] is False
    wipe = json.loads((SCHEMA_DIR / "wipe_request.json").read_text(encoding="utf-8"))
    assert wipe["additionalProperties"] is False and wipe["required"] == ["schema_version"]
