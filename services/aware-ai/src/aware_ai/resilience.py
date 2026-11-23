"""Clock, circuit breaker and decision cache. Time is injected so tests stay deterministic."""

from __future__ import annotations

import hashlib
import json
import time
from collections import OrderedDict
from collections.abc import Mapping, Sequence
from datetime import UTC, datetime
from typing import Any, Protocol

# Buckets make near-identical situations share a cache entry (a 63% vs 64% HP target is the same fight).
PERCENT_BUCKET = 10
DISTANCE_BUCKET_YD = 10


class Clock(Protocol):
    def now(self) -> datetime: ...

    def monotonic(self) -> float: ...


class SystemClock:
    def now(self) -> datetime:
        return datetime.now(UTC)

    def monotonic(self) -> float:
        return time.monotonic()


class CircuitBreaker:
    """Stops calling a failing provider for a while, then lets one trial call through."""

    def __init__(self, failure_threshold: int, open_s: float) -> None:
        self._threshold = failure_threshold
        self._open_s = open_s
        self._failures = 0
        self._opened_at: float | None = None
        self._trial_in_flight = False

    def state(self, now: float) -> str:
        if self._opened_at is None:
            return "closed"
        if now - self._opened_at >= self._open_s:
            return "half_open"
        return "open"

    def allow(self, now: float) -> bool:
        state = self.state(now)
        if state == "closed":
            return True
        if state == "half_open" and not self._trial_in_flight:
            self._trial_in_flight = True
            return True
        return False

    def record_success(self) -> None:
        self._failures = 0
        self._opened_at = None
        self._trial_in_flight = False

    def record_failure(self, now: float) -> None:
        self._trial_in_flight = False
        self._failures += 1
        if self._opened_at is not None or self._failures >= self._threshold:
            self._opened_at = now


def _bucket(key: str, value: Any) -> Any:
    if isinstance(value, Mapping):
        return {k: _bucket(k, v) for k, v in value.items()}
    if isinstance(value, list | tuple):
        return [_bucket(key, item) for item in value]
    if isinstance(value, bool) or not isinstance(value, int | float):
        return value
    if key.endswith("_pct"):
        return int(round(value / PERCENT_BUCKET) * PERCENT_BUCKET)
    if key.endswith("_yd"):
        return int(round(value / DISTANCE_BUCKET_YD) * DISTANCE_BUCKET_YD)
    return value


def cache_key(template_id: str, options: Sequence[str], state: Mapping[str, Any]) -> str:
    canonical = json.dumps(
        {"t": template_id, "o": list(options), "s": _bucket("", state)}, sort_keys=True, separators=(",", ":")
    )
    return hashlib.sha256(canonical.encode()).hexdigest()


class DecisionCache:
    def __init__(self, max_entries: int = 4096) -> None:
        self._max_entries = max_entries
        self._entries: OrderedDict[str, tuple[float, Any]] = OrderedDict()

    def get(self, key: str, now: float) -> Any | None:
        entry = self._entries.get(key)
        if entry is None:
            return None
        expires_at, value = entry
        if now >= expires_at:
            del self._entries[key]
            return None
        self._entries.move_to_end(key)
        return value

    def put(self, key: str, value: Any, ttl_s: float, now: float) -> None:
        if ttl_s <= 0:
            return
        self._entries[key] = (now + ttl_s, value)
        self._entries.move_to_end(key)
        while len(self._entries) > self._max_entries:
            self._entries.popitem(last=False)
