/*
* This file is part of the Legends of Azeroth Pandaria Project. See THANKS file for Copyright information
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Helper.h"
#include "RandomPlayerbotFactory.h"
#include "RandomPlayerbotMgr.h"

#include "AccountMgr.h"
#include "DBCStore.h"
#include "DatabaseEnv.h"
#include "CharacterCache.h"
#include "GuildMgr.h"
#include "Playerbots.h"
#include "PlayerbotAIConfig.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Timer.h"

#include <future>
#include <algorithm>
#include <cctype>

std::map<uint8, std::vector<uint8>> RandomPlayerbotFactory::availableRaces;

// AC Bot/Factory/RandomPlayerbotFactory.cpp:26-49 (#627). Cata/MoP races (goblin, worgen, pandaren) have no
// own name category in playerbots_names and fall to Generic, like AC's default.
constexpr RandomPlayerbotFactory::NameRaceAndGender RandomPlayerbotFactory::CombineRaceAndGender(uint8 race,
                                                                                                uint8 gender)
{
    NameRaceAndGender baseIndex;
    switch (race)
    {
        case RACE_ORC:        baseIndex = NameRaceAndGender::OrcMale; break;
        case RACE_DWARF:      baseIndex = NameRaceAndGender::DwarfMale; break;
        case RACE_NIGHTELF:   baseIndex = NameRaceAndGender::NightelfMale; break;
        case RACE_TAUREN:     baseIndex = NameRaceAndGender::TaurenMale; break;
        case RACE_GNOME:      baseIndex = NameRaceAndGender::GnomeMale; break;
        case RACE_TROLL:      baseIndex = NameRaceAndGender::TrollMale; break;
        case RACE_BLOODELF:   baseIndex = NameRaceAndGender::BloodelfMale; break;
        case RACE_DRAENEI:    baseIndex = NameRaceAndGender::DraeneiMale; break;
        case RACE_HUMAN:
        case RACE_UNDEAD_PLAYER:
        default:
            baseIndex = NameRaceAndGender::GenericMale;
            break;
    }

    return static_cast<NameRaceAndGender>(static_cast<uint8>(baseIndex) +
                                          ((gender >= GENDER_NONE) ? static_cast<uint8>(GENDER_MALE) : gender));
}

RandomPlayerbotFactory::RandomPlayerbotFactory(uint32 accountId) : accountId(accountId)
{
    uint32 const expansion = sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION);

    // warrior
    // alliance
    {
        availableRaces[CLASS_WARRIOR].push_back(RACE_HUMAN);
        availableRaces[CLASS_WARRIOR].push_back(RACE_DWARF);
        availableRaces[CLASS_WARRIOR].push_back(RACE_NIGHTELF);
        availableRaces[CLASS_WARRIOR].push_back(RACE_GNOME);
    }
    // horde
    {
        availableRaces[CLASS_WARRIOR].push_back(RACE_ORC);
        availableRaces[CLASS_WARRIOR].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_WARRIOR].push_back(RACE_TROLL);
        availableRaces[CLASS_WARRIOR].push_back(RACE_TAUREN);
    }

    // paladin
    // alliance
    {
        availableRaces[CLASS_PALADIN].push_back(RACE_HUMAN);
        availableRaces[CLASS_PALADIN].push_back(RACE_DWARF);
    }
    // horde
    {
        availableRaces[CLASS_PALADIN].push_back(RACE_TAUREN);
    }

    // hunter
    // alliance
    {
        availableRaces[CLASS_HUNTER].push_back(RACE_HUMAN);
        availableRaces[CLASS_HUNTER].push_back(RACE_DWARF);
        availableRaces[CLASS_HUNTER].push_back(RACE_NIGHTELF);
    }
    // horde
    {
        availableRaces[CLASS_HUNTER].push_back(RACE_ORC);
        availableRaces[CLASS_HUNTER].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_HUNTER].push_back(RACE_TROLL);
        availableRaces[CLASS_HUNTER].push_back(RACE_TAUREN);
    }

    // rogue
    // alliance
    {
        availableRaces[CLASS_ROGUE].push_back(RACE_HUMAN);
        availableRaces[CLASS_ROGUE].push_back(RACE_DWARF);
        availableRaces[CLASS_ROGUE].push_back(RACE_NIGHTELF);
        availableRaces[CLASS_ROGUE].push_back(RACE_GNOME);
    }
    // horde
    {
        availableRaces[CLASS_ROGUE].push_back(RACE_ORC);
        availableRaces[CLASS_ROGUE].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_ROGUE].push_back(RACE_TROLL);
    }

    // priest
    // alliance
    {
        availableRaces[CLASS_PRIEST].push_back(RACE_HUMAN);
        availableRaces[CLASS_PRIEST].push_back(RACE_DWARF);
        availableRaces[CLASS_PRIEST].push_back(RACE_NIGHTELF);
        availableRaces[CLASS_PRIEST].push_back(RACE_GNOME);
    }
    // horde
    {
        availableRaces[CLASS_PRIEST].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_PRIEST].push_back(RACE_TROLL);
        availableRaces[CLASS_PRIEST].push_back(RACE_TAUREN);
    }

    // shaman
    // alliance
    {
        availableRaces[CLASS_SHAMAN].push_back(RACE_DWARF);
    }
    // horde
    {
        availableRaces[CLASS_SHAMAN].push_back(RACE_ORC);
        availableRaces[CLASS_SHAMAN].push_back(RACE_TROLL);
        availableRaces[CLASS_SHAMAN].push_back(RACE_TAUREN);
    }

    // mage
    // alliance
    {
        availableRaces[CLASS_MAGE].push_back(RACE_HUMAN);
        availableRaces[CLASS_MAGE].push_back(RACE_DWARF);
        availableRaces[CLASS_MAGE].push_back(RACE_NIGHTELF);
        availableRaces[CLASS_MAGE].push_back(RACE_GNOME);
    }
    // horde
    {
        availableRaces[CLASS_MAGE].push_back(RACE_ORC);
        availableRaces[CLASS_MAGE].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_MAGE].push_back(RACE_TROLL);
    }

    // warlock
    // alliance
    {
        availableRaces[CLASS_WARLOCK].push_back(RACE_HUMAN);
        availableRaces[CLASS_WARLOCK].push_back(RACE_DWARF);
        availableRaces[CLASS_WARLOCK].push_back(RACE_GNOME);
    }
    // horde
    {
        availableRaces[CLASS_WARLOCK].push_back(RACE_ORC);
        availableRaces[CLASS_WARLOCK].push_back(RACE_UNDEAD_PLAYER);
        availableRaces[CLASS_WARLOCK].push_back(RACE_TROLL);
    }

    // druid
    // alliance
    {
        availableRaces[CLASS_DRUID].push_back(RACE_NIGHTELF);
    }
    // horde
    {
        availableRaces[CLASS_DRUID].push_back(RACE_TROLL);
        availableRaces[CLASS_DRUID].push_back(RACE_TAUREN);
    }
    if (expansion >= EXPANSION_THE_BURNING_CRUSADE)
    {
        // Draenei
        availableRaces[CLASS_WARRIOR].push_back(RACE_DRAENEI);
        availableRaces[CLASS_PALADIN].push_back(RACE_DRAENEI);
        availableRaces[CLASS_MAGE].push_back(RACE_DRAENEI);
        availableRaces[CLASS_SHAMAN].push_back(RACE_DRAENEI);
        availableRaces[CLASS_HUNTER].push_back(RACE_DRAENEI);
        availableRaces[CLASS_PRIEST].push_back(RACE_DRAENEI);

        // Blood
        availableRaces[CLASS_WARRIOR].push_back(RACE_BLOODELF);
        availableRaces[CLASS_PALADIN].push_back(RACE_BLOODELF);
        availableRaces[CLASS_HUNTER].push_back(RACE_BLOODELF);
        availableRaces[CLASS_ROGUE].push_back(RACE_BLOODELF);
        availableRaces[CLASS_PRIEST].push_back(RACE_BLOODELF);
        availableRaces[CLASS_MAGE].push_back(RACE_BLOODELF);
        availableRaces[CLASS_WARLOCK].push_back(RACE_BLOODELF);
    }

    if (expansion >= EXPANSION_CATACLYSM)
    {
        // Worgen
        availableRaces[CLASS_WARRIOR].push_back(RACE_WORGEN);
        availableRaces[CLASS_HUNTER].push_back(RACE_WORGEN);
        availableRaces[CLASS_ROGUE].push_back(RACE_WORGEN);
        availableRaces[CLASS_MAGE].push_back(RACE_WORGEN);
        availableRaces[CLASS_WARLOCK].push_back(RACE_WORGEN);
        availableRaces[CLASS_PRIEST].push_back(RACE_WORGEN);
        availableRaces[CLASS_DRUID].push_back(RACE_WORGEN);

        // Goblin
        availableRaces[CLASS_WARRIOR].push_back(RACE_GOBLIN);
        availableRaces[CLASS_HUNTER].push_back(RACE_GOBLIN);
        availableRaces[CLASS_ROGUE].push_back(RACE_GOBLIN);
        availableRaces[CLASS_PRIEST].push_back(RACE_GOBLIN);
        availableRaces[CLASS_SHAMAN].push_back(RACE_GOBLIN);
        availableRaces[CLASS_MAGE].push_back(RACE_GOBLIN);
        availableRaces[CLASS_WARLOCK].push_back(RACE_GOBLIN);
    }

    if (expansion >= EXPANSION_WRATH_OF_THE_LICH_KING)
    {
        // deathnight
        // alliance
        {
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_HUMAN);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_DWARF);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_DWARF);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_GNOME);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_DRAENEI);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_BLOODELF);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_WORGEN);
        }
        // horde
        {
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_ORC);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_UNDEAD_PLAYER);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_TROLL);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_TAUREN);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_BLOODELF);
            availableRaces[CLASS_DEATH_KNIGHT].push_back(RACE_GOBLIN);
        }
    }
    if (expansion >= EXPANSION_MISTS_OF_PANDARIA)
    {
        /*availableRaces[CLASS_WARRIOR].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_WARRIOR].push_back(RACE_PANDAREN_HORDE);

        availableRaces[CLASS_HUNTER].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_HUNTER].push_back(RACE_PANDAREN_HORDE);

        availableRaces[CLASS_ROGUE].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_ROGUE].push_back(RACE_PANDAREN_HORDE);

        availableRaces[CLASS_PRIEST].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_PRIEST].push_back(RACE_PANDAREN_HORDE);

        availableRaces[CLASS_MAGE].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_MAGE].push_back(RACE_PANDAREN_HORDE);

        availableRaces[CLASS_SHAMAN].push_back(RACE_PANDAREN_ALLIANCE);
        availableRaces[CLASS_SHAMAN].push_back(RACE_PANDAREN_HORDE);*/

        availableRaces[CLASS_WARRIOR].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_HUNTER].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_ROGUE].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_PRIEST].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_MAGE].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_SHAMAN].push_back(RACE_PANDAREN_NEUTRAL);
        availableRaces[CLASS_MONK].push_back(RACE_PANDAREN_NEUTRAL);
        // monk
        // alliance
        {
            availableRaces[CLASS_MONK].push_back(RACE_HUMAN);
            availableRaces[CLASS_MONK].push_back(RACE_DWARF);
            availableRaces[CLASS_MONK].push_back(RACE_NIGHTELF);
            availableRaces[CLASS_MONK].push_back(RACE_GNOME);
            availableRaces[CLASS_MONK].push_back(RACE_DRAENEI);
            availableRaces[CLASS_MONK].push_back(RACE_BLOODELF);
            //availableRaces[CLASS_MONK].push_back(RACE_PANDAREN_ALLIANCE);
        }
        // horde
        {
            availableRaces[CLASS_MONK].push_back(RACE_ORC);
            availableRaces[CLASS_MONK].push_back(RACE_UNDEAD_PLAYER);
            availableRaces[CLASS_MONK].push_back(RACE_TROLL);
            availableRaces[CLASS_MONK].push_back(RACE_TAUREN);
            availableRaces[CLASS_MONK].push_back(RACE_BLOODELF);
            //availableRaces[CLASS_MONK].push_back(RACE_PANDAREN_HORDE);
        }
    }
}

constexpr uint32 SPELL_DEATH_GATE = 50977;

uint32 RandomPlayerbotFactory::CreateRandomBot(WorldSession* session, Classes cls, std::unordered_map<NameRaceAndGender, std::vector<std::string>>& nameCache,
    int8 faction)
{
    SF_LOG_DEBUG("playerbots", "Creating new random bot for class %s", ClassToString(cls).c_str());

    uint8 gender = rand() % 2 ? GENDER_MALE : GENDER_FEMALE;
    bool alliance = faction < 0 ? (rand() % 2 ? true : false) : faction != 0;
    std::vector<uint8> raceOptions;
    for (const auto& race : availableRaces[cls])
    {
        if (alliance == IsAlliance(race))
        {
            raceOptions.push_back(race);
        }
    }

    if (raceOptions.empty())
    {
        SF_LOG_ERROR("playerbots", "No races available for class: %s", ClassToString(cls).c_str());
        return 0;
    }

    uint8 race = raceOptions[std::rand() % raceOptions.size()];
    // AC RandomPlayerbotFactory.cpp:95-121: names keyed by race+gender (#627)
    const auto raceAndGender = CombineRaceAndGender(race, gender);

    std::string name;
    if (!nameCache.empty())
    {
        if (nameCache[raceAndGender].empty())
        {
            SF_LOG_ERROR("playerbots", "No names found for the specified race: %u and gender: %u", race, gender);
            return 0;
        }

        uint32 i = urand(0, nameCache[raceAndGender].size() - 1);
        name = nameCache[raceAndGender][i];
        swap(nameCache[raceAndGender][i], nameCache[raceAndGender].back());
        nameCache[raceAndGender].pop_back();
    }
    else
    {
        name = CreateRandomBotName(raceAndGender);
    }

    if (name.empty())
    {
        SF_LOG_ERROR("playerbots", "Failed to get a valid random bot name");
        return 0;
    }

    std::vector<uint8> skinColors, facialHairTypes;
    std::vector<std::pair<uint8, uint8>> faces, hairs;

    // PORT: SkyFire does not load CharSections.dbc; the module reads it itself (compat/BotCharSections.h). Only options
    // a player can pick are used (SECTION_FLAG_PLAYER), Death Knight ones only for Death Knights, as the client offers.
    DBCStorage<BotCharSectionsEntry> const& sections = BotCharSectionStore();
    for (uint32 index = 0; index < sections.GetNumRows(); ++index)
    {
        BotCharSectionsEntry const* charSection = sections.LookupEntry(index);

        if (!charSection) continue;
        if (charSection->RaceID != race || charSection->SexID != gender) continue;
        if (!(charSection->Flags & SECTION_FLAG_PLAYER)) continue;
        if ((charSection->Flags & SECTION_FLAG_DEATH_KNIGHT) && cls != CLASS_DEATH_KNIGHT) continue;

        switch (charSection->BaseSection)
        {
            case SECTION_TYPE_SKIN:
                skinColors.push_back(charSection->ColorIndex);
                break;
            case SECTION_TYPE_FACE:
                faces.push_back(std::pair<uint8, uint8>(charSection->VariationIndex, charSection->ColorIndex));
                break;
            case SECTION_TYPE_FACIAL_HAIR:
                facialHairTypes.push_back(charSection->VariationIndex);
                break;
            case SECTION_TYPE_HAIR:
                hairs.push_back(std::pair<uint8, uint8>(charSection->VariationIndex, charSection->ColorIndex));
                break;
        }
    }

    // Without appearance data every option is 0 (the default look) instead of a division by zero.
    uint8 skinColor = skinColors.empty() ? 0 : skinColors[std::rand() % skinColors.size()];
    std::pair<uint8, uint8> face = faces.empty() ? std::pair<uint8, uint8>(0, 0) : faces[std::rand() % faces.size()];
    std::pair<uint8, uint8> hair = hairs.empty() ? std::pair<uint8, uint8>(0, 0) : hairs[std::rand() % hairs.size()];

    bool excludeCheck = (race == RACE_TAUREN) || (race == RACE_DRAENEI) ||
        (gender == GENDER_FEMALE && race != RACE_NIGHTELF && race != RACE_UNDEAD_PLAYER);
    uint8 facialHair = excludeCheck || facialHairTypes.empty() ? 0 : facialHairTypes[std::rand() % facialHairTypes.size()];

    // PORT: SkyFire builds and saves a new character only through WorldSession::CreateBotCharacter (CharacterCreateInfo
    // is the session's own), which does what LoA's code did here plus the core's first-login setup and starter pet.
    uint32 const lowGuid = session->CreateBotCharacter(name, race, cls, gender, face.second, face.first, hair.first,
        hair.second, facialHair, 0, 0);
    if (!lowGuid)
    {
        SF_LOG_ERROR("playerbots", "Unable to create random bot for account %u - name: \"%s\"; race: %s; class: %s",
            accountId, name.c_str(), RaceToString((Races)race).c_str(), ClassToString(cls).c_str());
        return 0;
    }

    // LoA teaches Death Knights Death Gate here: without it SkyFire keeps them in Acherus (Player::TeleportTo).
    if (cls == CLASS_DEATH_KNIGHT)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_SPELL);
        stmt->setUInt32(0, lowGuid);
        stmt->setUInt32(1, SPELL_DEATH_GATE);
        stmt->setBool(2, true);
        stmt->setBool(3, false);
        CharacterDatabase.Execute(stmt);
    }
    SF_LOG_DEBUG("playerbots", "Random bot created for account %u - name: \"%s\"; race: %s; class: %s", accountId,
        name.c_str(), RaceToString((Races)race).c_str(), ClassToString(cls).c_str());

    return lowGuid;
}

// AC Bot/Factory/RandomPlayerbotFactory.cpp:184-314 (#627): every candidate (pool, conlang, true random) is
// also checked against characters.name (CHAR_SEL_CHECK_NAME) before it is returned.
std::string const RandomPlayerbotFactory::CreateRandomBotName(NameRaceAndGender raceAndGender)
{
    std::string botName = "";
    int tries = 3;
    while (--tries)
    {
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT n.name "
            "FROM playerbots_names n "
            "LEFT OUTER JOIN characters c ON c.name = n.name "
            "WHERE c.guid IS NULL and n.gender = '%u' "
            "ORDER BY RAND() LIMIT 1",
            static_cast<uint32>(raceAndGender));
        if (!result)
        {
            break;
        }

        Field* fields = result->Fetch();
        botName = fields[0].GetString();
        if (ObjectMgr::CheckPlayerName(botName) == ResponseCodes::CHAR_NAME_SUCCESS)  // Checks for reservation & profanity, too
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
            stmt->setString(0, botName);

            if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
                continue;

            return botName;
        }
    }

    // CONLANG NAME GENERATION
    SF_LOG_ERROR("playerbots", "No more names left for random bots. Attempting conlang name generation.");
    const std::string groupCategory = "SCVKRU";
    const std::string groupFormStart[2][4] = { {"SV", "SV", "VK", "RV"}, {"V", "SU", "VS", "RV"} };
    const std::string groupFormMid[2][6] = { {"CV", "CVC", "CVC", "CVK", "VC", "VK"},
                                            {"CV", "CVC", "CVK", "KVC", "VC", "KV"} };
    const std::string groupFormEnd[2][4] = { {"CV", "VC", "VK", "CV"}, {"RU", "UR", "VR", "V"} };
    const std::string groupLetter[2][6] = {
        // S           C                            V               K           R         U
        {"dtspkThfS", "bcCdfghjkmnNqqrrlsStTvwxyz", "aaeeiouA", "ppttkkbdg", "lmmnrr", "AEO"},
        {"dtskThfS", "bcCdfghjkmmnNqrrlssStTvwyz", "aaaeeiiuAAEIO", "ppttkbbdg", "lmmnrrr", "AEOy"} };
    const std::string replaceRule[2][17] = {
        {"ST", "ka", "ko", "ku", "kr", "S", "T", "C", "N", "jj", "AA", "AI", "A", "E", "O", "I", "aa"},
        {"sth", "ca", "co", "cu", "cr", "sh", "th", "ch", "ng", "dg", "A", "ayu", "ai", "ei", "ou", "iu", "ae"} };

    const auto gender = static_cast<uint8>(raceAndGender) % 2;

    tries = 10;
    while (--tries)
    {
        botName.clear();
        // Build name from groupForms
        // Pick random start group
        botName = groupFormStart[gender][rand() % 4];
        // Pick up to 2 and then up to 1 additional middle group
        for (int i = 0; i < rand() % 3 + rand() % 2; i++)
        {
            botName += groupFormMid[gender][rand() % 6];
        }
        // Pick up to 1 end group
        botName += rand() % 2 ? groupFormEnd[gender][rand() % 4] : "";
        // If name is single letter add random end group
        botName += (botName.size() < 2) ? groupFormEnd[gender][rand() % 4] : "";

        // Replace Catagory value with random Letter from that Catagory's Letter string for a given bot gender
        for (int i = 0; i < botName.size(); i++)
        {
            botName[i] = groupLetter[gender][groupCategory.find(botName[i])]
                [rand() % groupLetter[gender][groupCategory.find(botName[i])].size()];
        }

        // Itterate over replace rules
        for (int i = 0; i < 17; i++)
        {
            int j = botName.find(replaceRule[0][i]);
            while (j > -1)
            {
                botName.replace(j, replaceRule[0][i].size(), replaceRule[1][i]);
                j = botName.find(replaceRule[0][i]);
            }
        }

        // Capitalize first letter
        botName[0] -= 32;

        if (ObjectMgr::CheckPlayerName(botName) != ResponseCodes::CHAR_NAME_SUCCESS) // Checks for reservation & profanity, too
        {
            botName.clear();
            continue;
        }
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
        stmt->setString(0, botName);

        if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
        {
            botName.clear();
            continue;
        }
        return botName;
    }

    // TRUE RANDOM NAME GENERATION
    SF_LOG_ERROR("playerbots", "Conlang name generation failed. True random name fallback.");
    tries = 10;
    while (--tries)
    {
        for (uint8 i = 0; i < 10; i++)
        {
            botName += (i == 0 ? 'A' : 'a') + rand() % 26;
        }
        if (ObjectMgr::CheckPlayerName(botName) != ResponseCodes::CHAR_NAME_SUCCESS)  // Checks for reservation & profanity, too
        {
            botName.clear();
            continue;
        }
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
        stmt->setString(0, botName);

        if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
        {
            botName.clear();
            continue;
        }
        return botName;
    }
    SF_LOG_ERROR("playerbots", "Random name generation failed.");
    botName.clear();
    return botName;
}

// Waits until the rows of these characters exist (SaveToDB queues them), up to a minute.
static void WaitForCharacterRows(std::vector<uint32> const& guids)
{
    if (guids.empty())
        return;

    std::ostringstream list;
    for (size_t i = 0; i < guids.size(); ++i)
        list << (i ? "," : "") << guids[i];  // numbers only, so the IN list cannot carry SQL
    std::string const query = "SELECT COUNT(*) FROM characters WHERE guid IN (" + list.str() + ")";

    constexpr uint32 maxWaitSeconds = 60;
    for (uint32 waited = 0; waited < maxWaitSeconds; ++waited)
    {
        QueryResult result = CharacterDatabase.Query(query.c_str());
        if (result && (*result)[0].GetUInt64() >= guids.size())
            return;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    SF_LOG_ERROR("playerbots", "Some new random bot characters were not saved within %u s", maxWaitSeconds);
}

void RandomPlayerbotFactory::CreateRandomBots()
{
    SF_LOG_INFO("playerbots", "Creating random bot accounts...");

    std::unordered_map<NameRaceAndGender, std::vector<std::string>> nameCache;
    // Hub ADR 0025: the higher batch's accounts follow the standard ones, under their own prefix.
    struct BotAccountSpec
    {
        std::string name;
        uint32 number;
        bool higherBatch;
    };
    std::vector<BotAccountSpec> accountSpecs;
    for (uint32 accountNumber = 0; accountNumber < sPlayerbotAIConfig->randomBotAccountCount; ++accountNumber)
        accountSpecs.push_back({ sPlayerbotAIConfig->randomBotAccountPrefix + std::to_string(accountNumber), accountNumber, false });
    for (uint32 accountNumber = 0; accountNumber < sPlayerbotAIConfig->higherBatchAccountCount; ++accountNumber)
        accountSpecs.push_back({ sPlayerbotAIConfig->higherBatchAccountPrefix + std::to_string(accountNumber), accountNumber, true });
    // A reload runs this again; the higher batch list is rebuilt below, so it must not grow twice.
    sPlayerbotAIConfig->higherBatchAccounts.clear();

    std::vector<std::future<void>> account_creations;
    int account_creation = 0;
    uint32 timer = getMSTime();

    for (BotAccountSpec const& spec : accountSpecs)
    {
        std::string const& accountName = spec.name;

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_ACCOUNT_ID_BY_USERNAME);
        stmt->setString(0, accountName);
        PreparedQueryResult result = LoginDatabase.Query(stmt);
        if (result)
        {
            continue;
        }
        account_creation++;
        std::string password = "";
        if (sPlayerbotAIConfig->randomBotRandomPassword)
        {
            // The server's seeded generator, not std::rand(), which is seeded from the start time.
            for (int i = 0; i < 20; i++)
                password += static_cast<char>(urand('!', 'z'));
        }
        else
            password = accountName;

        // PORT: SkyFire requires a unique email shape for every account (LoA passes ""). The reserved .invalid domain
        // never routes anywhere; the random password above still guards the login.
        AccountOpResult res = sAccountMgr->CreateAccount(accountName, password, accountName + "@bots.invalid");
        if (AccountOpResult::AOR_OK == res)
            SF_LOG_INFO("playerbots", "Account %s created for random bots", accountName.c_str());
        else
            SF_LOG_ERROR("server.loading", "Cannot create random bot account %s (result %u)", accountName.c_str(), uint32(res));
    }

    // PORT: AC waits here for the login database queue (#627). SkyFire's AccountMgr::CreateAccount writes the account
    // row synchronously (DirectExecute), so the lookups below already see the new accounts.
    if (account_creation)
        SF_LOG_INFO("playerbots", ">> %u Accounts created in %u ms", account_creation, GetMSTimeDiffToNow(timer));

    SF_LOG_INFO("playerbots", "Creating random bot characters...");
    uint32 totalRandomBotChars = 0;
    uint32 totalCharCount = sPlayerbotAIConfig->randomBotAccountCount * 10;
    std::vector<std::pair<Player*, uint32>> playerBots;
    std::vector<WorldSession*> sessionBots;
    std::vector<uint32> createdGuids;
    int bot_creation = 0;
    timer = getMSTime();
    bool nameCached = false;
    for (size_t specIndex = 0; specIndex < accountSpecs.size(); ++specIndex)
    {
        BotAccountSpec const& spec = accountSpecs[specIndex];
        std::string accountName = spec.name;
        std::transform(accountName.begin(), accountName.end(), accountName.begin(), ::toupper);

        LoginDatabasePreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_ACCOUNT_ID_BY_USERNAME);
        stmt->setString(0, accountName);
        PreparedQueryResult result = LoginDatabase.Query(stmt);
        if (!result)
            continue;

        Field* fields = result->Fetch();
        uint32 accountId = fields[0].GetUInt32();

        sPlayerbotAIConfig->randomBotAccounts.push_back(accountId);
        if (spec.higherBatch)
            sPlayerbotAIConfig->higherBatchAccounts.push_back(accountId);

        uint32 character_count = AccountMgr::GetCharactersCount(accountId);
        if (character_count >= 10)
        {
            continue;
        }

        // AC RandomPlayerbotFactory.cpp:688-713 (#627): build the name cache only when an account needs
        // characters, and leave out every name already taken in characters (CHAR_SEL_CHECK_NAME). The fork
        // cached all of playerbots_names, so a later boot that topped up accounts reused taken names.
        if (!nameCached)
        {
            nameCached = true;
            SF_LOG_INFO("playerbots", "Creating cache for names per gender and race...");
            QueryResult result = CharacterDatabase.Query("SELECT name, gender FROM playerbots_names");
            if (!result)
            {
                SF_LOG_ERROR("playerbots", "No more unused names left");
                return;
            }
            do
            {
                Field* fields = result->Fetch();
                std::string name = fields[0].GetString();
                NameRaceAndGender raceAndGender = static_cast<NameRaceAndGender>(fields[1].GetUInt8());
                if (sObjectMgr->CheckPlayerName(name) == ResponseCodes::CHAR_NAME_SUCCESS)
                {
                    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHECK_NAME);
                    stmt->setString(0, name);

                    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
                        continue;

                    nameCache[raceAndGender].push_back(name);
                }

            } while (result->NextRow());
        }

        SF_LOG_INFO("playerbots", "Creating random bot characters for account: [%u/%u]", uint32(specIndex + 1), uint32(accountSpecs.size()));
        RandomPlayerbotFactory factory(accountId);

        // A socketless bot session, as PlayerbotMgr::AddPlayerBot builds it, only to own the new characters.
        WorldSession* session = new WorldSession(accountId, nullptr, AccountTypes::SEC_PLAYER, uint8(Expansions::EXPANSION_MISTS_OF_PANDARIA),
            0, LOCALE_enUS, 0, false, false, false, false, std::string(), SessionKey{});
        session->SetBot(true);
        // BT-136: the same realm a real client names at login (see PlayerbotMgr::AddPlayerBot).
        session->SetVirtualRealmID(realmID);
        sessionBots.push_back(session);

        for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES - character_count; ++cls)
        {
            // skip nonexistent classes
            if (!((1 << (cls - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(cls))
                continue;

            if (bool const isClassDeathKnight = cls == CLASS_DEATH_KNIGHT;
                isClassDeathKnight && sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION) < EXPANSION_WRATH_OF_THE_LICH_KING)
            {
                continue;
            }
            if (bool const isClassMonk = cls == CLASS_MONK;
                isClassMonk&& sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION) < EXPANSION_MISTS_OF_PANDARIA)
            {
                continue;
            }

            if (cls <= CLASS_DRUID)
            {
                // CreateBotCharacter saves the character and adds its name to the character cache.
                // Higher batch: alternate the side by account number and class, so every class has both sides and the
                // batch stays about half and half.
                int8 const faction = spec.higherBatch ? int8((spec.number + cls) % 2 == 0) : int8(-1);
                if (uint32 const lowGuid = factory.CreateRandomBot(session, (Classes)cls, nameCache, faction))
                {
                    // BT-148, ADR 0016/0017: mark new standard characters for a one-time purse.
                    if (!spec.higherBatch)
                        sRandomPlayerbotMgr->MarkNewStandardBot(lowGuid);
                    createdGuids.push_back(lowGuid);
                    bot_creation++;
                }
                else
                {
                    SF_LOG_ERROR("playerbots", "Fail to create character for account %u", accountId);
                }
            }
        }
    }

    if (bot_creation)
    {
        // AC RandomPlayerbotFactory.cpp:757-764 (#627): wait until the characters are in the database, not a fixed
        // sleep, or they fail to log in. PORT: SkyFire's database pool has no queue size, so this waits until every new
        // character's row exists (at most a minute; a character still missing then logs in on a later boot).
        SF_LOG_INFO("playerbots", "Waiting for %u characters loading into database...", bot_creation);
        WaitForCharacterRows(createdGuids);
        SF_LOG_INFO("playerbots", ">> %u Characters loaded into database in %u ms", bot_creation, GetMSTimeDiffToNow(timer));
    }

    for (WorldSession* session : sessionBots)
        delete session;

    for (uint32 accountId : sPlayerbotAIConfig->randomBotAccounts)
    {
        totalRandomBotChars += AccountMgr::GetCharactersCount(accountId);
    }

    SF_LOG_INFO("server.loading", ">> %u random bot accounts with %u characters available", sPlayerbotAIConfig->randomBotAccounts.size(), totalRandomBotChars);
}

// #611 item 4: AC Bot/Factory/RandomPlayerbotFactory.cpp:774-803 (Query("{}") -> PQuery("%u"); Get<T>() -> GetUInt32()/GetString()).
std::string const RandomPlayerbotFactory::CreateRandomGuildName()
{
    std::string guildName = "";

    QueryResult result = CharacterDatabase.Query("SELECT MAX(name_id) FROM playerbots_guild_names");
    if (!result)
    {
        SF_LOG_ERROR("playerbots", "No more names left for random guilds");
        return guildName;
    }

    Field* fields = result->Fetch();
    uint32 maxId = fields[0].GetUInt32();

    uint32 id = urand(0, maxId);
    result = CharacterDatabase.PQuery(
        "SELECT n.name FROM playerbots_guild_names n "
        "LEFT OUTER JOIN guild e ON e.name = n.name WHERE e.guildid IS NULL AND n.name_id >= %u LIMIT 1",
        id);
    if (!result)
    {
        SF_LOG_ERROR("playerbots", "No more names left for random guilds");
        return guildName;
    }

    fields = result->Fetch();
    guildName = fields[0].GetString();

    return guildName;
}
