/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#include "TestHarness.h"

int main()
{
    for (Tests::Case const& test : Tests::Registry())
    {
        int const before = Tests::Failures();
        test.run();
        std::printf("%s %s\n", Tests::Failures() == before ? "ok    " : "FAILED", test.name.c_str());
    }

    std::printf("%zu tests, %d failed checks\n", Tests::Registry().size(), Tests::Failures());
    return Tests::Failures() ? 1 : 0;
}
