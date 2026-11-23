"""Applies member renames at the exact positions a compile log reports. Usage: fixloc.py <build log> <module root>

A rename is safe only on the type the compiler names (for example QuestObjective::ID becomes Id, while other structs
keep their ID), so this tool edits the token at the reported line and column, and only for the listed (type, member)
pairs. It prints what it changed and what it left.
"""
import collections
import pathlib
import re
import sys

LOG, ROOT = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
# optional: only edit files under these src/ prefixes (others are being edited elsewhere)
ONLY = tuple(sys.argv[3].split(",")) if len(sys.argv) > 3 else None

# (substring of the type the compiler names, LoA member) -> (text to find at the position, replacement)
MEMBERS = {
    ("QuestObjective", "ObjectID"): ("ObjectID", "ObjectId"),
    ("QuestObjective", "ID"): ("ID", "Id"),
    ("QuestObjective", "StorageIndex"): ("StorageIndex", "Index"),
    ("Quest", "GetLogTitle"): ("GetLogTitle", "GetTitle"),
    ("Quest", "GetAllowableClasses"): ("GetAllowableClasses", "GetRequiredClasses"),
    ("", "LearnSpell"): ("LearnSpell", "learnSpell"),
    ("", "GetThreatManager"): ("GetThreatManager", "getThreatManager"),
    ("", "HasUnitFlag"): ("HasUnitFlag(", "HasFlag(UNIT_FIELD_FLAGS, "),
    ("", "HasNpcFlag"): ("HasNpcFlag(", "HasFlag(UNIT_FIELD_NPC_FLAGS, "),
    ("", "HasPlayerFlag"): ("HasPlayerFlag(", "HasFlag(PLAYER_FIELD_PLAYER_FLAGS, "),
    ("", "HasSpiritOfRedemptionAura"): ("HasSpiritOfRedemptionAura()", "HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION)"),
    ("ObjectAccessor", "FindConnectedPlayer"): ("FindConnectedPlayer", "FindPlayer"),
    ("LFGMgr", "GetActiveState"): ("GetActiveState", "GetState"),  # SkyFire's LFG has one queue per player
    ("Battleground", "IsArena"): ("IsArena", "isArena"),
    ("Player", "RemoveSpell"): ("RemoveSpell", "removeSpell"),
    ("Player", "GetRaceMask"): ("GetRaceMask", "getRaceMask"),
    ("Player", "GetClassMask"): ("GetClassMask", "getClassMask"),
    ("Player", "LearnSkillRewardedSpells"): ("LearnSkillRewardedSpells", "learnSkillRewardedSpells"),
    ("TalentEntry", "PlayerClass"): ("PlayerClass", "playerClass"),
    ("TalentEntry", "ID"): ("ID", "TalentID"),
    # client data structs: SkyFire's field names
    ("AreaTableEntry", "ID"): ("ID", "m_ID"),
    ("AreaTableEntry", "Flags"): ("Flags", "m_flags"),
    ("AreaTableEntry", "ParentAreaID"): ("ParentAreaID", "m_ParentAreaID"),
    ("AreaTableEntry", "team"): ("team", "m_FactionGroupMask"),
    ("AreaTableEntry", "area_name"): ("area_name", "m_AreaName"),
    ("AreaTriggerEntry", "ID"): ("ID", "id"),
    ("AreaTriggerEntry", "ContinentID"): ("ContinentID", "mapid"),
    ("AreaTriggerEntry", "Pos"): ("Pos.", ""),  # Pos.X -> x: the axis is lowered below
    ("TaxiPathNodeEntry", "LocX"): ("LocX", "x"),
    ("TaxiPathNodeEntry", "LocY"): ("LocY", "y"),
    ("TaxiPathNodeEntry", "LocZ"): ("LocZ", "z"),
    ("TaxiPathNodeEntry", "ContinentID"): ("ContinentID", "mapid"),
    ("TalentEntry", "SpellID"): ("SpellID", "SpellId"),
    ("TalentEntry", "TierID"): ("TierID", "Row"),
    ("TalentEntry", "ColumnIndex"): ("ColumnIndex", "Col"),
    ("TransportAnimationEntry", "TimeIndex"): ("TimeIndex", "TimeSeg"),
    ("SkillLineAbilityEntry", "SkillLine"): ("SkillLine", "skillId"),
    ("SkillLineAbilityEntry", "Spell"): ("Spell", "spellId"),
    ("CreatureData", "mapId"): ("mapId", "mapid"),
    ("CreatureTemplate", "faction"): ("faction", "faction_A"),
}

# LoA members that SkyFire lacks and the module gets as free helpers (src/compat/BotTerrain.h):
# receiver->Member(args) becomes Helper(receiver, args).
FREE = {
    "GetMapHeight": "BotGetMapHeight",
    "GetMapWaterOrGroundLevel": "BotGetMapWaterOrGroundLevel",
    "GetHoverHeight": "BotGetHoverHeight",
    "isSwimming": "BotIsSwimming",
    "GetWorldLocation": "BotGetWorldLocation",
    "GetCreatureListWithEntryInGridAppend": "BotGetCreatureListWithEntryInGridAppend",
    "CheckCollisionAndGetValidCoords": "BotCheckCollisionAndGetValidCoords",
}
RECEIVER = re.compile(r"((?:[A-Za-z_]\w*(?:\([^()]*\))?(?:->|\.))*[A-Za-z_]\w*(?:\([^()]*\))?)(->|\.)$")

ERR = re.compile(r"^/src/modules/mod-aware-bots-548/(src/[^:]+):(\d+):(\d+): error: "
                 r"(?:'([^']+)' has no member named '([^']+)'|'([^']+)' is not a member of '([^']+)')")

edits = collections.defaultdict(set)  # file -> {(line, col, find, repl)}
left = collections.Counter()
for line in LOG.read_text(errors="replace").splitlines():
    m = ERR.match(line)
    if not m:
        continue
    path, ln, col = m.group(1), int(m.group(2)), int(m.group(3))
    if ONLY and not path.startswith(ONLY):
        continue
    owner, member = (m.group(4), m.group(5)) if m.group(5) else (m.group(7), m.group(6))
    rule = next((v for (t, mem), v in MEMBERS.items() if mem == member and t in owner), None)
    if member in FREE:
        edits[path].add((ln, col, "FREE", member))
    elif rule:
        edits[path].add((ln, col, rule[0], rule[1]))
    else:
        left[f"{owner} :: {member}"] += 1

done = 0
for path, items in edits.items():
    file = ROOT / path
    with open(file, encoding="utf-8", errors="surrogateescape", newline="") as fh:
        lines = fh.read().split("\n")
    # right to left within a line so earlier columns stay valid
    for ln, col, find, repl in sorted(items, key=lambda x: (x[0], -x[1])):
        text = lines[ln - 1]
        if find == "FREE":
            member = repl
            if not text.startswith(member + "(", col - 1):
                continue
            m = RECEIVER.search(text[:col - 1])
            if not m:
                continue
            after = text[col - 1 + len(member) + 1:]
            receiver = m.group(1)
            sep = ")" if after.startswith(")") else ", "
            body = after[1:] if sep == ")" else after
            lines[ln - 1] = text[:m.start()] + FREE[member] + "(" + receiver + (")" if sep == ")" else ", ") + body
            done += 1
            continue
        idx = text.find(find, col - 1)
        # only at the exact reported column, so running the tool twice on the same log changes nothing twice
        if idx == col - 1 and not text.startswith(repl, col - 1):
            rest = text[idx + len(find):]
            if find == "Pos." and rest[:1] in "XYZO":
                rest = rest[0].lower() + rest[1:]
            lines[ln - 1] = text[:idx] + repl + rest
            done += 1
    with open(file, "w", encoding="utf-8", errors="surrogateescape", newline="") as fh:
        fh.write("\n".join(lines))

print(f"applied {done} edits in {len(edits)} files")
for k, n in left.most_common(40):
    print(f"  left {n:4d} {k}")
