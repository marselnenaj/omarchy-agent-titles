# Agent Window Titles for Hyprland / Omarchy

[![CI](https://github.com/marselnenaj/omarchy-agent-titles/actions/workflows/ci.yml/badge.svg)](https://github.com/marselnenaj/omarchy-agent-titles/actions/workflows/ci.yml)

Native, editable title bars for Claude and Codex terminals. Click a title to
name the window after its project or task.

![Illustration of two generic agent windows](docs/preview.svg)

The preview uses synthetic names. The C++ decoration derives from
[Hyprbars](https://github.com/hyprwm/hyprland-plugins/tree/v0.56.0/hyprbars).
Hyprland handles positioning, clipping, workspace transitions and stacking.
There is no separate UI process or position polling. Text textures are cached;
activity spinner changes do not invalidate an unchanged title.

## Requirements

Tested with **Hyprland 0.56.2**, its matching development headers, and Ghostty.
Other Hyprland releases are not currently supported. Native plugins must be
rebuilt against the running compositor and its dependencies. An ABI check
rejects mismatched builds.

Build tools: a compatible C++23 compiler (prefer the compositor's toolchain),
CMake 3.27+, Ninja, pkg-config and nlohmann-json 3.11+. Development libraries:
Hyprland, libdrm, libinput, libudev, pixman, wayland-server, xkbcommon and GLib.

## Install

Using [Hyprland's plugin manager](https://wiki.hypr.land/Plugins/Using-Plugins/):

```sh
hyprpm add https://github.com/marselnenaj/omarchy-agent-titles
hyprpm enable agent-titles
hyprpm reload
```

Keep your existing `hyprpm reload` autostart configuration. Do not also load the
same plugin manually. Use `hyprpm update` to update and rebuild, or
`hyprpm disable agent-titles` to disable it.

For a local source build:

```sh
git clone https://github.com/marselnenaj/omarchy-agent-titles.git
cd omarchy-agent-titles
bash install.sh
```

The installer builds, tests and atomically installs the library under
`$XDG_DATA_HOME/hyprland/plugins/agent-titles/`, defaulting to
`~/.local/share/hyprland/plugins/agent-titles/`. It does not edit desktop config.
Add this to your Hyprland Lua config, then run `hyprctl reload`:

```lua
local data = os.getenv("XDG_DATA_HOME") or (os.getenv("HOME") .. "/.local/share")
hl.plugin.load(data .. "/hyprland/plugins/agent-titles/agent-titles.so")
```

For a legacy Hyprland config, use `plugin = /absolute/path/to/agent-titles.so`.
When updating a manual installation, unload the old library before installing,
then explicitly load the new library with `hyprctl plugin load /absolute/path`.
A config reload alone does not reload an already loaded library.

### Upgrading from version 1

Disable the old shell overlay before enabling the native plugin:

```sh
omarchy plugin disable io.github.marselnenaj.agent-titles
```

Version 2 uses Hyprland's plugin system. The former Omarchy Shell plugin
manifest is no longer used. Keep `~/.config/omarchy/agent-titles.json`:
custom names for existing windows in the same compositor session are preserved.
Appearance is configured through Hyprland; the former overlay width and offset
settings do not apply to the native title bar.

## Rename a window

Click the title bar and type. The existing name is selected when editing begins.
**Enter** or **Tab** saves, **Escape** cancels, and clicking outside saves.
Clear the name and save to restore the automatic title. Arrow keys, Home/End,
Shift selection, Ctrl+A/C/X/V, Unicode input and key repeat are supported.
The active editor uses green text and a visible caret. Hovering does not open
another surface or take keyboard focus.

Names belong to the window and survive plugin/config reloads. They reset for
a new window or compositor session. Renaming does not change the underlying
conversation or the terminal's title.

## Appearance

Add options after the plugin load in your Hyprland Lua config. See the complete
[example](examples/agent-titles.lua):

```lua
hl.config({ plugin = { agenttitles = {
  bar_height = 24,
  bar_text_size = 14,
  bar_text_align = "center",
  bar_color = "rgb(171717)",
  ["col.text"] = "rgb(eeeeee)",
} } })
```

| Option | Default | Meaning |
| --- | --- | --- |
| `enabled` | `true` | Show title bars |
| `bar_height` | `24` | Height, 18–64 logical pixels |
| `bar_text_size` | `14` | Font size, 10–32 |
| `bar_text_font` | `JetBrainsMono Nerd Font` | Font family |
| `bar_text_weight` | `600` | Font weight |
| `bar_text_align` | `center` | `left` or `center` |
| `bar_color` | `rgb(171717)` | Background |
| `col.text` | `rgb(eeeeee)` | Text color |
| `bar_padding` | `10` | Horizontal text padding |
| `bar_blur` | `false` | Blur translucent backgrounds; requires global blur |
| `bar_title_enabled` | `true` | Show title text |
| `bar_part_of_window` | `true` | Include the bar in the main decoration |
| `bar_precedence_over_border` | `false` | Place the border around the title bar |

Run `hyprctl reload` after editing. The bar reserves its own space above the
terminal content. It does not require terminal padding changes. Window rule
effects `agenttitles:no_bar`, `agenttitles:bar_color` and
`agenttitles:title_color` provide per-window overrides.

## Detection and privacy

Detection uses terminal app classes and their existing window titles: Claude
markers, Codex activity markers, agent names and Codex's `task | directory`
format. Recognized windows keep their provider hint through idle title changes;
returning to a shell/path title clears it. Terminal classes recognized include
Ghostty, Alacritty, Kitty, Foot and WezTerm, plus dedicated
`org.omarchy.agent`, `org.omarchy.claude` and `org.omarchy.codex` classes.
Only Ghostty has been tested interactively. Unrecognized custom title formats
may require a dedicated class. Ordinary terminals have no title bar.

Names are saved locally with owner-only permissions in
`$XDG_CONFIG_HOME/omarchy/agent-titles.json` (default
`~/.config/omarchy/agent-titles.json`). External edits are read on config reload.
The plugin makes no network requests and reads no chat histories, terminal
contents, API keys or agent credentials. Clipboard access occurs only when you
use the editor's copy, cut or paste actions. Unloading clears a clipboard
selection owned by the plugin.

## Development

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
python3 scripts/check-publication.py
shellcheck install.sh scripts/scan-secrets.sh
bash scripts/scan-secrets.sh
```

CI tests the standalone C++ title/editor/settings code with `BUILD_PLUGIN=OFF`,
checks publication contents and shell scripts, and scans history with Gitleaks.
The full decoration requires matching Hyprland headers and interactive desktop
validation. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Project additions: [MIT](LICENSE). Hyprbars-derived code retains its
[BSD-3-Clause license and attribution](vendor/README.md).
Not affiliated with Anthropic or OpenAI.
