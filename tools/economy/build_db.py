#!/usr/bin/env python3
"""Load the TSV files written by export_world.py and export_recipes.py into one SQLite file for the audit.

usage: build_db.py TSV_DIR [DB_FILE]     (DB_FILE defaults to TSV_DIR/econ.db)
"""
import sqlite3
import sys
from pathlib import Path

TEXT_COLUMNS = {'name', 'skillname', 'spellname', 'script'}
REAL_COLUMNS = {'chance', 'armor', 'weapon'}
LOOT = 'entry,item,chance,lootmode,groupid,minref,maxcount'

TABLES = {
    'item_template': 'entry,name,class,subclass,quality,flags,flagsextra,buycount,buyprice,sellprice,invtype,'
                     'allowclass,allowrace,ilvl,reqlevel,reqskill,reqskillrank,reqspell,reqrepfaction,maxcount,'
                     'stackable,bonding,startquest,itemset,bagfamily,limitcat,holiday,duration,flagscustom,spell1,'
                     'trig1,spell2,containerslots,randprop,randsuffix,scalingdist,map',
    'npc_vendor': 'entry,item,maxcount,incrtime,extcost,type',
    'creature_template': 'entry,d1,d2,d3,name,minlevel,maxlevel,rank,type,lootid,skinloot,pickloot,flagsextra,'
                         'npcflag,faction,script',
    'creature_maps': 'id,map,n,spawnmask',
    'gameobject_template': 'entry,type,name,d0,d1,d2,d3',
    'gameobject_maps': 'id,map,n',
    'instance_template': 'map,parent',
    'quest_rewards': 'r1,r2,r3,r4,c1,c2,c3,c4,c5,c6',
    'skill_discovery': 'spell,req,chance',
    'skill_extra_item': 'spell,spec,chance,maxnum',
    'recipes': 'skill,skillname,spell,spellname,minrank,trivlow,trivhigh,acquire,effect,item,count,die',
    'reagents': 'spell,item,count',
    'item_price_base': 'ilvl,armor,weapon',
    'maps': 'map,itype,name',
    'journal_items': 'item,encounter,map,diffmask',
}
for loot_table in ('creature_loot reference_loot gameobject_loot item_loot skinning_loot pickpocketing_loot '
                   'fishing_loot prospecting_loot milling_loot disenchant_loot spell_loot mail_loot').split():
    TABLES[loot_table] = LOOT


def column_type(name):
    if name in TEXT_COLUMNS:
        return 'TEXT'
    return 'REAL' if name in REAL_COLUMNS else 'INTEGER'


def load(con, tsv_dir, table, columns):
    names = columns.split(',')
    con.execute('DROP TABLE IF EXISTS ' + table)
    con.execute('CREATE TABLE {} ({})'.format(table, ','.join(c + ' ' + column_type(c) for c in names)))
    rows = []
    with open(tsv_dir / (table + '.tsv'), encoding='utf-8') as handle:
        for line in handle:
            cells = line.rstrip('\n').split('\t')
            if len(cells) != len(names):
                raise SystemExit('{}: expected {} columns, got {}: {}'.format(table, len(names), len(cells), cells[:3]))
            rows.append([None if c == '\\N' else c for c in cells])
    con.executemany('INSERT INTO {} VALUES ({})'.format(table, ','.join('?' * len(names))), rows)


def main():
    tsv_dir = Path(sys.argv[1])
    path = Path(sys.argv[2]) if len(sys.argv) > 2 else tsv_dir / 'econ.db'
    con = sqlite3.connect(path)
    for table, columns in TABLES.items():
        load(con, tsv_dir, table, columns)
    con.commit()
    print('wrote', path)


if __name__ == '__main__':
    main()
