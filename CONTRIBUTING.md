# Contributing

Use synthetic project names and example paths in tests and reports. Do not
include desktop configuration, chat histories, credentials or unreviewed
screenshots, logs or crash dumps.

## Checks

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 scripts/check-publication.py
bash -n install.sh scripts/scan-secrets.sh
shellcheck install.sh scripts/scan-secrets.sh
bash scripts/scan-secrets.sh
```

Use `-DBUILD_PLUGIN=OFF` in a separate build directory to test the model,
UTF-8 editor and private settings store without Hyprland headers. Full builds
must use headers and dependencies matching the running compositor. Preserve
the ABI guard and upstream license. The publication check inspects staged and
tracked files; explicitly stage new public files before running it.

## Desktop validation

Use a separate nested Hyprland instance and synthetic terminal titles first.
Keep test settings and captures outside the checkout. Never overwrite a shared
library inode while the compositor has it loaded; unload before rebuilding a
library in place, or test immutable copies.

Check that:

1. Existing and newly opened Claude/Codex windows get the correct title.
2. Ordinary terminals have no bar; idle activity frames keep stable titles.
3. Hovering does not steal focus or produce a popup.
4. Clicking selects the existing name; Unicode, selection, clipboard and key
   repeat work. Editor keystrokes never reach the terminal.
5. Enter, Tab and outside click save; Escape cancels; clearing restores the
   automatic title. Saved names survive config and plugin reloads.
6. The bar follows moving/resizing windows, different monitor scales, workspace
   transitions, fullscreen and floating-window occlusion.
7. Appearance options update without config errors.
8. Repeated explicit unload/load cycles work, including after copying text.
9. Ordinary terminal input works after editing or unloading.

CI does not run the compositor or exercise the rendering and input integration.
