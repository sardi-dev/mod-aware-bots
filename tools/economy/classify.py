#!/usr/bin/env python3
"""Classify every item by where it comes from and write the result into the audit database.

usage: classify.py DB_FILE        (DB_FILE from build_db.py)

Adds the tables `item_source` (one row per item and source) and `item_class` (one row per item: listable flag (can be put on an auction house; price is not required), source
tags, primary source class, best drop chance, number of sources, source level). The rules are in the docstrings of
`expand_loot`, `creature_category` and `primary_class`; the audit document describes them in prose.
"""
import sqlite3
import sys
from collections import defaultdict

BONDING_NONE, BONDING_PICKUP, BONDING_EQUIP, BONDING_USE, BONDING_QUEST = 0, 1, 2, 3, 4
FLAG_CONJURED, FLAG_DEPRECATED, FLAG_BIND_TO_ACCOUNT = 0x2, 0x10, 0x08000000
MAP_DUNGEON, MAP_RAID = 1, 2
RANK_BOSS, RANK_RARE_ELITE, RANK_RARE, RANK_ELITE = 3, 2, 4, 1
ITEM_CLASS_TRADE_GOODS, ITEM_CLASS_RECIPE, ITEM_CLASS_QUEST = 7, 9, 12
GAMEOBJECT_TYPE_CHEST = 3
MAX_REFERENCE_DEPTH = 6

# Easiest first: the primary class of an item is the first of these it has a source in.
PRIMARY_ORDER = ('vendor', 'crafted', 'gathered', 'world_normal', 'world_elite', 'world_rare', 'world_chest',
                 'dungeon_trash', 'dungeon_boss', 'raid_trash', 'raid_boss', 'container', 'quest_reward', 'other')


def group_rows(rows):
    by_entry = defaultdict(list)
    for entry, item, chance, groupid, minref, maxcount in rows:
        by_entry[entry].append((item, chance, groupid, minref, maxcount))
    return by_entry


def combine(old, new):
    """Chance that at least one of two independent rolls hits (both in percent)."""
    return 100.0 - (100.0 - old) * (100.0 - new) / 100.0


def expand_loot(entry, tables, depth=0, cache=None):
    """Items a loot table entry can yield, as {item: chance in percent}.

    Plain rows keep their chance; rows of a group (groupid > 0) with chance 0 share what the explicit rows leave
    over; negative chances are quest-only drops and are skipped; a reference row (negative mincountOrRef) pulls in
    the reference table `-mincountOrRef`, rolled `maxcount` times.
    """
    own, references = tables
    cache = cache if cache is not None else {}
    key = (id(own), entry)
    if key in cache:
        return cache[key]
    result = {}
    cache[key] = result
    rows = own.get(entry, [])
    groups = defaultdict(list)
    for item, chance, groupid, minref, maxcount in rows:
        if chance < 0:
            continue
        groups[groupid].append((item, chance, minref, maxcount))
    for groupid, members in groups.items():
        if groupid:
            explicit = sum(c for _, c, _, _ in members if c > 0)
            zeros = sum(1 for _, c, _, _ in members if c <= 0)
            share = max(0.0, 100.0 - explicit) / zeros if zeros else 0.0
        for item, chance, minref, maxcount in members:
            effective = (chance if chance > 0 else share) if groupid else chance
            if minref < 0:
                if depth >= MAX_REFERENCE_DEPTH:
                    continue
                if not groupid and chance <= 0:
                    effective = 100.0
                rolls = max(1, maxcount)
                for sub_item, sub_chance in expand_loot(-minref, (references, references), depth + 1, cache).items():
                    hit = effective * sub_chance / 100.0
                    for _ in range(rolls - 1):
                        hit = combine(hit, effective * sub_chance / 100.0)
                    result[sub_item] = combine(result.get(sub_item, 0.0), hit)
            elif effective > 0:
                result[item] = combine(result.get(item, 0.0), effective)
    return result


def load_loot(con, table):
    rows = con.execute('SELECT entry, item, chance, groupid, minref, maxcount FROM ' + table)
    return group_rows(rows)


def creature_category(rank, map_type, is_boss):
    """World or instance class of a creature from its rank, boss status and the type of the map it is spawned on."""
    if map_type == MAP_RAID:
        return 'raid_boss' if is_boss else 'raid_trash'
    if map_type == MAP_DUNGEON:
        return 'dungeon_boss' if is_boss else 'dungeon_trash'
    if rank in (RANK_RARE, RANK_RARE_ELITE, RANK_BOSS):
        return 'world_rare'
    return 'world_elite' if rank == RANK_ELITE else 'world_normal'


def primary_class(kinds):
    for name in PRIMARY_ORDER:
        if name in kinds:
            return name
    return 'other'


def main():
    con = sqlite3.connect(sys.argv[1])
    map_type = {row[0]: row[1] for row in con.execute('SELECT map, itype FROM maps')}
    instance_maps = {row[0] for row in con.execute('SELECT map FROM instance_template')}

    # Creature facts. Difficulty variants (difficulty_entry_N) have no spawns of their own: they inherit the base map.
    creatures = {}
    boss = set()
    for entry, d1, d2, d3, minlevel, maxlevel, rank, lootid, skinloot, pickloot, script in con.execute(
            'SELECT entry, d1, d2, d3, minlevel, maxlevel, rank, lootid, skinloot, pickloot, script '
            'FROM creature_template'):
        creatures[entry] = dict(level=(minlevel + maxlevel) / 2.0, rank=rank, loot=lootid, skin=skinloot,
                                pick=pickloot, variants=[v for v in (d1, d2, d3) if v])
        if rank == RANK_BOSS or script.startswith('boss_'):
            boss.add(entry)
            boss.update(creatures[entry]['variants'])
    maps_of = defaultdict(set)
    for creature_id, map_id, count in con.execute('SELECT id, map, n FROM creature_maps'):
        maps_of[creature_id].add(map_id)
    for entry, info in creatures.items():
        for variant in info['variants']:
            maps_of[variant] |= maps_of.get(entry, set())

    def creature_class(entry):
        info = creatures[entry]
        maps = maps_of.get(entry, set())
        types = {map_type.get(m, 0) for m in maps}
        if MAP_RAID in types:
            return creature_category(info['rank'], MAP_RAID, entry in boss)
        if MAP_DUNGEON in types:
            return creature_category(info['rank'], MAP_DUNGEON, entry in boss)
        if not maps:
            return 'summoned'
        if types & {3, 4}:
            return 'pvp'
        return creature_category(info['rank'], 0, entry in boss)

    sources = []   # (item, kind, owner, chance, level, map)

    def add_creature_loot(table, field, kind_override=None):
        tables = load_loot(con, table)
        pair = (tables, load_loot(con, 'reference_loot'))
        cache = {}
        owners = defaultdict(list)
        for entry, info in creatures.items():
            if info[field]:
                owners[info[field]].append(entry)
        for loot_id, entries in owners.items():
            items = expand_loot(loot_id, pair, 0, cache)
            for creature in entries:
                kind = kind_override or creature_class(creature)
                maps = maps_of.get(creature, set())
                map_id = min(maps) if maps else 0
                for item, chance in items.items():
                    sources.append((item, kind, creature, chance, creatures[creature]['level'], map_id))

    add_creature_loot('creature_loot', 'loot')
    add_creature_loot('skinning_loot', 'skin', 'skinning')
    add_creature_loot('pickpocketing_loot', 'pick', 'pickpocket')

    # Game objects: chests and nodes (type 3) carry the loot id in data1.
    go_loot = load_loot(con, 'gameobject_loot')
    go_pair = (go_loot, load_loot(con, 'reference_loot'))
    go_maps = defaultdict(set)
    for object_id, map_id, count in con.execute('SELECT id, map, n FROM gameobject_maps'):
        go_maps[object_id].add(map_id)
    item_class = {row[0]: row[1] for row in con.execute('SELECT entry, class FROM item_template')}
    cache = {}
    for entry, kind, name, d1 in con.execute('SELECT entry, type, name, d1 FROM gameobject_template WHERE type = 3'):
        if not d1:
            continue
        maps = go_maps.get(entry, set())
        types = {map_type.get(m, 0) for m in maps}
        for item, chance in expand_loot(d1, go_pair, 0, cache).items():
            if types & {MAP_DUNGEON, MAP_RAID}:
                kind_name = 'dungeon_chest' if MAP_DUNGEON in types else 'raid_chest'
            elif item_class.get(item) == ITEM_CLASS_TRADE_GOODS:
                kind_name = 'gather_node'
            else:
                kind_name = 'world_chest'
            sources.append((item, kind_name, entry, chance, 0, min(maps) if maps else 0))

    for table, kind in (('fishing_loot', 'fishing'), ('prospecting_loot', 'prospecting'), ('milling_loot', 'milling'),
                        ('disenchant_loot', 'disenchant'), ('item_loot', 'container'), ('spell_loot', 'spell_loot'),
                        ('mail_loot', 'mail')):
        pair = (load_loot(con, table), load_loot(con, 'reference_loot'))
        cache = {}
        for entry in list(pair[0]):
            for item, chance in expand_loot(entry, pair, 0, cache).items():
                sources.append((item, kind, entry, chance, 0, 0))

    for row in con.execute('SELECT item, extcost, entry, maxcount FROM npc_vendor WHERE type = 1'):
        item, extcost, vendor, limited = row
        sources.append((item, 'vendor_gold' if not extcost else 'vendor_token', vendor, 100.0, 0, 0))
    for item, in con.execute('SELECT DISTINCT item FROM recipes'):
        sources.append((item, 'craft', 0, 100.0, 0, 0))
    for item, map_id in con.execute('SELECT item, map FROM journal_items'):
        kind = 'journal_raid' if map_type.get(map_id) == MAP_RAID else 'journal_dungeon'
        sources.append((item, kind, map_id, 0.0, 0, map_id))
    for row in con.execute('SELECT r1, r2, r3, r4, c1, c2, c3, c4, c5, c6 FROM quest_rewards'):
        for item in row:
            if item:
                sources.append((item, 'quest_reward', 0, 100.0, 0, 0))

    con.execute('DROP TABLE IF EXISTS item_source')
    con.execute('CREATE TABLE item_source (item INTEGER, kind TEXT, owner INTEGER, chance REAL, level REAL, map INTEGER)')
    con.executemany('INSERT INTO item_source VALUES (?,?,?,?,?,?)', sources)
    con.execute('CREATE INDEX item_source_item ON item_source(item)')
    con.commit()
    classify_items(con)


def classify_items(con):
    """One row per item: listable?, tags, primary class, best chance and level of the primary class's sources."""
    per_item = defaultdict(lambda: defaultdict(list))
    for item, kind, owner, chance, level, map_id in con.execute('SELECT item, kind, owner, chance, level, map FROM item_source'):
        per_item[item][kind].append((owner, chance, level, map_id))
    con.execute('DROP TABLE IF EXISTS item_class')
    con.execute('CREATE TABLE item_class (item INTEGER PRIMARY KEY, listable INTEGER, primary_class TEXT, tags TEXT, '
                'best_chance REAL, owners INTEGER, src_level REAL, vendor_gold INTEGER, vendor_token INTEGER)')
    rows = []
    query = ('SELECT entry, class, bonding, flags, buyprice, sellprice FROM item_template')
    for entry, item_class, bonding, flags, buyprice, sellprice in con.execute(query):
        listable = int(bonding in (BONDING_NONE, BONDING_EQUIP, BONDING_USE)
                       and not flags & (FLAG_CONJURED | FLAG_BIND_TO_ACCOUNT | FLAG_DEPRECATED)
                       and item_class != ITEM_CLASS_QUEST)
        kinds = per_item.get(entry, {})
        classes = set()
        if 'vendor_gold' in kinds:
            classes.add('vendor')
        if 'craft' in kinds:
            classes.add('crafted')
        if kinds.keys() & {'skinning', 'gather_node', 'fishing', 'prospecting', 'milling', 'disenchant'}:
            classes.add('gathered')
        for name in ('world_normal', 'world_elite', 'world_rare', 'dungeon_trash', 'dungeon_boss', 'raid_trash',
                     'raid_boss', 'world_chest'):
            if name in kinds:
                classes.add(name)
        if kinds.keys() & {'dungeon_chest'}:
            classes.add('dungeon_boss')
        if kinds.keys() & {'raid_chest'}:
            classes.add('raid_boss')
        if 'journal_dungeon' in kinds:
            classes.add('dungeon_boss')
        if 'journal_raid' in kinds:
            classes.add('raid_boss')
        if kinds.keys() & {'container', 'spell_loot', 'mail', 'pickpocket'}:
            classes.add('container')
        if 'quest_reward' in kinds:
            classes.add('quest_reward')
        primary = primary_class(classes)
        pool = [r for k, v in kinds.items() for r in v if k in (primary, 'vendor_gold') or
                (primary == 'gathered' and k in ('skinning', 'gather_node', 'fishing', 'prospecting', 'milling', 'disenchant')) or
                (primary == 'dungeon_boss' and k in ('dungeon_boss', 'dungeon_chest')) or
                (primary == 'raid_boss' and k in ('raid_boss', 'raid_chest'))]
        best = max((r[1] for r in pool), default=0.0)
        levels = [r[2] for r in pool if r[2]]
        rows.append((entry, listable, primary, ','.join(sorted(kinds)), best, len({(k, r[0]) for k, v in kinds.items() for r in v}),
                     sum(levels) / len(levels) if levels else 0.0, int('vendor_gold' in kinds), int('vendor_token' in kinds)))
    con.executemany('INSERT INTO item_class VALUES (?,?,?,?,?,?,?,?,?)', rows)
    con.commit()


if __name__ == '__main__':
    main()
