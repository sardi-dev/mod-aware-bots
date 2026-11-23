#!/usr/bin/env bash
# Builds and runs the core-free tests of the AI layer plumbing (JSON helpers, protocol bodies, JSONL writer, HTTP
# client against an in-process server). Needs g++ 13 or newer and the Boost headers (libboost-dev); no game core.
# Usage: tests/aware_ai/run.sh   (uses g++, or the compiler in CXX)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../src/AwareAi"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
# stubs/ stands in for SkyFire's Define.h, Duration.h and Log.h; TestHarness.h and test_main.cpp are the module's own, copied here.
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -pthread -I"$here" -I"$here/stubs" -I"$src" \
  "$here/test_aware_ai.cpp" "$here/test_main.cpp" \
  "$src/AwareAiClient.cpp" "$src/AwareAiProtocol.cpp" "$src/JsonlWriter.cpp" "$src/BoostJsonImpl.cpp" \
  -o "$out/aware_ai_tests"
"$out/aware_ai_tests"
