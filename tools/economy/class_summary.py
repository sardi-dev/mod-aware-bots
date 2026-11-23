#!/usr/bin/env python3
"""Median (min-max) price per source class and quality group, in gold, as a Markdown table.

usage: class_summary.py DB_FILE
"""
import sqlite3
import sys

CLASSES = ('vendor', 'gathered', 'crafted', 'recipe', 'world_normal', 'world_elite', 'world_rare', 'world_chest',
           'dungeon_trash', 'dungeon_boss', 'raid_trash', 'raid_boss', 'container', 'quest_reward', 'vendor_token',
           'other')
GROUPS = ('grey', 'white', 'green', 'blue', 'epic')
COPPER_PER_GOLD = 10000
MIN_ITEMS = 3          # below this a cell only shows the count
GOLD_DECIMALS_BELOW = 10


def median(values):
    ordered = sorted(values)
    return ordered[len(ordered) // 2]


def gold(copper):
    value = copper / COPPER_PER_GOLD
    return '{:.2f}'.format(value) if value < GOLD_DECIMALS_BELOW else '{:,}'.format(round(value))


def main():
    con = sqlite3.connect(sys.argv[1])
    print('| Class | ' + ' | '.join(GROUPS) + ' |')
    print('|-------|' + '------|' * len(GROUPS))
    for kind in CLASSES:
        cells = []
        for group in GROUPS:
            rows = con.execute('SELECT lo, hi FROM item_price WHERE price_class = ? AND quality_group = ?',
                               (kind, group)).fetchall()
            if len(rows) >= MIN_ITEMS:
                cells.append('{}-{} ({:,})'.format(gold(median([r[0] for r in rows])), gold(median([r[1] for r in rows])), len(rows)))
            else:
                cells.append('n={}'.format(len(rows)) if rows else '-')
        print('| {} | {} |'.format(kind, ' | '.join(cells)))


if __name__ == '__main__':
    main()
