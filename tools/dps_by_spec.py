#!/usr/bin/env python3
"""Damage per specialisation from the [DPSFIGHT] lines of a proof run.

Reads only. Feed it the worldserver log of the run:

    docker logs <worldserver container> 2>&1 | dps_by_spec.py
    dps_by_spec.py Server.log --min-level 10 --max-level 35
    dps_by_spec.py --self-test

One [DPSFIGHT] line is written per fight of a damage dealer in a group (strategy/generic/DpsMeter.cpp). The damage is the
hit before absorbs and overkill, so numbers compare specialisations of the same level on the same packs; they are not a
combat log's. The table answers three questions of the damage class sheets (docs/class-sheets/):

  - damage: the median and the 25th and 75th percentile of damage per second per specialisation and fight weight, and the
    share of fights with no damage at all (a bot that stood idle while the group fought);
  - cooldowns: per specialisation, how many fights cast each spell with a recovery of a minute or more, and in which fight
    weight (trash, pack, elite pack, boss), so "never used" and "spent on trash" show;
  - tiers: the same median per skill tier, which must order expert above good above casual for a sheet to be applied.
"""
import argparse
import re
import statistics
import sys
from collections import defaultdict

SPECS = {
    62: "mage arcane", 63: "mage fire", 64: "mage frost", 70: "paladin retribution", 71: "warrior arms",
    72: "warrior fury", 102: "druid balance", 103: "druid feral", 251: "deathknight frost", 252: "deathknight unholy",
    253: "hunter beastmastery", 254: "hunter marksmanship", 255: "hunter survival", 258: "priest shadow",
    259: "rogue assassination", 260: "rogue combat", 261: "rogue subtlety", 262: "shaman elemental",
    263: "shaman enhancement", 265: "warlock affliction", 266: "warlock demonology", 267: "warlock destruction",
    269: "monk windwalker",
}

LINE = re.compile(
    r"\[DPSFIGHT\] (?P<bot>\S+) class=(?P<cls>\d+) spec=(?P<spec>\d+) level=(?P<level>\d+) tier=(?P<tier>\w+) "
    r"secs=(?P<secs>\d+) damage=(?P<damage>\d+) dps=(?P<dps>\d+) pet=(?P<pet>\d+) casts=(?P<casts>\d+) "
    r"attackers=(?P<attackers>\d+) elites=(?P<elites>\d+) boss=(?P<boss>\d) group=(?P<group>\d+) map=(?P<map>\d+) "
    r"cooldowns=(?P<cooldowns>.*)$")

TIERS = ("expert", "good", "casual")


def weight(fight):
    """Fight weight as the cooldown rule sees it (DpsCooldownPolicy: a boss, or two elites)."""
    if fight["boss"]:
        return "boss"
    if fight["elites"] >= 2:
        return "elite pack"
    if fight["attackers"] >= 3:
        return "pack"
    return "trash"


def parse(lines, min_level, max_level):
    fights = []
    for line in lines:
        m = LINE.search(line)
        if not m:
            continue
        f = {k: (int(v) if v.isdigit() else v) for k, v in m.groupdict().items()}
        f["boss"] = bool(f["boss"])
        if not min_level <= f["level"] <= max_level:
            continue
        cds = {}
        if f["cooldowns"] != "-":
            for item in f["cooldowns"].split(","):
                name, _, count = item.rpartition("=")
                if name and count.isdigit():
                    cds[name] = int(count)
        f["cds"] = cds
        f["weight"] = weight(f)
        fights.append(f)
    return fights


def quartiles(values):
    if len(values) < 4:
        return (min(values), statistics.median(values), max(values))
    q = statistics.quantiles(values, n=4)
    return (q[0], q[1], q[2])


def report(fights, out):
    by_spec = defaultdict(list)
    for f in fights:
        by_spec[f["spec"]].append(f)

    print("Damage per second by specialisation (fights of 8 s or more)", file=out)
    print(f"{'specialisation':<24}{'fights':>7}{'idle':>6}{'p25':>8}{'median':>8}{'p75':>8}{'pet med':>9}  by weight (median dps)", file=out)
    for spec, rows in sorted(by_spec.items(), key=lambda kv: SPECS.get(kv[0], str(kv[0]))):
        dps = [r["dps"] for r in rows]
        p25, med, p75 = quartiles(dps)
        idle = sum(1 for r in rows if r["damage"] == 0)
        weights = defaultdict(list)
        for r in rows:
            weights[r["weight"]].append(r["dps"])
        wtxt = "  ".join(f"{w} {int(statistics.median(weights[w]))} (n={len(weights[w])})"
                         for w in ("trash", "pack", "elite pack", "boss") if weights.get(w))
        print(f"{SPECS.get(spec, spec):<24}{len(rows):>7}{idle:>6}{int(p25):>8}{int(med):>8}{int(p75):>8}"
              f"{int(statistics.median([r['pet'] for r in rows])):>9}  {wtxt}", file=out)

    print("\nCooldowns cast (spells with a recovery of a minute or more): fights with the spell / fights, by fight weight", file=out)
    for spec, rows in sorted(by_spec.items(), key=lambda kv: SPECS.get(kv[0], str(kv[0]))):
        per = defaultdict(lambda: defaultdict(int))
        for r in rows:
            for name in r["cds"]:
                per[name][r["weight"]] += 1
        counts = defaultdict(int)
        for r in rows:
            counts[r["weight"]] += 1
        if not per:
            print(f"{SPECS.get(spec, spec):<24}none cast in {len(rows)} fights", file=out)
            continue
        for name, w in sorted(per.items()):
            cells = "  ".join(f"{k} {w[k]}/{counts[k]}" for k in ("trash", "pack", "elite pack", "boss") if counts.get(k))
            print(f"{SPECS.get(spec, spec):<24}{name:<32}{cells}", file=out)

    print("\nMedian damage per second by skill tier (expert should lead, casual trail)", file=out)
    print(f"{'specialisation':<24}" + "".join(f"{t:>14}" for t in TIERS), file=out)
    for spec, rows in sorted(by_spec.items(), key=lambda kv: SPECS.get(kv[0], str(kv[0]))):
        cells = []
        for t in TIERS:
            d = [r["dps"] for r in rows if r["tier"] == t]
            cells.append(f"{int(statistics.median(d))} (n={len(d)})" if d else "-")
        print(f"{SPECS.get(spec, spec):<24}" + "".join(f"{c:>14}" for c in cells), file=out)


SAMPLE = [
    "[DPSFIGHT] Aria class=1 spec=71 level=30 tier=good secs=21 damage=21000 dps=1000 pet=0 casts=17 attackers=3 elites=0 boss=0 group=5 map=33 cooldowns=-",
    "[DPSFIGHT] Brom class=1 spec=71 level=32 tier=expert secs=40 damage=60000 dps=1500 pet=0 casts=30 attackers=3 elites=3 boss=0 group=5 map=33 cooldowns=Recklessness=1",
    "[DPSFIGHT] Cira class=8 spec=63 level=31 tier=casual secs=12 damage=0 dps=0 pet=0 casts=0 attackers=1 elites=0 boss=0 group=5 map=33 cooldowns=-",
    "[DPSFIGHT] Dane class=1 spec=71 level=33 tier=casual secs=18 damage=9000 dps=500 pet=0 casts=9 attackers=1 elites=0 boss=0 group=5 map=33 cooldowns=-",
    "[DPSFIGHT] Edda class=8 spec=63 level=45 tier=expert secs=60 damage=120000 dps=2000 pet=0 casts=44 attackers=1 elites=1 boss=1 group=5 map=33 cooldowns=Combustion=1,Alter Time=1",
]


def self_test():
    fights = parse(SAMPLE, 1, 90)
    assert len(fights) == 5, len(fights)
    assert [f["weight"] for f in fights] == ["pack", "elite pack", "trash", "trash", "boss"]
    assert fights[4]["cds"] == {"Combustion": 1, "Alter Time": 1}
    assert len(parse(SAMPLE, 40, 90)) == 1
    import io
    out = io.StringIO()
    report(fights, out)
    text = out.getvalue()
    assert "warrior arms" in text and "mage fire" in text and "boss 1/1" in text
    print("self test ok")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", nargs="?", help="a log file (default: standard input)")
    ap.add_argument("--min-level", type=int, default=1)
    ap.add_argument("--max-level", type=int, default=90)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return 0
    src = open(args.log, errors="replace") if args.log else sys.stdin
    fights = parse(src, args.min_level, args.max_level)
    if not fights:
        print("no [DPSFIGHT] lines in range (the line is written at info level by damage dealers in a group)", file=sys.stderr)
        return 1
    report(fights, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
