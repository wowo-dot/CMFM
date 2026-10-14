# CMFM

Configuration Menu For Mods: an in-game settings menu for Monster Hunter:
World mods on PC (DirectX 12), built as a plugin for
[Astera Loader](https://github.com/wowo-dot/AsteraLoader).

CMFM adds a **Mods** entry to the game's own menus, below Options on the
title screen and in the Start menu's System tab (in town and in quests).
It opens a page drawn by the game itself, like its Options: your mods on
the left, the selected mod's settings on the right, with the game's
switches, sliders, pick lists, Yes/No questions and help line.

- **For players:** one place for every mod's settings, in the game's look,
  with keyboard and mouse. Rows that cannot change say why; settings
  that need a restart are marked; each page can be reset to its defaults.
- **For mods written for Astera Loader:** a versioned C interface
  (`cmfm.menu`): switches, lists, numbers, key bindings, read-only and live
  status lines, buttons, locks and refusals with a reason, live or
  restart-required settings, several pages per mod.
- **For every other mod** (Stracker plugins, SharpPluginLoader plugins,
  abandoned mods): a small JSON description file maps menu rows to values
  in the mod's own config file (JSON, INI or TOML). CMFM changes only those
  values, in the file's own style, and keeps a backup. The mod's author or
  any player can write one.

Each mod's own config file stays the source of truth: CMFM stores none of
their settings.

**Status:** 0.1.0, the first release (2026-10-14; notes: `docs/release-notes-0.1.0.md`). Tested with MHW 421810 (the
final version) under Mod Organizer 2. Needs **Astera Loader 0.2.0 or
newer**; nothing else to install (no Visual C++ runtime, no .NET).

## Install

CMFM is `AsteraLoader\plugins\cmfm\cmfm.dll` plus its page layout
`nativePC\ui\cmfm\mods.gui`. Under Mod Organizer 2, install the archive as
a mod and start the game through Astera Loader as usual; for a manual
install, copy both folders into the game folder. Details, usage and
troubleshooting: `docs/player-guide.md`.

## For mod authors

- `docs/author-guide.md`: adding your Astera Loader plugin's settings
  (`include/cmfm/cmfm.h`, plain C; `examples/cmfm_demo/cmfm_demo.c` uses
  every feature);
- `docs/config-pages-guide.md`: a page for any mod, without code, through a
  description file (`examples/config_pages/` has working ones for JSON,
  INI and TOML configs).

## Building

Windows, Visual Studio 2026 with C++ (x64), CMake 3.25 or newer:

```
cmake -S . -B local/build -A x64
cmake --build local/build --config Release
ctest --test-dir local/build -C Release
```

The plugin is `local\build\Release\cmfm.dll`; the layout is
`assets\nativePC\ui\cmfm\mods.gui`. `-DCMFM_MEASURE=ON` builds a
measurement version that logs the time CMFM's always-on hooks take (not
for release).

## Licence

MIT (`LICENSE`). `third_party/al` is Astera Loader's plugin header (MIT).
`assets/nativePC/ui/cmfm/mods.gui` is CMFM's edited copy of the game's
settings-page layout, shipped so the Mods page can have a layout of its
own; it is Capcom's file with CMFM's changes, not covered by CMFM's
licence.
