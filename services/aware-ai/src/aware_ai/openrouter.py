"""OpenRouter client for Jev decisions (TypeSafe's System One format at /api/v1/systemone)."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import httpx

TOKENS_PER_MILLION = 1_000_000


class ProviderError(Exception):
    """The provider call failed; `reason` is a short machine-readable code for logs and responses."""

    def __init__(self, reason: str) -> None:
        super().__init__(reason)
        self.reason = reason


class ProviderTimeout(ProviderError):
    def __init__(self) -> None:
        super().__init__("timeout")


@dataclass(frozen=True)
class JevAnswer:
    choice: str
    probabilities: dict[str, float]
    confidence: float
    cost_usd: float
    input_tokens: int
    model: str


def _status_reason(status: int) -> str:
    if status in (401, 403):
        return "auth"
    if status == 402:
        return "out_of_credit"
    if status == 429:
        return "rate_limited"
    if status >= 500:
        return "provider_unavailable"
    return f"http_{status}"


@dataclass(frozen=True)
class ChatAnswer:
    text: str
    cost_usd: float
    model: str


class OpenRouterClient:
    def __init__(self, http: httpx.AsyncClient, jev_model: str, jev_usd_per_mtok: float) -> None:
        self._http = http
        self._jev_model = jev_model
        self._jev_usd_per_mtok = jev_usd_per_mtok

    async def decide(
        self, state_text: str, question_name: str, question: dict[str, Any], options: list[str], timeout_s: float
    ) -> JevAnswer:
        body = {"model": self._jev_model, "state": state_text, "questions": {question_name: question}}
        try:
            response = await self._http.post("/systemone", json=body, timeout=timeout_s)
        except httpx.TimeoutException as exc:
            raise ProviderTimeout() from exc
        except httpx.HTTPError as exc:
            raise ProviderError("network") from exc
        if response.status_code != httpx.codes.OK:
            raise ProviderError(_status_reason(response.status_code))
        try:
            payload = response.json()
        except ValueError as exc:
            raise ProviderError("malformed") from exc
        return self._parse(payload, question_name, options)

    def _parse(self, payload: Any, question_name: str, options: list[str]) -> JevAnswer:
        if not isinstance(payload, dict):
            raise ProviderError("malformed")
        answer = (payload.get("answers") or {}).get(question_name)
        if not isinstance(answer, dict) or answer.get("type") != "choice":
            raise ProviderError("malformed")
        choice = answer.get("choice")
        if choice not in options:
            raise ProviderError("choice_not_offered")
        raw_probs = answer.get("probabilities") or {}
        probabilities = {
            option: float(raw_probs[option]) for option in options if isinstance(raw_probs.get(option), int | float)
        }
        confidence_raw = answer.get("confidence")
        confidence = (
            float(confidence_raw) if isinstance(confidence_raw, int | float) else probabilities.get(choice, 0.0)
        )
        usage = payload.get("usage") or {}
        input_tokens = int(usage.get("input_tokens") or 0)
        cost = usage.get("cost")
        cost_usd = (
            float(cost) if isinstance(cost, int | float) else input_tokens * self._jev_usd_per_mtok / TOKENS_PER_MILLION
        )
        return JevAnswer(
            choice=str(choice),
            probabilities=probabilities,
            confidence=confidence,
            cost_usd=cost_usd,
            input_tokens=input_tokens,
            model=str(payload.get("model") or self._jev_model),
        )

    async def chat(
        self,
        messages: list[dict[str, str]],
        *,
        model: str,
        max_tokens: int,
        temperature: float,
        zero_data_retention: bool,
        usd_per_mtok_in: float,
        usd_per_mtok_out: float,
        timeout_s: float,
        reasoning_effort: str | None = None,
    ) -> ChatAnswer:
        provider: dict[str, Any] = {"data_collection": "deny"}
        if zero_data_retention:
            provider["zdr"] = True
        body = {
            "model": model,
            "messages": messages,
            "max_tokens": max_tokens,
            "temperature": temperature,
            "provider": provider,
            "usage": {"include": True},
        }
        if reasoning_effort:
            body["reasoning"] = {"effort": reasoning_effort}
        try:
            response = await self._http.post("/chat/completions", json=body, timeout=timeout_s)
        except httpx.TimeoutException as exc:
            raise ProviderTimeout() from exc
        except httpx.HTTPError as exc:
            raise ProviderError("network") from exc
        if response.status_code != httpx.codes.OK:
            raise ProviderError(_status_reason(response.status_code))
        try:
            payload = response.json()
            text = payload["choices"][0]["message"]["content"]
        except (ValueError, KeyError, IndexError, TypeError) as exc:
            raise ProviderError("malformed") from exc
        if not isinstance(text, str) or not text.strip():
            raise ProviderError("empty_reply")
        usage = payload.get("usage") or {}
        cost = usage.get("cost")
        if isinstance(cost, int | float):
            cost_usd = float(cost)
        else:
            tokens_in = int(usage.get("prompt_tokens") or 0)
            tokens_out = int(usage.get("completion_tokens") or 0)
            cost_usd = (tokens_in * usd_per_mtok_in + tokens_out * usd_per_mtok_out) / TOKENS_PER_MILLION
        return ChatAnswer(text=text, cost_usd=cost_usd, model=str(payload.get("model") or model))
