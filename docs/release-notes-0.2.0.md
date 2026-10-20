# CMFM 0.2.0 (2026-10-20)

CMFM ("Configuration Menu For Mods") adds a **Mods** entry to Monster
Hunter: World's own menus: below Options on the title screen, and in the
Start menu's System tab, in town and in quests. It opens a settings page
drawn by the game itself, with your mods on the left and the selected
mod's settings on the right.

This release is about using the page: it now handles like the game's own
menus, and a mod's pages are tabs.

## You need

- Monster Hunter: World on PC, version 421810 (the final version).
- **Astera Loader 0.2.0 or newer** (as for 0.1.0).
- Nothing else (no Visual C++ runtime, no .NET).

## Install or update

The archive's top folder is the game folder:

```
AsteraLoader\plugins\cmfm\cmfm.dll     the plugin, with the guides and its licence
nativePC\ui\cmfm\mods.gui              the Mods page's layout
```

- **Mod Organizer 2:** install the archive as a mod (over CMFM 0.1.0, or
  as a new mod with the old one turned off), turn it on, and start the
  game with your Astera Loader entry.
- **Manual:** copy `AsteraLoader` and `nativePC` into the game folder,
  merging them with the folders already there and replacing CMFM's files.

Both files changed; update them together. Your settings stay where they
are (each mod's own config file).

## What is new

- **The mod list works like the game's Options.** Moving over an entry
  (pointer, arrows, stick) shows its settings on the right; a click or
  Confirm goes into them. Hovering no longer moves the focus by itself. A
  click on another mod from inside the settings still works.
- **A mod's pages are tabs above its settings** (they were a dropdown
  under the mod's entry):
  - click a tab, or use Q / E (LB / RB on a gamepad), also from inside the
    settings;
  - the arrows at both ends show those keys, with the game's own icons
    for your keyboard or gamepad, and can be clicked;
  - more than four tabs scroll: the mouse wheel over them, or the bar
    under them;
  - with keys or a gamepad, Up from a page's first setting moves to the
    tabs: Left / Right (or the stick) choose a page, Confirm opens it, Down
    or Back return to the settings.
- **Sounds** like the game's own menus: cursor moves, value changes, page
  switches.
- **The hand pointer** over the tabs and the mod list, as over the game's
  own buttons.
- **Mods are listed by name.** An Overview row opens its mod.
- **The help line** follows the tab under the pointer.
- **Config pages save sooner:** a mod's file is written half a second
  after you stop changing its settings, not only when the page closes.

## Fixes

- **The Mods page no longer shifts the game's list of GUI screens.** A
  speedrun tool that reads that list by position crashed with CMFM 0.1.0
  (a player's report). In game, the page now exists only while Mods is
  open.
- **With only one mod** (and the Overview), the settings drew but took no
  click or key (a client mod's report).

## For mod authors

- **`cmfm.menu` 1.3**, which only adds to 1.2; plugins built for 1.2 work
  unchanged. New: the callbacks' order is promised in the header, the text
  limits are defines, `cmfm_check.h` checks a page outside the game with
  the same rules CMFM uses, and a BUTTON row's Yes/No question can name
  what it does when pressed.
- **Page titles become tab labels:** a tab holds about 14 characters at
  full size; longer titles shrink to fit. The page header shows the mod's
  name for a mod with several pages.
- Description files (config pages) are unchanged: format 1.

## Known issues

- **A crash about two seconds after the game's window appears, naming
  CMFM** (`rGUIMessage::getMessage`): Steam was not started properly, so
  the game never loaded its texts. It crashes the same way without mods.
  Exit Steam completely and start it normally.
- **In game, WASD does not move the page's cursor (the arrow keys do):**
  the game's setting "Start Menu Keyboard Controls" decides that for every
  menu in game; choose a "Menu Selection: Main Key" option for WASD.
- **SharpPluginLoader:** an SPL plugin that adds its own Start menu entries
  can hide or replace CMFM's Start menu entry (the title screen's Mods
  stays, and the Overview's CMFM row says why). SPL 2.0.0 together with
  many Stracker plugins crashes on its own, with or without CMFM.
- **Crashes while the game exits, naming a Stracker plugin** (FextyMod,
  auto_food_skill, more_auto_shoutouts and others) are those plugins' own.
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

SHA-256 of the archive and its files: `CMFM-0.2.0.sha256` next to the
archive, and the release page.
