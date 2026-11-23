"""HTTP API. Reachable only on the Compose network; worldserver is the only caller."""

from __future__ import annotations

from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

import httpx
from fastapi import FastAPI, HTTPException, Request

from aware_ai import __version__
from aware_ai.chat import load_chat_template
from aware_ai.config import Config
from aware_ai.decision_log import DecisionLog
from aware_ai.openrouter import OpenRouterClient
from aware_ai.resilience import CircuitBreaker, Clock, SystemClock
from aware_ai.schemas import (
    ChatRequest,
    ChatResponse,
    DecideRequest,
    DecideResponse,
    MemoryEventRequest,
    OutcomeRequest,
    Settings,
    StatusResponse,
    WipeRequest,
    WipeResponse,
)
from aware_ai.service import ChatSettings, DecisionService, UnknownDecisionType
from aware_ai.storage import Store
from aware_ai.templates import TemplateError, load_templates

# Spend rows older than this are dropped at startup; only the current month counts toward the cap.
SPEND_HISTORY_DAYS = 400
DB_FILENAME = "aware-ai.sqlite3"


def _build_http_client(config: Config) -> httpx.AsyncClient:
    return httpx.AsyncClient(
        base_url=config.openrouter_base_url,
        http2=True,
        headers={"Authorization": f"Bearer {config.openrouter_api_key}", "X-Title": "aware-ai"},
        timeout=config.request_timeout_s,
        limits=httpx.Limits(max_connections=config.max_in_flight, max_keepalive_connections=config.max_in_flight),
    )


def create_app(
    config: Config | None = None, http: httpx.AsyncClient | None = None, clock: Clock | None = None
) -> FastAPI:
    config = config or Config.from_env()
    clock = clock or SystemClock()

    @asynccontextmanager
    async def lifespan(app: FastAPI) -> AsyncIterator[None]:
        store = Store(config.data_dir / DB_FILENAME)
        log = DecisionLog(config.log_dir, config.log_retention_days)
        now = clock.now()
        log.prune(now)
        store.prune_spend(now, SPEND_HISTORY_DAYS)
        store.prune_memory(now)
        store.prune_chat(now)
        client_http = http
        owns_http = False
        if client_http is None and config.openrouter_api_key:
            client_http = _build_http_client(config)
            owns_http = True
        client = (
            OpenRouterClient(client_http, config.jev_model, config.jev_usd_per_mtok)
            if client_http is not None and config.openrouter_api_key
            else None
        )
        app.state.service = DecisionService(
            templates=load_templates(config.template_dir),
            store=store,
            log=log,
            client=client,
            clock=clock,
            breaker=CircuitBreaker(config.breaker_failure_threshold, config.breaker_open_s),
            max_in_flight=config.max_in_flight,
            request_timeout_s=config.request_timeout_s,
            fake_decisions=config.fake_decisions,
            monthly_ceiling_usd=config.monthly_ceiling_usd,
            day_alarm_fraction=config.day_alarm_fraction,
            chat_conversation_per_min=config.chat_conversation_per_min,
            chat_bot_interval_s=config.chat_bot_interval_s,
            chat_human_per_minute=config.chat_human_per_minute,
            guild_recent_s=config.guild_recent_s,
            decision_usd_per_mtok=config.jev_usd_per_mtok,
            secret=config.openrouter_api_key,
            chat_template=load_chat_template(config.template_dir),
            chat_settings=ChatSettings(
                model=config.chat_model,
                max_tokens=config.chat_max_tokens,
                temperature=config.chat_temperature,
                zero_data_retention=config.chat_zero_data_retention,
                usd_per_mtok_in=config.chat_usd_per_mtok_in,
                usd_per_mtok_out=config.chat_usd_per_mtok_out,
                reasoning_effort=config.chat_reasoning_effort,
            ),
        )
        try:
            yield
        finally:
            if owns_http and client_http is not None:
                await client_http.aclose()
            store.close()

    app = FastAPI(title="Aware AI sidecar", version=__version__, lifespan=lifespan)

    def service(request: Request) -> DecisionService:
        svc: DecisionService = request.app.state.service
        return svc

    @app.get("/health")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    @app.get("/v1/status")
    async def get_status(request: Request) -> StatusResponse:
        return service(request).status()

    @app.put("/v1/settings")
    async def put_settings(settings: Settings, request: Request) -> StatusResponse:
        svc = service(request)
        svc.update_settings(settings)
        return svc.status()

    @app.post("/v1/decide")
    async def decide(body: DecideRequest, request: Request) -> DecideResponse:
        try:
            return await service(request).decide(body)
        except UnknownDecisionType as exc:
            raise HTTPException(status_code=422, detail=f"unknown decision_type: {exc}") from exc
        except TemplateError as exc:
            raise HTTPException(status_code=422, detail=str(exc)) from exc

    @app.post("/v1/chat")
    async def chat(body: ChatRequest, request: Request) -> ChatResponse:
        return await service(request).chat(body)

    @app.post("/v1/memory/event", status_code=204)
    async def memory_event(body: MemoryEventRequest, request: Request) -> None:
        service(request).record_memory_event(body)

    @app.post("/v1/memory/wipe")
    async def memory_wipe(body: WipeRequest, request: Request) -> WipeResponse:
        return service(request).wipe(body)

    @app.post("/v1/outcome", status_code=204)
    async def outcome(body: OutcomeRequest, request: Request) -> None:
        service(request).record_outcome(body)

    return app
