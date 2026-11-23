"""SQLite state: the spend ledger and the last settings pushed by worldserver.

Queries are small and local, so they run inline on the event loop.
"""

from __future__ import annotations

import sqlite3
from datetime import datetime, timedelta
from pathlib import Path

from aware_ai.schemas import BudgetSettings, BudgetStatus, Settings

# Warn worldserver (and so the player) before the cap cuts features off.
BUDGET_WARNING_FRACTION = 0.8
# Bots remember fights for this long; older events are pruned at startup.
MEMORY_RETENTION_DAYS = 365
# Grudge counts only recent fights, so it fades: each death adds 25, each kill of that player removes half of that.
GRUDGE_WINDOW_DAYS = 30
GRUDGE_PER_DEATH = 25
MAX_GRUDGE = 100
# Fixed relation changes per event; scores saturate at -100 and 100.
RELATION_WEIGHTS = {
    "grouped": 2,
    "dungeon_done": 10,
    "boss_killed_together": 5,
    "wipe_together": 2,
    "duel_won": 0,
    "duel_lost": -5,
    "loot_ninja": -25,
    "loot_passed": 10,
    "helped": 10,
    "insulted": -10,
    "thanked": 3,
}
FRIENDLY_SCORE = 20
# Conversation turns older than this are pruned at startup.
CHAT_RETENTION_DAYS = 30

_SCHEMA = """
CREATE TABLE IF NOT EXISTS spend (
    id INTEGER PRIMARY KEY,
    ts_utc TEXT NOT NULL,
    feature TEXT NOT NULL,
    decision_type TEXT NOT NULL,
    cost_usd REAL NOT NULL
);
CREATE INDEX IF NOT EXISTS spend_ts ON spend (ts_utc);
CREATE TABLE IF NOT EXISTS memory_event (
    id INTEGER PRIMARY KEY,
    ts_utc TEXT NOT NULL,
    bot_guid INTEGER NOT NULL,
    player_guid INTEGER NOT NULL,
    outcome TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS memory_pair ON memory_event (bot_guid, player_guid, ts_utc);
CREATE TABLE IF NOT EXISTS chat_turn (
    id INTEGER PRIMARY KEY,
    ts_utc TEXT NOT NULL,
    bot_guid INTEGER NOT NULL,
    player_guid INTEGER NOT NULL,
    role TEXT NOT NULL,
    text TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS chat_pair ON chat_turn (bot_guid, player_guid, ts_utc);
CREATE TABLE IF NOT EXISTS social_event (
    id INTEGER PRIMARY KEY, ts_utc TEXT NOT NULL, bot_guid INTEGER NOT NULL,
    player_guid INTEGER NOT NULL, event TEXT NOT NULL, detail TEXT
);
CREATE INDEX IF NOT EXISTS social_pair ON social_event (bot_guid, player_guid, ts_utc);
CREATE TABLE IF NOT EXISTS pair_note (
    bot_guid INTEGER NOT NULL, player_guid INTEGER NOT NULL, score INTEGER NOT NULL DEFAULT 0,
    last_detail TEXT, last_seen TEXT, topic TEXT, friend_whisper_at TEXT,
    PRIMARY KEY (bot_guid, player_guid)
);
CREATE TABLE IF NOT EXISTS day_alarm (day TEXT PRIMARY KEY);
CREATE TABLE IF NOT EXISTS settings (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    json TEXT NOT NULL
);
"""


def _day_start(now: datetime) -> datetime:
    # OpenRouter key limits reset at midnight UTC, so the ledger uses UTC periods too.
    return now.replace(hour=0, minute=0, second=0, microsecond=0)


def _month_start(now: datetime) -> datetime:
    return _day_start(now).replace(day=1)


class Store:
    def __init__(self, db_path: Path) -> None:
        db_path.parent.mkdir(parents=True, exist_ok=True)
        self._db = sqlite3.connect(db_path, check_same_thread=False, isolation_level=None)
        self._db.execute("PRAGMA journal_mode=WAL")
        self._db.executescript(_SCHEMA)

    def close(self) -> None:
        self._db.close()

    def record_spend(self, cost_usd: float, feature: str, decision_type: str, now: datetime) -> None:
        self._db.execute(
            "INSERT INTO spend (ts_utc, feature, decision_type, cost_usd) VALUES (?, ?, ?, ?)",
            (now.isoformat(), feature, decision_type, cost_usd),
        )

    def spent_since(self, since: datetime) -> float:
        row = self._db.execute("SELECT COALESCE(SUM(cost_usd), 0) FROM spend WHERE ts_utc >= ?", (since.isoformat(),))
        return float(row.fetchone()[0])

    def budget_status(self, budget: BudgetSettings, now: datetime) -> BudgetStatus:
        month_spent = self.spent_since(_month_start(now))
        day_spent = self.spent_since(_day_start(now))
        month_cap = budget.monthly_usd
        day_cap = budget.daily_usd
        exhausted = month_spent >= month_cap or (day_cap is not None and day_spent >= day_cap)
        warning = exhausted or month_spent >= month_cap * BUDGET_WARNING_FRACTION
        return BudgetStatus(
            month_spent_usd=round(month_spent, 6),
            month_cap_usd=month_cap,
            day_spent_usd=round(day_spent, 6),
            day_cap_usd=day_cap,
            warning=warning,
            exhausted=exhausted,
        )

    def prune_spend(self, now: datetime, keep_days: int) -> None:
        cutoff = _month_start(now) - timedelta(days=keep_days)
        self._db.execute("DELETE FROM spend WHERE ts_utc < ?", (cutoff.isoformat(),))

    def record_memory_event(self, bot_guid: int, player_guid: int, outcome: str, now: datetime) -> None:
        self._db.execute(
            "INSERT INTO memory_event (ts_utc, bot_guid, player_guid, outcome) VALUES (?, ?, ?, ?)",
            (now.isoformat(), bot_guid, player_guid, outcome),
        )

    def record_social_event(self, bot: int, player: int, event: str, detail: str | None, now: datetime) -> None:
        self._db.execute(
            "INSERT INTO social_event (ts_utc, bot_guid, player_guid, event, detail) VALUES (?, ?, ?, ?, ?)",
            (now.isoformat(), bot, player, event, detail),
        )
        self._db.execute(
            "INSERT INTO pair_note (bot_guid, player_guid, score, last_detail, last_seen) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(bot_guid, player_guid) DO UPDATE SET "
            "score = MAX(-100, MIN(100, score + excluded.score)), "
            "last_detail = COALESCE(excluded.last_detail, last_detail), last_seen = excluded.last_seen",
            (bot, player, RELATION_WEIGHTS[event], detail, now.isoformat()),
        )

    def social_note(self, bot: int, player: int) -> dict[str, object]:
        row = self._db.execute(
            "SELECT score, last_detail, last_seen, topic, friend_whisper_at FROM pair_note "
            "WHERE bot_guid = ? AND player_guid = ?",
            (bot, player),
        ).fetchone()
        counts = dict(
            self._db.execute(
                "SELECT event, COUNT(*) FROM social_event WHERE bot_guid = ? AND player_guid = ? GROUP BY event",
                (bot, player),
            ).fetchall()
        )
        return {
            "score": row[0] if row else 0,
            "detail": row[1] if row else None,
            "last_seen": row[2] if row else None,
            "topic": row[3] if row else None,
            "friend_whisper_at": row[4] if row else None,
            "counts": counts,
        }

    def update_pair_chat(
        self, bot: int, player: int, now: datetime, topic: str | None = None, friend_whisper: bool = False
    ) -> None:
        self._db.execute(
            "INSERT INTO pair_note (bot_guid, player_guid, last_seen, topic, friend_whisper_at) VALUES (?, ?, ?, ?, ?) "
            "ON CONFLICT(bot_guid, player_guid) DO UPDATE SET last_seen = excluded.last_seen, "
            "topic = COALESCE(excluded.topic, topic), "
            "friend_whisper_at = COALESCE(excluded.friend_whisper_at, friend_whisper_at)",
            (bot, player, now.isoformat(), topic, now.isoformat() if friend_whisper else None),
        )

    def mark_day_alarm(self, now: datetime) -> bool:
        cursor = self._db.execute("INSERT OR IGNORE INTO day_alarm (day) VALUES (?)", (now.date().isoformat(),))
        return cursor.rowcount == 1

    def memory_summary(self, bot_guid: int, player_guid: int, now: datetime) -> dict[str, int] | None:
        rows = self._db.execute(
            "SELECT ts_utc, outcome FROM memory_event WHERE bot_guid = ? AND player_guid = ? ORDER BY ts_utc",
            (bot_guid, player_guid),
        ).fetchall()
        if not rows:
            return None
        grudge_since = (now - timedelta(days=GRUDGE_WINDOW_DAYS)).isoformat()
        kills = sum(1 for _, outcome in rows if outcome == "bot_won")
        deaths = sum(1 for _, outcome in rows if outcome == "human_won")
        recent_kills = sum(1 for ts, outcome in rows if outcome == "bot_won" and ts >= grudge_since)
        recent_deaths = sum(1 for ts, outcome in rows if outcome == "human_won" and ts >= grudge_since)
        grudge = recent_deaths * GRUDGE_PER_DEATH - recent_kills * GRUDGE_PER_DEATH // 2
        last_seen = datetime.fromisoformat(rows[-1][0])
        return {
            "fights": len(rows),
            "kills": kills,
            "deaths": deaths,
            "disengages": sum(1 for _, outcome in rows if outcome == "disengaged"),
            "grudge": max(0, min(MAX_GRUDGE, grudge)),
            "last_seen_min": int((now - last_seen).total_seconds() // 60),
        }

    def prune_memory(self, now: datetime) -> None:
        cutoff = now - timedelta(days=MEMORY_RETENTION_DAYS)
        self._db.execute("DELETE FROM memory_event WHERE ts_utc < ?", (cutoff.isoformat(),))

    def add_chat_turn(self, bot_guid: int, player_guid: int, role: str, text: str, now: datetime) -> None:
        self._db.execute(
            "INSERT INTO chat_turn (ts_utc, bot_guid, player_guid, role, text) VALUES (?, ?, ?, ?, ?)",
            (now.isoformat(), bot_guid, player_guid, role, text),
        )

    def recent_chat(self, bot_guid: int, player_guid: int, since: datetime, limit: int) -> list[tuple[str, str]]:
        rows = self._db.execute(
            "SELECT role, text FROM chat_turn WHERE bot_guid = ? AND player_guid = ? AND ts_utc >= ? "
            "ORDER BY ts_utc DESC, id DESC LIMIT ?",
            (bot_guid, player_guid, since.isoformat(), limit),
        ).fetchall()
        return [(str(role), str(text)) for role, text in reversed(rows)]

    def prune_chat(self, now: datetime) -> None:
        cutoff = now - timedelta(days=CHAT_RETENTION_DAYS)
        self._db.execute("DELETE FROM chat_turn WHERE ts_utc < ?", (cutoff.isoformat(),))

    def wipe_player(self, player_guid: int | None) -> dict[str, int]:
        """Delete every row kept about one human (all humans when player_guid is None); the spend ledger stays."""
        counts: dict[str, int] = {}
        for table in ("memory_event", "chat_turn", "social_event", "pair_note"):
            # The table names are the fixed tuple above, never caller input.
            if player_guid is None:
                cursor = self._db.execute(f"DELETE FROM {table}")
            else:
                cursor = self._db.execute(f"DELETE FROM {table} WHERE player_guid = ?", (player_guid,))
            counts[table] = cursor.rowcount
        return counts

    def load_settings(self) -> Settings:
        row = self._db.execute("SELECT json FROM settings WHERE id = 1").fetchone()
        if row is None:
            return Settings()
        return Settings.model_validate_json(row[0])

    def save_settings(self, settings: Settings) -> None:
        self._db.execute(
            "INSERT INTO settings (id, json) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET json = excluded.json",
            (settings.model_dump_json(),),
        )
