"""Append-only JSONL log of every decision and outcome: the quality dashboard and future training data."""

from __future__ import annotations

import json
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

_PREFIX = "decisions-"
_SUFFIX = ".jsonl"


class DecisionLog:
    def __init__(self, directory: Path, retention_days: int) -> None:
        self._directory = directory
        self._retention_days = retention_days
        directory.mkdir(parents=True, exist_ok=True)

    def path_for(self, now: datetime) -> Path:
        return self._directory / f"{_PREFIX}{now:%Y-%m-%d}{_SUFFIX}"

    def write(self, record: dict[str, Any], now: datetime) -> None:
        line = json.dumps({"ts": now.isoformat(), **record}, separators=(",", ":"), default=str)
        with self.path_for(now).open("a", encoding="utf-8") as handle:
            handle.write(line + "\n")

    def prune(self, now: datetime) -> list[Path]:
        cutoff = (now - timedelta(days=self._retention_days)).date()
        removed: list[Path] = []
        for path in self._directory.glob(f"{_PREFIX}*{_SUFFIX}"):
            stamp = path.name[len(_PREFIX) : -len(_SUFFIX)]
            try:
                day = datetime.strptime(stamp, "%Y-%m-%d").date()
            except ValueError:
                continue
            if day < cutoff:
                path.unlink()
                removed.append(path)
        return removed

    def scrub(self, player_guid: int | None) -> int:
        """Drop the lines about one human (every line that names a human when player_guid is None); returns the count.

        Chat lines keep the human's name and words in their context, decision lines keep `target_guid`, so a wipe has
        to reach the files too. The other lines (settings, outcomes, decisions with no target) hold no human.
        """

        def about_human(record: dict[str, Any]) -> bool:
            guids = (record.get("player_guid"), record.get("target_guid"))
            if player_guid is None:
                return any(guid is not None for guid in guids)
            return player_guid in guids

        removed = 0
        for path in sorted(self._directory.glob(f"{_PREFIX}*{_SUFFIX}")):
            kept: list[str] = []
            dropped = 0
            with path.open(encoding="utf-8") as handle:
                for line in handle:
                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError:
                        kept.append(line)
                        continue
                    if isinstance(record, dict) and about_human(record):
                        dropped += 1
                    else:
                        kept.append(line)
            if not dropped:
                continue
            temporary = path.with_suffix(path.suffix + ".tmp")
            temporary.write_text("".join(kept), encoding="utf-8")
            temporary.replace(path)
            removed += dropped
        return removed
