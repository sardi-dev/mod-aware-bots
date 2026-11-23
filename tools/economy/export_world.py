#!/usr/bin/env python3
"""Export the world tables the pricing audit reads into TSV files (read-only SELECTs through db.sh).

usage: export_world.py OUT_DIR --db-sh PATH [--only TABLE ...]   (or set ECONOMY_DB_SH)

db.sh talks to the SELECT-only database account, so this script cannot change game data. Large tables are read
in key ranges to stay under the query timeout. Nothing here is committed: the TSV files stay in OUT_DIR.
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

STEP = 150000

ITEM_COLUMNS = ('entry, REPLACE(name, "\\t", " "), class, subclass, Quality, Flags, FlagsExtra, BuyCount, BuyPrice, '
                'SellPrice, InventoryType, AllowableClass, AllowableRace, ItemLevel, RequiredLevel, RequiredSkill, '
                'RequiredSkillRank, requiredspell, RequiredReputationFaction, maxcount, stackable, bonding, '
                'startquest, itemset, BagFamily, ItemLimitCategory, HolidayId, duration, flagsCustom, '
                'spellid_1, spelltrigger_1, spellid_2, ContainerSlots, RandomProperty, RandomSuffix, '
                'ScalingStatDistribution, Map')

LOOT_COLUMNS = 'entry, item, ChanceOrQuestChance, lootmode, groupid, mincountOrRef, maxcount'

# name -> (SQL, key column for range reads or None)
QUERIES = {
    'item_template': ('SELECT {} FROM world.item_template'.format(ITEM_COLUMNS), 'entry'),
    'npc_vendor': ('SELECT entry, item, maxcount, incrtime, ExtendedCost, type FROM world.npc_vendor', 'entry'),
    'creature_loot': ('SELECT {} FROM world.creature_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'reference_loot': ('SELECT {} FROM world.reference_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'gameobject_loot': ('SELECT {} FROM world.gameobject_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'item_loot': ('SELECT {} FROM world.item_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'skinning_loot': ('SELECT {} FROM world.skinning_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'pickpocketing_loot': ('SELECT {} FROM world.pickpocketing_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'fishing_loot': ('SELECT {} FROM world.fishing_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'prospecting_loot': ('SELECT {} FROM world.prospecting_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'milling_loot': ('SELECT {} FROM world.milling_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'disenchant_loot': ('SELECT {} FROM world.disenchant_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'spell_loot': ('SELECT {} FROM world.spell_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'mail_loot': ('SELECT {} FROM world.mail_loot_template'.format(LOOT_COLUMNS), 'entry'),
    'creature_template': (
        'SELECT entry, difficulty_entry_1, difficulty_entry_2, difficulty_entry_3, REPLACE(name, "\\t", " "), '
        'minlevel, maxlevel, npc_rank, type, lootid, skinloot, pickpocketloot, flags_extra, npcflag, faction_A, ScriptName '
        'FROM world.creature_template', 'entry'),
    'creature_maps': ('SELECT id, map, COUNT(*), BIT_OR(spawnMask) FROM world.creature GROUP BY id, map', None),
    'gameobject_template': (
        'SELECT entry, type, REPLACE(name, "\\t", " "), data0, data1, data2, data3 FROM world.gameobject_template',
        'entry'),
    'gameobject_maps': ('SELECT id, map, COUNT(*) FROM world.gameobject GROUP BY id, map', None),
    'instance_template': ('SELECT map, parent FROM world.instance_template', None),
    'quest_rewards': (
        'SELECT RewardItemId1, RewardItemId2, RewardItemId3, RewardItemId4, RewardChoiceItemId1, RewardChoiceItemId2, '
        'RewardChoiceItemId3, RewardChoiceItemId4, RewardChoiceItemId5, RewardChoiceItemId6 '
        'FROM world.quest_template', None),
    'skill_discovery': ('SELECT spellId, reqSpell, chance FROM world.skill_discovery_template', None),
    'skill_extra_item': ('SELECT spellId, requiredSpecialization, additionalCreateChance, additionalMaxNum '
                         'FROM world.skill_extra_item_template', None),
}


def parse_table(text):
    rows = []
    for line in text.splitlines():
        if line.startswith('|'):
            rows.append([None if c.strip() == 'NULL' else c.strip() for c in line.rstrip()[1:-1].split('|')])
    return rows


def run(db_sh, sql):
    result = subprocess.run([str(db_sh), sql], text=True, capture_output=True)
    if result.returncode:
        raise SystemExit('query failed: ' + (result.stderr.strip().splitlines() or ['?'])[0][:200])
    return parse_table(result.stdout)


def export(db_sh, out_dir, name, sql, key):
    """Write one table. A table with a key column is read in key ranges; a small one in one query."""
    path = out_dir / (name + '.tsv')
    count = 0
    with path.open('w') as handle:
        if key is None:
            batches = [run(db_sh, sql)[1:]]
        else:
            table = sql.split(' FROM ')[1]
            maxkey = int(run(db_sh, 'SELECT MAX({}) FROM {}'.format(key, table))[1][0] or 0)
            batches = []
            for low in range(0, maxkey + 1, STEP):
                part = run(db_sh, '{} WHERE {} >= {} AND {} < {}'.format(sql, key, low, key, low + STEP))
                batches.append(part[1:])
        for batch in batches:
            for row in batch:
                handle.write('\t'.join('\\N' if cell is None else cell for cell in row) + '\n')
                count += 1
    print('{}: {} rows'.format(name, count), file=sys.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('out_dir')
    parser.add_argument('--db-sh', default=os.environ.get('ECONOMY_DB_SH'), help='path of the read-only SQL wrapper script')
    parser.add_argument('--only', nargs='*')
    args = parser.parse_args()
    if not args.db_sh:
        parser.error('give --db-sh or set ECONOMY_DB_SH')
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, (sql, key) in QUERIES.items():
        if args.only and name not in args.only:
            continue
        export(Path(args.db_sh), out_dir, name, sql, key)


if __name__ == '__main__':
    main()
