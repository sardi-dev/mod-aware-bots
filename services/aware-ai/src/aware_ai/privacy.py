"""Remove obvious non-game identifiers before prompts, storage and logs."""

from __future__ import annotations

import re
from typing import Any

_EMAIL = re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")
# Full IPv6 needs four or more groups, so clock times such as 10:30:45 in chat stay readable
_IP = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b|(?<![\w:])(?:[0-9a-fA-F]{1,4}:){3,7}[0-9a-fA-F]{1,4}(?![\w:])")
_SENSITIVE = re.compile(r"account|e.?mail|(^|_)ip($|_)|api.?key|password|secret|token", re.I)


def clean(value: Any, secret: str | None = None) -> Any:
    if isinstance(value, dict):
        return {k: clean(v, secret) for k, v in value.items() if not _SENSITIVE.search(str(k))}
    if isinstance(value, list):
        return [clean(v, secret) for v in value]
    if isinstance(value, str):
        if secret:
            value = value.replace(secret, "[redacted]")
        return _IP.sub("[redacted]", _EMAIL.sub("[redacted]", value))
    return value
