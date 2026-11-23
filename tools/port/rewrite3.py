"""Mechanical rewrites, pass 3: opcode names. Usage: rewrite3.py <module src dir>

LoA and SkyFire both target client build 18414, so an opcode keeps its value; tools/port/opcodes.json maps every
LoA name the module uses to the SkyFire name with the same value (LoA's CMSG_GAMEOBJ_USE and CMSG_GAMEOBJ_REPORT_USE
are swapped relative to SkyFire's names, which the value mapping handles).
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(sys.argv[1])
MAP = json.load(open(pathlib.Path(__file__).with_name("opcodes.json")))
PATTERN = re.compile(r"\b(" + "|".join(sorted(MAP, key=len, reverse=True)) + r")\b")

files = changes = 0
for path in sorted(ROOT.rglob("*")):
    if path.suffix not in (".h", ".cpp") or "compat" in path.parts:
        continue
    with open(path, encoding="utf-8", errors="surrogateescape", newline="") as fh:
        text = fh.read()
    new, n = PATTERN.subn(lambda m: MAP[m.group(1)], text)
    if n:
        with open(path, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
            fh.write(new)
        files += 1
        changes += n
print(f"files changed: {files}, opcodes renamed: {changes}")
