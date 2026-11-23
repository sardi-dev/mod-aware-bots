from __future__ import annotations

import json
from collections.abc import Callable

from fastapi.testclient import TestClient

from aware_ai.config import Config
from conftest import enable


def test_defaults_are_off_with_twenty_dollar_cap(make_client: Callable[..., TestClient]) -> None:
    status = make_client().get("/v1/status").json()

    assert status["enabled"] is False
    assert status["features"] == {"pvp": True, "chat": False, "personality": True}
    assert status["budget"]["month_cap_usd"] == 20.0
    assert status["budget"]["day_cap_usd"] is None
    assert status["templates"] == ["bg_strategy@1", "pvp_engage@1", "pvp_reassess@1"]


def test_settings_persist_across_restart(make_client: Callable[..., TestClient]) -> None:
    first = make_client()
    enable(first, budget={"monthly_usd": 25, "daily_usd": 3}, features={"chat": True})

    restarted = make_client().get("/v1/status").json()

    assert restarted["enabled"] is True
    assert restarted["features"]["chat"] is True
    assert restarted["budget"]["month_cap_usd"] == 20
    assert restarted["budget"]["day_cap_usd"] == 3


def test_invalid_settings_rejected(make_client: Callable[..., TestClient]) -> None:
    client = make_client()

    negative = client.put("/v1/settings", json={"schema_version": 1, "budget": {"monthly_usd": -1}})
    unknown_field = client.put("/v1/settings", json={"schema_version": 1, "turbo": True})

    assert negative.status_code == 422
    assert unknown_field.status_code == 422
    assert client.get("/v1/status").json()["enabled"] is False


def test_unchanged_settings_are_not_logged_again(
    make_client: Callable[..., TestClient], make_config: Callable[..., Config]
) -> None:
    client = make_client()
    for _ in range(3):
        enable(client)
    enable(client, budget={"monthly_usd": 5})

    log_dir = make_config().log_dir
    lines = [json.loads(line) for line in (log_dir / "decisions-2026-10-17.jsonl").read_text().splitlines()]
    assert [line["settings"]["budget"]["monthly_usd"] for line in lines if line["kind"] == "settings"] == [20.0, 5.0]


def test_worldserver_contract_and_environment_ceiling(make_client: Callable[..., TestClient]) -> None:
    client = make_client(monthly_ceiling_usd=12)
    # Exact shape constructed by BuildSettingsBody in AwareAiProtocol.cpp.
    response = client.put(
        "/v1/settings",
        json={
            "schema_version": 1,
            "enabled": True,
            "features": {"pvp": True},
            "budget": {"monthly_usd": 20.0, "daily_usd": None},
        },
    )
    assert response.status_code == 200
    status = client.get("/v1/status").json()
    assert status["mode"] == "live"
    assert status["budget"]["month_cap_usd"] == 12
    assert status["budget"]["month_cap_ceiling_usd"] == 12
    assert status["budget"]["cap_clamped"] is True
    assert status["budget"]["day_cap_usd"] is None
    for key in ("version", "mode", "enabled", "has_api_key", "breaker", "in_flight", "budget"):
        assert key in status
    for key in ("month_spent_usd", "month_cap_usd", "day_spent_usd", "day_cap_usd", "warning", "exhausted"):
        assert key in status["budget"]
    client.put("/v1/settings", json={"budget": {"monthly_usd": 4}})
    assert client.get("/v1/status").json()["budget"]["month_cap_usd"] == 4
