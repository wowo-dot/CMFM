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
