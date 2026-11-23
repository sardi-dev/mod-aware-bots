"""Mechanical rewrites of LoA's module code to SkyFire's API (pass 1). Usage: rewrite1.py <module src dir>"""
import pathlib
import re
import sys
from collections import Counter

ROOT = pathlib.Path(sys.argv[1])
CHAIN = r"((?:[A-Za-z_]\w*(?:\([^()]*\))?(?:->|\.))+)"

RULES = [
    ("ObjectGuid", re.compile(r"\bObjectGuid\b"), "BotGuid"),
    ("HighGuid", re.compile(r"(?<![A-Za-z_])HighGuid::"), "BotHighGuid::"),
    ("GetCounter", re.compile(r"GetGUID\(\)\.GetCounter\(\)"), "GetGUIDLow()"),
    ("GetRawValue", re.compile(r"GetGUID\(\)\.GetRawValue\(\)"), "GetGUID()"),
    ("GUID member", re.compile(CHAIN + r"GetGUID\(\)\.(Is\w+|GetEntry|ToString|GetTypeName|GetHigh|GetTypeId)\b"),
     r"BotGuid(\1GetGUID()).\2"),
    ("SpellHistory", re.compile(CHAIN + r"GetSpellHistory\(\)"),
     lambda m: "BotSpellHistory(" + re.sub(r"(->|\.)$", "", m.group(1)) + ")"),
    ("unit getters", re.compile(r"(->|\.)(GetLevel|GetClass|GetRace|GetGender)\(\)"),
     lambda m: m.group(1) + {"GetLevel": "getLevel", "GetClass": "getClass", "GetRace": "getRace", "GetGender": "getGender"}[m.group(2)] + "()"),
    ("IsDead", re.compile(r"(->|\.)IsDead\(\)"), r"\1isDead()"),
    ("TC_LOG", re.compile(r"\bTC_LOG_(TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\b"), r"SF_LOG_\1"),
    ("TYPEID", re.compile(r"(?<![:\w])TYPEID_(OBJECT|ITEM|CONTAINER|UNIT|PLAYER|GAMEOBJECT|DYNAMICOBJECT|CORPSE|AREATRIGGER|SCENEOBJECT)\b"),
     r"TypeID::TYPEID_\1"),
    ("Trinity::", re.compile(r"\bTrinity::(?!ChatCommands)"), "Skyfire::"),
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
