#!/usr/bin/env bash
# Removes everything register_context_menu.sh installed. Per-user only,
# no root required.
set -euo pipefail

rm -f "$HOME/.local/share/applications/magnifyfactory.desktop"
rm -f "$HOME/.local/share/nautilus/scripts/Convert with MagnifyFactory"
rm -f "$HOME/.local/share/kio/servicemenus/magnifyfactory.desktop"
rm -f "$HOME/.local/share/kservices5/ServiceMenus/magnifyfactory.desktop"

command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true

echo "Removed MagnifyFactory context-menu integration."
