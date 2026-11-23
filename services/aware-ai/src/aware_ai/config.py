"""Process configuration, read once from environment variables at startup."""

from __future__ import annotations

import math
import os
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path

DEFAULT_OPENROUTER_BASE_URL = "https://openrouter.ai/api/v1"
# Pinned on purpose: an alias like ~typesafe/jev-latest would change bot behavior without a deploy.
DEFAULT_JEV_MODEL = "typesafe/jev-1.14"
# Listed Jev input price, only used when a response omits usage.cost.
DEFAULT_JEV_USD_PER_MTOK = 0.042
TRUE_VALUES = frozenset({"1", "true", "yes", "on"})


def _get_float(env: Mapping[str, str], name: str, default: float) -> float:
    raw = env.get(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        value = float(raw)
        if not math.isfinite(value) or value < 0:
            raise ValueError("must be finite and nonnegative")
        return value
    except ValueError as exc:
        raise ValueError(f"{name} must be a number, got {raw!r}") from exc


def _get_int(env: Mapping[str, str], name: str, default: int) -> int:
    raw = env.get(name)
    if raw is None or raw.strip() == "":
        return default
    try:
        return int(raw)
    except ValueError as exc:
        raise ValueError(f"{name} must be an integer, got {raw!r}") from exc


@dataclass(frozen=True)
class Config:
    data_dir: Path
    log_dir: Path
    template_dir: Path
    openrouter_api_key: str | None = field(default=None, repr=False)
    openrouter_base_url: str = DEFAULT_OPENROUTER_BASE_URL
    jev_model: str = DEFAULT_JEV_MODEL
    jev_usd_per_mtok: float = DEFAULT_JEV_USD_PER_MTOK
    log_retention_days: int = 90
    max_in_flight: int = 16
    # Hard ceiling per provider call; the caller's deadline is usually shorter.
    request_timeout_s: float = 2.0
    breaker_failure_threshold: int = 5
    breaker_open_s: float = 60.0
    # Development only: answer with deterministic fake choices when there is no API key.
    fake_decisions: bool = False
    monthly_ceiling_usd: float = 20.0
    day_alarm_fraction: float = 0.25
    chat_conversation_per_min: int = 4
    chat_bot_interval_s: float = 30.0
    chat_human_per_minute: int = 6
    guild_recent_s: float = 60.0
    # Chat (bot conversations). No default model on purpose: picking one is part of the AI-0 evaluation.
    chat_model: str | None = None
    chat_max_tokens: int = 120
    chat_temperature: float = 0.8
    # Only used when a response omits usage.cost; deliberately high so the budget errs on the safe side.
    chat_usd_per_mtok_in: float = 1.0
    chat_usd_per_mtok_out: float = 4.0
    # Route chat only to providers that keep no data (the user's data policy).
    chat_zero_data_retention: bool = True
    # Chat replies are short and time-boxed, so thinking only burns the token budget and the deadline (reasoning
    # tokens count toward max_tokens). Many chat models reason by default, DeepSeek V4.1 Flash at "high".
    # None sends no reasoning setting, keeping the model's default.
    chat_reasoning_effort: str | None = "none"

    @classmethod
    def from_env(cls, env: Mapping[str, str] | None = None) -> Config:
        env = os.environ if env is None else env
        key = env.get("OPENROUTER_API_KEY", "").strip() or None
        return cls(
            data_dir=Path(env.get("AWARE_AI_DATA_DIR", "/data")),
            log_dir=Path(env.get("AWARE_AI_LOG_DIR", "/logs")),
            template_dir=Path(env.get("AWARE_AI_TEMPLATE_DIR", "/app/templates")),
            openrouter_api_key=key,
            openrouter_base_url=env.get("OPENROUTER_BASE_URL", DEFAULT_OPENROUTER_BASE_URL),
            jev_model=env.get("AWARE_AI_JEV_MODEL", DEFAULT_JEV_MODEL),
            jev_usd_per_mtok=_get_float(env, "AWARE_AI_JEV_USD_PER_MTOK", DEFAULT_JEV_USD_PER_MTOK),
            log_retention_days=_get_int(env, "AWARE_AI_LOG_RETENTION_DAYS", 90),
            max_in_flight=_get_int(env, "AWARE_AI_MAX_IN_FLIGHT", 16),
            request_timeout_s=_get_float(env, "AWARE_AI_REQUEST_TIMEOUT_S", 2.0),
            breaker_failure_threshold=_get_int(env, "AWARE_AI_BREAKER_FAILURES", 5),
            breaker_open_s=_get_float(env, "AWARE_AI_BREAKER_OPEN_S", 60.0),
            fake_decisions=env.get("AWARE_AI_FAKE", "").strip().lower() in TRUE_VALUES,
            monthly_ceiling_usd=_get_float(env, "AWARE_AI_MONTHLY_CEILING_USD", 20.0),
            day_alarm_fraction=_get_float(env, "AWARE_AI_DAY_ALARM_FRACTION", 0.25),
            chat_conversation_per_min=_get_int(env, "AWARE_AI_CHAT_CONVERSATION_PER_MIN", 4),
            chat_bot_interval_s=_get_float(env, "AWARE_AI_CHAT_BOT_INTERVAL_S", 30.0),
            chat_human_per_minute=_get_int(env, "AWARE_AI_CHAT_HUMAN_PER_MINUTE", 6),
            guild_recent_s=_get_float(env, "AWARE_AI_GUILD_RECENT_S", 60.0),
            chat_model=env.get("AWARE_AI_CHAT_MODEL", "").strip() or None,
            chat_max_tokens=_get_int(env, "AWARE_AI_CHAT_MAX_TOKENS", 120),
            chat_temperature=_get_float(env, "AWARE_AI_CHAT_TEMPERATURE", 0.8),
            chat_usd_per_mtok_in=_get_float(env, "AWARE_AI_CHAT_USD_PER_MTOK_IN", 1.0),
            chat_usd_per_mtok_out=_get_float(env, "AWARE_AI_CHAT_USD_PER_MTOK_OUT", 4.0),
            chat_zero_data_retention=env.get("AWARE_AI_CHAT_ZDR", "1").strip().lower() in TRUE_VALUES,
            chat_reasoning_effort="none",
        )
