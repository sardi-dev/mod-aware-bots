"""Sidecar API v1. These models are the contract with worldserver; schemas/v1/ holds their JSON Schema export."""

from __future__ import annotations

import json
from enum import StrEnum
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field, field_validator, model_validator

SCHEMA_VERSION: Literal[1] = 1
# Keeps prompts short (latency, cost) and stops a bad caller from sending huge payloads.
MAX_STATE_JSON_BYTES = 8192
DEFAULT_MONTHLY_BUDGET_USD = 20.0

IDENTIFIER_PATTERN = r"^[a-z][a-z0-9_]{0,63}$"


class StrictModel(BaseModel):
    model_config = ConfigDict(extra="forbid", allow_inf_nan=False)


class Features(StrictModel):
    pvp: bool = True
    chat: bool = False
    personality: bool = True

    def is_enabled(self, feature: str) -> bool:
        value = getattr(self, feature, False)
        return bool(value)


class BudgetSettings(StrictModel):
    monthly_usd: float = Field(default=DEFAULT_MONTHLY_BUDGET_USD, ge=0, le=10_000)
    daily_usd: float | None = Field(default=None, ge=0, le=10_000)


class Settings(StrictModel):
    schema_version: Literal[1] = SCHEMA_VERSION
    enabled: bool = False
    features: Features = Field(default_factory=Features)
    budget: BudgetSettings = Field(default_factory=BudgetSettings)


class DecideRequest(StrictModel):
    schema_version: Literal[1]
    decision_type: str = Field(pattern=IDENTIFIER_PATTERN)
    bot_guid: int = Field(ge=0)
    # The player the decision is about; when set, the sidecar adds the bot's memory of that player to the state.
    target_guid: int | None = Field(default=None, ge=0)
    state_version: int = Field(ge=0)
    deadline_ms: int = Field(ge=50, le=5000)
    state: dict[str, Any]
    options: list[str] = Field(min_length=2, max_length=16)

    @field_validator("state")
    @classmethod
    def state_not_too_large(cls, value: dict[str, Any]) -> dict[str, Any]:
        if len(json.dumps(value, separators=(",", ":"))) > MAX_STATE_JSON_BYTES:
            raise ValueError(f"state exceeds {MAX_STATE_JSON_BYTES} bytes")
        return value

    @field_validator("options")
    @classmethod
    def options_unique(cls, value: list[str]) -> list[str]:
        if len(set(value)) != len(value):
            raise ValueError("options must be unique")
        return value


class Source(StrEnum):
    JEV = "jev"
    CACHE = "cache"
    RULES_HINT = "rules_hint"
    ERROR = "error"
    # Development only (AWARE_AI_FAKE): a deterministic stand-in answer, never charged.
    FAKE = "fake"
    LLM = "llm"
    RULES = "rules"


class DecideResponse(StrictModel):
    schema_version: Literal[1] = SCHEMA_VERSION
    decision_id: str
    source: Source
    reason: str | None = None
    choice: str | None = None
    probabilities: dict[str, float] = Field(default_factory=dict)
    confidence: float | None = None
    template: str | None = None
    latency_ms: int = 0
    cost_usd: float = 0.0


class OutcomeRequest(StrictModel):
    schema_version: Literal[1]
    decision_id: str = Field(min_length=1, max_length=64)
    outcome: str = Field(pattern=IDENTIFIER_PATTERN)
    details: dict[str, Any] = Field(default_factory=dict)


class ChatCharacter(StrictModel):
    name: str = Field(min_length=1, max_length=24)
    class_name: str = Field(pattern=IDENTIFIER_PATTERN)
    race: str = Field(pattern=IDENTIFIER_PATTERN)
    level: int = Field(ge=1, le=255)


class ChatPersonality(StrictModel):
    aggression: int = Field(ge=0, le=100)
    sociability: int = Field(ge=0, le=100)
    chattiness: int = Field(ge=0, le=100)
    honor: int = Field(ge=0, le=100)


RELATION_BANDS = ("cold", "stranger", "friendly", "close")
RelationBand = Literal["cold", "stranger", "friendly", "close"]


class ChatRequest(StrictModel):
    schema_version: Literal[1]
    bot_guid: int = Field(ge=0)
    player_guid: int = Field(ge=0)
    bot: ChatCharacter
    player: ChatCharacter
    personality: ChatPersonality
    same_faction: bool
    zone: str = Field(max_length=64)
    kind: Literal[
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
        "friend_whisper",
    ]
    conversation: bool = False
    guild_human_message_age_s: float | None = Field(default=None, ge=0)
    channel: Literal["whisper", "say", "party", "raid", "guild", "battleground"]
    # game chat lines are at most 255 bytes; worldserver may join a few queued lines into one request.
    message: str = Field(min_length=1, max_length=800)
    deadline_ms: int = Field(ge=500, le=20000)
    # Worldserver's own liking score of this bot for this human (design decision 0033 point 14): 0 to 1, and its band
    # word. When present they are the single source of truth for how the bot feels (see chat.feeling_band); absent, the
    # sidecar falls back to its own pair score. Both are optional and independent.
    liking: float | None = Field(default=None, ge=0, le=1)
    relation: RelationBand | None = None
    # Worldserver's AwareAi.Chat.DenyBeingBot: the bot denies being a bot when asked. Left out (false), it may admit it.
    deny_being_bot: bool = False


class ChatResponse(StrictModel):
    schema_version: Literal[1] = SCHEMA_VERSION
    decision_id: str
    source: Source
    reason: str | None = None
    reply: str | None = None
    template: str | None = None
    latency_ms: int = 0
    cost_usd: float = 0.0


class FightOutcome(StrEnum):
    BOT_WON = "bot_won"
    HUMAN_WON = "human_won"
    DISENGAGED = "disengaged"
    DIED_OTHER = "died_other"
    TIMEOUT = "timeout"


class MemoryEventRequest(StrictModel):
    model_config = ConfigDict(
        json_schema_extra={
            "anyOf": [
                {"required": ["event"], "properties": {"event": {"type": "string"}}},
                {"required": ["outcome"], "properties": {"outcome": {"type": "string"}}},
            ]
        }
    )

    schema_version: Literal[1]
    bot_guid: int = Field(ge=0)
    player_guid: int = Field(ge=0)
    outcome: FightOutcome | None = None
    event: (
        Literal[
            "grouped",
            "dungeon_done",
            "boss_killed_together",
            "wipe_together",
            "duel_won",
            "duel_lost",
            "loot_ninja",
            "loot_passed",
            "helped",
            "insulted",
            "thanked",
        ]
        | None
    ) = None
    detail: str | None = Field(default=None, max_length=64)

    @model_validator(mode="after")
    def require_event_or_outcome(self) -> MemoryEventRequest:
        if self.outcome is None and self.event is None:
            raise ValueError("event or outcome is required")
        return self


class WipeRequest(StrictModel):
    """Delete what the sidecar keeps about one human, or about every human (`all`). Exactly one of the two."""

    schema_version: Literal[1]
    player_guid: int | None = Field(default=None, ge=0)
    all: bool = False

    @model_validator(mode="after")
    def require_one_target(self) -> WipeRequest:
        if (self.player_guid is None) == (not self.all):
            raise ValueError("send either player_guid or all=true")
        return self


class WipeResponse(StrictModel):
    schema_version: Literal[1] = SCHEMA_VERSION
    memory_events: int = 0
    chat_turns: int = 0
    social_events: int = 0
    pair_notes: int = 0
    log_lines: int = 0


class BudgetStatus(StrictModel):
    month_spent_usd: float
    month_cap_usd: float
    day_spent_usd: float
    day_cap_usd: float | None
    day_alarm: bool = False
    month_cap_ceiling_usd: float = 20.0
    cap_clamped: bool = False
    warning: bool
    exhausted: bool


class StatusResponse(StrictModel):
    schema_version: Literal[1] = SCHEMA_VERSION
    version: str
    mode: Literal["off", "fake", "live"]
    enabled: bool
    has_api_key: bool
    fake_decisions: bool
    chat_model: str | None
    features: Features
    budget: BudgetStatus
    breaker: str
    in_flight: int
    templates: list[str]
