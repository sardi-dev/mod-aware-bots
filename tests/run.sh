#!/usr/bin/env bash
# Builds and runs the pure-logic unit tests in a few seconds: no game core, no test framework.
# Usage: tests/run.sh   (uses g++, or the compiler in CXX)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
# The sources outside tests/ are the ones free of game types (src/PvP).
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I"$here" -I"$here/../src/Utils" -I"$here/../src/Arena" -I"$here/../src/PvP" \
    "$here"/test_*.cpp "$here/../src/PvP/PvpMatchState.cpp" -o "$out/aware_bots_tests"
"$out/aware_bots_tests"
