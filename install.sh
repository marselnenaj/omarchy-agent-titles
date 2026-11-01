#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
plugin_dir="${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/plugins/io.github.marselnenaj.agent-titles"
for dependency in omarchy omarchy-shell jq sha256sum install; do
  command -v "$dependency" >/dev/null || { echo "Missing dependency: $dependency" >&2; exit 1; }
done
omarchy plugin validate "$source_dir"
if [[ -e "$plugin_dir/.git" ]]; then
  echo "This plugin is git-managed. Use: omarchy plugin update io.github.marselnenaj.agent-titles" >&2
  exit 1
fi
if [[ -d "$plugin_dir" ]]; then
  backup="${XDG_STATE_HOME:-$HOME/.local/state}/omarchy/agent-titles-backups/$(date +%Y%m%d-%H%M%S)"
  mkdir -p "$backup"
  cp -a "$plugin_dir" "$backup/plugin"
fi
revision=$(cat "$source_dir/Service.qml" "$source_dir/TitleModel.js" "$source_dir/TitleSettings.qml" "$source_dir/TitleSettingsModel.js" | sha256sum | cut -c1-12)
runtime_dir="runtime-$revision"
mkdir -p "$plugin_dir/$runtime_dir"
for file in Service.qml TitleModel.js TitleSettings.qml TitleSettingsModel.js; do
  install -m 644 "$source_dir/$file" "$plugin_dir/$runtime_dir/$file"
done
# Unique entry URLs avoid stale QML components during a live shell rescan.
manifest_tmp=$(mktemp "$plugin_dir/.manifest.XXXXXX")
trap 'rm -f -- "$manifest_tmp"' EXIT
jq --arg entry "$runtime_dir/Service.qml" '.entryPoints.panel = $entry' "$source_dir/manifest.json" > "$manifest_tmp"
mv -- "$manifest_tmp" "$plugin_dir/manifest.json"
omarchy-shell shell rescanPlugins
# Discovery is asynchronous; wait for the registry before enabling.
discovered=false
for ((attempt = 0; attempt < 30; attempt++)); do
  if omarchy-shell shell listPlugins | jq -e '.[] | select(.id == "io.github.marselnenaj.agent-titles")' >/dev/null; then
    discovered=true
    break
  fi
  sleep 0.2
done
if [[ "$discovered" != true ]]; then
  echo "Plugin discovery timed out; check that Omarchy Shell is running." >&2
  exit 1
fi
omarchy plugin enable io.github.marselnenaj.agent-titles
