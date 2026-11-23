"""Write the API v1 JSON Schemas to schemas/v1/. Run `python -m aware_ai.export_schemas` after changing schemas.py.

The committed files are the contract C++ golden tests validate against; a test fails if they drift from the models.
"""

from __future__ import annotations

import json
from pathlib import Path

from pydantic import BaseModel

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

SCHEMA_DIR = Path(__file__).resolve().parents[2] / "schemas" / "v1"
MODELS: dict[str, type[BaseModel]] = {
    "chat_request": ChatRequest,
    "chat_response": ChatResponse,
    "decide_request": DecideRequest,
    "decide_response": DecideResponse,
    "memory_event_request": MemoryEventRequest,
    "outcome_request": OutcomeRequest,
    "settings": Settings,
    "status_response": StatusResponse,
    "wipe_request": WipeRequest,
    "wipe_response": WipeResponse,
}


def render_schemas() -> dict[str, str]:
    return {
        name: json.dumps(model.model_json_schema(), indent=2, sort_keys=True) + "\n" for name, model in MODELS.items()
    }


def main() -> None:
    SCHEMA_DIR.mkdir(parents=True, exist_ok=True)
    for name, text in render_schemas().items():
        (SCHEMA_DIR / f"{name}.json").write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
