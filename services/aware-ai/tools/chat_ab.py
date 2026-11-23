#!/usr/bin/env python3
"""Compare chat models on a fixed set of prompts using an environment-only key."""

from __future__ import annotations

import argparse
import json
import os
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

DEFAULT_MODELS = ["deepseek/deepseek-v4-flash-0731", "z-ai/glm-5.3-flash", "qwen/qwen3.7-flash", "openai/gpt-6-luna"]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("models", nargs="*", default=DEFAULT_MODELS)
    parser.add_argument("--output", type=Path, default=Path("chat-ab-output"))
    args = parser.parse_args()
    key = os.environ.get("OPENROUTER_API_KEY", "").strip()
    if not key:
        parser.error("OPENROUTER_API_KEY is required")
    prompts = json.loads(Path(__file__).with_name("chat_prompts.json").read_text(encoding="utf-8"))
    rows: list[dict[str, Any]] = []
    for prompt in prompts:
        for model in args.models:
            body = {
                "model": model,
                "messages": [
                    {"role": "system", "content": prompt["system"]},
                    {"role": "user", "content": prompt["message"]},
                ],
                "max_tokens": 60,
                "temperature": 0.8,
                "reasoning": {"effort": "none"},
                "usage": {"include": True},
            }
            request = urllib.request.Request(
                "https://openrouter.ai/api/v1/chat/completions",
                data=json.dumps(body).encode(),
                headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
                method="POST",
            )
            started = time.monotonic()
            result: dict[str, Any] = {"kind": prompt["kind"], "prompt_id": prompt["id"], "model": model}
            try:
                with urllib.request.urlopen(request, timeout=30) as response:
                    payload = json.load(response)
                result.update(reply=payload["choices"][0]["message"]["content"], usage=payload.get("usage", {}))
            except (urllib.error.URLError, ValueError, KeyError, IndexError, TypeError):
                result.update(reply="", error="request_failed", usage={})
            result["latency_ms"] = round((time.monotonic() - started) * 1000)
            rows.append(result)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "results.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")

    def cell(row: dict[str, Any]) -> str:
        usage = row["usage"]
        reply = str(row.get("reply") or row.get("error", "")).replace("|", "\\|").replace("\n", " ")
        tokens = f"{usage.get('prompt_tokens', '?')}/{usage.get('completion_tokens', '?')}"
        return f"{reply} ({row['latency_ms']} ms; tokens {tokens}; cost {usage.get('cost', '?')})"

    lines = ["| Prompt | " + " | ".join(args.models) + " |", "|---|" + "---|" * len(args.models)]
    for index, prompt in enumerate(prompts):
        group = rows[index * len(args.models) : (index + 1) * len(args.models)]
        lines.append(f"| {prompt['id']} {prompt['kind']} | " + " | ".join(cell(row) for row in group) + " |")
    (args.output / "comparison.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
