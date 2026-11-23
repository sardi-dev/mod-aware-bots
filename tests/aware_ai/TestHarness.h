/*
 * This file is part of mod-aware-bots-548 (GPL-2.0-or-later, see LICENSE).
 */

#ifndef _PLAYERBOT_TESTHARNESS_H
#define _PLAYERBOT_TESTHARNESS_H

// A test registry and two checks: enough for the pure-logic tests, with no framework to install.

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace Tests
{
struct Case
{
    std::string name;
    std::function<void()> run;
};

inline std::vector<Case>& Registry()
{
    static std::vector<Case> cases;
    return cases;
}

inline int& Failures()
{
    static int failures = 0;
    return failures;
}

struct Registrar
{
    Registrar(char const* name, std::function<void()> run) { Registry().push_back({ name, std::move(run) }); }
};
}  // namespace Tests

#define TEST(name) \
    static void name(); \
    static Tests::Registrar registrar_##name(#name, name); \
    static void name()

#define CHECK(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++Tests::Failures(); \
        } \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance) \
    do \
    { \
        double const actualValue = (actual); \
        double const expectedValue = (expected); \
        if (std::fabs(actualValue - expectedValue) > (tolerance)) \
        { \
            std::printf("  FAILED %s:%d: %s is %g, expected %g\n", __FILE__, __LINE__, #actual, actualValue, expectedValue); \
            ++Tests::Failures(); \
        } \
    } while (false)

#endif
