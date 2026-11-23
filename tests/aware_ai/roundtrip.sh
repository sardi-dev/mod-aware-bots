#!/usr/bin/env bash
# Builds sidecar_roundtrip.cpp and runs it against a running sidecar (core-free, like run.sh).
# Usage: tests/aware_ai/roundtrip.sh [url] [expected mode]    e.g. roundtrip.sh http://127.0.0.1:8090 fake
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../src/AwareAi"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -pthread -I"$here/stubs" -I"$src" \
  "$here/sidecar_roundtrip.cpp" \
  "$src/AwareAiClient.cpp" "$src/AwareAiProtocol.cpp" "$src/BoostJsonImpl.cpp" \
  -o "$out/sidecar_roundtrip"
"$out/sidecar_roundtrip" "${1:-http://127.0.0.1:8090}" "${2:-fake}"
