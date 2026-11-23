/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 * Character appearance options (CharSections.dbc). Legends of Azeroth's core loads this client table as
 * sChrSectionStore; SkyFire does not load it, so the module reads it once from the server's DBC folder with SkyFire's
 * own loader. Random bots pick their skin, face and hair from it.
 */

#ifndef MOD_AWARE_BOTS_COMPAT_BOTCHARSECTIONS_H
#define MOD_AWARE_BOTS_COMPAT_BOTCHARSECTIONS_H

#include "DBCStore.h"
#include "Log.h"
#include "World.h"

#include <string>

// LoA's CharSectionsEntry (DBCStructure.h) and its format "diiixxxiii": the id is the index, the three texture
// names are skipped.
struct BotCharSectionsEntry
{
    uint32 RaceID;
    uint32 SexID;
    uint32 BaseSection;
    uint32 Flags;
    uint32 VariationIndex;
    uint32 ColorIndex;
};

constexpr char BotCharSectionsFmt[] = "diiixxxiii";

// LoA's CharSectionFlags and CharSectionType (DBCStructure.h).
constexpr uint32 SECTION_FLAG_PLAYER = 0x01;
constexpr uint32 SECTION_FLAG_DEATH_KNIGHT = 0x04;
constexpr uint32 SECTION_TYPE_SKIN = 0;
constexpr uint32 SECTION_TYPE_FACE = 1;
constexpr uint32 SECTION_TYPE_FACIAL_HAIR = 2;
constexpr uint32 SECTION_TYPE_HAIR = 3;

// Loaded on first use; empty (with an error logged) when the file cannot be read.
inline DBCStorage<BotCharSectionsEntry> const& BotCharSectionStore()
{
    static DBCStorage<BotCharSectionsEntry> store(BotCharSectionsFmt);
    static bool const loaded = [] {
        std::string const path = sWorld->GetDataPath() + "dbc/CharSections.dbc";
        bool const ok = store.Load(path.c_str(), nullptr);
        if (!ok)
            SF_LOG_ERROR("playerbots", "Cannot read %s: random bots get the default appearance", path.c_str());
        return ok;
    }();
    (void)loaded;
    return store;
}

#endif
