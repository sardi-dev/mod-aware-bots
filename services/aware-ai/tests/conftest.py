from __future__ import annotations

import json
from collections.abc import Callable, Iterator
from dataclasses import dataclass, field
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import Any

import httpx
import pytest
from fastapi.testclient import TestClient

from aware_ai.config import Config
from aware_ai.main import create_app

TEMPLATE_DIR = Path(__file__).resolve().parents[1] / "templates"
FAKE_BASE_URL = "https://openrouter.test/api/v1"
START = datetime(2026, 10, 17, 12, 0, tzinfo=UTC)


class FakeClock:
    def __init__(self, start: datetime = START) -> None:
        self._now = start
        self._mono = 1000.0

    def now(self) -> datetime:
        return self._now

    def monotonic(self) -> float:
        return self._mono

    def advance(self, seconds: float) -> None:
        self._now += timedelta(seconds=seconds)
        self._mono += seconds


Handler = Callable[[httpx.Request], httpx.Response]


@dataclass
class FakeOpenRouter:
    """Stands in for OpenRouter; tests swap `handler` to script responses and inspect `requests`."""

    handler: Handler | None = None
    requests: list[dict[str, Any]] = field(default_factory=list)

    def __call__(self, request: httpx.Request) -> httpx.Response:
        self.requests.append(json.loads(request.content))
        if self.handler is not None:
            return self.handler(request)
        if request.url.path.endswith("/chat/completions"):
            return llm_reply("Sure, see you around.")(request)
        # By default answer the first option the request offered, so any option list works.
        options = list(next(iter(json.loads(request.content)["questions"].values()))["criteria"])
        return jev_choice(options[0])(request)


def jev_choice(choice: str, cost: float | None = 0.00002, probabilities: dict[str, float] | None = None) -> Handler:
    def handler(request: httpx.Request) -> httpx.Response:
        body = json.loads(request.content)
        question_name, question = next(iter(body["questions"].items()))
        options = list(question["criteria"])
        probs = probabilities or {option: (0.6 if option == choice else 0.4 / (len(options) - 1)) for option in options}
        usage: dict[str, Any] = {"input_tokens": 250, "output_tokens": 0}
        if cost is not None:
            usage["cost"] = cost
        return httpx.Response(
            200,
            json={
                "model": "typesafe/jev-1.14-20261001",
                "answers": {
                    question_name: {
                        "type": "choice",
                        "choice": choice,
                        "probabilities": probs,
                        "confidence": probs[choice],
                    }
                },
                "usage": usage,
            },
        )

    return handler


def llm_reply(text: str, cost: float | None = 0.0001) -> Handler:
    def handler(request: httpx.Request) -> httpx.Response:
        usage: dict[str, Any] = {"prompt_tokens": 300, "completion_tokens": 20}
        if cost is not None:
            usage["cost"] = cost
        return httpx.Response(
            200, json={"model": "test/chat-model", "choices": [{"message": {"content": text}}], "usage": usage}
        )

    return handler


def chat_request(**overrides: Any) -> dict[str, Any]:
    body: dict[str, Any] = {
        "schema_version": 1,
        "bot_guid": 42,
        "player_guid": 7,
        "bot": {"name": "Ritzes", "class_name": "rogue", "race": "undead", "level": 32},
        "player": {"name": "Perrin", "class_name": "paladin", "race": "human", "level": 31},
        "personality": {"aggression": 80, "sociability": 20, "chattiness": 90, "honor": 10},
        "same_faction": False,
        "zone": "Stranglethorn Vale",
        "kind": "whisper_reply",
        "channel": "whisper",
        "message": "hey, want to duel?",
        "deadline_ms": 8000,
    }
    body.update(overrides)
    return body


@pytest.fixture
def clock() -> FakeClock:
    return FakeClock()


@pytest.fixture
def fake_openrouter() -> FakeOpenRouter:
    return FakeOpenRouter()


@pytest.fixture
def make_config(tmp_path: Path) -> Callable[..., Config]:
    def factory(**overrides: Any) -> Config:
        values: dict[str, Any] = {
            "data_dir": tmp_path / "data",
            "log_dir": tmp_path / "logs",
            "template_dir": TEMPLATE_DIR,
            "openrouter_api_key": "test-key",
            "chat_bot_interval_s": 0,
            "openrouter_base_url": FAKE_BASE_URL,
        }
        values.update(overrides)
        return Config(**values)

    return factory


@pytest.fixture
def make_client(
    make_config: Callable[..., Config], clock: FakeClock, fake_openrouter: FakeOpenRouter
) -> Iterator[Callable[..., TestClient]]:
    clients: list[TestClient] = []

    def factory(**config_overrides: Any) -> TestClient:
        http = httpx.AsyncClient(base_url=FAKE_BASE_URL, transport=httpx.MockTransport(fake_openrouter))
        app = create_app(config=make_config(**config_overrides), http=http, clock=clock)
        client = TestClient(app)
        client.__enter__()
        clients.append(client)
        return client

    yield factory
    for client in clients:
        client.__exit__(None, None, None)


def enable(client: TestClient, **settings: Any) -> None:
    body: dict[str, Any] = {"schema_version": 1, "enabled": True}
    body.update(settings)
    response = client.put("/v1/settings", json=body)
    assert response.status_code == 200, response.text


def engage_request(**overrides: Any) -> dict[str, Any]:
    body: dict[str, Any] = {
        "schema_version": 1,
        "decision_type": "pvp_engage",
        "bot_guid": 42,
        "state_version": 7,
        "deadline_ms": 600,
        "state": {
            "self": {"class": "rogue", "level": 32, "hp_pct": 100, "stealthed": True},
            "target": {"class": "mage", "level": 31, "hp_pct": 64, "distance_yd": 48},
            "personality": {"aggression": 80, "caution": 30},
        },
        "options": ["flee", "attack_now", "avoid"],
    }
    body.update(overrides)
    return body
