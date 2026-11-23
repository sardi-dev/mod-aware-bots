#include "ClassSkillLines.h"
#include "TestHarness.h"

// Bots learned their pets' skill lines as class skills and carried the pets' passives on themselves (live 2026-11-20: a
// warlock pulsing the Infernal's Immolation every 2 s, death knights with the ghoul's passives).

TEST(class_skill_lines_keep_the_class_lines)
{
    std::vector<std::pair<uint32_t, uint32_t>> const rows = {{593, 256}, {354, 256}, {924, 64}};
    std::set<uint32_t> const lines = ClassSkillLines::For(rows, 256, {});
    CHECK(lines.count(593) == 1);
    CHECK(lines.count(354) == 1);
    CHECK(lines.count(924) == 0);
}

TEST(class_skill_lines_drop_the_pet_family_lines)
{
    std::vector<std::pair<uint32_t, uint32_t>> const rows = {{593, 256}, {189, 256}, {188, 256}, {782, 32}};
    std::set<uint32_t> const pets = {188, 189, 782};
    std::set<uint32_t> const warlock = ClassSkillLines::For(rows, 256, pets);
    CHECK(warlock.count(593) == 1);
    CHECK(warlock.count(189) == 0);
    CHECK(warlock.count(188) == 0);
    CHECK(ClassSkillLines::For(rows, 32, pets).empty());
}
