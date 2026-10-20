# CMFM for players

CMFM ("Configuration Menu For Mods") adds a **Mods** entry to Monster
Hunter: World's own menus: below Options on the title screen, and in the
Start menu's System tab, in town and in quests. It opens a settings page
that looks like the game's own: your mods on the left, the selected mod's
settings on the right.

A mod shows up there in one of two ways:
- it was written for CMFM (an Astera Loader plugin that registers its
  settings), or
- someone wrote a small **description file** for it (a "config page"),
  which lets CMFM change that mod's own config file for you. Stracker and
  SharpPluginLoader plugins can join this way without any change to them.

CMFM keeps no settings of its own. Each mod's config file stays where it
was, and you can still edit it by hand.

## What you need

- Monster Hunter: World on PC, version 421810 (the final version).
- **Astera Loader 0.2.0 or newer.** With an older Astera Loader, CMFM does
  not start and Astera Loader tells you to update it.
- Nothing else: no Visual C++ runtime, no .NET.

## Install

CMFM is two files:

```
AsteraLoader\plugins\cmfm\cmfm.dll   the plugin
nativePC\ui\cmfm\mods.gui            the Mods page's layout (CMFM's own copy of the game's settings page)
```

**Mod Organizer 2 (recommended):** install the archive as a mod and turn
it on. For MHW a mod's root is the game folder, so the two folders above
sit at the mod's top. Start the game with your "Astera Loader" entry, as
always.

**Manual:** copy the `AsteraLoader` and `nativePC` folders from the archive
into the game folder (next to `MonsterHunterWorld.exe`), merging them with
the folders already there. Start the game the way you start Astera Loader.

**Config pages** (description files for mods without CMFM support) go into
`AsteraLoader\plugins\cmfm\pages\`. Under MO2, a pack of pages can be its
own mod.

## Using the Mods page

- **The mod list** (left): "Overview" first, then one entry per mod. It
  works like the game's Options: move over an entry (pointer or arrows) and
  its settings show on the right; a click or Confirm goes into them, and
  Back or a right click returns to the list. A mod with several pages
  shows them as tabs above its settings: click a tab, or use Q / E (LB /
  RB on a gamepad), also from the settings side. When there are more tabs
  than fit, scroll them with the mouse wheel over them or the bar under
  them. With keys or a gamepad, Up from a page's first setting moves to its
  tabs: Left / Right switch pages, Down goes back to the settings. Each mod
  opens on the page you used last.
- **The settings** (right): switches and lists change with the arrows (or
  a click), or a list opens the game's pick list; numbers are sliders. A
  greyed row cannot change right now; the help line at the bottom says
  why. A page with more than 14 rows scrolls.
- **Key bindings:** Confirm on a key row, then press the new key (with
  Ctrl, Shift or Alt if you like). Esc or a click cancels, Backspace sets
  no key. A key shown in red is also used by another mod; the help line
  names it.
- **Restart marks:** some settings only take effect after the game
  restarts. When you change one, its row gets a star and the page says
  "Restart needed", until you restart or set the value back.
- **Reset to defaults:** the red row at the end of a page puts every
  setting on that page back to the mod's default, after a Yes/No question.
- **The Overview** lists every mod and its state: Active, Restart needed,
  or Refused with the reason (a mod's page with a mistake, a config page
  whose config file is missing). CMFM's own row says if part of CMFM is
  off and why.

### Config pages: what CMFM does to a mod's file

- Your changes are written when you close the Mods page, into the mod's
  own config file, and only the values you changed. The rest of the file
  (comments, order, spacing) stays exactly as it was.
- The first time CMFM changes a file, it keeps the original next to it as
  `<file>.cmfm-backup`. To undo everything, put that copy back.
- If CMFM cannot find a value in the file, that row is read-only and says
  why. If you change the same value by hand while the game runs, your hand
  edit wins over the menu's.
- Whether a change applies at once or after a restart depends on the mod;
  the page marks the ones that need a restart.

## Logs

CMFM's log is `AsteraLoader\logs\plugins\cmfm.log` (under MO2, in the
Astera Loader mod's folder). It lists the mods that joined the menu, every
value you set, and the reason for anything refused. Crash reports are in
`AsteraLoader\logs\crashes`. For a bug report, send cmfm.log, Astera
Loader's `core.<pid>.log` and the crash report from that launch.

## Troubleshooting

- **The game crashes about two seconds after its window appears, and the
  crash report names CMFM** (in `rGUIMessage::getMessage`, the game's text
  lookup). Steam was not started properly, so the game never loaded its
  texts; it crashes the same way without any mods, but CMFM's hook on that
  function puts CMFM's name on the report. Exit Steam completely (from its
  tray icon), start Steam normally, then start the game again.
- **"Update Astera Loader" at start-up:** CMFM needs Astera Loader 0.2.0
  or newer.
- **Mods is on the title screen but not in the Start menu:** another tool
  changed the Start menu first, for example a SharpPluginLoader plugin that
  adds its own Start menu entries. The Overview's CMFM row says what
  happened. The title screen's Mods entry still works.
- **SharpPluginLoader together with many Stracker plugins crashes,** with
  or without CMFM and Astera Loader (Astera Loader's known issues). Try
  without one of them.
- **A crash while the game exits, naming a Stracker plugin** (FextyMod,
  auto_food_skill, more_auto_shoutouts and others): those plugins'
  own; the game was closing anyway.
- **A mod is missing from the list:** check the Overview. A mod written
  for CMFM may be refused with a reason; a config page may be refused
  (a mistake in it, with its line) or replaced by the mod author's own
  page.

## Uninstall

Turn off the mod in MO2, or delete `AsteraLoader\plugins\cmfm` and
`nativePC\ui\cmfm`. CMFM changes nothing else; config files it edited for
you keep your settings (their `.cmfm-backup` copies hold the originals).
