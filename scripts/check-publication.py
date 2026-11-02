#!/usr/bin/env python3
"""Check plugin packaging and prevent accidental publication of local files."""

import tomllib
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def main():
    manifest = tomllib.loads((ROOT / 'hyprpm.toml').read_text())
    assert manifest['repository']['name'] == 'agent-titles'
    plugin = manifest['agent-titles']
    assert plugin['output'] == 'build/agent-titles.so'
    assert len(plugin['build']) == 2
    assert (ROOT / 'native/main.cpp').is_file()
    assert (ROOT / 'vendor/HYPRBARS-LICENSE').is_file()

    tracked = subprocess.run(
        ['git', 'ls-files', '-z'], cwd=ROOT, check=True, capture_output=True
    ).stdout.decode().split('\0')
    files = [ROOT / name for name in tracked if name]
    assert files, 'No tracked files; explicitly stage the public package before checking.'
    blocked_names = {'.env', 'auth.json', 'credentials.json', 'agent-titles.json', 'shell.before.json'}
    blocked_parts = {'.aws', '.ssh', '.codex', '.claude', '.config', '.local', 'node_modules', '__pycache__', 'build'}
    blocked_suffixes = {'.pem', '.key', '.p12', '.pfx', '.log', '.jsonl', '.db', '.sqlite', '.png', '.jpg', '.jpeg', '.mp4', '.webm', '.so', '.o', '.a', '.core'}
    for path in files:
        relative = path.relative_to(ROOT)
        assert not path.is_symlink(), f'Symlink: {relative}'
        assert path.name not in blocked_names, f'Private state: {relative}'
        assert not blocked_parts.intersection(relative.parts), f'Private directory: {relative}'
        assert not any(part.startswith('runtime-') for part in relative.parts), f'Generated runtime: {relative}'
        assert path.suffix.lower() not in blocked_suffixes, f'Local artifact: {relative}'
        assert not re.search(r'\.(bak|before)(\.|$)', path.name), f'Backup: {relative}'
        content = path.read_bytes()
        assert len(content) < 1024 * 1024 and b'\0' not in content, f'Unexpected binary/large file: {relative}'
        text = content.decode('utf-8')
        assert not re.search(r'/(?:home|Users)/[A-Za-z0-9_.-]+/', text), f'Personal absolute path: {relative}'
        assert not re.search(r'-----BEGIN (?:[A-Z0-9 ]+ )?PRIVATE KEY-----', text), f'Private key: {relative}'
    print(f'Publication checks passed for {len(files)} tracked text files.')


if __name__ == '__main__':
    main()
