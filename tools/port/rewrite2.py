"""Mechanical rewrites, pass 2: specialisation and player checks. Usage: rewrite2.py <module src dir>"""
import pathlib
import re
import sys
from collections import Counter

ROOT = pathlib.Path(sys.argv[1])
CHAIN = r"((?:[A-Za-z_]\w*(?:\([^()]*\))?(?:->|\.))+)"


def strip_access(chain):
    return re.sub(r"(->|\.)$", "", chain)


RULES = [
    # Player::GetSpecialization() -> BotGetSpecialization(player) (compat/BotCompat.h)
    ("GetSpecialization", re.compile(CHAIN + r"GetSpecialization\(\)"),
     lambda m: "BotGetSpecialization(" + strip_access(m.group(1)) + ")"),
    # Unit::IsPlayer() does not exist in SkyFire; BotGuid has GetTypeId() too, so the rule is safe for GUIDs.
    ("IsPlayer", re.compile(CHAIN + r"IsPlayer\(\)"),
     lambda m: "(" + m.group(1) + "GetTypeId() == TypeID::TYPEID_PLAYER)"),
]

counts = Counter()
files = 0
for path in sorted(ROOT.rglob("*")):
    if path.suffix not in (".h", ".cpp") or "compat" in path.parts:
        continue
    with open(path, encoding="utf-8", errors="surrogateescape", newline="") as fh:
        text = fh.read()
    new = text
    for name, pattern, repl in RULES:
        new, n = pattern.subn(repl, new)
        counts[name] += n
    if new != text:
        with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
            fh.write(new)
        files += 1

print(f"files changed: {files}")
for name, _, _ in RULES:
    print(f"  {name}: {counts[name]}")
