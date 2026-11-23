#!/usr/bin/env python3
"""Compare chat models on multi-message conversations, through the sidecar's own prompt and reply cleanup.

Each scenario in chat_dialogs.json is played turn by turn: the human's lines are fixed, the model's cleaned replies are
fed back as history, as the live sidecar does. Two judge models from other families then rate each pair of transcripts
blind (random order) for how much the bot sounds like a real person typing in game chat. The key is read from the
environment only and never printed.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import re
import statistics
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

from aware_ai.chat import build_messages, clean_reply, load_chat_template
from aware_ai.schemas import ChatRequest

URL = "https://openrouter.ai/api/v1/chat/completions"
DEFAULT_MODELS = ["deepseek/deepseek-v4-flash-0731", "openai/gpt-6-luna"]
DEFAULT_JUDGES = ["anthropic/claude-sonnet-5.5", "google/gemini-3.5-flash"]
JUDGE_PROMPT = (
    "You judge bot chat in a fantasy MMO private server. Below are two transcripts, A and B, of the same "
    "conversation: the human's lines are identical, only the bot's replies differ. Rate each bot from 1 to 10 for how "
    "much it sounds like a real human player typing in game chat (length, tone, typing style, natural reactions, "
    "staying in the moment, not overacting or roleplaying like an NPC, not inventing wrong game facts). Answer with "
    'JSON only: {"A": <1-10>, "B": <1-10>, "better": "A" or "B" or "tie", "why": "<one short sentence>"}'
)


def call(key: str, body: dict[str, Any]) -> tuple[dict[str, Any], int]:
    request = urllib.request.Request(
        URL,
        data=json.dumps(body).encode(),
        headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
        method="POST",
    )
    started = time.monotonic()
    with urllib.request.urlopen(request, timeout=60) as response:
        payload = json.load(response)
    return payload, round((time.monotonic() - started) * 1000)


def play(key: str, model: str, scenario: dict[str, Any], template: Any) -> list[dict[str, Any]]:
    history: list[tuple[str, str]] = []
    turns: list[dict[str, Any]] = []
    for line in scenario["turns"]:
        request = ChatRequest.model_validate(
            {
                "schema_version": 1,
                "bot_guid": 1,
                "player_guid": 2,
                "bot": scenario["bot"],
                "player": scenario["player"],
                "personality": scenario["personality"],
                "same_faction": scenario["same_faction"],
                "zone": scenario["zone"],
                "kind": scenario["kind"],
                "channel": scenario["channel"],
                "message": line,
                "deadline_ms": 8000,
                **({"guild_human_message_age_s": 2} if scenario["kind"] == "guild_reply" else {}),
            }
        )
        body = {
            "model": model,
            "messages": build_messages(template, request, None, history),
            "max_tokens": 120,
            "temperature": 0.8,
            "reasoning": {"effort": "none"},
            "usage": {"include": True},
        }
        try:
            payload, latency = call(key, body)
            raw = payload["choices"][0]["message"]["content"] or ""
            usage = payload.get("usage", {})
        except (urllib.error.URLError, ValueError, KeyError, IndexError, TypeError):
            raw, latency, usage = "", 0, {}
        reply = clean_reply(raw, scenario["bot"]["name"])
        turns.append({"human": line, "raw": raw, "reply": reply, "latency_ms": latency, "usage": usage})
        history += [("user", line), ("assistant", reply)]
    return turns


def transcript(scenario: dict[str, Any], turns: list[dict[str, Any]]) -> str:
    player, bot = scenario["player"]["name"], scenario["bot"]["name"]
    return "\n".join(f"{player}: {t['human']}\n{bot}: {t['reply'] or '(silent)'}" for t in turns)


def judge(key: str, judge_model: str, scenario: dict[str, Any], a: str, b: str) -> dict[str, Any]:
    context = f"Channel: {scenario['channel']}, zone: {scenario['zone']}."
    body = {
        "model": judge_model,
        "messages": [{"role": "user", "content": f"{JUDGE_PROMPT}\n\n{context}\n\nA:\n{a}\n\nB:\n{b}"}],
        "max_tokens": 300,
        "temperature": 0,
        "usage": {"include": True},
    }
    try:
        payload, _ = call(key, body)
        text = payload["choices"][0]["message"]["content"] or ""
        match = re.search(r"\{.*\}", text, re.S)
        return dict(json.loads(match.group(0))) if match else {"error": "no_json"}
    except (urllib.error.URLError, ValueError, KeyError, IndexError, TypeError):
        return {"error": "request_failed"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", nargs=2, default=DEFAULT_MODELS)
    parser.add_argument("--judges", nargs="+", default=DEFAULT_JUDGES)
    parser.add_argument("--output", type=Path, default=Path("chat-dialog-ab-output"))
    parser.add_argument("--seed", type=int, default=548)
    args = parser.parse_args()
    key = os.environ.get("OPENROUTER_API_KEY", "").strip()
    if not key:
        parser.error("OPENROUTER_API_KEY is required")
    template = load_chat_template(Path(__file__).resolve().parents[1] / "templates")
    if template is None:
        parser.error("chat template missing")
    scenarios = json.loads(Path(__file__).with_name("chat_dialogs.json").read_text(encoding="utf-8"))
    rng = random.Random(args.seed)
    results: list[dict[str, Any]] = []
    for scenario in scenarios:
        played = {model: play(key, model, scenario, template) for model in args.models}
        verdicts = []
        for judge_model in args.judges:
            flip = rng.random() < 0.5
            first, second = (args.models[1], args.models[0]) if flip else (args.models[0], args.models[1])
            verdict = judge(
                key, judge_model, scenario, transcript(scenario, played[first]), transcript(scenario, played[second])
            )
            scores = {first: verdict.get("A"), second: verdict.get("B")}
            better = {"A": first, "B": second}.get(str(verdict.get("better")), "tie")
            verdicts.append({"judge": judge_model, "scores": scores, "better": better, "why": verdict.get("why", "")})
        results.append({"scenario": scenario["id"], "turns": played, "verdicts": verdicts})
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "dialogs.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    write_report(args, scenarios, results)


def write_report(args: argparse.Namespace, scenarios: list[dict[str, Any]], results: list[dict[str, Any]]) -> None:
    lines = [
        "| Model | Median ms | p90 ms | Cost | Avg chars | Judge score (avg) | Scenario wins |",
        "|---|---|---|---|---|---|---|",
    ]
    for model in args.models:
        turns = [t for r in results for t in r["turns"][model]]
        lat = sorted(t["latency_ms"] for t in turns if t["latency_ms"])
        cost = sum(float(t["usage"].get("cost") or 0) for t in turns)
        chars = statistics.mean(len(t["reply"]) for t in turns) if turns else 0
        scores = [v["scores"][model] for r in results for v in r["verdicts"] if isinstance(v["scores"].get(model), int)]
        wins = sum(1 for r in results for v in r["verdicts"] if v["better"] == model)
        p90 = lat[max(0, int(len(lat) * 0.9) - 1)] if lat else 0
        avg = statistics.mean(scores) if scores else 0
        median = statistics.median(lat) if lat else 0
        lines.append(f"| {model} | {median:.0f} | {p90} | ${cost:.5f} | {chars:.0f} | {avg:.1f} | {wins} |")
    lines.append("")
    by_id = {s["id"]: s for s in scenarios}
    for result in results:
        scenario = by_id[result["scenario"]]
        lines.append(f"## {result['scenario']} ({scenario['channel']}, {scenario['zone']})\n")
        for model in args.models:
            lines.append(f"**{model}**\n\n```\n{transcript(scenario, result['turns'][model])}\n```\n")
        for verdict in result["verdicts"]:
            lines.append(f"- {verdict['judge']}: {verdict['scores']} better {verdict['better']}: {verdict['why']}")
        lines.append("")
    (args.output / "dialogs.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
