#!/usr/bin/env bash
set -euo pipefail

# Official release archive, pinned by version and GitHub-published SHA-256.
version=8.30.1
checksum=551f6fc83ea457d62a0d98237cbad105af8d557003051f41f3e7ca7b3f2470eb
scanner_dir=$(mktemp -d)
trap 'rm -rf -- "$scanner_dir"' EXIT
archive="$scanner_dir/gitleaks.tar.gz"
curl --proto '=https' --proto-redir '=https' --tlsv1.2 --fail --silent --show-error --location \
  "https://github.com/gitleaks/gitleaks/releases/download/v${version}/gitleaks_${version}_linux_x64.tar.gz" \
  --output "$archive"
printf '%s  %s\n' "$checksum" "$archive" | sha256sum --check --status
tar -xzf "$archive" -C "$scanner_dir" gitleaks
"$scanner_dir/gitleaks" git --redact --no-banner .
