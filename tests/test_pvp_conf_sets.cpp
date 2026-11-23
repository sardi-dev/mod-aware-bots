#include "PvpTalentSetPolicy.h"
#include "TestHarness.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <utility>

// The shipped PvP sets (conf/playerbots.conf.dist, "PREMADE PVP SETS", BT-320, BT-450): every specialization has a
// link of six tier characters and six distinct glyph item ids, and the picks the 2026-11-18 ladder check changed
// stay as they are until someone changes them on purpose (the ladder line above each set in the file says why; the
// 2v2 pooled check of BT-453 is pinned too).
// Whether an id exists on 5.4.8 needs the game data and is checked by hand (docs/class-sheets, PvP sections).

namespace
{
using SpecKey = std::pair<int, int>;  // class id, specialization index

std::map<SpecKey, std::string> ReadSets(std::string const& kind)
{
    std::filesystem::path const conf =
        std::filesystem::path(__FILE__).parent_path().parent_path() / "conf" / "playerbots.conf.dist";
    std::ifstream in(conf);
    std::map<SpecKey, std::string> sets;
    std::string const prefix = "AiPlayerbot.PremadeSpec" + kind + ".";
    std::string line;
    while (std::getline(in, line))
    {
        if (line.rfind(prefix, 0) != 0)
            continue;
        std::size_t const eq = line.find(" = ");
        if (eq == std::string::npos)
            continue;
        int cls = 0;
        int spec = 0;
        char dot = 0;
        std::istringstream key(line.substr(prefix.size(), eq - prefix.size()));
        key >> cls >> dot >> spec;
        sets[{ cls, spec }] = line.substr(eq + 3);
    }
    return sets;
}

int SpecsOf(int cls)
{
    return cls == 11 ? 4 : 3;  // druids have four specializations
}
}

TEST(pvp_conf_every_spec_has_a_six_tier_link)
{
    std::map<SpecKey, std::string> const links = ReadSets("LinkPvp");
    CHECK(links.size() == 34);
    for (int cls = 1; cls <= 11; ++cls)
        for (int spec = 0; spec < SpecsOf(cls); ++spec)
        {
            auto const it = links.find({ cls, spec });
            CHECK(it != links.end());
            if (it == links.end())
                continue;
            CHECK(it->second.size() == PvpTalentSetPolicy::TALENT_TIERS);
            for (std::uint32_t tier = 0; tier < PvpTalentSetPolicy::TALENT_TIERS && tier < it->second.size(); ++tier)
                CHECK(PvpTalentSetPolicy::WantedColumn(PvpTalentSetPolicy::ParseLink(it->second), tier) !=
                      PvpTalentSetPolicy::NO_CHOICE);
        }
}

TEST(pvp_conf_every_spec_has_six_distinct_glyphs)
{
    std::map<SpecKey, std::string> const glyphs = ReadSets("GlyphPvp");
    CHECK(glyphs.size() == 34);
    for (auto const& [key, list] : glyphs)
    {
        std::vector<long> ids;
        std::istringstream in(list);
        std::string item;
        while (std::getline(in, item, ','))
            ids.push_back(std::strtol(item.c_str(), nullptr, 10));
        CHECK(ids.size() == 6);
        for (long id : ids)
            CHECK(id > 0);
        std::sort(ids.begin(), ids.end());
        CHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
    }
}

TEST(pvp_conf_ladder_picks_of_2026_11_18_stay)
{
    std::map<SpecKey, std::string> const links = ReadSets("LinkPvp");
    std::map<SpecKey, std::string> const glyphs = ReadSets("GlyphPvp");
    // Talents: 3.2 survival Wyvern Sting, 7.0 elemental Windwalk Totem, 8.2 frost Flameglow, 9.0 affliction Unbound
    // Will, 10.1 mistweaver Leg Sweep.
    CHECK(links.at({ 3, 2 }) == "012000");
    CHECK(links.at({ 7, 0 }) == "020110");
    CHECK(links.at({ 8, 2 }) == "010212");
    CHECK(links.at({ 9, 0 }) == "020200");
    CHECK(links.at({ 10, 1 }) == "102201");
    // The pooled 3v3 and 2v2 check (BT-453): 11.0 balance Force of Nature and Mighty Bash.
    CHECK(links.at({ 11, 0 }) == "122220");
    // Glyphs: 1.0 arms Mortal Strike (43421), 9.0 affliction Siphon Life (42455) and Soulwell (43394), 11.3
    // restoration druid the Cheetah (89868).
    CHECK(glyphs.at({ 1, 0 }).find("43421") != std::string::npos);
    CHECK(glyphs.at({ 9, 0 }).find("42455") != std::string::npos);
    CHECK(glyphs.at({ 9, 0 }).find("43394") != std::string::npos);
    CHECK(glyphs.at({ 11, 3 }).find("89868") != std::string::npos);
    // BT-453: 11.0 balance Stars (44922) and the Cheetah (89868); 7.2 restoration shaman Hex (45777) and Deluge (45775).
    CHECK(glyphs.at({ 11, 0 }).find("44922") != std::string::npos);
    CHECK(glyphs.at({ 11, 0 }).find("89868") != std::string::npos);
    CHECK(glyphs.at({ 7, 2 }).find("45777") != std::string::npos);
    CHECK(glyphs.at({ 7, 2 }).find("45775") != std::string::npos);
    // BT-683 (closes BT-452): 2.0 holy paladin takes the Denounce glyph (43867, 3v3 70 percent) in place of Blessed Life (41110, 40 percent)
    // because a Holy rule now casts Denounce.
    CHECK(glyphs.at({ 2, 0 }).find("43867") != std::string::npos);
    CHECK(glyphs.at({ 2, 0 }).find("41110") == std::string::npos);
}
