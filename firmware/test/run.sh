#!/usr/bin/env bash
# Builds and runs the host tests (no board needed; ArduinoJson comes from a firmware build): ./run.sh
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/goli-tests"
json="$(ls -d "$here"/../.pio/libdeps/*/ArduinoJson/src 2>/dev/null | head -1)"
[ -n "$json" ] || { echo "ArduinoJson not found: build the firmware once (pio run -e goli -d firmware)" >&2; exit 1; }
c++ -std=c++17 -O1 -Wall -Wno-format-security -I "$here/shim" -I "$here/../src" -I "$json" \
  "$here/test_main.cpp" "$here/../src/ics.cpp" "$here/../src/text.cpp" "$here/../src/rules.cpp" -o "$out"
"$out"
