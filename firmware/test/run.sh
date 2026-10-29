#!/usr/bin/env bash
# Builds and runs the host tests (no board needed): ./run.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/goli-tests"
c++ -std=c++17 -O1 -Wall -Wno-format-security -I "$here/shim" -I "$here/../src" \
  "$here/test_main.cpp" "$here/../src/ics.cpp" "$here/../src/text.cpp" -o "$out"
"$out"
