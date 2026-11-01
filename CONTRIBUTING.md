# Contributing

Open an issue describing the expected and actual behavior, or submit a focused
pull request. Use synthetic project names and example paths in tests and bug
reports. Do not include personal desktop configuration, chat histories,
credentials or unreviewed screenshots.

## Checks

```sh
node --test tests/*.test.cjs
python3 scripts/check-publication.py
bash -n install.sh scripts/scan-secrets.sh
shellcheck install.sh scripts/scan-secrets.sh
omarchy plugin validate .
```

The publication check inspects tracked files when run in a git checkout.
Run `bash scripts/scan-secrets.sh` for a checksum-verified Gitleaks history scan.
It downloads a pinned scanner to a temporary directory and removes it on exit.

## Interactive checks

CI does not run a Wayland desktop. On a compatible Omarchy installation, check:

1. Two agent windows keep distinct labels on different monitors/workspaces.
2. Hovering a label changes only the hover border and pointer cursor.
3. Clicking a label selects the existing name and accepts typing immediately.
4. Enter saves, Escape cancels, and clicking outside saves and releases focus.
5. Clearing the name restores the automatic title.
6. A renamed window keeps its name after a shell reload.
7. Fullscreen hides the labels; leaving fullscreen shows them again.
8. Ordinary terminal input still works before and after an edit.

When changing runtime files, a development install uses a content-addressed
runtime directory to avoid stale QML URLs during live shell reloads. Do not
commit those generated directories or private settings.
