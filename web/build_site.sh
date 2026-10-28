#!/usr/bin/env bash
# Assembles the GitHub Pages site (web installer + 3D viewer) into $1 (default: site/).
# Needs a firmware build first: pio run -e goli -d firmware
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/site}"
version="${GOLI_VERSION:-$(date -u +%Y.%m.%d)-$(git -C "$root" rev-parse --short HEAD)}"

rm -rf "$out" && mkdir -p "$out/firmware" "$out/img"
cp "$root/web/index.html" "$root/web/manifest.json" "$out/"
cp "$root/firmware/.pio/build/goli/goli-firmware.bin" "$out/firmware/"
cp "$root/viewer.html" "$out/viewer.html"
cp "$root"/docs/{hero.jpg,faces.png,setup-screen.png,setup-page.jpg} "$out/img/"
sed -i.bak "s/__VERSION__/$version/g" "$out/manifest.json" "$out/index.html" && rm "$out"/*.bak
echo "site in $out (firmware $version)"
