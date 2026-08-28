#!/usr/bin/env bash
# Builds MagnifyFactory in Release mode and packages it into a Linux
# AppImage (dist/MagnifyFactory-<version>-x86_64.AppImage).
#
# Requires: cmake, ninja, qt6-base-dev (or your distro's equivalent), and
# linuxdeploy + linuxdeploy-plugin-qt (downloaded automatically into
# ~/.cache/magnifyfactory-appimage-tools if not already present).
#
# Qt itself is bundled into the AppImage; ffmpeg/poppler-utils/qpdf/
# p7zip-full/libreoffice are NOT (same policy as the Windows installer) —
# users install those via their package manager. See the README.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(grep -oP '(?<=project\(MagnifyFactory VERSION )[0-9.]+' "$REPO_ROOT/CMakeLists.txt")"
TOOLS_DIR="${MAGNIFY_APPIMAGE_TOOLS_DIR:-$HOME/.cache/magnifyfactory-appimage-tools}"
BUILD_DIR="$REPO_ROOT/build-release"
APPDIR="$REPO_ROOT/AppDir"
DIST_DIR="$REPO_ROOT/dist"

echo "== Fetching linuxdeploy tools (cached in $TOOLS_DIR) =="
mkdir -p "$TOOLS_DIR"
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
[ -x "$LINUXDEPLOY" ] || wget -q -O "$LINUXDEPLOY" \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
[ -x "$LINUXDEPLOY_QT" ] || wget -q -O "$LINUXDEPLOY_QT" \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$LINUXDEPLOY" "$LINUXDEPLOY_QT"

echo "== Configuring Release build =="
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

echo "== Building MagnifyFactory (Release) =="
cmake --build "$BUILD_DIR" --target MagnifyFactory -j"$(nproc)"

echo "== Assembling AppDir =="
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$BUILD_DIR/MagnifyFactory" "$APPDIR/usr/bin/"
cp "$REPO_ROOT/resources/icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/magnifyfactory.png"
cat > "$APPDIR/usr/share/applications/magnifyfactory.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=MagnifyFactory
Comment=Convert video, audio, images, PDFs, documents, and archives
Exec=MagnifyFactory %f
Icon=magnifyfactory
Terminal=false
Categories=AudioVideo;
EOF

echo "== Running linuxdeploy (bundles Qt + libs) =="
export QML_SOURCES_PATHS=/dev/null
"$LINUXDEPLOY" --appimage-extract-and-run \
    --appdir "$APPDIR" \
    -e "$APPDIR/usr/bin/MagnifyFactory" \
    -d "$APPDIR/usr/share/applications/magnifyfactory.desktop" \
    -i "$APPDIR/usr/share/icons/hicolor/256x256/apps/magnifyfactory.png" \
    --plugin qt

# linuxdeploy's own AppRun is a plain symlink to the binary. WSLg (and some
# Wayland desktops) default Qt to the "wayland" platform plugin, which this
# AppImage doesn't bundle (fragile/uncommon to ship) — only "xcb" (X11) is,
# via linuxdeploy's Qt plugin. Qt has no automatic fallback between platform
# plugins, so without forcing xcb it fails outright instead of degrading to
# X11 (available virtually everywhere, including XWayland).
rm -f "$APPDIR/AppRun"
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/usr/bin/MagnifyFactory" "$@"
EOF
chmod +x "$APPDIR/AppRun"

echo "== Packaging AppImage =="
mkdir -p "$DIST_DIR"
EXTRACTED_LINUXDEPLOY="$(find /tmp -maxdepth 1 -iname 'appimage_extracted_*' -newer "$LINUXDEPLOY" -type d | head -1)"
APPIMAGETOOL="$EXTRACTED_LINUXDEPLOY/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool"
(cd "$REPO_ROOT" && ARCH=x86_64 "$APPIMAGETOOL" AppDir)
mv "$REPO_ROOT/MagnifyFactory-x86_64.AppImage" "$DIST_DIR/MagnifyFactory-$VERSION-x86_64.AppImage"

echo "Done. AppImage is at $DIST_DIR/MagnifyFactory-$VERSION-x86_64.AppImage"
