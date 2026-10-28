#!/usr/bin/env bash
# Assembles the GitHub Pages site (web installer, Wi-Fi update manifest, 3D viewer) into $1 (default: site/).
# Needs a firmware build first: pio run -e goli -d firmware
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/site}"
version="${GOLI_VERSION:-$(date -u +%Y.%m.%d)-$(git -C "$root" rev-parse --short HEAD)}"

rm -rf "$out" && mkdir -p "$out/firmware" "$out/img"
cp "$root/web/index.html" "$root/web/manifest.json" "$out/"
cp "$root/firmware/.pio/build/goli/goli-firmware.bin" "$out/firmware/"          # merged, for the installer
cp "$root/firmware/.pio/build/goli/firmware.bin" "$out/firmware/goli-app.bin"   # app only, for Wi-Fi updates
printf '{"version": "%s", "path": "firmware/goli-app.bin"}\n' "$version" > "$out/ota.json"
cp "$root/viewer.html" "$out/viewer.html"
# The browser simulator, when it was built (sim/build.sh)
if [ -f "$root/sim/out/goli.wasm" ]; then mkdir -p "$out/sim" && cp "$root"/sim/out/{index.html,goli.js,goli.wasm} "$out/sim/"; fi
cp "$root"/docs/{hero.jpg,faces.png,setup-screen.png,setup-page.jpg,selftest.png,pages.png,face-alert.jpg,face-critical.jpg,face-good.jpg} "$out/img/"
sed -i.bak "s/__VERSION__/$version/g" "$out/manifest.json" "$out/index.html" && rm "$out"/*.bak
echo "site in $out (firmware $version)"
