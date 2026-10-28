#!/usr/bin/env bash
# Builds the browser simulator into sim/out/ (goli.js + goli.wasm + index.html).
# Needs Emscripten (emcc) and LVGL from a firmware build: pio run -e snapshot -d firmware
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
fw="$here/../firmware"
lv="$(ls -d "$fw"/.pio/libdeps/*/lvgl 2>/dev/null | head -1)"
[ -n "$lv" ] || { echo "LVGL not found: run 'pio run -e snapshot -d firmware' first" >&2; exit 1; }
out="$here/out"
obj="$here/.obj"
mkdir -p "$out" "$obj"
lvflags="-O2 -DLV_CONF_INCLUDE_SIMPLE -I $fw/include -I $lv"
flags=(-O2 -DLV_CONF_INCLUDE_SIMPLE "-DGOLI_VERSION=\"${GOLI_VERSION:-dev}\"" -I "$fw/include" -I "$here/shim" -I "$fw/src" -I "$lv")

# LVGL (C), compiled once and cached
find "$lv/src" -name '*.c' | while read -r f; do
  o="$obj/lv_$(echo "${f#$lv/src/}" | tr '/' '_' | sed 's/\.c$/.o/')"
  [ -f "$o" ] || echo "$f $o"
done | xargs -P 8 -n 2 sh -c 'emcc '"$lvflags"' -std=c11 -c "$0" -o "$1"'

# Goli: the same sources as the firmware, plus the simulator
srcs=(bot.cpp face.cpp screens.cpp demo.cpp motion.cpp synth.cpp)
objs=()
for f in "${srcs[@]}"; do
  emcc "${flags[@]}" -std=c++17 -c "$fw/src/$f" -o "$obj/goli_${f%.cpp}.o"
  objs+=("$obj/goli_${f%.cpp}.o")
done
emcc "${flags[@]}" -std=c11 -c "$fw/src/font_clock.c" -o "$obj/goli_font_clock.o"
emcc "${flags[@]}" -std=c++17 -c "$here/sim.cpp" -o "$obj/sim.o"

em++ -O2 "$obj"/lv_*.o "${objs[@]}" "$obj/goli_font_clock.o" "$obj/sim.o" -o "$out/goli.js" \
  -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall,HEAPU8,HEAP16 \
  -sEXPORTED_FUNCTIONS=_main,_sim_fb,_sim_take_dirty,_sim_pointer,_sim_tilt,_sim_flat,_sim_shake,_sim_bump,_sim_clap,_sim_noise,_sim_alert,_sim_power,_sim_hint
cp "$here/index.html" "$out/index.html"
echo "simulator in $out (serve it: python3 -m http.server -d sim/out)"
