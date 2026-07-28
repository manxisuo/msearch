#!/usr/bin/env bash
# Build a simple .deb for the current architecture (run on target or after cross-build).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
ARCH="${ARCH:-$(dpkg --print-architecture 2>/dev/null || echo amd64)}"
VERSION="${VERSION:-0.4.0}"

# Stage on a Linux-native filesystem. Staging under /mnt/* (NTFS/DrvFs) often
# yields mode 777 and makes dpkg-deb reject DEBIAN/.
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/msearch-deb.XXXXXX")"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

if [[ ! -f "$BUILD/msearch" ]]; then
  echo "error: $BUILD/msearch not found. Build first, e.g.:"
  echo "  cmake -S \"$ROOT\" -B \"$BUILD\" -DCMAKE_BUILD_TYPE=Release && cmake --build \"$BUILD\" -j"
  exit 1
fi

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

# dpkg-deb requires DEBIAN/ permissions in [0755, 0775]
chmod 0755 "$STAGE" "$STAGE/DEBIAN"
chmod 0644 "$STAGE/DEBIAN/control"
find "$STAGE/usr" -type d -exec chmod 0755 {} +
find "$STAGE/usr" -type f -exec chmod 0644 {} +
chmod 0755 "$STAGE/usr/bin/msearch"

OUT_DIR="$ROOT/packaging/deb"
mkdir -p "$OUT_DIR"
OUT="$OUT_DIR/msearch_${VERSION}_${ARCH}.deb"
rm -f "$OUT"
dpkg-deb --build "$STAGE" "$OUT"
echo "Created: $OUT"
