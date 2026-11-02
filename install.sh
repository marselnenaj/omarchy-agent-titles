#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
install_dir="${XDG_DATA_HOME:-$HOME/.local/share}/hyprland/plugins/agent-titles"
for dependency in cmake ninja pkg-config; do
  command -v "$dependency" >/dev/null || { echo "Missing dependency: $dependency" >&2; exit 1; }
done
cmake -S "$source_dir" -B "$source_dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$source_dir/build" -j 2
ctest --test-dir "$source_dir/build" --output-on-failure
mkdir -p "$install_dir"
temporary=$(mktemp "$install_dir/.agent-titles.XXXXXX")
trap 'rm -f -- "$temporary"' EXIT
install -m 755 "$source_dir/build/agent-titles.so" "$temporary"
# Replace the directory entry, never truncate a library mapped by Hyprland.
mv -f -- "$temporary" "$install_dir/agent-titles.so"
printf 'Installed %s\nSee README.md for loading and updating the plugin.\n' "$install_dir/agent-titles.so"
