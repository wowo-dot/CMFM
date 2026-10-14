# CMFM 0.1.0 (2026-10-14)

CMFM ("Configuration Menu For Mods") adds a **Mods** entry to Monster
Hunter: World's own menus: below Options on the title screen, and in the
Start menu's System tab, in town and in quests. It opens a settings page
drawn by the game itself, with your mods on the left and the selected
mod's settings on the right.

The first release. It ships together with the DLSS / Frame Generation mod,
CMFM's first client, and on its own.

## You need

- Monster Hunter: World on PC, version 421810 (the final version).
- **Astera Loader 0.2.0 or newer.** With an older one, CMFM does not start
  and Astera Loader asks you to update it.
- Nothing else (no Visual C++ runtime, no .NET).

## Install

The archive's top folder is the game folder:

```
AsteraLoader\plugins\cmfm\cmfm.dll     the plugin, with the guides and its licence
nativePC\ui\cmfm\mods.gui              the Mods page's layout
```

- **Mod Organizer 2:** install the archive as a mod, turn it on, and start
  the game with your Astera Loader entry.
- **Manual:** copy `AsteraLoader` and `nativePC` into the game folder,
  merging them with the folders already there.

`README.md` next to `cmfm.dll` is the player guide: how to use the page,
what CMFM does to a mod's config file, logs, troubleshooting.
`config-pages-guide.md` explains description files, which put any mod's
config file into the menu without code.

## What is in it

- The Mods page in the game's own look: one entry per mod, a dropdown for
  mods with several pages (Q / E or LB / RB switch them), switches, lists,
  sliders, key bindings, read-only and live status lines, buttons with the
  game's Yes/No question, greyed rows with their reason, restart marks,
  "Reset to defaults" per page, an Overview of every mod's state.
- For Astera Loader plugins: the C interface `cmfm.menu` 1.2 (frozen).
- For every other mod: description files (format 1, frozen) that map menu
  rows to values in the mod's own JSON, INI or TOML config file; CMFM
  changes only those values, in the file's own style, and keeps a backup.

## Changes since the closed beta

- The mod list kept the height of its first fill: with two mods it showed
  two rows, and an opened dropdown had to scroll. It now always shows its
  full window (beta.2).
- With three pages or fewer, D-pad (and keyboard) Down moved the mod list
  up. Up and Down now always go the way you press (beta.3).

Both found and diagnosed by the DLSS mod's beta testing.

## Known issues

- **A crash about two seconds after the game's window appears, naming
  CMFM** (`rGUIMessage::getMessage`): Steam was not started properly, so
  the game never loaded its texts. It crashes the same way without mods.
  Exit Steam completely and start it normally.
- **SharpPluginLoader:** an SPL plugin that adds its own Start menu entries
  can hide or replace CMFM's Start menu entry (the title screen's Mods
  stays, and the Overview's CMFM row says why). SPL 2.0.0 together with
  many Stracker plugins crashes on its own, with or without CMFM.
- **Crashes while the game exits, naming a Stracker plugin** (FextyMod,
  auto_food_skill, more_auto_shoutouts and others) are those plugins' own.
- Config pages write a mod's file when the Mods page closes; changes made
  while the page is open are lost if the game crashes before then.
- CMFM's own texts are in English only.

## Reporting a problem

Say what you did and what happened, and send from that launch:
- `AsteraLoader\logs\plugins\cmfm.log`;
- Astera Loader's `AsteraLoader\logs\core.<pid>.log`;
- the crash report from `AsteraLoader\logs\crashes`, if there is one.

Under MO2 these are in the Astera Loader mod's folder.

## Licence

MIT. `nativePC\ui\cmfm\mods.gui` is CMFM's edited copy of the game's
settings-page layout (Capcom's file with CMFM's changes), shipped so the
Mods page can have a layout of its own; it is not covered by CMFM's
licence.

## Files

SHA-256 of the archive and its files: `CMFM-0.1.0.sha256` next to the
archive, and the release page.
