#!/usr/bin/env bash
# Builds and locally installs the MagnifyFactory Flatpak from
# flatpak/org.magnifyfactory.MagnifyFactory.yml.
#
# Requires: flatpak, flatpak-builder, and the org.kde.Platform/org.kde.Sdk
# runtime (installed automatically from Flathub if missing).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$REPO_ROOT/flatpak/org.magnifyfactory.MagnifyFactory.yml"
RUNTIME_VERSION="$(grep -oP '(?<=runtime-version: '\'')[^'\'']+' "$MANIFEST")"

echo "== Ensuring Flathub remote and KDE runtime/SDK ($RUNTIME_VERSION) are present =="
flatpak remote-add --if-not-exists --user flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y flathub "org.kde.Platform/x86_64/$RUNTIME_VERSION" "org.kde.Sdk/x86_64/$RUNTIME_VERSION"

echo "== Building =="
cd "$REPO_ROOT"
rm -rf flatpak-build flatpak-repo
flatpak-builder --repo=flatpak-repo --force-clean flatpak-build "$MANIFEST"

echo "== Installing locally =="
flatpak remote-add --user --no-gpg-verify --if-not-exists magnifyfactory-repo flatpak-repo
flatpak install --user -y magnifyfactory-repo org.magnifyfactory.MagnifyFactory

echo "Done. Run it with: flatpak run org.magnifyfactory.MagnifyFactory"
echo "(or the CLI: flatpak run --command=magnify org.magnifyfactory.MagnifyFactory --help)"
