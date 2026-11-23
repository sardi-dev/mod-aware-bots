"""Mechanical rewrites, pass 5: world queries LoA has as members and SkyFire does not (src/compat/BotWorld.h).

receiver->GetCombatManager()            -> BotCombatManager(receiver)
map->GetCreatureBySpawnIdStore()        -> BotCreatureBySpawnIdStore(map)
map->GetGameObjectBySpawnIdStore()      -> BotGameObjectBySpawnIdStore(map)
map->GetTransportForPos(args)           -> BotGetTransportForPos(map, args)
Usage: rewrite5.py <module src dir>
"""
import pathlib
import re
import sys

CHAIN = r"((?:[A-Za-z_]\w*(?:\([^()]*\))?(?:->|\.))+)"


def strip(chain):
    return re.sub(r"(->|\.)$", "", chain)


RULES = [
    (re.compile(CHAIN + r"GetCombatManager\(\)"), lambda m: f"BotCombatManager({strip(m.group(1))})"),
    (re.compile(CHAIN + r"GetCreatureBySpawnIdStore\(\)"), lambda m: f"BotCreatureBySpawnIdStore({strip(m.group(1))})"),
    (re.compile(CHAIN + r"GetGameObjectBySpawnIdStore\(\)"), lambda m: f"BotGameObjectBySpawnIdStore({strip(m.group(1))})"),
    (re.compile(CHAIN + r"GetTransportForPos\("), lambda m: f"BotGetTransportForPos({strip(m.group(1))}, "),
]

total = 0
for path in pathlib.Path(sys.argv[1]).rglob("*"):
    if path.suffix not in (".h", ".cpp") or "compat" in path.parts:
        continue
    with open(path, newline="") as fh:
        text = fh.read()
    new = text
    for pattern, repl in RULES:
        new, n = pattern.subn(repl, new)
        total += n
    if new != text:
        with open(path, "w", newline="") as fh:
            fh.write(new)
print(f"rewritten: {total}")
