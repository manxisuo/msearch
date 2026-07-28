#!/usr/bin/env bash
# Build a simple .deb for the current architecture (run on target or after cross-build).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo amd64)}"
VERSION="${VERSION:-0.4.0}"
STAGE="$ROOT/packaging/deb/stage"

if [[ ! -x "$BUILD/msearch" ]]; then
  echo "error: $BUILD/msearch not found. Build first, e.g.:"
  echo "  cmake -S \"$ROOT\" -B \"$BUILD\" -DCMAKE_BUILD_TYPE=Release && cmake --build \"$BUILD\" -j"
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
         "$STAGE/usr/bin" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/icons/hicolor/scalable/apps" \
         "$STAGE/usr/share/doc/msearch"

install -m 755 "$BUILD/msearch" "$STAGE/usr/bin/msearch"
install -m 644 "$ROOT/packaging/msearch.desktop" "$STAGE/usr/share/applications/msearch.desktop"
install -m 644 "$ROOT/packaging/msearch.svg" "$STAGE/usr/share/icons/hicolor/scalable/apps/msearch.svg"
install -m 644 "$ROOT/README.md" "$STAGE/usr/share/doc/msearch/README.md"
install -m 644 "$ROOT/ROADMAP.md" "$STAGE/usr/share/doc/msearch/ROADMAP.md"

sed -e "s/^Architecture:.*/Architecture: ${ARCH}/" \
    -e "s/^Version:.*/Version: ${VERSION}/" \
    "$ROOT/packaging/deb/control.in" > "$STAGE/DEBIAN/control"

SIZE_KB="$(du -sk "$STAGE" | awk '{print $1}')"
echo "Installed-Size: ${SIZE_KB}" >> "$STAGE/DEBIAN/control"

OUT="$ROOT/packaging/deb/msearch_${VERSION}_${ARCH}.deb"
dpkg-deb --build "$STAGE" "$OUT"
echo "Created: $OUT"
