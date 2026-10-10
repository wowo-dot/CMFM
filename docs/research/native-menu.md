# Native menu study

Decisions item 7: can CMFM add a row or a page to the game's own menus,
show its own text, and see when the player changes a value? This file
collects what is known, labelled **confirmed** (decompiler output or
measured) or **hypothesis**. Addresses are RVAs from image base
0x140000000 unless an IDA EA (0x14…) is given.

## Databases (2026-10-11)

- **Clean image and IDA:** `local/ida/mhw421810/mhw.exe` (SHA-256
  `a78ea69b0566414a42f16d0f8ff7d729e095041e2ae76a34cc02dbe0939f1d80`), a
  read-only memory copy of the tested exe at the title screen with no mods
  (run `local/runs/20261011-clean-dump`, tool `tools/re/dump_image.py`).
  Its code section is identical to Fexty's dump (`tools/re/diff_dumps.py`).
  `mhw.i64` next to it: IDA 9.4 auto-analysis with `-Opdb:off`, about
  4 minutes.
- **The first database** came from the old decoded image
  (`ai\glavenus\MonsterHunterWorld_decrypted.exe`, d786…845c) and is kept in
  `local/ida/old-dump-20261011/`. On that first load IDA asked Microsoft's
  symbol server for `World300.pdb` (its default; nothing found), hence
  `-Opdb:off` since.
- **Fexty's Ghidra database:** `MonsterHunterWorld_15.23_Fexty_20241219.gzf`
  (the user's copy, `D:\reverse engine jank\fexty ghidra`, read only).
  Imported headless into `local/ghidra/project` (Ghidra 12.0.2) and exported
  with `tools/re/ghidra/ExportProgram.java` to `local/ghidra/export`:
  53,040 named functions of 187,559, 48,121 hand-made labels, 925 structs,
  2,430 comments.
- **Same build, confirmed:** both record PDB `World300.pdb`, GUID
  `7c554b32-2b94-41f7-8e81-dc3593003b74`, age 1, and the same sections.
  53,039 of 53,040 named functions had identical first 16 bytes in the old
  image (the one exception was a hook in the old image), and the clean
  image's code is identical to Fexty's. Fexty's names apply at the same
  addresses.
- **Imported into IDA** with `tools/re/ida/import_ghidra_export.py`:
  100,804 names and 2,430 comments (`[Fexty …]` prefix); places IDA had
  already named were kept.

### Caveats

- **The old decoded image is not pristine (confirmed).** Against Fexty's
  dump its code differs in 29 places: 22 functions start with a jump out of
  the image (`Monster::generateFilePaths` at 0x1ccf6c0 among them), NOPs in
  `uWeapon::calcBloatedRaw`, two conditional jumps made unconditional, two
  redirected calls. It was dumped with mods loaded. The clean image has none
  of these (0 code differences from Fexty's), so Fexty's dump is clean too.
  The full list is `local/ghidra/diff_ours_vs_fexty.json`. AL's and the DLSS
  mod's pattern checks give the same results on both images (no site is
  among the 29), and all 182 DLSS code gates match the clean image.
- **Folded stubs carry one arbitrary name.** The linker merged identical
  small functions, so a name like `ActivateModifier::createUi` (0x14023d440)
  or `switchD_142af6b01::caseD_0` (0x140240720) in a vtable is a shared
  stub, not that function.

## GUI classes

The exe names 436 GUI classes (MT Framework DTI names: `uGUI*` are units,
`cGUI*` parts, `rGUI*` resources). The ones that matter first:

| Area | Classes |
|---|---|
| Options | `uGUIOption`, `uGUIMenuOption`, `uGUIGraphicsOption`, `uGUIGraphicsSetting`, `uGUISoundSetting`, `uGUIViewModeOption`, `uGUIHDRSetting`, `uGUIKeyboardConfig2` |
| Start menu | `uGUIStartMenu`, `uGUIStartMenuBase`, `uGUITopMenuList`, `uGUISubMenuList`, `uGUIHudStartMenuIcon` |
| Building blocks | `cGUIInstButtonList`, `cGUIInstButtonTree`, `cGUIInstSlider`, `cGUIInstText`, `cGUIFixedScrollList`, `cGUIScrollPageControl` |
| Text | `rGUIMessage`, `cGUIMessageAnalyzer`, `cGUIObjMessage`, `cGUIInstMessage` |
| Popups, notices | `uGUICommonDialog`, `uGUICommonSelectWindow`, `uGUICommonMessageWindow`, `uGUINotice`, `uGUIAnnounce`, `uGUIInfoWindow` |
| Curiosity | `uGUIDebug` |

## uGUIOption (confirmed, static)

- DTI registration at 0x140200b20: `MtDti::ctor(&uGUIOption::DTI::Instance,
  "uGUIOption", &uMhGUI::DTI::Instance, 0x7810, …)`. Object size 0x7810.
- Parent `uMhGUI` (DTI 0x14502CFF0, registered at 0x140203460, size
  0x28F0), whose parent is `uGUI` (DTI 0x1451C6298).
- Constructor 0x141E88400; vtable `uGUIOption::vft` 0x14347DA20, 93 slots.
  Its own overrides: slots 0 (dtor), 4 (getDti), 71, 73, 74, 76, 81, 90, 91;
  the rest are `uGUI`/`uMhGUI` code (`uGUI::setResource` 39,
  `uGUI::setPlay` 42, `uGUI::initInstance` 62, …).
- The constructor builds 7 sub-objects of 0x968 bytes at +0x2918, vtable
  0x14347D970. **Hypothesis:** one per Options tab.

## uGUIMenuOption (confirmed, static)

- DTI registration at 0x140200790, size 0x2CA0, parent `uMhGUI`.
- Constructor 0x141E7FCB0; vtable `uGUIMenuOption::vft` 0x14347BF78.

## Open questions

1. How `uGUIOption` builds its rows: where the item list comes from (a
   table in the exe, a resource file, or code per tab), and where each
   row's label and value text come from (`rGUIMessage` ids?).
2. How a value change reaches the game's settings (which slot or callback
   runs when the player moves a slider or switches a choice).
3. What `uGUIMenuOption` is next to `uGUIOption` (the Start menu's Options
   entry, or the title screen's?).
4. How the Start menu's lists (`uGUITopMenuList`, `uGUISubMenuList`) get
   their entries; a "Mods" entry could live there.
5. Whether `uGUICommonDialog` / `uGUINotice` can be opened with our own
   text for CMFM's popups.
