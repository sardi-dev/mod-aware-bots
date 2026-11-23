#!/usr/bin/env python3
"""Export the crafting recipes (profession spells that create an item) from the 5.4.8 client data.

usage: export_recipes.py DATA_DIR OUT_DIR

Reads DATA_DIR/dbc (WDBC) and DATA_DIR/db2 (WDB2) in place and writes five TSV files to OUT_DIR:
  recipes.tsv      skill_id, skill_name, spell_id, spell_name, min_rank, trivial_low, trivial_high, acquire,
                   effect, item_id, count (EffectBasePoints, at least 1), die_sides
  reagents.tsv     spell_id, reagent_item_id, count
  maps.tsv         map_id, instance_type (1 dungeon, 2 raid, 3 battleground, 4 arena, 5 scenario), name
  journal_items.tsv  item_id, encounter_id, map_id, difficulty_mask (JournalEncounterItem, JournalEncounter,
                   JournalInstance: the Dungeon Journal's boss loot)
  item_price_base.tsv  item_level, armor_factor, weapon_factor
Files: SkillLineAbility.dbc (skill, spell, ranks), SkillLine.dbc (names), Spell.dbc (names), SpellEffect.dbc (effects
24, 59 and 157 create an item, field 11 is the item), SpellReagents.db2 (reagents, db2 because the dbc is empty; the record id is Spell.dbc field 19, counts follow the
eight reagent ids),
Map.dbc, the Journal tables, ItemPriceBase.dbc.
"""
import struct
import sys
from pathlib import Path

EFFECT_CREATE_ITEM, EFFECT_CREATE_RANDOM_ITEM, EFFECT_CREATE_ITEM_2 = 24, 59, 157
CREATE_EFFECTS = (EFFECT_CREATE_ITEM, EFFECT_CREATE_RANDOM_ITEM, EFFECT_CREATE_ITEM_2)
SPELL_REAGENTS_FIELD = 19   # Spell.dbc: id of the SpellReagents.db2 record (core DBCStructure.h, SpellReagentsId)
PROFESSION_SKILLS = {164, 165, 171, 182, 185, 186, 197, 202, 333, 356, 393, 755, 773, 129, 794}


def read_wdbc(path):
    data = Path(path).read_bytes()
    magic, count, fields, size, string_size = struct.unpack_from('<4s4I', data, 0)
    assert magic == b'WDBC' and size == fields * 4, path
    unpack = struct.Struct('<{}i'.format(fields)).unpack_from
    rows = [unpack(data, 20 + i * size) for i in range(count)]
    base = 20 + count * size
    return rows, data[base:base + string_size]


def read_wdb2(path):
    data = Path(path).read_bytes()
    magic, count, fields, size, string_size, _hash, _build, _stamp, low, high = struct.unpack_from('<4s9I', data, 0)
    assert magic == b'WDB2', path
    offset = 48
    if high:
        offset += (high - low + 1) * 6
    unpack = struct.Struct('<{}i'.format(fields)).unpack_from
    return [unpack(data, offset + i * size) for i in range(count)]


def text(strings, offset):
    if offset <= 0 or offset >= len(strings):
        return ''
    end = strings.find(b'\0', offset)
    return strings[offset:end].decode('utf-8', 'replace').replace('\t', ' ')


def main():
    data_dir, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    dbc = data_dir / 'dbc'
    lines, strings = read_wdbc(dbc / 'SkillLine.dbc')
    skill_names = {row[0]: text(strings, row[2]) for row in lines}
    spells, strings = read_wdbc(dbc / 'Spell.dbc')
    spell_names = {row[0]: text(strings, row[1]) for row in spells}
    effects, _ = read_wdbc(dbc / 'SpellEffect.dbc')
    creates = {}
    for row in effects:
        if row[2] in CREATE_EFFECTS and row[11] > 0 and row[1] == 0:
            creates.setdefault(row[27], []).append((row[2], row[11], row[6], row[10]))
    abilities, _ = read_wdbc(dbc / 'SkillLineAbility.dbc')
    with (out_dir / 'recipes.tsv').open('w') as handle:
        for row in abilities:
            skill, spell = row[1], row[2]
            if skill not in PROFESSION_SKILLS or spell not in creates:
                continue
            for effect, item, base, die in creates[spell]:
                handle.write('\t'.join(str(v) for v in (
                    skill, skill_names.get(skill, ''), spell, spell_names.get(spell, ''), row[5], row[9], row[8],
                    row[7], effect, item, max(base, 1), die)) + '\n')
    reagents_of = {}
    for row in read_wdb2(data_dir / 'db2' / 'SpellReagents.db2'):
        reagents_of[row[0]] = [(row[1 + i], row[9 + i]) for i in range(8) if row[1 + i] > 0 and row[9 + i] > 0]
    with (out_dir / 'reagents.tsv').open('w') as handle:
        for row in spells:
            for reagent, count in reagents_of.get(row[SPELL_REAGENTS_FIELD], []):
                handle.write('{}\t{}\t{}\n'.format(row[0], reagent, count))
    maps, strings = read_wdbc(dbc / 'Map.dbc')
    with (out_dir / 'maps.tsv').open('w') as handle:
        for row in maps:
            handle.write('{}\t{}\t{}\n'.format(row[0], row[2], text(strings, row[5])))
    instances, _ = read_wdbc(dbc / 'JournalInstance.dbc')
    instance_map = {row[0]: row[1] for row in instances}
    encounters, _ = read_wdbc(dbc / 'JournalEncounter.dbc')
    encounter_instance = {row[0]: row[6] for row in encounters}
    journal_items, _ = read_wdbc(dbc / 'JournalEncounterItem.dbc')
    with (out_dir / 'journal_items.tsv').open('w') as handle:
        for row in journal_items:
            instance = encounter_instance.get(row[1], 0)
            handle.write('{}\t{}\t{}\t{}\n'.format(row[2], row[1], instance_map.get(instance, 0), row[3]))
    prices, _ = read_wdbc(dbc / 'ItemPriceBase.dbc')
    with (out_dir / 'item_price_base.tsv').open('w') as handle:
        for row in prices:
            armor, weapon = struct.unpack('<ff', struct.pack('<ii', row[2], row[3]))
            handle.write('{}\t{}\t{}\n'.format(row[1], armor, weapon))


if __name__ == '__main__':
    main()
