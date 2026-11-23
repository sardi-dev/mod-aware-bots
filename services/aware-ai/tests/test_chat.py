from __future__ import annotations

import json
import typing
from collections.abc import Callable
from pathlib import Path

import httpx
import pytest
from fastapi.testclient import TestClient

from aware_ai.chat import MAX_REPLY_CHARS, SITUATIONS, build_messages, clean_reply, load_chat_template
from aware_ai.config import Config
from aware_ai.schemas import ChatRequest
from aware_ai.templates import TemplateError
from conftest import FakeOpenRouter, chat_request, enable, llm_reply

CHAT_MODEL = "test/chat-model"


def enable_chat(client: TestClient) -> None:
    enable(client, features={"pvp": True, "chat": True, "personality": True})


def test_chat_off_by_default(make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable(client)

    body = client.post("/v1/chat", json=chat_request()).json()

    assert (body["source"], body["reason"]) == ("rules_hint", "feature_disabled")
    assert fake_openrouter.requests == []


def test_chat_needs_a_model(make_client: Callable[..., TestClient]) -> None:
    client = make_client()
    enable_chat(client)

    assert client.post("/v1/chat", json=chat_request()).json()["reason"] == "no_chat_model"
    assert client.get("/v1/status").json()["chat_model"] is None


def test_chat_reply_uses_persona_memory_and_privacy(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)
    client.post(
        "/v1/memory/event", json={"schema_version": 1, "bot_guid": 42, "player_guid": 7, "outcome": "human_won"}
    )
    fake_openrouter.handler = llm_reply('Ritzes: "maybe later, I just died to you"', cost=0.0002)

    body = client.post("/v1/chat", json=chat_request()).json()

    assert body["source"] == "llm"
    assert body["reply"] == "maybe later, I just died to you"
    assert body["cost_usd"] == 0.0002
    sent = fake_openrouter.requests[0]
    assert sent["model"] == CHAT_MODEL
    assert sent["provider"] == {"data_collection": "deny", "zdr": True}
    system = sent["messages"][0]["content"]
    assert "You are Ritzes, a level 32 undead rogue" in system
    assert "sarcastic" in system and "slurs" in system
    assert "they killed you 1 time(s)" in system
    assert sent["messages"][-1] == {"role": "user", "content": "hey, want to duel?"}
    assert client.get("/v1/status").json()["budget"]["month_spent_usd"] == 0.0002


def test_chat_history_is_sent_on_the_next_message(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)
    fake_openrouter.handler = llm_reply("no thanks")
    client.post("/v1/chat", json=chat_request())

    client.post("/v1/chat", json=chat_request(message="why not?"))

    messages = fake_openrouter.requests[1]["messages"]
    assert messages[1:] == [
        {"role": "user", "content": "hey, want to duel?"},
        {"role": "assistant", "content": "no thanks"},
        {"role": "user", "content": "why not?"},
    ]


def test_chat_fake_mode(make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True)
    enable_chat(client)

    body = client.post("/v1/chat", json=chat_request()).json()

    assert body["source"] == "fake"
    assert body["reply"] == "(test reply) Hi Perrin, Ritzes here."
    assert fake_openrouter.requests == []


def test_chat_provider_error_and_empty_reply(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)

    fake_openrouter.handler = lambda request: httpx.Response(429, json={"error": "slow down"})
    limited = client.post("/v1/chat", json=chat_request()).json()
    fake_openrouter.handler = llm_reply("   ")
    empty = client.post("/v1/chat", json=chat_request()).json()

    assert (limited["source"], limited["reason"]) == ("error", "rate_limited")
    assert (empty["source"], empty["reason"]) == ("error", "empty_reply")


def test_chat_is_logged(make_client: Callable[..., TestClient], make_config: Callable[..., Config]) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)
    client.post("/v1/chat", json=chat_request())

    log_dir = make_config().log_dir
    records = [json.loads(line) for line in (log_dir / "decisions-2026-10-17.jsonl").read_text().splitlines()]
    chat = next(record for record in records if record["kind"] == "chat")
    assert chat["context"]["message"] == "hey, want to duel?"
    assert chat["reply"] == "Sure, see you around."


def test_chat_rejects_oversized_or_unknown_channel(make_client: Callable[..., TestClient]) -> None:
    client = make_client(chat_model=CHAT_MODEL)

    assert client.post("/v1/chat", json=chat_request(message="x" * 801)).status_code == 422
    assert client.post("/v1/chat", json=chat_request(channel="yell")).status_code == 422


def test_clean_reply_trims_noise_and_length() -> None:
    assert clean_reply('  "hello\nthere"  ', "Ritzes") == "hello there"
    assert clean_reply("ritzes: hi", "Ritzes") == "hi"
    long_reply = clean_reply("word " * 200, "Ritzes")
    assert len(long_reply) <= MAX_REPLY_CHARS
    assert not long_reply.endswith(" ")


def test_chat_turns_reasoning_off_by_default(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)
    fake_openrouter.handler = llm_reply("sure")

    client.post("/v1/chat", json=chat_request())

    assert fake_openrouter.requests[0]["reasoning"] == {"effort": "none"}


def test_chat_always_disables_reasoning(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL, chat_reasoning_effort=None)
    enable_chat(client)
    fake_openrouter.handler = llm_reply("sure")

    client.post("/v1/chat", json=chat_request())

    assert fake_openrouter.requests[0]["reasoning"] == {"effort": "none"}


def test_chat_kinds_and_guild_gate(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True, chat_human_per_minute=100)
    enable_chat(client)
    kinds = (
        "duel_challenge",
        "duel_win",
        "duel_loss",
        "bg_win",
        "bg_loss",
        "bg_capture",
        "whisper_reply",
        "nearby_reply",
        "party_moment",
        "guild_reply",
        "opener",
    )
    for kind in kinds:
        result = client.post("/v1/chat", json=chat_request(kind=kind, guild_human_message_age_s=1)).json()
        assert result["source"] == "fake"
        assert result["cost_usd"] == 0
    assert (
        client.post("/v1/chat", json=chat_request(kind="guild_reply")).json()["reason"]
        == "guild_human_message_not_recent"
    )
    assert (
        client.post("/v1/chat", json=chat_request(kind="guild_reply", guild_human_message_age_s=61)).json()["reason"]
        == "guild_human_message_not_recent"
    )
    assert client.post("/v1/chat", json=chat_request(kind="city")).status_code == 422


def test_chat_rate_limits(make_client: Callable[..., TestClient]) -> None:
    client = make_client(openrouter_api_key=None, fake_decisions=True, chat_bot_interval_s=30, chat_human_per_minute=2)
    enable_chat(client)
    assert client.post("/v1/chat", json=chat_request()).json()["source"] == "fake"
    assert client.post("/v1/chat", json=chat_request()).json()["reason"] == "rate_limited"
    assert client.post("/v1/chat", json=chat_request(bot_guid=43)).json()["source"] == "fake"
    assert client.post("/v1/chat", json=chat_request(bot_guid=44)).json()["reason"] == "rate_limited"


def test_chat_privacy(make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)
    assert client.post("/v1/chat", json=chat_request(account_name="secret")).status_code == 422
    client.post("/v1/chat", json=chat_request(message="mail me at a@example.com or 127.0.0.1 test-key"))
    sent = str(fake_openrouter.requests)
    assert "a@example.com" not in sent and "127.0.0.1" not in sent and "test-key" not in sent


def test_chat_prompt_names_the_moment_and_the_expansion() -> None:
    template = load_chat_template(Path(__file__).resolve().parents[1] / "templates")
    assert template is not None
    request = ChatRequest.model_validate(chat_request(kind="guild_reply", channel="guild", guild_human_message_age_s=5))
    system = build_messages(template, request, None, [])[0]["content"]
    assert "MoP" in system and "Lich King" not in system
    assert "no accents" in system and "say you are not sure" in system
    assert SITUATIONS["guild_reply"] in system and "whispering" not in system
    assert set(SITUATIONS) == set(typing.get_args(ChatRequest.model_fields["kind"].annotation))


def test_chat_prompt_lets_the_bot_admit_it_unless_told_to_deny() -> None:
    template = load_chat_template(Path(__file__).resolve().parents[1] / "templates")
    assert template is not None
    open_request = ChatRequest.model_validate(chat_request())
    denying_request = ChatRequest.model_validate(chat_request(deny_being_bot=True))
    assert open_request.deny_being_bot is False

    admitting = build_messages(template, open_request, None, [])[0]["content"]
    denying = build_messages(template, denying_request, None, [])[0]["content"]

    assert "say so plainly" in admitting and "deny it playfully" not in admitting
    assert "deny it playfully" in denying and "say so plainly" not in denying
    assert "$identity" not in admitting + denying


def test_chat_template_without_the_identity_sentences_is_rejected(tmp_path: Path) -> None:
    (tmp_path / "chat").mkdir()
    old = 'version = 1\nsystem = "You are $bot_name. $identity"\n'
    (tmp_path / "chat" / "system.toml").write_text(old, encoding="utf-8")

    with pytest.raises(TemplateError):
        load_chat_template(tmp_path)


def test_chat_denial_flag_reaches_the_model(
    make_client: Callable[..., TestClient], fake_openrouter: FakeOpenRouter
) -> None:
    client = make_client(chat_model=CHAT_MODEL)
    enable_chat(client)

    client.post("/v1/chat", json=chat_request(bot_guid=1))
    client.post("/v1/chat", json=chat_request(bot_guid=2, deny_being_bot=True))
    client.post("/v1/chat", json=chat_request(bot_guid=3, deny_being_bot=False))

    prompts = [sent["messages"][0]["content"] for sent in fake_openrouter.requests]
    assert ["deny it playfully" in prompt for prompt in prompts] == [False, True, False]
    assert client.post("/v1/chat", json=chat_request(deny_being_bot="maybe")).status_code == 422


def test_clean_reply_types_like_a_player() -> None:
    raw = "*Chuckles, cracking knuckles.* Sure \u2014 I\u2019ll take you on \u201cfriend\u201d"
    assert clean_reply(raw, "Taran") == "Sure - I'll take you on friend"
