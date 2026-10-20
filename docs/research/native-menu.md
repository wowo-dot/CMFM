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
  makes `count` new ones, and **stores `count` into the list's
  `DuplicateNum`** (`+0x298`; `mov [r14+0x298], ebx` at +0x1F8), so after a
  resize that field no longer holds the layout's window size. CMFM reads the
  mod list's window once, before its first resize (found in the closed beta
  with 2 mods: the list stayed 2 rows high; the DLSS session's
  `handoff-cmfm-20261014`). `setRowCount` (0x140B7FED0) calls it and
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

## How GUIs are registered, created and opened (confirmed, static)

- **Registry.** `sMhGUI` (instance `sMhGUI::Instance` 0x1451C4640) keeps 11
  groups at `+85936` (0x14FB0): `{record table, count}` pairs, 16 bytes
  each. A record is 0x18 bytes: `{DTI*, const char* layout path, uint32
  flags}`. Tables (named `GUIRegistry_Group*_cmfm` in IDA):

  | Group | Table | Count | Contents |
  |---|---|---|---|
  | 0 | 0x143F9CF40 | 18 | common: select/message windows, dialog, save/load, cursor |
  | 1 | 0x143F9D0F0 | 125 | menus: hud_info, start_menu, item menus, **option_hud (index 12, uGUIMenuOption)**, guild card… (filled by 0x141ADC060) |
  | 2 | 0x143F9DCB0 | 45 | facilities and shops |
  | 3 | 0x143F9E190 | 26 | HUD |
  | 5 | 0x143F9E430 | 29 | includes language, HDR, keyboard, PC graphics |
  | 6 | 0x143F9E6F0 | 9 | includes sound settings |
  | 4, 7–10 | 0x143F9E400, 0x143F9E100, 0x143F9E0E8, 0x143F9E178, 0x143F9E130 | 1–3 | small |

- **Creation.** `sMhGUI::createGUIObjects(gui, group, out)` (0x141AD6230)
  creates every unit of a group at once (`sMhUnit::newUnit`, then
  `addTop`/`addBottom` on move line 20 or 22). Flags: 1 = skip, 2 = line 22,
  4 = top, 8 = conditional flag. So a page's unit exists before it opens.
- **Opening.** Each page has an open function (`sMhGUI::open_uGUIMenuOption`
  0x141EC40B0 for item 71): find the live unit by DTI id among
  `sMhGUI`'s units (`+616` count, `+632` array; compare
  `unit->getDti()->id` at DTI+0x34), wrap the caller's arguments in two
  functors, set flag 1, and push the request into the queue at
  `sMhGUI+0x8F68` (0x14114F680, hypothesis: "push open request").
- **Start menu entries.** Static tables of 16-byte entries
  `{item id, label text id, help text id, extra}`; 76 ends a group.
  `StartMenu_ItemTable_cmfm` 0x143FC0BD0 has 5 groups (likely the 5 tabs):
  items 0–9, 11–22, 23–45, 46–63, 71–75. Submenus have small tables of their
  own (items 64–69 under 63, 58–62 under 57, …). `uGUIStartMenu::populateSubMenu`
  (0x141ED1880) keeps up to 10 visible entries; the action switch
  `uGUIStartMenu::onItemAction` (0x141ECDC30) has one case per item id
  (yes/no dialogs, page opens, submenus). Item 71 opens `uGUIMenuOption`.
- **Native popups exist:** `sMhGUI::DisplayYesNoDialog` (0x141AE2050) and
  `sMhGUI::DisplayMessageWindow` (0x141AE3EF0), used by the Start menu.

### What a CMFM page could look like (design idea, not tested)

1. A page class of our own (an `uMhGUI` subclass with our own vtable and an
   MT DTI registered under the parent `uMhGUI`), reusing an existing list
   layout such as `ui\start_menu\option_hud`.
2. A record for it in group 1's registry (a larger copy of the table, with
   the group's pointer and count changed before the group is created).
3. A Start menu entry: an extra entry in one category table, our label via
   `formatText`, and our item id handled before the action switch (a hook on
   `onItemAction` through `al.hooks`).
4. Rows built with `setRowCount` and `formatText`, values changed through the
   list callbacks, everything read from and written to the owning plugin
   through CMFM's interface.

Each step needs checking in the running game first (with Frida, read-only
logging on functions we already know, after the user's OK).

## Live checks (run `local/runs/20261011-gui-inspect`, read-only)

Confirmed in the running game with `tools/re/gui_inspect.py`:

- The registry groups and live unit list are where the static study put
  them. Group 1 (menus) does not exist at the title screen; in Astera the
  Start menu and `uGUIMenuOption` are alive while closed. Opening a menu
  creates nothing; it flips the unit's state word (`+8856`, low byte `c9`
  shown, `ca` hidden).
- Start menu labels are message category 11. The System tab is items 71
  Options, 10 View Mode, 72 Steam Store, 73 Save, 74 Return to Title, 75 Quit
  Game. **Item 71 opens `uGUIMenuOption`, which is the in-game Options
  page** (not a HUD page); its modes are 0 Options, 1 Auto Accept & Reply,
  2 Squad Settings, 3 Recipient Settings.
- In Options (mode 0) the setting-name list (`+10504`) and the value list
  (`+10608`) have 10 rows each (`DuplicateNum` 10); the 5 tab icons are a
  fixed 5-row list (`+10528`). Long tabs show pages of 10 ("1/2").
  Switching tabs changes no unit and no list size.

## The Options row tables (confirmed, static and live)

`uGUIMenuOption::fillTabPage(page, tab, pageIndex)` (0x141E82780) sets the
tab title (message category 12, id `tab + 1`), picks the tab's row table
and copies the visible rows of the current page (10 per page) into
`this+10664`:

- **`OptionTabTables` (0x143FBFE90, in `.data`, writable):** 7 pointers;
  [0..4] the five tabs of mode 0 (Game Settings, Controls, Camera, Display,
  Audio), [5..6] the Accept / Reply tables of mode 1. Modes 2 and 3 use
  fixed tables (0x1431BE438, 0x1431BE848).
- **Row** (`cmfm_OptionRow` in IDA, 40 bytes): `int kind; int labelId; int
  flag; int helpCat; int helpId; int settingId; int valueCount; int pad;
  const int* valueLabels;`. `kind` -1 ends a table. Texts are category 12
  (ids above 10000 are category 23, as in `GetGameMessage`); help texts are
  `helpCat`/`helpId` (18, 21 or 23). Without `valueLabels`, value texts are
  `labelId + 1 …`; some PC rows use a value-array format not decoded yet.
- **`kind` = where the setting lives** (from the live map): 0 game options
  (Autosave 12, HUD Display 129, Minimap 0, Damage Indicators 30, …), 1
  system settings (subtitles, volumes, brightness), 2 PC graphics settings
  (the Display tab). Rows with no values are sliders or open a page of
  their own (HUD Display 129, Keyboard Settings 130, Graphics Settings).
- `OptionRow_isVisible` (0x140EEB090) hides rows per platform or state.
- Tab sizes: 17, 16, 24, 18, 9 rows. The full map with names and values is
  `local/runs/20261011-gui-inspect/option_rows.txt`.

**Consequences:**

- **A "Mods" row** needs no code patch to appear: point one tab's entry in
  `OptionTabTables` at our own copy of its table with one extra row (and put
  the original pointer back on unload). Its text and its action (opening
  our page, like HUD Display does) still need the decide path traced.
- **The DLSS mod's AA lockout:** the Display tab (table 3) holds
  "NVIDIA DLSS" (row 9, kind 2 setting 2), "DLSS Sharpness" (10, setting 3),
  "FidelityFX CAS + Upscaling" (11, setting 13) and "FidelityFX Sharpness"
  (12, setting 14). The same table swap can hide or mark them while the mod
  owns AA.

## The Start menu's System tab (confirmed, static; decision 8)

- **Where the entries come from.** Two tab lists in `.rdata` (read-only;
  section flags R--): `StartMenu_TabList_cmfm` 0x143481870 and
  `StartMenu_TabList_InQuest_cmfm` 0x143481820, each 5 x `{u32 tab label id
  (category 11), pad, item table*}`, pointing at the same item tables in
  `.data`. The System tab's table is `StartMenu_SystemItems_cmfm`
  0x143FC0E40: items 71 Options, 10 View Mode, 72 Steam Store, 73 Save,
  74 Return to Title, 75 Quit Game, then the 76 end marker. It cannot grow
  in place: the View Tutorials submenu table (0x143FC0EB0, referenced by
  address from code) follows it.
- **Who reads the tab lists:** `uGUIStartMenu::hoverCategory` (0x141ECAAD0,
  fills a tab: pointers to the visible items go into 11 slots at
  `menu+10696`, padded with the end marker, then rows are shown, labelled
  and greyed out), `uGUIStartMenu::setupTabs` (0x141ECCE30, tab icons and
  names when the menu opens) and `uGUIStartMenu::updateFrame` (0x141ED0810,
  every frame while open).
- **Confirm path:** list callback -> `uGUIStartMenu::pressButton(menu,
  slot)` (0x141ECC8B0): if the menu is active and `isItemAvailable(id)`,
  store the slot (`+10588`) and call `onItemAction(menu, id)`.
- **An unknown item id (77, say)** is visible (`isOptionVisible` default
  true, in and out of quests), available (`isItemAvailable` default true),
  has no badge (`isItemVisible` default false) and is labelled from category
  11 by its label id (`setItemLabel` 0x141ED1BF0; ids 6, 7, 8, 43, 54, 74,
  75 use category 23, 72 category 20, 56 category 4). But **the action
  switch's default opens the Item Pouch** (`sMhGUI::open_uGUIItemPouch`),
  so our id must be intercepted before `onItemAction` runs. 76 is the end
  marker and must not be used.
- **The help line** is written by 0x141ED2300 through the help-text
  function 0x141AE0E60 (not traced yet).

**What a System-tab "Mods" entry needs (design, not tested):**
1. The entry: a System table with `{77, label, help, 0}` after Options. The
   cleanest way is to point the System slot of both tab lists (2 qwords in
   `.rdata` at 0x143481868 and 0x1434818B8) at our own table; all three
   readers then agree. That is a write to read-only game data, which AL has
   no service for yet.
2. Its texts: a label and help id whose strings are ours (a hook on
   `rGUIMessage::getMessage` for those ids, or another way to add strings).
3. Its action: a hook on `pressButton` or `onItemAction` that handles id 77
   (opens the CMFM page) and passes every other id on.
4. The CMFM page itself (registry, later).

Whether the game's code check covers `.rdata` or the hooked functions is
not known (and is not something we reverse); a long play session with the
change in place will show whether it is stable.

## Native popups (from SharpPluginLoader, verified on the clean image)

SharpPluginLoader (MIT; the user's local clone at
`D:\mh world modding\base body\plugin\reference\SharpPluginLoader`, commit
1d57554; AL keeps its notice in `third_party/notices`) wraps the game's
messages in `SharpPluginLoader.Core/Gui.cs` and
`mhw-cs-plugin-loader/GuiModule.cpp`, with patterns in
`Assets/Common/AddressRecords.json`. Each pattern matches exactly once on
the clean image (target = match + offset):

| SPL record | Target | Call (from SPL) |
|---|---|---|
| `Gui:DisplayPopup` | 0x141AE33D0 | `(sMhGUI*, const char* msg, float seconds, float delay, bool, float xOff, float yOff)` |
| `Gui:DisplayMessage` | 0x141A540D0 | chat: `(sChat*, const char* msg, float delay, u32 0, bool important)` |
| `Gui:DisplayMessageWindow` | 0x141AE3EF0 (= Fexty's `sMhGUI::DisplayMessageWindow`) | `(sMhGUI*, const char* msg, 0, float* offset, bool)` |
| `Gui:DisplayYesNoDialog` | 0x141AE2050 (= Fexty's `sMhGUI::DisplayYesNoDialog`) | `(sMhGUI*, request*)`; SPL builds two 0x48-byte elements, the second a callback object with a dialog vtable whose slot 2 receives the answer |
| `Gui:LoadDialogVTable` | 0x141ECE597 (inside `uGUIStartMenu::onItemAction`) | rel32 to the dialog callback vtable SPL copies slots 3-5 from |
| `Gui:DisplayAlert` | 0x1418D6630 | `(const char* msg)` |
| `Chat:MessageSent` | 0x14239EA20 | SPL hooks it; this is one of the 29 hooked spots in the old glavenus image, so that dump had SPL loaded |

SPL has nothing on the game's menus themselves (Start menu, Options,
registry, text injection); its own menu is an ImGui overlay. Its
`game_functions.h` hard-codes addresses from an older game build; only the
pattern records apply to 421810.

## SPL's Start menu API (found after prototype 1)

SharpPluginLoader gained `SharpPluginLoader.Core/UI/StartMenu.cs` on
2026-09-20 (commit e92003a "Implement start menu API", Fexty), after the
local clone (2026-09-14) we checked first; current master read at 7be2393
(2026-10-08), MIT. `StartMenu.AddOption` takes a page, hub/quest/both,
name, description, disabled description, visible and clickable conditions
and a click handler. It works on the same structures we found:

| SPL record | Target on 421810 | Our name |
|---|---|---|
| `StartMenu:AccessPages` | 0x141ECAB2D | the tab-list access in `uGUIStartMenu::hoverCategory` |
| `StartMenu:FormatOptionName` | 0x141ED1BF0 | `uGUIStartMenu::setItemLabel` |
| `StartMenu:SelectOption` | 0x141ECC8B0 | `uGUIStartMenu::pressButton` (slots at `gui + 0x29C8`) |
| `StartMenu:HoverOption` | 0x141ED2300 | the help-line function |
| `Gui:QueueCommonTextOperation` | 0x141AE0E60 | the help-text queue (SPL passes GMD 11) |
| `StartMenu:IsOptionVisible` | 0x141ECCC00 | `uGUIStartMenu::isItemVisible`: the **"!" badge** check |
| `StartMenu:IsOptionClickable` | 0x141ECF4D0 | `uGUIStartMenu::isItemAvailable` |
| `StartMenu:Close` | 0x141FB55E0 | (closes the Start menu) |

How SPL differs from CMFM's prototype:

- It appends entries at the end of a page (hard-coded original counts 5, 8,
  11, 11, 6); CMFM inserts after a chosen entry (after Options).
- It extends the `cm_start_menu_<lang>` message table once, for the
  language active then (new array from the rGUIMessage allocator); CMFM
  answers for its own text ids in `rGUIMessage::getMessage`, so a language
  change keeps working and the game's resource stays untouched.
- Four hooks (badge, clickable, select, hover) against CMFM's two (text,
  action): unknown ids are already visible and clickable, and the help line
  resolves through `getMessage`.
- **Its "IsOptionVisible" hook is on the badge check**, so SPL's entries show
  the "!" notification icon (`hoverCategory` shows it when 0x141ECCC00
  returns true; the game uses it for Receive Items, Join Requests and the
  like). The real visibility check is 0x141ED03F0, which already accepts
  unknown ids.
- It writes the page pointer under `MemoryUtil.WithRwx` without checking the
  old value; CMFM checks, swaps atomically and restores.

**Compatibility to handle later:** both swap the page pointer. If an SPL
plugin adds a System-page option after CMFM has swapped, SPL copies its
hard-coded 6 entries from our 8-entry table (dropping Quit Game, keeping
Mods) and replaces our pointer; CMFM's restore then refuses (value
changed). If SPL swaps first, CMFM's check refuses and the entry stays
hidden. A later CMFM should detect a foreign table and insert into it.

## Prototype 1 in game (runs 20261011-native-m1, -m1b)

The first attempt refused its own table swap (wrong field) and changed
nothing; after the fix (c65a397) **"Mods" appears right below Options in
the System tab, labelled by our text hook, with our help line, also in
quests; selecting it runs our action** (the Options page as a stand-in);
the other entries work; the original table is restored on quit. Long-session
stability is still to be checked.

## Option values: getter, setter, rows (confirmed, static)

The Options page draws and changes rows through a shared option-row module
(0x140EE8F90-0x140EEC800), also used by the title screen's `uGUIOption`,
the sound page and the camera page:

- **Mode** is set by the open request's functor: `sMhGUI::open_uGUIMenuOption`
  wraps functor `off_143482600` whose call (0x141EC9220) writes
  `unit+10484 = 0` (mode) and `unit+10488 = 1` (opened from the Start menu);
  modes 1 and 3 use functors 0x141EC94F0 / 0x141EC9510. Item 71 is the only
  path to mode 0 (0x141EC7870 is the functor's clone method).
- **Getter** `sub_140EEA350(kind, id)`: kind 1 -> byte `sUserData+256+id`;
  kind 2 -> graphics getters; kind 0 **and any unknown kind** -> byte
  `save+1311704+id`.
- **Setter** `sub_140EEB450(kind, id, value)`: kind 1 -> `sub_141329080
  (sUserData+224, id, value)` plus side effects for ids 11, 16, 25; kind 2 ->
  graphics setters; kind 0 **and any unknown kind** ->
  `cUserData::setHudElementVisible(save, id, value)`, a save-data write.
  Callers: the Options page (0x141E82890), the title `uGUIOption`
  (0x141E89EA0, 0x141E8EF50), the sound page, 0x140EEB860.
- **Row drawing** `refreshRows` (0x141E82170) -> `sub_140EEA6A0(rowInst,
  nameText, valueText, row)`: label from file 12; with values, value index =
  getter, text id = `sub_140EEA620(row, v)`: `valueLabels[v]` (16-byte
  records) or, without them, `labelId + 1 + v` (flag 1) /
  `labelId + valueCount + 1 + v` (flag 0); without values: hidden for kind 0
  ids 77, 129, 130 (rows that open a page), else a number.
- **Enabled** `sub_140EEAE80` and **visible** `OptionRow_isVisible`: unknown
  kinds are enabled and visible. In game the visible check hides kind 2 ids
  0, 1, 2 (NVIDIA DLSS), 6, 7, 10, 12, 13 (FidelityFX CAS): title-only rows.
- **Value help** `sub_140EEB160(page, row, v)`: `valueLabels[v]` +8/+12, or
  `helpCat`, `helpId + v + 1`.
- **Sliders** `sub_140EEA2D0(kind, id)`: slider types only for kind 0 ids
  45-48, 50-53, 87; anything else is not a slider.

### Prototype 2 design: a CMFM view of the Options page (not built yet)

"Mods" opens the same `uGUIMenuOption` page in mode 0 with CMFM's own tab
tables:

1. On item 77: point `OptionTabTables[0..4]` (writable `.data`) at CMFM's
   row tables, set "CMFM view", then run the Options action (item 71).
2. CMFM rows: `kind` 7, `settingId` = CMFM row index, `flag` 1, label and
   value texts in file 12 ids 9100-9999 (value ids follow the label), help in
   file 18 ids 9002-9999 (value help follows the help id); all answered by
   the existing text hook.
3. Hooks on the getter and setter answer kind 7 from CMFM and pass every
   other kind on; without them a kind-7 change would write the save data.
4. While the view is on, the text hook also answers the header (file 12 id
   0, "Options" -> "Mods") and the tab names (ids 1-5).
5. A hook on the page's close (0x141E80D40) and on item 71 restores the
   original tab tables and ends the view.

Hooks then: text, action, getter, setter, close (5). Sliders, read-only
lines and rows that open sub-pages come after.

### Prototype 2 in game (runs 20261011-native-m2, -m2b)

- **The Mods page works**: header "Mods", tab "CMFM", the test rows with
  their values, our help lines, value changes (logged by the setter hook),
  the game's tables back on close, real Options unaffected.
- **Every tab needs a row.** The Options list's cursor callback
  (0x141E806B0) reads the row in slot 0 (`page+0x29A8`) at +0x10 without a
  null check; an empty tab crashed (m2). The tab switch (0x141E80990)
  resets the page counter and the cursor to row 0, so one row per tab is
  enough. OptionsModel gives empty tabs a one-value placeholder row.
- **Set To Default resets everything.** The page's Set To Default answer
  (0x141E800A0, a closure with the page at +8) resets, in mode 0, all game
  options in the save (`sub_141358130(save, 1)`) and all system options
  (`sub_1413289C0(sUserData+224, 0, 0)`), then re-applies graphics settings
  4 and 5; mode 1 resets the Accept/Reply group (`sub_141358130(save, 2)`).
  Pressed in the Mods view (m2b) it reset the player's real options. A
  sixth hook now answers it in the view: Yes resets CMFM's rows only and
  redraws through `refreshRows` (pattern `40 56 41 54 41 57 48 83 EC 40`).

## The title screen (decision 8, second entry point; static)

**Title menu** (`uGUITitle`, layout `ui\title\title`, vtable 0x1434870E8):
code-driven, not a table. Setup (slot 74, 0x141F00600) takes
`btnlist_menuList`, sets its `DuplicateNum` to 7 directly and attaches
callbacks: selected 0x141EFF480 (row index; e.g. the row stored at
`+12332` opens a Yes/No with text 23/0x171), back 0x141EFF3C0, cursor
0x141EFF690 (row -> item: 0->0, 1->2, 2->3, 3->4, 4->5, 5->6 under a
condition), buttons 0x141EFF750. `uGUITitle::play` (slot 90, 0x141EFFDF0)
writes each row's two texts by row index (texts 13/0, 13/1, file 20 id 7,
...). A "Mods" row below Options means an 8th row and translating row
indices in those callbacks (not built).

Flow: the cursor callback stores an **item id** at `+10488` from the row
(Start Game 0, Gallery 2, Steam Store 3, Options 4, Credits 5, Support 6
when its condition holds); Confirm goes through the button handler, which
acts on that item id; the Quit row is special: the selected callback
compares the row with `+12332` and opens a Yes/No (text 23/0x171). Only
0x141F01800 touches the list directly (a reset of `+560`). A Mods row at
index 4 therefore needs: `DuplicateNum` 8, the cursor mapping shifted (our
row -> a new item id, rows below -1), `+12332` moved to 7, our item id
handled where Confirm acts on item ids (not found yet), and rows 4-7
relabelled after `play()` writes the labels.

**Title Options** (`uGUIOption`, `ui\title\options`): each of its tab
sub-objects (0x968 bytes at `+0x2918`, tab index at `+80`) builds its list
in 0x141E8DDD0 from **`off_143FC0480[tab]`** (`.data`, writable; 6 pointers:
0x1431BEEE0, 0x1431BE870 = in-game Display, 0x1431BEB70 = Audio,
0x1431BED00, 0x1431BF000, 0x1431BE190 = Controls), the same 40-byte rows.
It keeps only **kind 1** rows and kind 2 rows whose id byte is 0-1, 4-7 or
9+ (2-3 conditional, 8 never); kind 0 (save data, no save at the title)
and unknown kinds are dropped. It resizes the list with `setRowCount` and
fills rows with 0x141E8D890; values go through the same getter and setter.

**Who acts on the title menu (run `20261011-title-opener`, x64dbg hardware
breakpoint, user's OK):** the selected callback sets the title's state to 8
for every row but Quit; its state machine (`uGUITitle::updateState`
0x141F020F0, slot 82) then closes the title (`uMhGUI::requestClose`
0x141FB7CC0: state |= 0xA0). The **title scene** (`TitleScene::updateMenu`
0x140261C50; title unit at scene+216, uGUIOption unit at scene+312) waits
in its state 1 for the title to close, copies the item (`unit+10488`, read
at 0x140261F09) to scene+120 and goes to state 12, which switches on the
item: 0 start game, 1 (scene transition 7), 2 (state 2), **3 Options ->
sub-flow scene+116 = 9**, 4 -> 25, 5 -> 23, 6 -> 22. On PC (`title+11720`
set) the item equals the row index. `TitleScene::updateFlows` (0x14025E0D0)
runs flow 9: step 0 `uMhGUI::requestOpen(uGUIOption unit, 0)` (0x141FB7CF0:
state |= 0x10, parent at +8448), step 1 wait for closing/closed
(state & 0x22), step 2 `0x141E8BFE0(unit)` then flow 12.

**Design for the title (built as prototype 3, below):**
- *Entry:* an 8th row (`DuplicateNum` 8). Hooks on the cursor and selected
  callbacks translate the row: rows 0-3 unchanged, row 4 = Mods (acts as
  row 3, Options, after entering the title view), rows 5-7 -> 4-6 (Quit's
  check against `+12332` = 6 then still matches). Rows 4-7 relabelled after
  `play()`; Mods' help line set through the text hook.
- *Page:* entering the title view swaps `TitleOptionTabTables[0..4]`, makes
  the text hook answer the header and category labels, re-runs
  `uGUIOption::buildCategories` (0x141E8D5A0, `(unit+10488 object, unit)`)
  and trims the category list to 5 rows; the game's own flow 9 then opens
  the page. Leaving it (the page's close, or flow 9 step 2) restores the
  tables and rebuilds again.

**Title Options layout (run `20261011-title-inspect`, read-only):** a
category list on the left (`btnlist_menu`): Interface, Display, Audio,
Language, Privacy, Key Config, Default; the highlighted category's rows on
the right. `sub_141E8D5A0` (the +10488 object's vtable slot 5, called from
setup 0x141E8B360) builds it: names `settings`, `display`, `audio`,
`language`, `display_G`, `controls`, `default`, label ids 392, 347, 348,
349, 395, 10212, 350; `setRowCount(6 + Key-Config-condition)`; then
`sub_141E8DFE0(sub-object, gui, tab, name, ...)` per category, which finds
`btnlist_<name>` and fills it from `off_143FC0480[tab]` (0x141E8DDD0).
Live: sub-objects 0-4 = tabs 0-4 with 6, 12, 9, 3, 2 rows (Interface:
HUD/Help Text Size...; Display: Display Output Settings...; Audio: Voice
Volume...; Language: Text, Voice, Subtitles; Privacy: Privacy Policy, Play
Data Collection/Analysis); sub-objects 5-6 have no list (Key Config,
Default are actions). **The lists are built once, at setup**, so a CMFM
view must re-run the category builder after swapping the tables (and again
after restoring), and trim the category list to 5 rows so Key Config and
Default (a full reset) are out of reach.

**Consequence for CMFM rows:** use **kind 1 with setting ids 100-199**
instead of kind 7. Both pages keep kind 1 rows, draw their values through
the generic path (value count > 0), treat them as enabled and visible, and
the getter/setter hooks answer them first. Real kind-1 ids are 0-34 (system
settings in `sUserData`); a missed hook would write `sUserData` out of
range, so the view stays off unless every hook is active (as now).

### Prototype 3: what the build needed (confirmed, static)

- **`uGUITitle::play` (slot 90, the open handler) builds the menu by code**:
  row 0 Start Game (13/0); row 1 Gallery (13/1) only when a save condition
  holds (`+11720` = 1, `+12240` = its row, else -1; on PC with Gallery the
  item equals the row); then Steam Store (file 20 id 7, `+12244`), Options
  (13/0xF, `+12248`), Credits (13/0x66), Support (13/0x5F), Quit (23/0x170,
  `+12332`). Every row first gets `sub_140B732A0(row, 1, 0)` (flag 2 at
  `+1576`), flag 0x10 (enabled) and `setPlay(row, 1)`; the rows after Quit
  get flag 2 cleared and `setPlay(row, 0)`. So with `DuplicateNum` 8 the 8th
  row is hidden unless CMFM shows it. `+12332` is read only by the selected
  callback; `+12236..+12248` (four rows) only by `sub_141F023E0`, which does
  nothing on PC (`switchD_142af6b01::caseD_0` returns 0).
- **`cGUIInstButtonList::setRowCount` rebuilds the row copies but keeps the
  cursor** (`+628`), and `setCursor` (0x140B76190) reads the row under the
  old cursor unchecked. Each title Options tab object holds 24 row objects
  (96 bytes from +96; constructor 0x141E87F80: vtable, zeros, tab 7 at +80,
  -1 at +88) that the page iterates in full every frame (`sub_141E8EC90`)
  and on open (`sub_141E8EB40`), using rows beyond the new count. Before a
  rebuild CMFM sets every cursor to row 0 (while the old rows still exist)
  and resets the row objects to the constructor's values.
- **24 rows per tab** on the title page: `fillCategoryRows` collects rows in a
  24-entry stack array without a bound check.
- **The page header** (`a_title_options` / `msg_title`, 12/0) is written once
  at setup (0x141E8B360); CMFM writes it with `formatText`. The gear next to
  it is placed by the header's width at setup.
- **The help line while a category is highlighted** comes from
  `sub_141E8C8F0`: file 18 ids 852, 782, 783, 784, 859 (tabs 0-4), 10123 (Key
  Config), 10215 (Default). A row's help is its own `helpCat`/`helpId`.
- `buildCategories`' Key Config condition (`switchD_142af6b01::caseD_7`)
  returns 1: always seven categories on PC.

### Prototype 3 in game (run 20261011-native-m3)

- **The title Mods entry and page work**: the menu reads Start Game,
  Gallery, Steam Store, Options, **Mods**, Credits, Support, Quit; Mods opens
  the title Options page as "Mods" with five categories (CMFM, four "-"),
  the test rows, value changes, close; Key Config and Default are gone;
  Options afterwards shows the game's page again. No crash (the exit report
  was the Stracker baseline).
- With the cursor on a category the help line showed the game's category
  description (852 etc.); fixed afterwards by answering those ids (run
  `-m3b`: correct).
- Run `-m3b` showed the game's "VRAM Usage" panel on the second category:
  `sub_141E8C8F0` (showCategory) plays `unit+0x7230` (`null_vram`) for
  category 1, its only switch. A sixth title hook hides it after
  showCategory while the Mods page is on.
- Back to the title from the game (a new uGUITitle unit): the Mods row and
  page work again (run `-m3b`).

## Scrolling and a page of CMFM's own (study, 2026-10-11)

**The game's scroll controller** (`cGUIFixedScrollList`, embedded in every
`cGUIInstButtonList` at +736, attached at list init; enabled as a game menu
does at 0x140556030: +744 on, +852 visible rows, +856 total, `0xB65B20`
(scroll, owner, callback), +745 and the list's wrap +656) slides rows during
a scroll and expects a clipped list with spare rows; on the title page's
plain category list it drifted (run `20261011-scroll-1`). CMFM keeps its own
window instead: a hook on the list's cursor move (`0xB74060`, vtable slot 28
via a thunk) turns Down on the bottom row / Up on the top row into a window
move; the wheel comes from al.input's window messages and is applied in
`uGUIOption::update` (slot 81, `0x1E8EF50`; +10480 is 0 while the category
list has the focus). Works (run `20261011-scroll-2`).

**GUI layout files** (`.gui`, `rGUI`): `rGUI::Deserialize` (0x1422A70B0)
checks magic `GUI\0` and version 0x23316 and reads the whole file;
`sub_1422A7310` turns the header's section offsets (u32 in u64 slots,
0xA8..0x190) into pointers and fixes up every record table. Counts at
0x28..0x64; design size at 0x98/0x9C (1920x1080). Class ids are MT DTI ids,
`(~crc32(name)) & 0x7FFFFFFF`. `tools/re/gui_dump.py` prints a file:
animations (templates such as one settings row), sequences, the parts
(`cGUIObj*`) and the instances (`cGUIInst*`). `ui\title\options.gui` holds
**seven** panels with row lists (settings, controls, camera, display, audio,
language, display_G); `uGUIOption`'s code, not the layout, keeps the page at
five (category rows 5 and 6 are Key Config and Default in code).

**Pages and their lifetime:**
- A class description (MT DTI, e.g. `uGUIOption::DTI::Instance`
  0x14502B370): vtable (slot 1 creates an object: `sub_141E87060` allocates
  0x7810 bytes with the DTI's allocator and runs the constructor
  `0x141E88400`), name at +8, parent at +0x20, size/flags at +0x30, id at
  +0x34. Registered by `MtDti::ctor(dti, name, parent, size, ...)`.
- `sMhUnit::newUnit(sMhUnit, dti)` = `dti->vtable[1]()`.
  `sMhUnit::addBottom(sMhUnit, line, unit, group bits)` (0x142219070) puts
  it on a move line (GUI pages: line 20, bits 0x40000000).
- The registry (`createGUIObjects`) is not the only way: **the title scene
  (`aTitle`, `sub_14025CF70`) creates its pages itself** with newUnit +
  addBottom(20) and keeps tracked references to them (scene+216 the title
  menu, scene+312 the Options page); its destructor (`sub_14025B670`) only
  drops its references. Group 0 (`sub_141ADC8A0`, once) holds the common
  pages alive at the title and in game (units get +20 |= 8).
- **Tracked references** (corrected, earlier "reference-counted"): a 24-byte
  slot `{unit, next, prev}` linked into the unit's list (head +208, count
  +216). `setRef(lock, unit, slot)` (0x142229160, IDA's
  "InterlockedExchange64") unlinks the old unit and links the new one;
  `clearRef(lock, slot)` (0x1422291D0); the lock is a global critical
  section (`sub_142229230`); setRef does nothing while the byte at lock+0x2C
  is set. The unit's destructor (`sub_1422283C0`) zeroes every slot in its
  list, so a reference turns null when the unit dies.
- **Who kills pages:** `sUnit::kill` (`sub_142219370`, via `cUnit` slot 18)
  refuses units with `+20 & 8` and otherwise sets state 16 and queues the
  unit; state `+12 & 0xE` is non-zero while alive (2 new, 8 running). A
  scene's units list (`sub_1411D8830` adds a unit and sets `+20 |= 8`;
  `sub_1411D8880` clears the flag and kills them). **At the title scene's end
  (`sub_14025C710`, aTitle slot 8)** the game kills every unit on every move
  line except flagged ones (`sub_14221AC20`) and drops unflagged units from
  sMhGUI's list (`sub_141ADE330`). So a page made without the flag dies with
  the title scene, like the title's own pages.
- **A new page sets itself up:** a unit in state 2 gets `cUnit::invokeSetup`
  on its line's next pass: uMhGUI slot 6 (`sub_141FB4540`) loads the layout
  named by slot 71 (`sMhResource`, `rGUI`), slot 5 (`sub_141FB7ED0`) adds the
  unit to sMhGUI's list (+608, `sub_141AD5F40`), which draws it and gives it
  input; then the page's own setup (slot 74).
- **The title scene's slots:** 6 `sub_14025CF70` (makes the pages), 7
  `TitleScene::updateFlows` (every frame), 8 `sub_14025C710` (end). Flow 9
  touches scene+312 only through its pointer: step 0 requestOpen, step 1
  polls `+8856 & 0x22`, step 2 `sub_141E8BFE0(page)` (did system settings
  change: 32 kind-1 values, a few floats; yes -> flow 12, no -> flow 13).
- `uGUIOption::close` (slot 91) looks for the uGUIOption unit by class id
  only when its display or language values changed (+30503/+30504,
  +30524/+30525); otherwise `sub_141ADB5D0(sMhGUI, page)`.
- uGUIOption is also in registry group 5 (0x143F9E460), so a uGUIOption unit
  exists in game too (for step 2).
- Class descriptions: MtDTI is 0x38 bytes (vtable, name, next sibling, first
  child, parent, class-table chain, flags = size/4 in the low 23 bits plus
  allocator bits, id = crc32 without the final inversion & 0x7FFFFFFF). Its
  vtable has 4 slots (0 delete, 1 new object, 2 construct in place, 3
  construct an array). uGUIOption's create (`sub_141E87060`) allocates with
  uGUIOption's own class description, whatever DTI it is called with. The
  uGUIOption vtable has 93 slots and no RTTI pointer before it.
- A page names its layout through vtable slot 71 (`uGUIOption`:
  `ui\title\options`).
- `uMhGUI::requestOpen(unit, opener)` sets state 0x10 and stores the opener
  at +8448; on close (slot 87 `0x141FB89B0`) the opener gets vtable +160
  with a "child closed" message (`off_143499310`). The title scene passes no
  opener and polls the page's state instead (flow 9).

**A page of CMFM's own (probe P1, built, title only;
`src/native/page_class.cpp`):**
- Class `uGUICmfmPage` (id 0x0DBC4ADA, no collision among the game's 24,051
  ids), a child of uGUIOption: a copy of uGUIOption's class description with
  CMFM's name, id and a vtable whose slots 1-3 run uGUIOption's own and then
  give the object CMFM's vtable. Not entered in the game's class table.
- The page vtable is a copy of uGUIOption's 93 slots with getDti (4),
  update (81), open (90) and close (91) replaced; CMFM's handlers call the
  originals. The copy is made at WIN_MAIN after checking that slot 4 returns
  uGUIOption's class description and that slots 81/90/91 hold the functions
  their patterns find. Layout still `ui\title\options` (slot 71 unchanged).
- Made in a hook on `TitleScene::updateFlows` (the scene's own update, the
  same context in which the scene makes its pages) when CMFM has no live
  page: create, `addBottom(units, 20, page, 0x40000000, 0)`, tracked with
  CMFM's own reference slot. It dies with the title scene; the next title
  makes a new one.
- When "Mods" started flow 9, the same hook puts CMFM's page into the
  scene's reference pointer (scene+312, the pointer only; the links stay
  the game's) for the length of each update while flow 9 runs, then puts the
  game's page back. The game's flow code opens, polls and checks CMFM's
  page; the game's Options page is never opened, swapped or rebuilt.
- On open, CMFM fills the page as before (prototype 3 and the scrolling
  window), with `TitleOptionTabTables[0..4]` pointing at CMFM's tables only
  during each build (the game reads them only in fillCategoryRows). If that
  fails the page asks to close (requestClose).
- Hooks: title menu play, cursor, selected; title scene flows; Options
  showCategory (CMFM's page only); list moveCursor (CMFM's list only). The
  uGUIOption open/close/update hooks are gone.

Tested in run `20261011-own-page-1` (tag own-page-1).

**The Start menu and pages in game (study, step 2):**
- **How the Start menu opens Options** (onItemAction case 71): `+0x2BA8 = 76`
  (no dialog; done for every item), `sMhGUI::open_uGUIMenuOption(sMhGUI,
  &menu, &menu, 0)` pushes an open request into sMhGUI's queue (+0x8F68,
  processed in `sub_141AD9D20`: when the predicate functor allows,
  `requestOpen(unit, opener)` then functor 1 with the unit), then pushes a
  **return record** on sMhGUI's return stack (+952, count +5176, 128-byte
  records built by `sub_141AE90E0(record, menu, child)`) whose functor
  (`off_143482660`, `sub_1410A3B00`) is `requestOpen(menu, 0)`; the top
  record fires on the first frame the child's state machine is in state 3
  (closing: `+7428 == 3 && +7424 != 3`). Then `menu+0x29BC = 1` (byte),
  `requestClose(menu, 0)`, `menu+0x2960 = -1`.
- **Why CMFM cannot reuse that path:** functor 1 for uGUIMenuOption
  (`sub_141EC9220`) writes `unit+10484 = 0` and the byte `unit+10488 = 1`;
  on a uGUIOption, +10488 is the categories object's vtable pointer.
- **Menu mode:** sMhGUI computes per frame `+83664` = Start menu open or its
  `+0x29BC` set; sMhKeyboard::handleInput and other input code read it, and
  sMhGUI's update (`sub_141AE0300`) then locks player input channels 4, 1
  and 5 (`sub_141B16F80(input manager, channel, player)`, a per-frame mask).
  The menu clears `+0x29BC` when it opens again. **That alone does not hold
  the hunter** (run `20261011-own-page-2`): the Start menu locks every
  channel in its own update while open, and the in-game Options page locks
  channel 1, plus channel 2 when the Start menu opened it (+10488, set by
  the open functor), and sets sMhArea's menu bytes (`+0xD31C = 0x0101`, as
  every game menu does each frame), in `uGUIMenuOption::update`'s open state
  (RVA 0x1E82596). CMFM's page does the same in its update (slot 81, state 2
  only) while the Start menu opened it.
- **Close timing:** `sub_141FB55E0` (begin close) sets `+7428 = 3` and calls
  slot 91 (the close handler) on the same frame, the frame the return record
  fires. Slot 72 (force close, used for kept pages at a scene change through
  message 75) runs begin-close and then slot 87 (closed), so slot 91 runs
  there too. At the title scene's end the game kills the units first (the
  Start menu is then no longer alive) and sends message 75 after.
- **Opener message:** slot 87 sends the opener (+8448) message 68; the
  generic uMhGUI handler (slot 20, `sub_141FB7D30`) only acts on 75 (scene
  change: drop the opener, force-close kept open pages).
- **uGUIOption's open handler** (`sub_141E8AF60`) links two pages through
  sMhGUI's link queue (+20192/+36704): the first `uGUIKeyboardConfig2` in
  sMhGUI's list -> page+30720, then `sub_140549220(it)` copies the saved key
  config into it; `uGUIKeyConfigMySetPc` -> page+30728, `sub_14054EC50(it)`.
  Neither functor checks for null, so the page must only open where both
  exist: the title (its own pages) and in game (registry group 1, indexes 94
  and 95, created with the Start menu).
- All GUI classes allocate from allocator 27 (class flags), the common pages
  of group 0 included, so a page kept for the session is ordinary.

**Step 2 design (built; run `20261011-own-page-2`: page correct, hunter moved, fixed as above):** CMFM's page is made once, at the
first title, and marked kept (`+20 |= 8`, like group 0), so the title and
the Start menu share it. Start menu > Mods (item 77): if the page is
running, CMFM does what case 71 does to the menu with its own page (no
open-request queue, no return record): `+0x2BA8 = 76`, `requestOpen(page,
0)`, `+0x29BC = 1`, `requestClose(menu, 0)`, `+0x2960 = -1`, and keeps a
tracked reference to the menu. The page's close handler opens the menu again
(`requestOpen(menu, 0)`) while that menu is alive and still has `+0x29BC`
set. Without a page the old in-game view (uGUIMenuOption with CMFM's rows)
is the fallback. Code: `src/native/mods_page.cpp` (the page's content and
both entry points), `src/native/gui.cpp` (shared GUI helpers),
`title_menu.cpp` (title row and scene flow only).

**A layout of CMFM's own (loading probe, run `20261011-layout-1`, tag
layout-probe-1):**
- The game reads loose files under `nativePC` (sMhMain::ctor registers the
  `nativePC` / `app:\nativePC` root next to the chunk archives); the user's
  MO2 file mods ship `nativePC/...` at the mod root. CMFM's layout is
  `nativePC/ui/cmfm/mods.gui` in its mod folder, resource path
  `ui\cmfm\mods`.
- `sMhResource::loadResource(resources, class, path, flags)` (vtable slot
  11, 0x1422228D0) returns nullptr when the file is missing (flag 1 = load
  from file, as uMhGUI's layout load in slot 6 uses; flag 0x10 = only if
  already loaded). CMFM loads its layout once before making the page and
  only then points slot 71 at it (one extra reference for the session).
- Format details used by `tools/gui/gui_edit.py`: animations (+4 u16
  instance count, +12 first instance, +16 name), instances (+4 u8 parameter
  count, +16 name, +24 class id, +32 first parameter), parameters (32 bytes:
  +0 type, +16 name, +24 value). Named parameters such as `PosX`, `Size`,
  `FontSize`, `VertexColorLeft`, `Visible`, `Texture`; type 2 (float) points
  into the value pool at slot 0x180 and type 4 (four floats) into the one at
  0x188 (corrected; docs/research/gui-format.md), and one pool entry
  may serve several parameters (the tool checks who shares a value before
  changing it). Animation keyframes (keysets) may override base values.
- Probe edit: the header band's colours (`title_options/tex_base_r` and
  `tex_base_l` `VertexColorLeft/Right`, (32,32,32) -> (180,30,30)): red at
  the title and in game.

**Layout study (2026-10-11, static):**
- **The category cursor is the panel index everywhere:** `uGUIOption::update`
  (slot 81) indexes the tab objects by `categoryList+0x274` in four places
  (hover test on the panel, entering it, its row cursor), and so do
  `sub_141E89CB0`, `sub_141E89E00`, `sub_141E89EA0`, the categories object's
  slots 6-9 (`sub_141E8C4E0`, `C530`, `C680`, `C6E0`), `sub_141E8CDD0`
  (calls showCategory), `sub_141E8EB40` and `sub_141E8EC90`. Seven tab
  objects are embedded at fixed offsets, so a taller category list cannot
  work: every row past the panels would index past them.
- **A free list in the layout:** `options.gui` has a `camera` panel
  (`a_window_camera`, `btnlist_camera`, `cursor_camera`, arrows, number
  input) that the page's code never names (buildCategories uses settings,
  display, audio, language, display_G, controls, default).
- **Panel height follows the rows:** fillCategoryRows scales the panel's
  window (tab+48) by `rows * 1.44` on Y.
- **Sliders:** the row filler (`sub_141E8D890`) takes the row's slider child
  by object index (53 `slider_audio`, else 88 `slider_display`), so only
  rows of the audio and display panels have one. It turns the slider on
  (`sub_141E8E410(row, entry, msg_config, slider, msg_slider_01,
  msg_slider_00, text_config_L)`) only for kind 1 ids 3, 4, 5, 29, kind 0
  ids 45-48 and 50-53, and kind 2 type bytes 3 and 14 (value text
  `%02.01f`); the default branch clears the row flags. Row flags: +84,
  +85 (opens a sub-window: kind 1 ids 6 and 33, kind 2 type 40), +86 (star
  `null_star` shown), +87 (kind 1 id 34).
- **Pop-up list:** `a_window_select` / `btnlist_select` with check-mark rows
  (`menu_language` template), for rows with flag +85; contents from code
  (not studied further yet).

**Split mode and the mouse (runs `20261011-split-1` to `-split-3`, static
study afterwards):**
- Split mode: the layout's camera list, styled like the category list,
  shows every menu entry on the left (CMFM fills and moves it); the game's
  category list sits off screen with its cursor kept on the audio panel
  (2), which shows the highlighted entry. Up/Down on the category list are
  turned into mod list moves: since 0.1.0-beta.3 in the step hooks
  (`0xB75BD0` next, `0xB75C80` previous), which know the direction and keep
  the category cursor. Before, the moveCursor hook compared rows; the
  category list holds min(pages, 5) rows, so with three pages the parked
  row 2 was the last one and Down wrapped to row 0, reading as Up (closed
  beta; the DLSS session's `handoff-cmfm-20261014-dpad`). The moveCursor
  hook still catches other moves, a wrap counted the right way.
- **Why Confirm got stuck (split-2, -3):** a list's update
  (`sub_140B755C0`) in state 1 puts the cursor on the first enabled row from
  its start row (+560) when its opening animation ends. That came one frame
  after CMFM's open handler had set the category cursor to 2, so it went
  back to 0, and Confirm entered tab 0 (a hidden panel) while the shown
  panel stayed inactive. initCategory writes +560 = 0 for panel lists; CMFM
  writes 2 for the category list and the cursor row for the mod list.
- **Input routing (`sub_141FB6A70`, uMhGUI's update):** after the instance
  tree's update, the GUI sets +369 ("input this frame") on the registered
  input lists of the top priority that are shown and not marked inactive
  (+371 == 0, `sub_140B87020`); then slot 81 runs. A list reads keys and
  mouse only with +369 set and +371 clear (`sub_140B872A0`,
  `sub_140B87410`). The page switches focus by flipping +371: focus 0 the
  category list, focus 1 the panel under the category cursor
  (`sub_141E8CDD0`; categories slot 6 and tab slot 6 write +371).
- **The game's mouse on the page:** a list's hover handler (vtable +248,
  `sub_140B76AF0`) moves the cursor to the row under the pointer when the
  mouse moved and +641 is set, and a click on the current row decides;
  right click cancels. Focus 0: the category list follows the pointer, and
  slot 81 enters the panel for a click on its window (tab+48,
  `a_window_<tab>`). Focus 1: the category list takes no input; Back or a
  right click returns (tab slot 9, `sub_141E89AB0`).
- **Helpers CMFM calls:** mouse position `sub_141FB5D30(gui, float[4],
  list)`, hit test `sub_141FB6190(gui, instance, position, int*)`, left
  click `sub_141FB6560`, mouse moved `sub_141FB65A0`; all answer only while
  the GUI has the input (+8897/+8898). Found through the calls in
  `sub_141E8A510` (pattern `kOptionClickPattern`) and `kMouseMovedPattern`.
- **CMFM's mouse (split-4, untested):** slot 81 checks the mouse on mouse
  activity only. Over a mod row: that mod is highlighted, and from focus 1
  the panel's Back runs first (tab slot 9). Moving onto the panel's window
  in focus 0: the category list's Confirm (categories slot 8,
  `sub_141E89C40` -> `sub_141E8A890`) enters it. Both slots are checked
  against their RVAs before each call. Tested in run `20261011-split-4`
  (tag split-layout-1).

**Rows, marks and text (static, 2026-10-11; used by run `20261011-design-1`):**
- A list row's +0x628 (1576) flags: bit 0x2 "active" (the list's moves,
  hover and start row skip rows without it), bit 0x4 greyed.
  `sub_140B732A0(row, on, animate)` flips 0x2 and plays animation 5 (on) or
  9 (off); `sub_140B72F00` / `sub_140B72EE0` set and clear 0x4 (9 = the
  greyed look). The row filler calls `sub_140B732A0(row, 1, 1)` on every
  fill, so a heading made by clearing 0x2 directly (no animation, normal
  look) has to be redone after each fill.
- The row filler (`sub_141E8D890`) finds `text_config_L` (value),
  `msg_config` (label), `msg_slider_00/01`, and `null_star`, a star icon it
  hides unless row +86 is set; nothing in uGUIOption sets +86, so the title
  page never shows it. CMFM shows it as the restart mark
  (`cGUIObject::setHidden`, 0x142343310).
- The greyed state comes from `sub_140EEAE80(row entry, ..., 1)` in the
  row's deactivate (`sub_141E8D200`): false gives animation 9 and text
  colour 7.
- Text style tags as the game's message files write them:
  `<STYL MOJI_YELLOW_DEFAULT>...</STYL>` (also RED, LIGHTBLUE, ORANGE,
  LIGHTGREEN), icons `<ICON STAR_1>`, `<ICON NOTE>`. formatText renders them
  for CMFM's texts as for the game's.
- More tags (from the notes of the mod "Colored Shoutout" by Popyhead/Lyn,
  which the user pointed to, 2026-10-12; the English message files use only
  some of them): `<SIZE n>...</SIZE>` sets the font size (0 to 60; 20 the
  usual default), `<CNTR>` centres a line, `<col ff0000>...</col>` any colour,
  more colours (`MOJI_GREEN`, `BLUE`, `PURPLE`, `LIGHTYELLOW`, `SLGREEN`,
  `BROWN`, `BLACK`, `WHITE`, `WHITE_DEFAULT2` a light grey, `WHITE_SELECTED2`
  silver, all `_DEFAULT`), icons for pad buttons (`PAD_L1`, `PAD_R1`, `PAD_RD`
  ...) and keyboard menu keys (`FORCED_KB_MENU_KEY_L` / `_R`), and these
  characters: ▲ △ □ ○ ◎ ● ■ ∞ ♥ † α ß Γ Ω Φ δ π Σ ¥ £ ¢ and box drawing. CMFM's
  mod list marks mods with pages with `<SIZE 14>` triangles in
  `MOJI_WHITE_DEFAULT2` (run `modlist-2`; the full-size triangle of run
  `modlist-1` was too heavy).
- Panel and header texts: the header `a_title_options` has one text,
  `msg_title`; each panel title `a_title_menu_<tab>` has `msg_sub_title`
  (300 x 50, font 24, centred, AutoScale shrinks long text).
- Visibility in the layout: every `a_title_menu_<tab>` and the row
  template's `null_star` ship with `Visible` 0 (a type-3 parameter: one byte
  in the pool at slot 0x178); `setHidden` cannot show them, so CMFM's layout
  points those flags at a byte that is 1. Panel titles stay off (a title
  sits on the first row's line, under its value box).

**What re-shows the rows' parts (runs `design-3` to `-5`, static):**
- The row text filler (`sub_140EEA6A0`, via `sub_140EEA680`) calls
  `setHidden(text_config_L, 0)` (and on the value text in it) every time it
  writes a row. The tab refresh (`sub_141E8EB40`) runs it for every row,
  then the row looks (`sub_141E8E600`); the tab's cursor handler (tab slot
  10, `sub_141E89CB0`) runs the refresh and then the row looks again on
  every cursor move.
- The row looks replay each row's look (`sub_141E8D200`: animation 0 or 5,
  or 9 greyed), hide the arrows (`num_input`) and set list +375 for rows
  with +84/+85, and play the pick list (`select` +32 and +24) only when the
  cursor row has +86, hiding it otherwise.
- Tab setActive (`sub_141E8C530`) hides each row's star unless +86, and
  shows it only while the tab is active.
- `moveCursor` calls the list's cursor callback (so the row looks run with
  the new row) before it stores the new cursor (+628).
- CMFM hooks the row looks (`kRowLooksPattern`) and redoes its touches
  (headings, stars, sliders, pick rows) right after them, with the row they
  were given; and on every focus change.

**Sliders (design step 2, run `sliders-1`):**
- The row filler makes a row a slider for the game's volume and gamma rows
  (`sub_141E8E410`): row +84 = 1, `sub_140EEAD20(slider, text, entry)`
  (slider +1664 = 32, +1708 = 1, knob at getter * 0.01 through
  `sub_140B8ABC0(slider, float)`), slider +1664 = 2, `msg_slider_00/01`
  shown, the value box hidden. Row object: +40 slider, +64 `msg_slider_00`,
  +72 `msg_slider_01`, +32 `text_config_L`, +16 row entry, +8 the page.
- Every frame the tab update (`sub_141E8EC90`) runs, for rows with +84,
  the slider's mouse drag (`sub_140B8ADB0`) and `sub_140EEB860`: knob
  position * 100 (rounded up) against the getter, the setter when they
  differ. The slider's own update (`sub_140B8A5E0`) moves it by keys and
  stick while +1708 is set (the row look sets it for the cursor row).
- Slider fields: +1640 orientation, +1656 knob (+160/+164 position), +1672
  and +1676 track ends, +1696 steps (positions snap to k / steps), +1708
  active, +1728 dragging. A drag does not snap.
- CMFM: INT/FLOAT rows with 3+ values; steps = values - 1; the option hooks
  map index <-> 0-100 and keep the last raw value (no set every frame for a
  knob between steps); after a drag the knob snaps; CMFM's label goes into
  `msg_slider_00`.

**The pick list (design step 3, runs `picklist-1` to `-3`):**
- The page's select object (+27376; set up by `sub_141E8D350`): +24
  `btnlist_select` (callbacks: decide `sub_141E89B20(select, row)`, cancel
  `sub_141E897D0`), +32 `null_prog_color2`, +40 14 check-mark pointers,
  +152 the setting it holds, +160 `a_window_select` (scale Y = rows *
  1.44), +168 the line's `poly_line_mask` (frame = rows). Rows:
  `msg_language`, `checkMark`.
- Language rows: +85 (Confirm, no arrows) and +86 (the list shows while
  the row is the cursor row: the row looks fill it with `sub_141E8CFA0`,
  which knows only the language lists). Their row decide (`sub_141E8BA10`)
  fills it and sets focus 2; the list's decide applies the language and
  returns to focus 1. The select object's update (`sub_141E8E4A0`, on focus
  changes) hides every check mark in its array for a setting that is not a
  language.
- +86 is not usable for other ids: with +85 the row refresh
  (`sub_141E8E8B0`) reads `[table + 0x100 + 0xFFFFFFFF]` for setting ids
  from 32.
- CMFM: CHOICE rows of 6-14 values (or `CMFM_FLAG_LIST`) get +85 only; the
  decoration fills the list (texts, window, line; its own check marks, kept
  out of the select object's array) and shows it while the cursor is on the
  row; hooks on the row decide (open: focus 2, cursor on the current value)
  and on the list's decide (set the value; the game then closes the list).

**Buttons and locked rows (design step 4, runs `buttons-1` to `-3`):**
- The game's Yes/No question: `sMhGUI::DisplayYesNoDialog(sMhGUI, request)`
  (RVA 0x1AE2050) copies a 0x90-byte request: +0x08 text, +0x10 / +0x14 the
  screen's centre (width and height at RVA 0x3FC0560 / 0x3FC0564, halved),
  +0x20 three floats from RVA 0x503C430, +0x3C kind 2 (Yes/No), +0x48 a
  `std::function` (vtable, capture) and +0x80 a pointer to it. The title
  page's Default (`sub_141E8A890`) builds it with a lambda (vtable RVA
  0x347DF98: copy 0x1E893E0, call 0x1E89600) that captures the categories
  object; its body `sub_141E88AA0(capture*, ?, answer)` resets every option
  on 0 (Yes).
- CMFM: Confirm on a button row goes through the row decide hook. Without a
  question it presses at once; with one it builds the same request with the
  same lambda (vtables checked first) and CMFM's text, and hooks the
  lambda's body: when the capture is CMFM's page (which never asks the
  Default question itself), Yes presses the button and the game's reset
  never runs. After a press the tab refresh (`sub_141E8EB40`) re-reads and
  re-writes every row (the mod may have changed any value or text).
- Greyed rows: the row look (`sub_141E8D200`) asks the option rows' enabled
  check (`sub_140EEAE80(row entry, int* reason, flag)`, RVA 0xEEAE80); false
  plays animation 9 and text colour 7, and the row takes no Confirm and shows
  no arrows. For CMFM's ids the hook answers the page's `locked`; the help
  line gives the reason (CMFM writes it into the row's help).
- Why a locked slider flickered (`buttons-2`): the row look restarts each
  row's animation on every call (`sub_140B73110` plays a sequence without
  checking what already plays). For a slider row it first plays the slider
  (`setPlay(slider, 1)`); for a disabled row it then restarts animation 9,
  which hides the row's parts. Every cursor move showed the slider and hid
  it again. The game never greys a row it drives as a slider.
- CMFM: a locked number row is no slider (`slider_steps` 0): it shows its
  value as text, greyed like any locked row. When locks change, CMFM clears
  +84 on such rows, stops their slider (+1708 = 0, not playing), hides
  `msg_slider_00`, runs the tab refresh (which writes the value text), then
  its decoration; unlocked, the row filler makes it a slider again. The
  option setter hook also ignores locked rows.

**Long pages and scrollbars (design step 5, runs `longpage-1`,
`scrollbar-1`, `-2`):**
- The panel never scrolls: fillCategoryRows sizes its window by rows * 1.44
  and draws every row (24 ran past the bottom of the screen, rows 1-15
  fit). CMFM's panel window (`native/panel_window.hpp`) gives the game at
  most 14 rows of the tab (a table of its own, `g_panel_rows`, with an end
  row; the row objects keep pointers to these entries).
- Keys: the button list's key handler (vtable +232, `sub_140B81060`) steps
  with `sub_140B75BD0` (next) / `sub_140B75C80` (previous) `(list, cursor,
  steps, wrap, bool* wrapped)` over selectable rows (+0x628 bit 0x2, +1560
  != 1), then `moveCursor` (+224) moves to the result. CMFM hooks both
  steps: for the panel list they step over the whole tab and move the
  window (headings right above the cursor come into view). `moveCursor`
  does nothing for an unchanged row, so when the window moves under a cursor
  that keeps its row, CMFM sets the list's cursor field to the row the old
  setting moved to: the game then runs a real cursor change (sound, row
  looks, the tab's cursor handler).
- The tab's cursor handler (slot 10, `sub_141E89CB0(tab, list, row)`): the
  tab refresh, the row looks, the arrows to `row * 52 + 34`, special rows,
  and the row's help line (`sub_141E8EA70`).
- The tab refresh (`sub_141E8EB40`) rewrites only values: per row
  `sub_140EEA680(row, nullptr, text_config_L, entry)` (or the kind-2
  variant), the row refresh `sub_141E8E8B0`, sliders (+84) from their
  setting. The label is written only by the row filler, through the same
  `sub_140EEA680` given `msg_config` (run `scrollbar-1`: the bar moved, the
  labels stayed). A scroll writes every row's label that way.
- Before entries move, CMFM ends every slider row (+84 = 0) and clears +85:
  the per-frame slider sync (`sub_140EEB860`) would otherwise set the new
  setting to the old knob position.
- Scrollbars: CMFM's layout carries the game's track and bar
  (`docs/research/scrollbar.md`, `tools/gui/make_mods_gui.py`). Found by
  name, class checked through getDti; shown with `setPlay` only while their
  list scrolls (setPlay acts only on a change and does not touch children
  when turning on); total, visible and ratio written on every window move.
  Every frame the page calls `cGUIInstScrollBar::updateMouse` (RVA
  0xB89670) for each shown bar: a drag moves the window (the mod list keeps
  the highlighted mod inside it; the panel keeps its cursor row and runs the
  cursor handler when it has the focus); otherwise the thumb is put back on
  whole rows. The bars' own wheel is off (+0x262 = 0).

**The mod's header (design step 6, run `header-1`):** only `initCategory`
(`sub_141E8DFE0`, the one user of the string `msg_sub_title`) writes the
panel titles' text, on every fill; CMFM's layout switches the audio panel's
title on and moves it above the panel, and CMFM writes the header after
each fill, on open and when a restart state changes.

**Fine sliders (runs `fineslider-1` to `-3`):**
- Per-value texts: a row's `values` pointer (+32) is a table of 16-byte
  entries `{text id, text id, help id, help file}`. The value's text id is
  entry +4 (or +0 when -1), else `label_id + 1 + value` (`sub_140EEA620`);
  for rows without +84/+85 the help is entry +8/+12, else `help_id + value +
  1` (`sub_140EEB160`); slider and list rows use the row's help
  (`sub_141E8EA70`). CMFM's number rows share one text (the current value,
  written on every read and change) through a 101-entry table: the text
  filler reads a slider row's text at its 0-100 getter value.
- The slider's set-position (`sub_140B8ABC0`) puts the knob on step
  `floor(steps * position)` and stores it in the step counter +1704; the
  keys (`sub_140B8A5E0`) move that counter one step at a time (thresholds in
  `range / (steps + 1)`); a drag (`sub_140B8ADB0`) moves the knob only. The
  game's slider-from-setting passes `getter * 0.01` (99 * 0.01 -> 98.99999 ->
  step 98) on every tab refresh, and its per-frame sync (`sub_140EEB860`)
  sets `ceil(position * 100)`; with 100 steps both can be a step off.
- CMFM: knobs placed with `(index + 0.5) / steps` after every refresh (from
  the row looks, before the sync) and after a drag, also when +1704 is
  stale; a knob's value is its zone of the track (steps + 1 equal zones,
  the ends included), read by the option setter instead of the game's
  number, which is kept with the value it meant so the game does not set it
  again every frame.

**Rows that hide, and the arrows' own value (runs `hide-1` to `-3`):**
- The arrows over the cursor row (`num_input`, cGUIInstChangeNumInput, tab
  +72) keep a value of their own (+576, count +580); a press sends it plus or
  minus one through the tab's callback (`sub_141E89E00`, which does nothing
  while the tab byte +2400 is set) to the row's arrow handler
  (`sub_141E89EA0`: the option setter, then a tab refresh). The row looks set
  the count from the row (`sub_140B82900`) but the value (`sub_140B82890`)
  only for the game's kind-1 ids below 32, inside +2400 = 1. CMFM's rows
  kept a stale value (hide-1: after a refill, index 0 was never reached).
- `sub_140B82890` sets +576 = -1 before the new value, so it always reports
  a change to the arrows' callback: called without +2400 it ran the setter
  and a refresh whose row looks called CMFM again (hide-2: stack overflow).
  CMFM now sets the value for its rows after every row-looks pass, under
  +2400 as the game does.
- The page's focus setter (`sub_141E8CDD0`, focus 1) closes the pick list's
  parts and activates the tab under the category cursor (tab slot 6); no
  sound, no cursor move. CMFM calls it after refilling the panel for rows
  that came or went, then puts the cursor back on its setting.

**Centred labels (plain button rows, run `plain-1`; static):**
- The game's "Advanced Graphics Settings" row is a label alone: no value
  box, the row itself takes Confirm. CMFM draws its reset row that way: the
  value box hidden as for headings, the label centred and red
  (`<STYL MOJI_RED_DEFAULT>`; light blue in run `plain-1`). Section headings are centred the same
  way (yellow, the cursor skips them; run `plain-2`).
- Text parts are `cGUIObjMessage` (property list `0x14233A920`, getters and
  setters plain field accesses): +232 `FontStyleId`, +236 `Color`, +248
  `Size` (w, h floats), +328 flags (bit 0 `AutoScale`), +332 `Font`, +336
  `Layout` (a byte; low nibble 0 left, 1 centre, 2 right, high nibble the
  vertical, 1 middle; bits 8-11 `LetterHAlign`/`VAlign`, 18-19 `AutoWrap`),
  +348 `MessageId`. The base `cGUIObject` (`0x142338650`): +160 `PosX`,
  +164 `PosY`, +192 `ControlPoint` (bits 0-1 horizontal: 0 left, 1 centre,
  2 right; bits 2-3 vertical).
- The text is laid out once and cached. The layout (`cGUIMessageAnalyzer`,
  `sub_1424DF0C0` reads `Layout & 0xF`, `sub_1424E1CD0` the width) runs
  again when flags +328 bit 0x10000 is set: the `FontSize` setter
  (`0x142343210`) sets it, vtable slot 13 (`0x1423424D0`) clears it and
  calls slot 28 (`0x1423427B0`, re-format and dirty). `sub_142340EC0` gives
  a line's x from the same fields (`(width - line) / 2` for centre, the box
  starting at 0, -w/2 or -w by `ControlPoint`).
- The row template's label (`menu_config/msg_config`) sits at PosX 10,
  `ControlPoint` 4 (left-middle), box 340 x 40, `Layout` 0x10. Centred:
  `Layout` 0x11 and a box 786 wide (to x 796), so the text centres on x 403
  like the mod's header. Row objects are reused as the panel scrolls, so
  `decorate_panel` sets every label back (left, 340) unless it is a
  heading's or a plain button's, and requests the re-layout only when it changed something.

**Key bindings (KEY rows, run `keys-1`):**
- A KEY row is a one-value row to the game (like a TEXT row: its value text
  is the key's name, the getter answers 0, the setter is refused); the row
  takes Confirm (+85 set, as for buttons and pick rows), and the decide hook
  starts CMFM's wait.
- The wait uses `al.input` only: `capture` of every device Astera Loader can
  capture (the game sees idle keyboard, mouse and pads, so the menu does not
  move, close or confirm), `key_down` for the real keys every frame (left
  and right modifiers report as VK_CONTROL / VK_SHIFT / VK_MENU, mouse
  buttons as VK_LBUTTON..VK_XBUTTON2), `pad_state` for B. A key counts when
  it goes down after the wait began (keys held then, such as the Confirm
  that started it, count only after a release). `release` at the end; keys
  still held stay hidden from the game until released, so the bound key does
  nothing in the menu. On focus loss Astera Loader reports nothing captured
  (CMFM's capture stays counted): CMFM ends the wait and releases.
- The help line changes in place twice (the wait's instructions, then the
  row's line again): CMFM requests file 18 ids 9998 / 9999 in turn, both
  answered with the row's current help line, since the help bar redraws only
  when its id changes (as for the mod list's 9996 / 9997).

## A crash: the game's tab show and CMFM's slider rows (2026-10-12)

- Crash `crash-20261012-030509-37104` (build 3e7cd3f, CMFM's page opened from
  the in-game Start menu): access violation at `sub_141E8C6E0+0x186`, called
  from showCategory (`sub_141E8C8F0`), called from the category list's cursor
  callback (`sub_141E89C70`, through `sub_140B76190` / `sub_140B75D20`), called
  from CMFM's page update (`keep_panel_cursor` putting the category cursor
  back on the panel).
- `sub_141E8C6E0(tab, show)` is the tab show, called only by showCategory:
  when the tab's window was hidden it sets the list cursor to 0, plays the
  window, runs the row looks, and for each of the 24 row objects marked as a
  slider (+84) places the knob from the setting's value read directly: kind 2
  types 3 and 14 through their getters, otherwise the byte at
  `sUserData + 0x100 + id` for kind-1 ids below 32 and **index 0xFFFFFFFF**
  for anything else, CMFM's ids from 100 included: a read 4 GB past
  sUserData.
- Fix: CMFM's showCategory hook makes CMFM's slider rows plain rows (all 24
  row objects of the split panel) before the game's function and redoes the
  decoration after it, so the game never places a CMFM knob. The first
  version still crashed (`crash-20261012-034753-49328`, run
  `slidercrash-1`, the page's second opening in game): the tab show sets the
  panel list's cursor and runs the row looks before its knob loop, and
  CMFM's row-looks hook marked the sliders again in between. CMFM now marks
  no slider while showCategory runs (`show_slider` returns false) and
  decorates once it returns. Why the
  category cursor had left the panel is not known (that session's log was
  overwritten; the in-game Options page may switch categories with LB/RB or
  Q/E); `keep_panel_cursor` logs it once per opening.

## The mod list as mods and pages (roadmap 1.2 item 7; run `modlist-1`)

- The select philosophy needs the category list's Confirm (the keys) to go
  to the mod list first. It is slot 8 of the categories object (at
  `uGUIOption + 0x28F8`; its vtable, 11 slots, sits right before
  uGUIOption's at RVA 0x347D9C8): a thunk to `sub_141E8A890(categories,
  category)`, which for categories 0-4 calls the focus setter
  `sub_141E8CDD0(unit, 1)` (the panel takes the focus), 5 opens Key Config
  and 6 the Default question. The page's update (`sub_141E8EF50`) calls
  `sub_141E8A890` directly for a click on the panel, so a replaced slot 8
  catches the keys only.
- CMFM's own page gets a copy of that vtable with slot 8 replaced (as it
  already has its own copy of uGUIOption's vtable, `page_class`); the game's
  Options pages keep the game's. Checked on every open (the object must hold
  the game's vtable with the Confirm expected), put back on unload. Without
  it the mod list selects as its cursor moves, as before.
- The highlight is the mod list's own cursor (`gui_set_cursor`); the hidden
  category list's moves set it, the pointer sets it, the wheel and the
  scrollbar move only the window. Q / E and pad LB / RB are read through
  `al.input` (`key_down`, `pad_state`) in the page's update, as edges.
- **0.1.1: the game's own handling instead** (decisions item 16, players
  found the select philosophy and the hover focus confusing). The game's
  page: with the category list's focus, the pointer moves the cursor and
  the panel shows the highlighted category; a click on the category (its
  Confirm) or on the panel's window enters; with the panel's focus the
  category list takes no input, and Back or a right click returns. CMFM now
  does the same: the highlight shows its row (`ModList::preview`, which
  never opens or folds a dropdown, so no row moves under the pointer); a
  click or Confirm activates (the dropdown opens) and enters in one press;
  the pointer no longer enters the panel by hovering it (the game's update
  enters on a click there) nor leaves it by hovering the list. From inside
  the panel a click on a mod row shows it and the panel keeps the focus (as
  Q / E); the wheel goes to the panel; a drag on the list's thumb gives the
  list the focus back. Without CMFM's Confirm (slot 8 not as expected),
  Confirm only enters; dropdowns then open by a click or Q / E.

## Page tabs (static, 2026-10-20; decisions item 17)

- **The game's tab bars** are fixed sets of drawn tabs: `ui\common\page_tab`
  (a button list `btnlist_LR`, one tab template `auto_tab00`, arrows
  `auto_arrowPrev_tab` / `auto_arrowNext_tab` at x -196 / 196, art for 2,
  3 and 4 tabs: `tab2cover` .. `tab4cover`, `MtabFocus2..4`), referenced by
  about 20 layouts (chara_make, chat_menu, equip_appearance, item_boxSell,
  guild_card, expedition, login, ...); menus also carry their own
  (`btnlist_tab` with `auto_tab00..03` in item_boxSell and chat_menu, two
  in equip_appearance). No text instance in them: the names are part of the
  tabs' art. Not usable for page names CMFM writes.
- **A button list lays its copies out sideways when `Type` (+0x294) is 1**:
  `cGUIInstButtonList::rebuildRows` (0x140B7FEF0) calls
  `cGUIInstAnimation::SetPosX` for Type 1, else `SetPosY(i * DuplicateSpace
  + template y + i * DuplicateButtonSize)`.
- **CMFM's tabs:** `tools/gui/make_mods_gui.py` grafts a copy of the game's
  category list (`btnlist_menu` from options.gui as shipped: the title
  menu's text row, `msg_1`, and its cursor) under the audio panel as
  `cmfm_tab_list` (`cmfm_tab_row`, `cmfm_tab_cursor`): Type 1, 4 tabs of
  190 px 10 apart, at the panel's x -12, y -30 (above its frame), the cursor
  scaled to 19 x 5 (36 x 5 spans the category rows' 360 px). The panels
  (`null_prog_color1`) move from y 212 to 262 and the mod's header from
  -52 to -102 under them, so it stays level with the restart line. CMFM
  finds the list by name (same vtable as the category list), reads its
  window from DuplicateNum once, keeps it inactive like the mod list and
  fills it: the shown mod's pages, the shown one light green under the
  cursor, labels centred in 190 px (logged once as found), hidden by
  setPlay for a mod with one page. Clicks are hit-tested per tab row (the
  last row hit wins, in case a row's art is wider than its tab).
- **Run tabs-2** (the demo's 12 pages): the window, the « » marks and the
  cursor worked; the title row's text (about 28, laid out 0x10 in 400 px)
  ran into the next tabs. Now `<SIZE 22>` and the text part's AutoScale
  (flags +328 bit 0; the layout, `sub_1424E0B30` -> `sub_1424E10F0`, scales
  each line wider than the box down to the box, never up).
- **Hover, wheel, scrollbar** (the user's wish): over a tab the list's
  cursor follows the pointer (either focus); off the tabs it returns to the
  shown page, or is hidden (`setPlay` on `cmfm_tab_cursor`) while the shown
  page is scrolled out of the window. The wheel over a tab or the tab
  scrollbar moves the window by one per notch. The scrollbar is a third
  graft of the game's pair, `cmfm_tab_scroll`, made horizontal as in
  `equip\upgrade_tree` (`ScrollType` 0, `Rotation` -90 on track and bar),
  12 px thick (`SclX` 0.75), track 790 px from x 8, its lower edge at y -3;
  the tabs moved up to y -42 and their cursor to 42 px high for it. Seen
  in run tabs-3: in place, the thumb right, hover and wheel as intended.
- **The hand pointer** (static): the pointer is `ui\pc\cursor_pc` (one
  animation), driven by `sub_140507200`, which plays sequence 6 or 4 (hand
  or arrow) by `sMhGUI +83877`. Each frame `sub_141AD9AD0` (from sMhGUI's
  update, `sub_141AEAF30`) copies the 32-byte block at `+83820` to `+83852`
  and clears it; pages set `+83845` (so `+83877` the next frame) while the
  pointer is over something clickable: about 75 places, among them the
  list hover `sub_140B766D0`, the scroll bar's mouse, the Options page's
  update and its click check (whose CMFM pattern holds the write
  `C6 80 85 47 01 00 01`). CMFM's lists take no input (inactive), so the game
  never sets it for them: CMFM sets `+83845` in the page's update while the
  pointer is over a tab or a mod row (focus 0 or 1).
- **Menu input** (static): a list's input (`sub_140B87410`) asks the GUI per
  menu key: pressed `sub_141FB6960(gui, key, list)`, repeated
  `sub_141FB6860`, held `sub_141FB6760`, each through the input manager
  (`qword_1451C4558`), so the player's bindings apply, and only while the
  page has the input (+8897/+8898). The default checks a cGUIInstInput gets
  (ctor `sub_140B86A50`): decide `sub_140B86FB0` = key 0, cancel
  `sub_140B86F90` = key 1. Directions: `sub_1418B9D30(gui, int* sources,
  int* mode, uint* flags out)` returns **1 left, 2 right, 3 up, 4 down**;
  mode bits 1 pressed, 2 repeat (lists use 3). A button list's init
  (`sub_140B80D00`) sets the sources it steps by (+264): 0x10 for Type 0
  (menu keys 9 / 10, up / down), 0x20 for Type 1 (7 / 8, left / right); its
  step check (`sub_140B81060` via `sub_140B86DD0`) maps 1 / 3 to the previous
  row and 2 / 4 to the next. CMFM finds the reader through its call in
  `sub_140B86F30` (+85).
- **Run tabs-4** (the first keyboard / pad try): CMFM read 1-4 the wrong way
  round and asked sources 1 and 2 (bits 0 and 1): Up / Down switched pages,
  Left / Right entered the panel (where the same press then changed the
  row's value), the keyboard did nothing; the tabs went grey with the
  list's focus (they were under the panel, `null_audio`).
- **Keyboard and pad on the tabs** (since): the tabs and their scrollbar sit
  next to the panels (`null_prog_color1`, the same place), so the panel's
  focus changes leave them alone. The panel list's step up from the first
  selectable row (CMFM's step hook) asks for the tabs instead of wrapping,
  for a mod with tabs; the page's update runs the panel's Back (focus 0),
  marks the category list inactive and the **tab list active**: the game's
  own sideways-list input then steps it (Left / Right as in any such list,
  keyboard and pad, with its sound), and CMFM's step hook turns each step
  into the previous or next page (wrapping; inside the window the list's
  cursor moves, at its edge the page's update moves the labels). CMFM reads
  Down (sources 0x10, as a vertical list, pressed), Confirm (key 0, with the
  tab list) and Back (key 1): Down / Confirm enter the panel, Back gives the
  mod list the keys back (the category list active again; a list reads keys
  only from the frame after the GUI marked it, so the same press does not
  reach it). A focus change by the mouse ends it; the tab list stops
  reading keys whenever it ends.

## No caps (static, 2026-10-12; run `caps-1`)

Every shared limit CMFM had (200 rows over all mods, about 900 label and
value texts, 24 pages, 24 then 64 settings per page) was CMFM's own. What
the game allows:

- **Text numbers.** Row labels and value names go through `sub_14044A280(12,
  id)`: `id > 10000` (unsigned) -> `rGUIMessage::getMessage(file 23, id -
  10000)`, else file 12. Help lines go through the help bar
  (`sub_141AE0E60` stores id and file; `sub_141D91740` draws): `id <= 10000`
  (signed) -> `GetGameMessage(file, id)`, else `getMessage(file 23, id -
  10000)`; `GetGameMessage` itself does the same. `getMessage` checks the
  number against the file's count (+0xD8), but CMFM's hook answers first.
  So CMFM's row texts live in file 23 far above the game's own: labels and
  value names from row number 10000 + 0x10000000, help lines from 10000 +
  0x20000000 (2^28 numbers each, within a signed int). Every other reader
  of a row's text numbers seen (`sub_141E81FF0`, `sub_141E8BD70` value texts;
  `sub_141E8EA70`, `sub_141E80310`, `sub_141E808A0` help) uses the same two
  paths. The row filler compares label numbers with a few fixed ones (40-45,
  314, 316-321, 394, 396, 10135-10136), none in CMFM's range.
- **Setting ids.** The functions that take a row's id: the getter
  `sub_140EEA350` (kind 1: `sUserData + 256 + id`, unchecked) and the setter
  `sub_140EEB450` (hooked by CMFM; every caller in the exe goes through them),
  the slider type `sub_140EEA2D0`, enabled `sub_140EEAE80` (hooked),
  visible `OptionRow_isVisible`, and the row looks (ids 3-5, 6, 29, 33, 34,
  below 32): all compare with fixed numbers. So kind-1 ids from 100 can run
  far (CMFM: 100 to 100 + 2^20).
- **Pages.** CMFM's own layout lists mods in its own scrolling list; the
  game's five category rows are not used for them. Only the fallback without
  CMFM's layout scrolls the game's category list (any length).
- **Rows per page.** The game's tab object holds 24 row objects, but the
  panel window gives the game only the 14 rows it shows. The views that show
  a tab whole (the game's Options page fallback, CMFM's page without its own
  layout) get the first 24 rows (`OptionsModel::game_table`).

What remains: CMFM's limits against mistakes (1024 settings per page, 1024
pages, 2^20 rows in all), and two per-setting limits of the game's parts:
CHOICE names (32; the pick list shows 14 rows) and number values (101, the
slider's 0-100 positions).

## The game's GUI unit list (static, 2026-10-15; a player's report)

A player's speedrun tool (IN-Q, a Cheat Engine table) crashed with our mods:
it takes the GUI unit in **slot 46** of `sMhGUI`'s unit list as the item
box's Item Loadouts (`uGUIBoxItemMySet`), and with CMFM it is the Sell
screen. The DLSS session measured it in the user's game (read only, Frida
`evaluate`): CMFM's page sits in slot 18, every later unit one slot on
(`dlssupdate/docs/handoff-cmfm-20261015-unit-list.md`).

How the list works (decompiler, CMFM's database, names written back):
- `sMhGUI::Instance` (RVA 0x51C4640) + 0x260 is an `MtArray` of GUI units
  (`MtArray::vft`; length +0x268, capacity +0x26C, auto-delete +0x270,
  data +0x278); the constructor reserves 256.
- **Joining:** uMhGUI vtable slot 5 (`sub_141FB7ED0`) calls
  `sMhGUI::registerUnit` (`sub_141AD5F40`), which **appends** the unit at
  the end, under sMhGUI's lock (+8). So a unit takes the next free slot when
  it starts; CMFM's page, made at the first title frame, took slot 18 and
  every unit started later (HUD, item box) sits one slot on.
- **Leaving:** uMhGUI vtable slot 25 (`sub_141FB59C0`) calls
  `sMhGUI::unregisterUnit` (`sub_141AE2C70`), and a scene reset
  (`sub_141ADE330`) drops every unit at once, **both only for units without
  the keep bit** (`+0x14` bit 8, the bit CMFM sets on its page so that no
  scene change kills it). Removal is `MtArray::erase`, which **closes the
  gap**: every later unit moves back one slot.

What follows for the request (make the page when Mods opens, release it
when Mods closes): a page made on open is appended behind every unit that
exists and shifts none; released (keep bit cleared, then killed) it leaves
through `unregisterUnit` and the list is as the game made it. Not done yet
(the user: test first). To check: the page's start-up needs a frame or two
before it can open (the title flow and the Start menu wait for it today
only once); its layout stays loaded (kept already); opening Mods at the
title, then entering the game, must leave no trace.

Measuring: `tools/re/gui_units.py` (read only: PROCESS_QUERY_LIMITED_INFORMATION
| PROCESS_VM_READ, as AL's modwatch.py) lists the units by slot and class
(vtable names from CMFM's database, `local/ida/vft_names.json`) and diffs
two snapshots.

Measured (run `20261015-guilist-1`, CMFM 0.1.0):
- without CMFM: title 43 units; hub 190 and quest 164, **uGUIBoxItemMySet
  in slot 46** in both (slots 0-142 shared by hub and quest);
- with CMFM: at the title the page is **appended at the end** (slot 43),
  nothing shifted; in the quest the list is the baseline with the page
  **inserted at slot 18**, exactly (the title's own units are dropped at
  the scene change and the kept page slides down into their place); slot
  46 is uGUIBoxItemSell, IN-Q's crash. Opening and closing Mods changes
  nothing. The DLSS mod adds no GUI unit.

So only the page's surviving the change from title to game shifts the list;
made on open and released on close it would not.

**Fixed in 0.1.1** (commits 4175630, 1807795; run `20261015-unitfix-1`): the
page is not kept. The title's page dies with the title scene; in game, Mods
in the Start menu makes a page, sets uMhGUI's release-when-closed bit
(`+8856 & 4`: slot 87, `sub_141FB89B0`, kills the unit when it has closed,
through cUnit slot 18 / `sUnit::kill`, which refuses kept units) and asks
for its open, which the game carries out after the page's setup (6-8 ms
later in the run); the open handler closes the Start menu. While open the
page is kept (a scene change then closes it through message 75), and the
close handler clears that. Measured: hub and quest identical to the
baseline before and after a Mods visit; while Mods is open the page sits
at the end of the list (slot 164 in the quest) and slot 46 is unchanged.
The cost: in game the page is built on each Mods press (one slower frame).

## Answers so far

1. **Can CMFM add a row or a page?** A row in Options: yes in principle,
   through the writable `OptionTabTables` (no code patch); the visible list
   pages through the table itself. A page: the registry, creation and open
   path are known; a page of our own looks possible but is not tested.
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
