"""Versioned decision templates: how a structured game state becomes a Jev question.

Templates are TOML files so prompts can be tuned without rebuilding worldserver.
"""

from __future__ import annotations

import re
import tomllib
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from aware_ai.schemas import IDENTIFIER_PATTERN

_IDENTIFIER = re.compile(IDENTIFIER_PATTERN)


class TemplateError(ValueError):
    """A template file is invalid, or a request does not fit its template."""


@dataclass(frozen=True)
class DecisionTemplate:
    name: str
    version: int
    feature: str
    cache_ttl_s: float
    intro: str
    instructions: str
    # Option name -> criteria text. Order is fixed by the file: Jev's answers shift when options are reordered.
    options: Mapping[str, str]
    # The model answers a choice with probabilities and no text: with this on, the reason is the margin of the
    # choice ("option_1 0.72, next option_0 0.20"), so the decision log and worldserver's log say how sure the pick was.
    reason_from_probabilities: bool = False

    @property
    def template_id(self) -> str:
        return f"{self.name}@{self.version}"

    def ordered_options(self, requested: Sequence[str]) -> list[str]:
        unknown = [option for option in requested if option not in self.options]
        if unknown:
            raise TemplateError(f"unknown options for {self.name}: {', '.join(sorted(unknown))}")
        wanted = set(requested)
        return [option for option in self.options if option in wanted]

    def render_state(self, state: Mapping[str, Any]) -> str:
        lines = [self.intro.strip()]
        lines.extend(f"{key}: {_render_value(value)}" for key, value in state.items() if value is not None)
        return "\n".join(lines)

    def question(self, options: Sequence[str]) -> dict[str, Any]:
        return {
            "type": "choice",
            "instructions": self.instructions.strip(),
            "criteria": {option: self.options[option] for option in options},
        }


def _render_value(value: Any) -> str:
    if isinstance(value, Mapping):
        parts = [f"{key}={_render_value(item)}" for key, item in value.items() if item is not None]
        return "{" + ", ".join(parts) + "}"
    if isinstance(value, list | tuple):
        return "[" + "; ".join(_render_value(item) for item in value) + "]"
    if isinstance(value, bool):
        return "yes" if value else "no"
    return str(value)


def _require(data: Mapping[str, Any], key: str, kind: type, path: Path) -> Any:
    value = data.get(key)
    if not isinstance(value, kind):
        raise TemplateError(f"{path.name}: '{key}' must be a {kind.__name__}")
    return value


def parse_template(data: Mapping[str, Any], path: Path) -> DecisionTemplate:
    name = _require(data, "name", str, path)
    if not _IDENTIFIER.match(name):
        raise TemplateError(f"{path.name}: invalid name {name!r}")
    raw_options = _require(data, "options", list, path)
    options: dict[str, str] = {}
    for entry in raw_options:
        if not isinstance(entry, Mapping):
            raise TemplateError(f"{path.name}: each option must be a table")
        option_name = _require(entry, "name", str, path)
        if not _IDENTIFIER.match(option_name) or option_name in options:
            raise TemplateError(f"{path.name}: invalid or duplicate option {option_name!r}")
        options[option_name] = _require(entry, "criteria", str, path).strip()
    if len(options) < 2:
        raise TemplateError(f"{path.name}: needs at least two options")
    cache_ttl = data.get("cache_ttl_s", 0)
    if not isinstance(cache_ttl, int | float) or cache_ttl < 0:
        raise TemplateError(f"{path.name}: 'cache_ttl_s' must be a non-negative number")
    reason_from_probabilities = data.get("reason_from_probabilities", False)
    if not isinstance(reason_from_probabilities, bool):
        raise TemplateError(f"{path.name}: 'reason_from_probabilities' must be a boolean")
    return DecisionTemplate(
        name=name,
        version=_require(data, "version", int, path),
        feature=_require(data, "feature", str, path),
        cache_ttl_s=float(cache_ttl),
        intro=_require(data, "intro", str, path),
        instructions=_require(data, "instructions", str, path),
        options=options,
        reason_from_probabilities=reason_from_probabilities,
    )


def load_templates(directory: Path) -> dict[str, DecisionTemplate]:
    templates: dict[str, DecisionTemplate] = {}
    for path in sorted(directory.glob("*.toml")):
        with path.open("rb") as handle:
            template = parse_template(tomllib.load(handle), path)
        if template.name in templates:
            raise TemplateError(f"{path.name}: duplicate template name {template.name!r}")
        templates[template.name] = template
    return templates
