"""Bot conversations: persona prompt, history and reply cleanup. The model call itself lives in openrouter.py."""

from __future__ import annotations

import re
import string
import tomllib
from dataclasses import dataclass
from pathlib import Path

from aware_ai.schemas import ChatPersonality, ChatRequest
from aware_ai.storage import FRIENDLY_SCORE
from aware_ai.templates import TemplateError

CHAT_TEMPLATE_PATH = Path("chat") / "system.toml"
# Replies longer than this are cut at a word boundary; worldserver splits them into 255-byte chat lines.
MAX_REPLY_CHARS = 400
HISTORY_TURNS = 8
HISTORY_HOURS = 24
HIGH_TRAIT = 66
LOW_TRAIT = 33


@dataclass(frozen=True)
class ChatTemplate:
    version: int
    system: string.Template
    identity_denying: str
    identity_open: str

    @property
    def template_id(self) -> str:
        return f"chat@{self.version}"


def load_chat_template(template_dir: Path) -> ChatTemplate | None:
    path = template_dir / CHAT_TEMPLATE_PATH
    if not path.exists():
        return None
    with path.open("rb") as handle:
        data = tomllib.load(handle)
    version, system = data.get("version"), data.get("system")
    denying, open_ = data.get("identity_denying"), data.get("identity_open")
    if not isinstance(version, int) or not isinstance(system, str):
        raise TemplateError(f"{path.name}: needs an integer 'version' and a string 'system'")
    if not isinstance(denying, str) or not isinstance(open_, str):
        raise TemplateError(f"{path.name}: needs the strings 'identity_denying' and 'identity_open'")
    return ChatTemplate(
        version=version,
        system=string.Template(system.strip()),
        identity_denying=denying.strip(),
        identity_open=open_.strip(),
    )


def archetype(personality: ChatPersonality) -> str:
    if personality.chattiness <= 25:
        return "quiet"
    if personality.aggression >= 70 and personality.honor <= 40:
        return "rough"
    if personality.chattiness >= 80 and personality.aggression < 70:
        return "chatty"
    return "friendly"


def describe_voice(personality: ChatPersonality) -> str:
    voices = {
        "quiet": "Use few words, often lowercase, without punctuation.",
        "rough": "Be sarcastic and competitive; you may swear like a 2013 game server, "
        "but never use slurs, hate, threats or real-life insults.",
        "chatty": "Be talkative, use full sentences and make jokes.",
        "friendly": "Be friendly and easygoing.",
    }
    return voices[archetype(personality)]


# The band edges worldserver ships by default (AiPlayerbot.Social.Liking.ColdBelow, FriendlyAt, CloseAt). They only
# matter when a request carries `liking` without `relation`; with both, the band worldserver computed wins, so a
# server owner who retunes the edges there needs no change here.
COLD_BELOW = 0.20
FRIENDLY_AT = 0.55
CLOSE_AT = 0.75
FRIENDLY_BANDS = frozenset({"friendly", "close"})

BAND_FEELINGS = {
    "cold": "You do not like them and keep your distance.",
    "stranger": "",
    "friendly": "You like them.",
    "close": "You like them a lot and count them as a close friend.",
}


def band_of(liking: float) -> str:
    if liking < COLD_BELOW:
        return "cold"
    if liking < FRIENDLY_AT:
        return "stranger"
    return "friendly" if liking < CLOSE_AT else "close"


def feeling_band(request: ChatRequest) -> str | None:
    """How the bot feels about the human according to worldserver, or None when the request carries nothing.

    One score in the end (owner decision of 2026-11-22): worldserver's liking is the source of truth when it is sent;
    the sidecar's own pair score is only the fallback for a request without it.
    """
    if request.relation is not None:
        return request.relation
    if request.liking is not None:
        return band_of(request.liking)
    return None


def is_friend(request: ChatRequest, own_score: int) -> bool:
    band = feeling_band(request)
    if band is not None:
        return band in FRIENDLY_BANDS
    return own_score >= FRIENDLY_SCORE


def describe_feeling(request: ChatRequest) -> str | None:
    """The prompt sentence for worldserver's score (band word and whole percent only), None to use the sidecar's own."""
    band = feeling_band(request)
    if band is None:
        return None
    percent = f", {round(request.liking * 100)} of 100" if request.liking is not None else ""
    return f"{BAND_FEELINGS[band]} (Your feeling for them: {band}{percent}.)".strip()


def describe_memory(memory: dict[str, int] | None, note: dict[str, object], feeling: str | None = None) -> str:
    counts = note.get("counts", {})
    assert isinstance(counts, dict)
    facts: list[str] = []
    labels = {
        "grouped": "grouped with them",
        "dungeon_done": "run dungeons with them",
        "boss_killed_together": "killed bosses together",
        "wipe_together": "wiped together",
        "duel_won": "won duels against them",
        "duel_lost": "lost duels to them",
        "loot_ninja": "been ninja-looted by them",
        "loot_passed": "received loot they passed",
        "helped": "been helped by them",
        "insulted": "been insulted by them",
        "thanked": "been thanked by them",
    }
    for event, label in labels.items():
        if counts.get(event):
            facts.append(f"{label} {counts[event]} time(s)")
    if memory:
        facts.append(f"fought them {memory['fights']} time(s), they killed you {memory['deaths']} time(s)")
    if note.get("topic"):
        facts.append(f"last talked about {note['topic']}")
    first = "You have " + ", ".join(facts[:2]) if facts else "You have no shared history"
    if note.get("detail"):
        first += f"; last detail: {note['detail']}"
    if feeling is None:
        score = note.get("score", 0)
        assert isinstance(score, int)
        if score >= FRIENDLY_SCORE:
            feeling = "You like them."
        else:
            feeling = "You hold a grudge against them." if score < 0 else ""
    return f"{first}. {feeling}".strip()


# What the human is doing, by chat kind, so the model knows the channel and the moment instead of assuming a whisper.
SITUATIONS: dict[str, str] = {
    "duel_challenge": "has just challenged you to a duel; you say something to them in /say.",
    "duel_win": "has just lost a duel to you; you say something to them in /say.",
    "duel_loss": "has just beaten you in a duel; you say something to them in /say.",
    "bg_win": "is on your battleground team, which has just won; you say something in battleground chat.",
    "bg_loss": "is on your battleground team, which has just lost; you say something in battleground chat.",
    "bg_capture": "is on your battleground team, which has just scored; you say something in battleground chat.",
    "whisper_reply": "is whispering you.",
    "nearby_reply": "is standing near you and says something in /say.",
    "party_moment": "is in your group and writes in party chat.",
    "opener": "is present at the moment described; you speak first in the requested channel.",
    "friend_whisper": "is your friend; you whisper them first about the moment described.",
    "guild_reply": "is in your guild and writes in guild chat.",
}


def build_messages(
    template: ChatTemplate,
    request: ChatRequest,
    memory: dict[str, int] | None,
    history: list[tuple[str, str]],
    note: dict[str, object] | None = None,
) -> list[dict[str, str]]:
    system = template.system.safe_substitute(
        bot_name=request.bot.name,
        bot_level=request.bot.level,
        bot_race=request.bot.race.replace("_", " "),
        bot_class=request.bot.class_name.replace("_", " "),
        zone=request.zone or "the world",
        voice=describe_voice(request.personality),
        player_name=request.player.name,
        player_level=request.player.level,
        player_race=request.player.race.replace("_", " "),
        player_class=request.player.class_name.replace("_", " "),
        relation="same faction" if request.same_faction else "enemy faction",
        memory=describe_memory(memory, note or {}, describe_feeling(request)),
        situation=SITUATIONS.get(request.kind, "is talking to you."),
        identity=template.identity_denying if request.deny_being_bot else template.identity_open,
    )
    system += f"\nMoment: {request.kind}."
    messages = [{"role": "system", "content": system}]
    messages.extend({"role": role, "content": text} for role, text in history)
    messages.append({"role": "user", "content": request.message})
    return messages


def clean_reply(text: str, bot_name: str) -> str:
    reply = " ".join(text.split())
    # Models sometimes prefix their own name ("Ritzes: hey"); chat already shows who speaks.
    if reply.lower().startswith(bot_name.lower() + ":"):
        reply = reply[len(bot_name) + 1 :]
    reply = reply.strip().strip('"').strip("'").strip()
    # Players type a hyphen, not typographic dashes, and do not narrate *actions* in chat.
    reply = re.sub(r"\s*[\u2014\u2013]\s*", " - ", reply)
    reply = re.sub(r"\*[^*]{1,60}\*\s*", "", reply).strip()
    reply = reply.replace("\u2019", "'").replace("\u2018", "'").replace("\u201c", "").replace("\u201d", "")
    if len(reply) > MAX_REPLY_CHARS:
        cut = reply.rfind(" ", 0, MAX_REPLY_CHARS)
        reply = reply[: cut if cut > 0 else MAX_REPLY_CHARS].rstrip()
    return reply
