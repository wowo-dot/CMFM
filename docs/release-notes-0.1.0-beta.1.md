# CMFM 0.1.0-beta.1 (closed beta, 2026-10-14)

CMFM ("Configuration Menu For Mods") adds a **Mods** entry to Monster
Hunter: World's own menus: below Options on the title screen, and in the
Start menu's System tab, in town and in quests. It opens a settings page
drawn by the game itself, with your mods on the left and the selected
mod's settings on the right.

This is a closed beta: please do not share the archive. Thank you for
testing.

## You need

- Monster Hunter: World on PC, version 421810 (the final version).
- **Astera Loader 0.2.0 or newer.** With an older one, CMFM does not start
  and Astera Loader asks you to update it.
- Nothing else (no Visual C++ runtime, no .NET).

## Install

The archive's top folder is the game folder:

```
AsteraLoader\plugins\cmfm\cmfm.dll     the plugin, with this guide and its licence
nativePC\ui\cmfm\mods.gui              the Mods page's layout
```

- **Mod Organizer 2:** install the archive as a mod, turn it on, and start
  the game with your Astera Loader entry.
- **Manual:** copy `AsteraLoader` and `nativePC` into the game folder,
  merging them with the folders already there.

`README.md` (next to `cmfm.dll`) is the player guide: how to use the
page, what CMFM does to a mod's config file, logs, troubleshooting.
`config-pages-guide.md` explains description files, which put any mod's
config file into the menu without code.

## What to test

- Open Mods from the title screen and from the Start menu (in town and in
  a quest); move around the mod list and the pages with keyboard, mouse
  and, if you use one, a gamepad.
- Change settings of the mods that use CMFM (the DLSS mod, for one);
  restart marks, "Reset to defaults", key bindings.
- Alt-tab, a screen-mode change and loading screens with CMFM installed.
- Long sessions: crashes, stutter, anything odd.

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
- Never tried so far: switching pages with LB / RB on a gamepad, alt-tab
  and screen-mode changes while the Mods page is open.
- CMFM's own texts are in English only.

## Reporting a problem

Say what you did and what happened, and send from that launch:
- `AsteraLoader\logs\plugins\cmfm.log`;
- Astera Loader's `AsteraLoader\logs\core.<pid>.log`;
- the crash report from `AsteraLoader\logs\crashes`, if there is one.

Under MO2 these are in the Astera Loader mod's folder.

## Build

CMFM 0.1.0-beta.1, commit and SHA-256 values in `CMFM-0.1.0-beta.1.sha256`
next to the archive.
