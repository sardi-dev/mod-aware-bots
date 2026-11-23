"""Mechanical rewrites, pass 4: grid visits. Usage: rewrite4.py <module src dir>

Cell::VisitAllObjects(object, searcher, range) is WorldObject::VisitNearbyObject(range, searcher) on SkyFire (the same
grid and world visit).
"""
import pathlib
import re
import sys

PATTERN = re.compile(r"Cell::VisitAllObjects\(\s*([A-Za-z_][\w>.\-]*)\s*,\s*([A-Za-z_]\w*)\s*,\s*([^;]+?)\)\s*;")
count = 0
for path in pathlib.Path(sys.argv[1]).rglob("*.cpp"):
    if "compat" in path.parts:
        continue
    with open(path, newline="") as fh:
        text = fh.read()
    new, n = PATTERN.subn(lambda m: f"{m.group(1)}->VisitNearbyObject({m.group(3)}, {m.group(2)});", text)
    if n:
        with open(path, "w", newline="") as fh:
            fh.write(new)
        count += n
print(f"rewritten: {count}")
