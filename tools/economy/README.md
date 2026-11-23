# Economy audit tools

Read-only helpers behind `docs/economy/pricing-audit.md`. They read the 5.4.8 world database through a
SELECT-only database wrapper script (any script that runs one query against the world database will do) and the client data in `data/dbc` and `data/db2`; nothing writes to a database or to the client
files. Python 3, standard library only. Generated files go to a scratch folder, never into the repository.

```sh
OUT=/path/to/scratch/econ                                   # any empty folder
python3 tools/economy/export_world.py $OUT --db-sh <read-only-db-wrapper>   # world tables to TSV (about 30 s)
python3 -I tools/economy/export_recipes.py <client-data-dir> $OUT  # recipes, reagents, maps, journal loot, price base
python3 -I tools/economy/build_db.py $OUT                   # TSV files to $OUT/econ.db (SQLite)
python3 -I tools/economy/classify.py $OUT/econ.db           # item_source and item_class tables
python3 -I tools/economy/price_model.py $OUT/econ.db        # item_price table: the (min, max) range per item
python3 -I tools/economy/examples.py $OUT/econ.db           # the audit's worked examples as Markdown
python3 -I tools/economy/affordability.py $OUT/econ.db bot_money.tsv
python3 -I tools/economy/export_price_table.py $OUT/econ.db data/sql/db-world/<YYYY_MM_DD_NN>_playerbots_item_price.sql
python3 -I tools/economy/export_reserve_table.py <client-data-dir> src/Utils/ReserveTable.h --db-sh <read-only-db-wrapper>
python3 -I tools/economy/reserve_impact.py bot_money.tsv    # how much of the bots' gold the reserve fund keeps back
python3 -I -m unittest discover -s tools/economy/tests -v
```

`export_world.py` also reads the wrapper path from `ECONOMY_DB_SH`. `bot_money.tsv` is one line per character
(level, copper, 1 for a random bot) from `characters.characters` joined to `auth.account`; only aggregates are printed.

Data checks worth running after a change to `price_model.py` (all must return 0):

```sql
-- a vendor-sold item never starts below its vendor price plus 10 percent (rounded up to the copper)
SELECT COUNT(*) FROM item_price p JOIN item_template i ON i.entry = p.item
 WHERE p.price_class = 'vendor' AND p.lo < 1.1 * i.buyprice / i.buycount - 0.000001;
-- nothing starts below what a vendor pays for it
SELECT COUNT(*) FROM item_price p JOIN item_template i ON i.entry = p.item WHERE p.lo < i.sellprice * 1.1 - 1;
-- every range has width
SELECT COUNT(*) FROM item_price WHERE lo > hi;
```

| File | Does |
|------|------|
| `export_world.py` | `item_template`, `npc_vendor`, the loot tables, `creature_template`, spawn maps, game objects, quest rewards |
| `export_recipes.py` | profession recipes (SkillLineAbility, Spell, SpellEffect), reagents (SpellReagents.db2), Map, Dungeon Journal loot, ItemPriceBase |
| `build_db.py` | loads the TSV files into one SQLite file |
| `classify.py` | expands loot tables (groups, references) and tags every item with its sources and a primary class |
| `price_model.py` | the proposed model, producing `item_price` |
| `listing_policy.py` | where in the range a listing lands, and the buy caps (the Python reference of `src/Utils/PricePolicy.h`) |
| `export_reserve_table.py` | writes `src/Utils/ReserveTable.h` (class trainer costs by level, profession ranks, the riding steps, the cheapest mount) from `npc_trainer`, `item_template`, `npc_vendor` and `Spell.dbc`; read-only, deterministic. The header is committed: nothing needs the database to build or test |
| `reserve_impact.py` | the Python reference of `src/Utils/ReservePolicy.h` and the median-bot impact table of `docs/economy/reserve-fund.md` |
| `affordability.py` | the bots' gold against the proposed prices |
| `examples.py`, `class_summary.py`, `export_price_table.py` | the worked examples; the median range per class; the SQL table the module loads (`data/sql/db-world`) |

A retune is a new dated SQL file: the core's updater applies each file once and never reruns an edited one (design decision 0028),
so regenerate into a file with a later date, keep the old file, and describe the change in section 13 of the audit.
