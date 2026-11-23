from __future__ import annotations

from datetime import UTC, datetime, timedelta
from pathlib import Path

import pytest

from aware_ai.config import Config
from aware_ai.decision_log import DecisionLog
from aware_ai.export_schemas import SCHEMA_DIR, render_schemas
from aware_ai.resilience import CircuitBreaker, DecisionCache, cache_key
from aware_ai.schemas import BudgetSettings
from aware_ai.storage import Store
from aware_ai.templates import TemplateError, load_templates, parse_template
from conftest import TEMPLATE_DIR

NOW = datetime(2026, 10, 17, 12, 0, tzinfo=UTC)


def test_committed_json_schemas_match_models() -> None:
    for name, text in render_schemas().items():
        committed = (SCHEMA_DIR / f"{name}.json").read_text(encoding="utf-8")
        assert committed == text, f"schemas/v1/{name}.json is stale; run python -m aware_ai.export_schemas"


def test_shipped_templates_load() -> None:
    templates = load_templates(TEMPLATE_DIR)

    engage = templates["pvp_engage"]
    assert engage.feature == "pvp"
    assert next(iter(engage.options)) == "attack_now"


def test_ordered_options_follow_template_and_reject_unknown() -> None:
    engage = load_templates(TEMPLATE_DIR)["pvp_engage"]

    assert engage.ordered_options(["flee", "attack_now"]) == ["attack_now", "flee"]
    with pytest.raises(TemplateError):
        engage.ordered_options(["attack_now", "dance"])


def test_render_state_is_compact_and_skips_nulls() -> None:
    engage = load_templates(TEMPLATE_DIR)["pvp_engage"]

    text = engage.render_state(
        {"self": {"level": 32, "mana_pct": None, "stealthed": True}, "allies": [{"class": "priest"}], "skip": None}
    )

    assert text.splitlines()[-2:] == ["self: {level=32, stealthed=yes}", "allies: [{class=priest}]"]


@pytest.mark.parametrize(
    "data",
    [
        {
            "name": "x",
            "version": 1,
            "feature": "pvp",
            "intro": "i",
            "instructions": "q",
            "options": [{"name": "a", "criteria": "c"}],
        },
        {
            "name": "x",
            "version": 1,
            "feature": "pvp",
            "intro": "i",
            "instructions": "q",
            "options": [{"name": "a", "criteria": "c"}, {"name": "a", "criteria": "d"}],
        },
        {
            "name": "Bad Name",
            "version": 1,
            "feature": "pvp",
            "intro": "i",
            "instructions": "q",
            "options": [{"name": "a", "criteria": "c"}, {"name": "b", "criteria": "d"}],
        },
        {
            "name": "x",
            "version": "1",
            "feature": "pvp",
            "intro": "i",
            "instructions": "q",
            "options": [{"name": "a", "criteria": "c"}, {"name": "b", "criteria": "d"}],
        },
    ],
)
def test_invalid_templates_rejected(data: dict[str, object]) -> None:
    with pytest.raises(TemplateError):
        parse_template(data, Path("bad.toml"))


def test_breaker_opens_after_threshold_and_recovers() -> None:
    breaker = CircuitBreaker(failure_threshold=2, open_s=10)

    breaker.record_failure(0)
    assert breaker.allow(1)
    breaker.record_failure(1)
    assert breaker.state(2) == "open" and not breaker.allow(2)
    assert breaker.state(11) == "half_open"
    assert breaker.allow(11) and not breaker.allow(11)
    breaker.record_failure(11)
    assert breaker.state(12) == "open"
    assert breaker.allow(21)
    breaker.record_success()
    assert breaker.state(21) == "closed"


def test_cache_expires_and_evicts_oldest() -> None:
    cache = DecisionCache(max_entries=2)
    cache.put("a", 1, ttl_s=5, now=0)
    cache.put("b", 2, ttl_s=5, now=0)
    cache.put("c", 3, ttl_s=5, now=0)

    assert cache.get("a", 1) is None
    assert cache.get("b", 1) == 2
    assert cache.get("b", 5) is None
    cache.put("d", 4, ttl_s=0, now=0)
    assert cache.get("d", 0) is None


def test_cache_key_buckets_percent_and_distance_only() -> None:
    base = {"t": {"hp_pct": 64, "distance_yd": 48, "level": 31}}

    assert cache_key("x@1", ["a"], base) == cache_key(
        "x@1", ["a"], {"t": {"hp_pct": 61, "distance_yd": 52, "level": 31}}
    )
    assert cache_key("x@1", ["a"], base) != cache_key(
        "x@1", ["a"], {"t": {"hp_pct": 64, "distance_yd": 48, "level": 32}}
    )
    assert cache_key("x@1", ["a"], base) != cache_key("x@2", ["a"], base)


def test_budget_periods_are_utc_day_and_month(tmp_path: Path) -> None:
    store = Store(tmp_path / "s.sqlite3")
    store.record_spend(0.5, "pvp", "pvp_engage", NOW - timedelta(days=20))
    store.record_spend(0.2, "pvp", "pvp_engage", NOW - timedelta(hours=13))
    store.record_spend(0.1, "pvp", "pvp_engage", NOW)

    status = store.budget_status(BudgetSettings(monthly_usd=1.0), NOW)

    assert status.month_spent_usd == pytest.approx(0.3)
    assert status.day_spent_usd == pytest.approx(0.1)
    assert status.day_cap_usd is None
    assert status.exhausted is False
    assert status.warning is False
    store.close()


def test_budget_warning_at_eighty_percent(tmp_path: Path) -> None:
    store = Store(tmp_path / "s.sqlite3")
    store.record_spend(8.0, "pvp", "pvp_engage", NOW - timedelta(days=2))

    status = store.budget_status(BudgetSettings(monthly_usd=10.0, daily_usd=5.0), NOW)

    assert status.warning is True and status.exhausted is False
    store.close()


def test_decision_log_prunes_old_files(tmp_path: Path) -> None:
    log = DecisionLog(tmp_path, retention_days=90)
    log.write({"kind": "x"}, NOW - timedelta(days=91))
    log.write({"kind": "x"}, NOW)
    (tmp_path / "notes.txt").write_text("keep")

    removed = log.prune(NOW)

    assert [p.name for p in removed] == ["decisions-2026-07-18.jsonl"]
    assert sorted(p.name for p in tmp_path.iterdir()) == ["decisions-2026-10-17.jsonl", "notes.txt"]


def test_config_from_env_hides_key_and_validates_numbers() -> None:
    config = Config.from_env({"OPENROUTER_API_KEY": " sk-secret ", "AWARE_AI_MAX_IN_FLIGHT": "4"})

    assert config.openrouter_api_key == "sk-secret"
    assert config.max_in_flight == 4
    assert config.fake_decisions is False
    assert Config.from_env({"AWARE_AI_FAKE": "true"}).fake_decisions is True
    assert "sk-secret" not in repr(config)
    assert Config.from_env({"OPENROUTER_API_KEY": "  "}).openrouter_api_key is None
    with pytest.raises(ValueError, match="AWARE_AI_REQUEST_TIMEOUT_S"):
        Config.from_env({"AWARE_AI_REQUEST_TIMEOUT_S": "fast"})


def test_config_chat_reasoning_defaults_to_off() -> None:
    assert Config.from_env({}).chat_reasoning_effort == "none"
    assert Config.from_env({"AWARE_AI_CHAT_REASONING_EFFORT": "low"}).chat_reasoning_effort == "none"
