"""Decision orchestration: every request ends in an answer or a reason the engine's rules should decide."""

from __future__ import annotations

import json
import logging
import uuid
from dataclasses import dataclass
from datetime import datetime, timedelta

from aware_ai import __version__
from aware_ai.chat import HISTORY_HOURS, HISTORY_TURNS, ChatTemplate, build_messages, clean_reply, is_friend
from aware_ai.decision_log import DecisionLog
from aware_ai.openrouter import OpenRouterClient, ProviderError
from aware_ai.privacy import clean
from aware_ai.resilience import CircuitBreaker, Clock, DecisionCache, cache_key
from aware_ai.schemas import (
    BudgetSettings,
    BudgetStatus,
    ChatRequest,
    ChatResponse,
    DecideRequest,
    DecideResponse,
    MemoryEventRequest,
    OutcomeRequest,
    Settings,
    Source,
    StatusResponse,
    WipeRequest,
    WipeResponse,
)
from aware_ai.storage import Store
from aware_ai.templates import DecisionTemplate, TemplateError

# Below this, a provider call cannot finish before the caller's deadline, so don't start one.
MIN_CALL_BUDGET_S = 0.02
# A call's price is only known after it ends, so the pool sets aside an estimate while it runs: concurrent calls then
# see each other and the monthly cap is overshot by one estimate at most, not by one call per slot. The estimate is
# generous on purpose.
CHARS_PER_TOKEN = 3
TOKENS_PER_MILLION = 1_000_000
DECISION_RESERVE_FACTOR = 2.0
DECISION_RESERVE_MIN_USD = 0.0001


class UnknownDecisionType(ValueError):
    pass


@dataclass(frozen=True)
class ChatSettings:
    model: str | None
    max_tokens: int
    temperature: float
    zero_data_retention: bool
    usd_per_mtok_in: float
    usd_per_mtok_out: float
    reasoning_effort: str | None = "none"


@dataclass(frozen=True)
class _CachedAnswer:
    choice: str
    probabilities: dict[str, float]
    confidence: float


class DecisionService:
    def __init__(
        self,
        *,
        templates: dict[str, DecisionTemplate],
        store: Store,
        log: DecisionLog,
        client: OpenRouterClient | None,
        clock: Clock,
        breaker: CircuitBreaker,
        max_in_flight: int,
        request_timeout_s: float,
        fake_decisions: bool = False,
        chat_template: ChatTemplate | None = None,
        chat_settings: ChatSettings | None = None,
        decision_usd_per_mtok: float = 0.042,
        monthly_ceiling_usd: float = 20.0,
        day_alarm_fraction: float = 0.25,
        chat_conversation_per_min: int = 4,
        chat_bot_interval_s: float = 30.0,
        chat_human_per_minute: int = 6,
        guild_recent_s: float = 60.0,
        secret: str | None = None,
    ) -> None:
        self._templates = templates
        self._store = store
        self._log = log
        self._client = client
        self._clock = clock
        self._breaker = breaker
        self._cache = DecisionCache()
        self._max_in_flight = max_in_flight
        self._request_timeout_s = request_timeout_s
        self._in_flight = 0
        self._fake_decisions = fake_decisions
        self._chat_template = chat_template
        self._chat = chat_settings or ChatSettings(None, 120, 0.8, True, 1.0, 4.0)
        self._decision_usd_per_mtok = decision_usd_per_mtok
        self._reserved_usd = 0.0
        self._settings = store.load_settings()
        self._ceiling = max(0.0, monthly_ceiling_usd)
        self._alarm_fraction = day_alarm_fraction
        self._conversation_limit = max(0, chat_conversation_per_min)
        self._bot_interval = max(0.0, chat_bot_interval_s)
        self._human_limit = max(0, chat_human_per_minute)
        self._guild_recent = max(0.0, guild_recent_s)
        self._secret = secret
        self._bot_times: dict[int, float] = {}
        self._pair_times: dict[tuple[int, int], list[float]] = {}
        self._friend_in_flight: set[tuple[int, int]] = set()
        self._human_times: dict[int, list[float]] = {}

    @property
    def settings(self) -> Settings:
        return self._settings

    def update_settings(self, settings: Settings) -> None:
        # worldserver re-sends its settings every 30 s; only real changes are worth a log line.
        if settings == self._settings:
            return
        self._store.save_settings(settings)
        self._settings = settings
        self._log.write({"kind": "settings", "settings": settings.model_dump()}, self._clock.now())

    def _budget(self) -> BudgetSettings:
        budget = self._settings.budget
        return budget.model_copy(update={"monthly_usd": min(budget.monthly_usd, self._ceiling)})

    def _budget_status(self) -> BudgetStatus:
        now = self._clock.now()
        budget = self._store.budget_status(self._budget(), now)
        budget.day_alarm = budget.day_spent_usd > budget.month_cap_usd * self._alarm_fraction
        if budget.day_alarm and self._store.mark_day_alarm(now):
            logging.getLogger(__name__).warning(
                "Daily AI spend alarm: $%.6f of $%.2f monthly cap", budget.day_spent_usd, budget.month_cap_usd
            )
        return budget

    def _budget_blocked(self) -> bool:
        """True when the pool is spent or what is spent plus what running calls may still cost reaches a cap."""
        status = self._budget_status()
        if status.exhausted:
            return True
        if status.month_spent_usd + self._reserved_usd >= status.month_cap_usd:
            return True
        return status.day_cap_usd is not None and status.day_spent_usd + self._reserved_usd >= status.day_cap_usd

    def _decision_reserve(self, state_text: str, question: dict[str, object]) -> float:
        tokens = (len(state_text) + len(json.dumps(question))) / CHARS_PER_TOKEN
        estimate = tokens * self._decision_usd_per_mtok / TOKENS_PER_MILLION * DECISION_RESERVE_FACTOR
        return max(DECISION_RESERVE_MIN_USD, estimate)

    def _chat_reserve(self, messages: list[dict[str, str]]) -> float:
        prompt_tokens = sum(len(message["content"]) for message in messages) / CHARS_PER_TOKEN
        return (
            prompt_tokens * self._chat.usd_per_mtok_in + self._chat.max_tokens * self._chat.usd_per_mtok_out
        ) / TOKENS_PER_MILLION

    def status(self) -> StatusResponse:
        budget = self._budget_status()
        budget.month_cap_ceiling_usd = self._ceiling
        budget.cap_clamped = self._settings.budget.monthly_usd > self._ceiling
        return StatusResponse(
            version=__version__,
            mode="live" if self._client is not None else ("fake" if self._fake_decisions else "off"),
            enabled=self._settings.enabled,
            has_api_key=self._client is not None,
            fake_decisions=self._fake_decisions and self._client is None,
            chat_model=self._chat.model,
            features=self._settings.features,
            budget=budget,
            breaker=self._breaker.state(self._clock.monotonic()),
            in_flight=self._in_flight,
            templates=sorted(t.template_id for t in self._templates.values()),
        )

    def record_memory_event(self, event: MemoryEventRequest) -> None:
        now = self._clock.now()
        if event.outcome is not None:
            self._store.record_memory_event(event.bot_guid, event.player_guid, event.outcome.value, now)
            self._store.update_pair_chat(event.bot_guid, event.player_guid, now)
        if event.event is not None:
            detail = clean(event.detail, self._secret) if event.detail is not None else None
            self._store.record_social_event(event.bot_guid, event.player_guid, event.event, detail, now)

    def wipe(self, request: WipeRequest) -> WipeResponse:
        """Forget one human (or every human): database rows, log lines and the in-memory rate and cooldown books."""
        guid = request.player_guid
        counts = self._store.wipe_player(guid)
        log_lines = self._log.scrub(guid)
        if guid is None:
            self._pair_times.clear()
            self._human_times.clear()
            self._friend_in_flight.clear()
        else:
            self._pair_times = {pair: times for pair, times in self._pair_times.items() if pair[1] != guid}
            self._human_times.pop(guid, None)
            self._friend_in_flight = {pair for pair in self._friend_in_flight if pair[1] != guid}
        # Counts only, never the guid: the log must not keep what the wipe removed.
        logging.getLogger(__name__).info("Wipe: %s", counts)
        return WipeResponse(
            memory_events=counts["memory_event"],
            chat_turns=counts["chat_turn"],
            social_events=counts["social_event"],
            pair_notes=counts["pair_note"],
            log_lines=log_lines,
        )

    def record_outcome(self, outcome: OutcomeRequest) -> None:
        self._log.write(
            clean({"kind": "outcome", **outcome.model_dump(exclude={"schema_version"})}, self._secret),
            self._clock.now(),
        )

    async def decide(self, request: DecideRequest) -> DecideResponse:
        request = request.model_copy(update={"state": clean(request.state, self._secret)})
        started = self._clock.monotonic()
        template = self._templates.get(request.decision_type)
        if template is None:
            raise UnknownDecisionType(request.decision_type)
        options = template.ordered_options(request.options)
        if len(options) < 2:
            raise TemplateError("at least two known options are required")

        if request.target_guid is not None:
            memory = self._store.memory_summary(request.bot_guid, request.target_guid, self._clock.now())
            if memory is not None:
                request = request.model_copy(update={"state": {**request.state, "memory_of_target": memory}})

        response = await self._decide(request, template, options, started)
        response.latency_ms = int((self._clock.monotonic() - started) * 1000)
        self._log.write(
            {
                "kind": "decision",
                "decision_type": request.decision_type,
                "bot_guid": request.bot_guid,
                "target_guid": request.target_guid,
                "state_version": request.state_version,
                "state": request.state,
                "options": options,
                **response.model_dump(exclude={"schema_version"}),
            },
            self._clock.now(),
        )
        return response

    @staticmethod
    def _fake_answer(
        decision_id: str, template: DecisionTemplate, options: list[str], request: DecideRequest
    ) -> DecideResponse:
        # Same situation, same fake answer, so in-game tests are repeatable.
        digest = cache_key(template.template_id, options, request.state)
        choice = options[int(digest, 16) % len(options)]
        return DecideResponse(
            decision_id=decision_id,
            source=Source.FAKE,
            reason="fake_decisions",
            choice=choice,
            probabilities={option: (1.0 if option == choice else 0.0) for option in options},
            confidence=1.0,
            template=template.template_id,
        )

    async def _decide(
        self, request: DecideRequest, template: DecisionTemplate, options: list[str], started: float
    ) -> DecideResponse:
        decision_id = f"d-{uuid.uuid4().hex[:16]}"

        def hint(reason: str, source: Source = Source.RULES_HINT) -> DecideResponse:
            return DecideResponse(decision_id=decision_id, source=source, reason=reason, template=template.template_id)

        settings = self._settings
        if not settings.enabled:
            return hint("disabled")
        if not settings.features.is_enabled(template.feature):
            return hint("feature_disabled")
        if self._client is None:
            if self._fake_decisions:
                return self._fake_answer(decision_id, template, options, request)
            return hint("no_api_key")
        if self._budget_blocked():
            return hint("budget_exhausted")

        key = cache_key(template.template_id, options, request.state)
        cached = self._cache.get(key, self._clock.monotonic())
        if isinstance(cached, _CachedAnswer):
            return DecideResponse(
                decision_id=decision_id,
                source=Source.CACHE,
                choice=cached.choice,
                reason=self._reason(template, cached.choice, cached.probabilities),
                probabilities=cached.probabilities,
                confidence=cached.confidence,
                template=template.template_id,
            )

        if self._in_flight >= self._max_in_flight:
            return hint("busy")
        remaining_s = request.deadline_ms / 1000 - (self._clock.monotonic() - started)
        if remaining_s < MIN_CALL_BUDGET_S:
            return hint("deadline")
        if not self._breaker.allow(self._clock.monotonic()):
            return hint("breaker_open")

        state_text = template.render_state(request.state)
        question = template.question(options)
        reserve = self._decision_reserve(state_text, question)
        self._in_flight += 1
        self._reserved_usd += reserve
        try:
            answer = await self._client.decide(
                state_text,
                template.name,
                question,
                options,
                timeout_s=min(remaining_s, self._request_timeout_s),
            )
        except ProviderError as exc:
            self._breaker.record_failure(self._clock.monotonic())
            return hint(exc.reason, Source.ERROR)
        finally:
            self._in_flight -= 1
            self._reserved_usd -= reserve

        self._breaker.record_success()
        self._store.record_spend(answer.cost_usd, template.feature, template.name, self._clock.now())
        self._budget_status()
        self._cache.put(
            key,
            _CachedAnswer(answer.choice, answer.probabilities, answer.confidence),
            template.cache_ttl_s,
            self._clock.monotonic(),
        )
        return DecideResponse(
            decision_id=decision_id,
            source=Source.JEV,
            choice=answer.choice,
            reason=self._reason(template, answer.choice, answer.probabilities),
            probabilities=answer.probabilities,
            confidence=answer.confidence,
            template=template.template_id,
            cost_usd=answer.cost_usd,
        )

    @staticmethod
    def _reason(template: DecisionTemplate, choice: str, probabilities: dict[str, float]) -> str | None:
        """The margin of a choice for templates whose model answer has no text, else nothing."""
        if not template.reason_from_probabilities or choice not in probabilities:
            return None
        rivals = sorted((-p, name) for name, p in probabilities.items() if name != choice)
        if not rivals:
            return f"{choice} {probabilities[choice]:.2f}"
        return f"{choice} {probabilities[choice]:.2f}, next {rivals[0][1]} {-rivals[0][0]:.2f}"

    def _allow_chat(self, request: ChatRequest) -> bool:
        now = self._clock.monotonic()
        recent = [t for t in self._human_times.get(request.player_guid, []) if now - t < 60]
        previous = self._bot_times.get(request.bot_guid)
        pair = (request.bot_guid, request.player_guid)
        pair_recent = [t for t in self._pair_times.get(pair, []) if now - t < 60]
        bot_limited = (
            len(pair_recent) >= self._conversation_limit
            if request.conversation
            else (previous is not None and now - previous < self._bot_interval)
        )
        if bot_limited or len(recent) >= self._human_limit:
            return False
        self._pair_times[pair] = [*pair_recent, now]
        self._bot_times[request.bot_guid] = now
        self._human_times[request.player_guid] = [*recent, now]
        return True

    async def chat(self, request: ChatRequest) -> ChatResponse:
        request = ChatRequest.model_validate(clean(request.model_dump(), self._secret))
        started = self._clock.monotonic()
        response = await self._chat_reply(request, started)
        if request.kind not in {"opener", "friend_whisper"}:
            topic = request.message.split(" / ")[-1][:80]
            self._store.update_pair_chat(request.bot_guid, request.player_guid, self._clock.now(), topic)
        response.latency_ms = int((self._clock.monotonic() - started) * 1000)
        self._log.write(
            {
                "kind": "chat",
                "bot_guid": request.bot_guid,
                "player_guid": request.player_guid,
                "context": request.model_dump(exclude={"schema_version"}),
                **response.model_dump(exclude={"schema_version"}),
            },
            self._clock.now(),
        )
        return response

    async def _chat_reply(self, request: ChatRequest, started: float) -> ChatResponse:
        decision_id = f"c-{uuid.uuid4().hex[:16]}"
        template = self._chat_template
        template_id = template.template_id if template else None

        def hint(reason: str, source: Source = Source.RULES_HINT) -> ChatResponse:
            return ChatResponse(decision_id=decision_id, source=source, reason=reason, template=template_id)

        settings = self._settings
        if not settings.enabled:
            return hint("disabled")
        if not settings.features.chat:
            return hint("feature_disabled")
        if template is None:
            return hint("no_chat_template")
        if request.kind == "guild_reply" and (
            request.guild_human_message_age_s is None or request.guild_human_message_age_s > self._guild_recent
        ):
            return hint("guild_human_message_not_recent")
        now = self._clock.now()
        note = self._store.social_note(request.bot_guid, request.player_guid)
        if request.kind == "friend_whisper":
            score = note["score"]
            assert isinstance(score, int)
            if not is_friend(request, score):
                return hint("relation_too_low", Source.RULES)
            last = note["friend_whisper_at"]
            if (request.bot_guid, request.player_guid) in self._friend_in_flight or (
                isinstance(last, str) and now - datetime.fromisoformat(last) < timedelta(hours=12)
            ):
                return hint("friend_whisper_cooldown", Source.RULES)
        if self._client is None:
            if self._fake_decisions:
                if not self._allow_chat(request):
                    return hint("rate_limited")
                self._remember_chat(request, now)
                return ChatResponse(
                    decision_id=decision_id,
                    source=Source.FAKE,
                    reason="fake_decisions",
                    reply=f"(test reply) Hi {request.player.name}, {request.bot.name} here.",
                    template=template_id,
                )
            return hint("no_api_key")
        if self._chat.model is None:
            return hint("no_chat_model")
        now = self._clock.now()
        if self._budget_blocked():
            return hint("budget_exhausted")
        if self._in_flight >= self._max_in_flight:
            return hint("busy")
        remaining_s = request.deadline_ms / 1000 - (self._clock.monotonic() - started)
        if remaining_s < MIN_CALL_BUDGET_S:
            return hint("deadline")
        if not self._breaker.allow(self._clock.monotonic()):
            return hint("breaker_open")
        if not self._allow_chat(request):
            return hint("rate_limited")

        memory = self._store.memory_summary(request.bot_guid, request.player_guid, now)
        history = self._store.recent_chat(
            request.bot_guid, request.player_guid, now - timedelta(hours=HISTORY_HOURS), HISTORY_TURNS
        )
        messages = build_messages(template, request, memory, history, note)
        pair = (request.bot_guid, request.player_guid)
        if request.kind == "friend_whisper":
            self._friend_in_flight.add(pair)
        reserve = self._chat_reserve(messages)
        self._in_flight += 1
        self._reserved_usd += reserve
        try:
            answer = await self._client.chat(
                messages,
                model=self._chat.model,
                max_tokens=self._chat.max_tokens,
                temperature=self._chat.temperature,
                zero_data_retention=self._chat.zero_data_retention,
                usd_per_mtok_in=self._chat.usd_per_mtok_in,
                usd_per_mtok_out=self._chat.usd_per_mtok_out,
                reasoning_effort="none",
                timeout_s=remaining_s,
            )
        except ProviderError as exc:
            self._breaker.record_failure(self._clock.monotonic())
            return hint(exc.reason, Source.ERROR)
        finally:
            self._in_flight -= 1
            self._reserved_usd -= reserve
            self._friend_in_flight.discard(pair)

        self._breaker.record_success()
        self._store.record_spend(answer.cost_usd, "chat", "chat", self._clock.now())
        self._budget_status()
        reply = clean_reply(str(clean(answer.text, self._secret)), request.bot.name)
        if not reply:
            return hint("empty_reply", Source.ERROR)
        self._remember_chat(request, now)
        self._store.add_chat_turn(request.bot_guid, request.player_guid, "user", request.message, now)
        self._store.add_chat_turn(request.bot_guid, request.player_guid, "assistant", reply, self._clock.now())
        return ChatResponse(
            decision_id=decision_id,
            source=Source.LLM,
            reply=reply,
            template=template_id,
            cost_usd=answer.cost_usd,
        )

    def _remember_chat(self, request: ChatRequest, now: datetime) -> None:
        if request.kind == "friend_whisper":
            self._store.update_pair_chat(request.bot_guid, request.player_guid, now, friend_whisper=True)
