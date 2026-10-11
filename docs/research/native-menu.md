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

## Which class is which (confirmed, static)

Each GUI class names its layout file in a small virtual function (slot 71),
and the class name string sits next to the path in `.rdata`.

| Layout | Class | What it is |
|---|---|---|
| `ui\title\options` | `uGUIOption` | **title screen** Options; tabs `settings`, `display`, `audio`, `display_G`, `controls`, `camera`, `language` (the 7 sub-objects, hypothesis) |
| `ui\start_menu\option_hud` | `uGUIMenuOption` | in-game page with `btnlist_slider` (HUD settings, hypothesis) |
| `ui\start_menu\option_camera` | `uGUIViewModeOption` | in-game camera page |
| `ui\start_menu\option_sound` | `uGUISoundSetting` | in-game sound page |
| `ui\start_menu\option_C_Support` | (class at 0x140AA5A60's owner) | colour support page |
| `ui\start_menu\graphic` | `uGUIGraphicsSetting` | in-game graphics page |
| `ui\pc\graphic_option` | `uGUIGraphicsOption` | PC graphics options |
| `ui\start_menu\start_menu` | `uGUIStartMenu` | the Start menu itself |

So each in-game settings page is its own class and layout.

## How a settings page works (confirmed, static, from uGUIMenuOption)

The page classes fill the same `uGUI`/`uMhGUI` virtual slots:

| Slot | uGUIMenuOption | Does |
|---|---|---|
| 71 | 0x141E82160 `getGuiPath` | returns the layout path |
| 74 | 0x141E812F0 `setupInstances` | takes layout elements from the instance table (`this+0x2A0`), attaches the page's callbacks to its lists, writes row labels |
| 90 | 0x141E80E50 `open` | reads the current settings through getters (0x140441F20, 0x140442CF0, 0x140442310, 0x140441490, 0x140442C00) into the page (`+11328..+11344`) and writes texts |
| 91 | 0x141E80D40 `close` | writes the page's values back through setters (0x140444D10, 0x140445A80, 0x140444220, 0x1404453F0) |
| 76 | 0x141E82590 `update` | per frame; shows the selected row's help text (`sMhGUI` 0x141AE0E60 with a text id) |
| 73 | 0x141E80CF0 `release` | frees a buffer |

Layout elements can also be found by name: `uGUI::getInstanceFromName(gui,
"btnlist_select")`, and inside a row `cGUIObjRoot::getObjectFromName(row,
"msg_language")`.

## Lists, rows and text (confirmed, static)

- **Lists** are `cGUIInstButtonList` (on `cGUIInstButtonTree`, on
  `cGUIInstInput`). Rows: count at `+0x1D8`, array at `+0x1E8`. Field names
  come from each class's `populatePropertyList` (MT Framework keeps them):
  button list `Type`, `InputLoop`, `DuplicateNum` (+0x298), `DuplicateSpace`
  (+0x29C, float), `DuplicateButtonSize`, `PrevArrowInstID`,
  `NextArrowInstID`; slider `MoveType`, `CursorID`, `MinPos`, `MaxPos`;
  scroll bar `ScrollType`, `MaxFixFrame`, `BarMinPos`, `BarMaxPos`.
- **Rows can be added at runtime.** The layout defines a template row;
  `cGUIInstButtonList::rebuildRows` (0x140B7FEF0) removes the old copies and
  makes `count` new ones. `setRowCount` (0x140B7FED0) calls it and
  refreshes the list. The game calls `setRowCount` from 48 places, with fixed
  and runtime counts (the language list sizes itself to the number of
  languages).
- **Callbacks** on a list are plain function pointers: owner `+0x120`,
  callbacks at `+0x128`, `+0x130`, `+0x138` (`cGUIInstInput`) and `+0x210`
  (`cGUIInstButtonTree`). Which one is decide, cancel or cursor move is not
  known yet.
- **Text:** `cGUIObjSizeAdjustMessage::formatText(obj, const char*)` takes a
  plain zero-terminated string, so a row can show **our own text**. The
  game's texts come from 24 loaded message files: `GetGameMessage(category,
  id)` picks one of the `rGUIMessage` globals at 0x145012DC0… and
  `rGUIMessage::getMessage` indexes its string table (`+0xF8`, count
  `+0xD8`). uGUIMenuOption's row labels are category 12 (qword_145012E20),
  ids 426 + row. Encoding not verified yet (MT Framework uses UTF-8).

## Answers so far

1. **Can CMFM add a row or a page?** Rows: yes in principle; the game resizes
   lists at runtime. A page: not answered yet (how pages are opened, and
   whether a page class of our own can reuse a game layout).
2. **Can CMFM show its own text?** Yes in principle: `formatText` takes any
   string.
3. **Can CMFM see value changes?** Probably, through the list callbacks; not
   traced yet.

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
