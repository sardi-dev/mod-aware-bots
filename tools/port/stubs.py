"""Rebuilds docs/stubs.md from the "// PORT:" notes in src/. Usage: stubs.py <module root>

Every place where SkyFire lacks something the module uses carries a "// PORT:" comment, so the list is generated
from the code instead of kept by hand. A note continues over the following comment lines of the same block.
"""
import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
MARK = "// PORT:"
HEADER = """# Stubs and gaps

Places where SkyFire lacks something the module uses. Each is marked `// PORT:` in the code; this file is generated
by `tools/port/stubs.py`, so edit the comments, not this file. A feature counts as working only when its entries
here are resolved or accepted.

| Where | What is missing |
|-------|-----------------|
"""


def notes(path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for number, line in enumerate(lines, 1):
        start = line.find(MARK)
        if start < 0:
            continue
        text = [line[start + len(MARK):].strip()]
        # the note goes on over the next lines that are only a comment
        for follow in lines[number:]:
            stripped = follow.strip()
            if not stripped.startswith("//") or MARK in stripped:
                break
            text.append(stripped[2:].strip())
        yield number, " ".join(part for part in text if part)


rows = []
for path in sorted((ROOT / "src").rglob("*")):
    if path.suffix not in (".h", ".cpp"):
        continue
    rel = path.relative_to(ROOT / "src").as_posix()
    for number, text in notes(path):
        rows.append(f"| `{rel}:{number}` | {text.replace('|', '/')} |")

(ROOT / "docs" / "stubs.md").write_text(HEADER + "\n".join(rows) + "\n", encoding="utf-8")
print(f"{len(rows)} notes")
