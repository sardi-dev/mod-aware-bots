#!/usr/bin/env python3
"""Write src/Utils/ReserveTable.h: what a bot should keep in gold for training, a riding skill and a mount.

usage: export_reserve_table.py DATA_DIR OUT_HEADER --db-sh PATH      (or set ECONOMY_DB_SH)

Read-only: world.npc_trainer, world.creature_template, world.item_template and world.npc_vendor through the SELECT-only
db.sh, and the spell names from DATA_DIR/dbc/Spell.dbc. The result is a small header (about 10 KB) the pure policy
src/Utils/ReservePolicy.h reads, so the bot's reserve needs no database access and no loader at run time:

  CLASS_SPELL_COST   per class, the cumulative cost in copper of the class trainer's spells by the level they need
  PROFESSION_RANKS   the cost and level of each profession rank spell (Apprentice to Illustrious Grand Master)
  RIDING_STEPS       Apprentice to Master Riding and the flight licence, in the order a bot buys them
  MOUNT_PRICE        the cheapest mount a vendor sells for plain gold

The output is deterministic: the same database gives the same bytes. A change in the trainer data is a new run and a
reviewed diff of the header (the header is committed, so nobody needs the database to build or test the module).
"""
import argparse
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import export_recipes
import export_world

MAX_LEVEL = 90
CLASS_SLOTS = 12   # class ids 1 to 11 (10 is unused); slot 0 stays empty
CRAFT_PROFESSIONS = ('Alchemist', 'Blacksmith', 'Enchanter', 'Engineer', 'Jewelcrafter', 'Leatherworker', 'Scribe', 'Tailor')
RANK_NAMES = ('Apprentice', 'Journeyman', 'Expert', 'Artisan', 'Master', 'Grand Master', 'Illustrious Grand Master')
RANK_RE = re.compile(r'^({}) (.+)$'.format('|'.join(sorted(RANK_NAMES, key=len, reverse=True))))
RIDING_RE = re.compile(r'^(Apprentice|Journeyman|Expert|Artisan|Master) Riding$')
LICENCE_NAME = "Flight Master's License"
# The riding skill value each rank gives (the module's BotFactory::InitMountSpells and BotMountPolicy use the same numbers).
RIDING_SKILL = {'Apprentice': 75, 'Journeyman': 150, 'Expert': 225, 'Artisan': 300, 'Master': 375}
RIDING_ORDER = ('Apprentice', 'Journeyman', 'Expert', LICENCE_NAME, 'Artisan', 'Master')
MOUNT_ITEM_CLASS, MOUNT_ITEM_SUBCLASS = 15, 5
UINT32_MAX = 4294967295

CLASS_REFERENCES = (
    'SELECT ct.trainer_class, -t.spell, COUNT(*) FROM world.npc_trainer t JOIN world.creature_template ct ON ct.entry = t.entry '
    'WHERE ct.trainer_type = 0 AND ct.trainer_class > 0 AND t.spell < 0 GROUP BY ct.trainer_class, -t.spell')
TRAINER_ROWS = 'SELECT entry, spell, spellcost, reqskill, reqskillvalue, reqlevel FROM world.npc_trainer WHERE entry >= 200000'
MOUNT_PRICES = (
    'SELECT MIN(i.BuyPrice), MIN(i.RequiredLevel) FROM world.item_template i JOIN world.npc_vendor v ON v.item = i.entry '
    'WHERE i.class = {} AND i.subclass = {} AND i.BuyPrice > 0 AND v.ExtendedCost = 0 AND i.RequiredLevel <= 20').format(
        MOUNT_ITEM_CLASS, MOUNT_ITEM_SUBCLASS)


def class_templates(references):
    """The trainer template each class trainer uses most: {class_id: template_entry}. references: (class, template, count)."""
    best = {}
    for class_id, template, count in references:
        if class_id not in best or (count, -template) > (best[class_id][0], -best[class_id][1]):
            best[class_id] = (count, template)
    return {class_id: template for class_id, (_, template) in best.items()}


def class_cost_table(templates, trainer_rows):
    """Per class a list of MAX_LEVEL + 1 cumulative costs: index n is the cost of every spell needing level n or less.

    trainer_rows: (entry, spell, cost, reqskill, reqskillvalue, reqlevel). A spell with no level counts from level 1.
    """
    by_entry = defaultdict(dict)
    for entry, spell, cost, _, _, level in trainer_rows:
        by_entry[entry][spell] = (cost, level)
    table = {}
    for class_id, template in sorted(templates.items()):
        per_level = [0] * (MAX_LEVEL + 1)
        for cost, level in by_entry.get(template, {}).values():
            per_level[min(max(level, 1), MAX_LEVEL)] += cost
        running, cumulative = 0, []
        for value in per_level:
            running += value
            cumulative.append(running)
        table[class_id] = cumulative
    return table


def profession_ranks(trainer_rows, spell_names):
    """[(level, cost, name)] one per rank, by level: the rank spells every crafting profession shares."""
    found = defaultdict(lambda: (UINT32_MAX, 0))
    for _, spell, cost, _, _, level in trainer_rows:
        match = RANK_RE.match(spell_names.get(spell, ''))
        if not match or match.group(2) not in CRAFT_PROFESSIONS:
            continue
        low, top = found[(match.group(2), match.group(1))]
        found[(match.group(2), match.group(1))] = (min(low, cost), max(top, level))
    per_rank = {}
    for (_, rank), (cost, level) in found.items():
        # The most expensive and the latest of the professions: the bot may hold any of them.
        old = per_rank.get(rank, (0, 0))
        per_rank[rank] = (max(old[0], cost), max(old[1], level))
    return [(level, cost, rank) for rank, (cost, level) in sorted(per_rank.items(), key=lambda kv: (kv[1][1], kv[1][0]))]


def riding_steps(trainer_rows, spell_names):
    """[(skill_given, cost, level, name)] in buying order; skill_given 0 is the flight licence (a spell, not a skill)."""
    found = {}
    for _, spell, cost, _, _, level in trainer_rows:
        name = spell_names.get(spell, '')
        match = RIDING_RE.match(name)
        key = match.group(1) if match else (LICENCE_NAME if name == LICENCE_NAME else None)
        if key is None or cost <= 0:
            continue
        if key not in found or cost < found[key][0]:
            found[key] = (cost, level)
    steps = []
    for key in RIDING_ORDER:
        if key in found:
            steps.append((RIDING_SKILL.get(key, 0), found[key][0], found[key][1], key))
    return steps


def render(class_table, ranks, riding, mount):
    lines = ['/*',
             ' * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).',
             ' *',
             ' * Generated by tools/economy/export_reserve_table.py from the 5.4.8 world data (world.npc_trainer, the class',
             ' * trainer templates, the riding trainer rows, the cheapest vendor mount). Do not edit by hand: run the script again',
             ' * and review the diff. Money is in copper.',
             ' */',
             '#ifndef MOD_AWARE_BOTS_RESERVETABLE_H',
             '#define MOD_AWARE_BOTS_RESERVETABLE_H',
             '',
             '#include <cstdint>',
             '',
             'namespace ReserveTable',
             '{',
             'constexpr uint32_t MAX_LEVEL = {};'.format(MAX_LEVEL),
             'constexpr uint32_t CLASS_SLOTS = {};  // class ids 1 to 11; slot 0 and 10 are empty'.format(CLASS_SLOTS),
             '',
             '// Cumulative cost of the class trainer\'s spells needing level n or less, per class id (the class trainer template',
             '// most trainers of that class use). The window between two levels is the difference of two entries.',
             'constexpr uint32_t CLASS_SPELL_COST[CLASS_SLOTS][MAX_LEVEL + 1] = {']
    for class_id in range(CLASS_SLOTS):
        values = class_table.get(class_id, [0] * (MAX_LEVEL + 1))
        assert all(v <= UINT32_MAX for v in values)
        lines.append('    {  // class %d' % class_id)
        for start in range(0, MAX_LEVEL + 1, 12):
            lines.append('        ' + ', '.join(str(v) for v in values[start:start + 12]) + ',')
        lines.append('    },')
    lines += ['};', '',
              'struct RankStep',
              '{',
              '    uint32_t level;   // the character level the trainer wants',
              '    uint32_t cost;    // copper',
              '};',
              '',
              '// One profession rank each (Apprentice to Illustrious Grand Master), by level; the crafting professions share them.',
              'constexpr RankStep PROFESSION_RANKS[] = {']
    for level, cost, rank in ranks:
        lines.append('    { %d, %d },  // %s' % (level, cost, rank))
    lines += ['};', '',
              'struct RidingStep',
              '{',
              '    uint32_t skillGiven;  // the riding skill value the step gives; 0 is the flight licence (a spell)',
              '    uint32_t cost;        // copper at the riding trainer',
              '    uint32_t level;       // the character level the trainer wants',
              '};',
              '',
              '// In the order a bot buys them; the licence comes after Expert Riding.',
              'constexpr RidingStep RIDING_STEPS[] = {']
    for skill, cost, level, name in riding:
        lines.append('    { %d, %d, %d },  // %s' % (skill, cost, level, name))
    lines += ['};', '',
              '// The cheapest mount a vendor sells for plain gold (no token, no reputation filter) and the level it wants.',
              'constexpr uint32_t MOUNT_PRICE = %d;' % mount[0],
              'constexpr uint32_t MOUNT_LEVEL = %d;' % mount[1],
              '}  // namespace ReserveTable',
              '',
              '#endif',
              '']
    return '\n'.join(lines)


def collect(db_sh, dbc_dir):
    """Reads the database and the spell names; returns the arguments of render()."""
    references = [(int(a), int(b), int(c)) for a, b, c in export_world.run(db_sh, CLASS_REFERENCES)[1:]]
    trainer_rows = [tuple(int(cell) for cell in row) for row in export_world.run(db_sh, TRAINER_ROWS)[1:]]
    spells, strings = export_recipes.read_wdbc(Path(dbc_dir) / 'Spell.dbc')
    spell_names = {row[0]: export_recipes.text(strings, row[1]) for row in spells}
    mount_row = export_world.run(db_sh, MOUNT_PRICES)[1]
    if not mount_row or mount_row[0] is None:
        raise SystemExit('no vendor mount found')
    # npc_trainer.entry also holds the class trainers' own rows (negative spells point at templates); templates are >= 200000.
    templates = class_templates(references)
    return (class_cost_table(templates, trainer_rows), profession_ranks(trainer_rows, spell_names),
            riding_steps(trainer_rows, spell_names), (int(mount_row[0]), int(mount_row[1])))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('data_dir')
    parser.add_argument('out_header')
    parser.add_argument('--db-sh', default=os.environ.get('ECONOMY_DB_SH'))
    args = parser.parse_args()
    if not args.db_sh:
        parser.error('give --db-sh or set ECONOMY_DB_SH')
    class_table, ranks, riding, mount = collect(Path(args.db_sh), Path(args.data_dir) / 'dbc')
    Path(args.out_header).write_text(render(class_table, ranks, riding, mount))
    print('classes {} ranks {} riding steps {} mount {}'.format(len(class_table), len(ranks), len(riding), mount), file=sys.stderr)


if __name__ == '__main__':
    main()
