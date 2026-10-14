# CMFM handoff, 2026-10-11 (end of the first native-menu session)

Read AGENTS.md first, then this file, then `docs/research/native-menu.md`
(the full map, every claim labelled) and `docs/decisions.md` items 6-9.

## Where things stand

- **The in-game native menu works (prototype 2).** Start menu -> System has
  **Mods** right below Options (also in quests). It opens the game's own
  Options page as a **"Mods" page**: tab "CMFM" with two test rows
  (Test toggle Off/On, Test choice Alpha/Beta/Gamma), the other four tabs
  hold a one-value placeholder row ("Free tab"). Values, help lines, tab
  switching, R (Set To Default, resets CMFM rows only) and closing all work;
  the real Options are untouched. Runs: `local/runs/20261011-native-m1b`,
  `-m2b`, `-m2c`.
- **Installed test build:** MO2 mod `CMFM (dev)` (profile `al-stracker`),
  build 02297c0 (code d0b8a26), `cmfm.dll` SHA-256
  `98769338415c8dd14d03dfd13c3a14463b07cbe1fc40dbf74149c9b05ea9b0fd`.
- **Clean game image** (`local/ida/mhw421810/mhw.exe`, a78ea69b...) and our
  IDA database with Fexty's names; the old glavenus image is dirty
  (handoff-clean-image-20261011.md went to the other projects).
- **The player's real options were reset once** (run m2b, R before the
  guard existed). The user re-set them; no lasting harm.

## What CMFM changes in the game (exe 421810 only, fail closed)

Six `al.hooks` detours (patterns in `src/native/game_421810.hpp`):
`rGUIMessage::getMessage` (CMFM's texts), `uGUIStartMenu::onItemAction`
(item 77 = Mods), option getter 0xEEA350 and setter 0xEEB450 (CMFM rows;
an unknown kind would otherwise write the save data), `uGUIMenuOption::close`
0x1E80D40 (restore), Set To Default answer 0x1E800A0 (CMFM rows only).
Data: the System tab pointer in both Start menu tab lists (read-only data,
the prototype helper `QwordPatch`, decision 9) and `OptionTabTables[0..4]`
(writable) while the Mods page is open. `refreshRows` is found with
`al.scanner`.

## Next steps, in order

1. **Title screen (decision 8, second entry point).** Plan in the research
   notes, "The title screen":
   - CMFM rows move from kind 7 to **kind 1 with setting ids 100-199**
     (the title Options drops unknown kinds; kind 1 works on both pages);
     getter/setter hooks answer `kind == 1 && id >= 100`.
   - Title Mods page = `uGUIOption` with `off_143FC0480[0..4]` swapped,
     category labels (ids 392, 347, 348, 349, 395) and header (12/0) from
     the text hook, the category builder `sub_141E8D5A0` re-run after the
     swap and after the restore (lists are built once at setup), and the
     category list trimmed to 5 rows (Key Config and **Default**, a full
     reset, out of reach). Find the title Options' close/open functions.
   - Title Mods entry = an 8th row in `uGUITitle`'s `btnlist_menuList`:
     shift the cursor mapping (row -> item id), move `+12332` (the Quit row)
     to 7, relabel rows 4-7 after `play()`, and **find where Confirm acts on
     the item id** (`+10488`) to handle ours (not found yet).
2. **The plugin interface** `include/cmfm/cmfm.h` (`cmfm.menu` 1.0): typed
   settings mapped onto CMFM rows (enum/bool now; sliders and read-only lines
   need more of the row module: `sub_140EEA2D0` slider types, rows that open
   sub-pages), plus popups through the game's own dialogs (SPL's verified
   records, research notes). First client: the DLSS mod.
3. **SPL compatibility:** SPL's Start menu API swaps the same page pointers;
   detect a foreign table and insert into it (research notes).
4. **A long play session** with the hooks in place (stability; deferred by
   the user), then turn the data-patch helper into an AL request
   (`docs/al-requests.md`, decision 9).

## Notes for these sessions

- Fun project, no deadline; broad autonomy; plain reports.
- Exit crash reports from Stracker plugins (FextyMod, more_auto_shoutouts,
  auto_food_skill) are known noise; check owners, don't raise them.
- SPL's Start menu API (2026-09-20) was pointed out from outside the
  project; useful input.
