// Addresses, patterns and expected values for MonsterHunterWorld.exe 421810
// (SHA-256 c2ebbbd2...d90ea), from docs/research/native-menu.md. The native
// menu runs only on this exe; every value is checked before it is used.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cmfm::native::game {

inline constexpr char kTestedExeSha256[] = "c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea";

// rGUIMessage::getMessage(self, id) at RVA 0x2244930: the game's text lookup.
inline constexpr char kGetMessagePattern[] = "39 91 D8 00 00 00 76 1D";
// uGUIStartMenu::onItemAction(menu, item id, arg) at RVA 0x1ECDC30: the Start
// menu's action switch. Its default case opens the Item Pouch.
inline constexpr char kItemActionPattern[] = "48 89 5C 24 18 55 56 57 48 8D 6C 24 80";

// The option-row module shared by the Options pages: getter(kind, id) at
// RVA 0xEEA350 and setter(kind, id, value) at RVA 0xEEB450. Unknown kinds
// fall through to save-data reads and writes, so CMFM rows must be answered
// before the game's code runs.
inline constexpr char kOptionGetPattern[] = "48 89 5C 24 08 57 48 83 EC 20 33 DB 8B FA 83 E9 01";
inline constexpr char kOptionSetPattern[] = "48 89 5C 24 08 57 48 83 EC 20 41 8B D8 8B FA 83 E9 01";
// uGUIMenuOption::close (vtable slot 91) at RVA 0x1E80D40.
inline constexpr char kMenuOptionClosePattern[] = "40 53 48 83 EC 20 48 8B D9 0F B6 89 40 2C 00 00";
// The Options page's "Set To Default" answer (closure, ?, int* answer) at
// RVA 0x1E800A0: on Yes in mode 0 it resets ALL game and system options
// (run 20261011-native-m2b), whatever rows are shown.
inline constexpr char kMenuOptionDefaultsPattern[] =
    "40 53 48 83 EC 20 41 83 38 00 48 8B D9 0F 85 ?? ?? ?? ?? 48 8B 51 08 48 8B 0D";
// uGUIMenuOption::refreshRows(page) at RVA 0x1E82170 (called, not hooked).
inline constexpr char kMenuOptionRefreshPattern[] = "40 56 41 54 41 57 48 83 EC 40";

// OptionTabTables in .data (writable): 7 row-table pointers, [0..4] the five
// Options tabs of mode 0, with these original tables.
inline constexpr std::uintptr_t kRvaOptionTabTables = 0x3FBFE90;
inline constexpr std::uintptr_t kRvaOptionRows[5] = {0x31BDEC0, 0x31BE190, 0x31BE460, 0x31BE870, 0x31BEB70};

// GetGameMessage's loaded message files (rGUIMessage* globals).
inline constexpr std::uintptr_t kRvaMessages11 = 0x5012E18;  // Start menu labels
inline constexpr std::uintptr_t kRvaMessages12 = 0x5012E20;  // option texts (header 0, tabs 1-5)
inline constexpr std::uintptr_t kRvaMessages18 = 0x5012E50;  // help lines

// The Start menu's tab lists in .rdata (read-only): 5 x {u32 tab label id,
// pad, item table*}. The System tab is entry 4 (tab label id 3).
inline constexpr std::uintptr_t kRvaTabList = 0x3481870;
inline constexpr std::uintptr_t kRvaTabListInQuest = 0x3481820;
inline constexpr std::uint32_t kSystemTabIndex = 4;
inline constexpr std::uint32_t kSystemTabLabel = 3;
inline constexpr std::uintptr_t kTabListEntrySize = 16;
inline constexpr std::uintptr_t kTabListItemsOffset = 8;  // the item table pointer within an entry
// The System tab's item table pointer in each list (run 20261011-native-m1
// patched the label field instead; these pin the right slots).
static_assert(kRvaTabList + kSystemTabIndex * kTabListEntrySize + kTabListItemsOffset == 0x34818B8);
static_assert(kRvaTabListInQuest + kSystemTabIndex * kTabListEntrySize + kTabListItemsOffset == 0x3481868);
// The System tab's item table in .data: 71, 10, 72, 73, 74, 75, end marker.
inline constexpr std::uintptr_t kRvaSystemItems = 0x3FC0E40;
inline constexpr std::uint32_t kSystemItemsMaxRead = 16;

// uGUIStartMenu's vtable and the fields its Options action (onItemAction
// case 71) sets around opening the Options page: no dialog pending (76), a
// page of the menu's own open (byte; sMhGUI keeps menu mode on while it is
// set, which holds the hunter's input), no item action (-1).
inline constexpr std::uintptr_t kRvaStartMenuVtable = 0x3481908;
inline constexpr std::size_t kStartMenuDialog = 0x2BA8;
inline constexpr std::int32_t kStartMenuNoDialog = 76;
inline constexpr std::size_t kStartMenuChildOpen = 0x29BC;
inline constexpr std::size_t kStartMenuAction = 0x2960;

// How the game's in-game Options page (uGUIMenuOption::update, RVA 0x1E82590)
// holds the hunter while it is open: every frame in its open state it locks
// player input channel 1, and channel 2 when the Start menu opened it, and
// sets the area manager's two menu bytes (+0xD31C = 0x0101, as every game
// menu does each frame). The pattern starts at RVA 0x1E82596; CMFM reads the
// input manager and area manager globals and the lock function from it.
inline constexpr char kOptionsInputLockPattern[] =
    "F7 81 98 22 00 00 11 00 00 80 48 8B D9 0F 84 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 45 33 C0 41 8D 50 01 E8 ?? ?? ?? ?? 80 BB F8 28 00 00 01 75 13 48 8B 0D ?? ?? ?? ?? 45 33 C0 41 8D 50 02 E8 ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 66 C7 80 1C D3 00 00 01 01";
inline constexpr std::size_t kInputLockInput1 = 19;  // mov rcx, [rip+rel32] (input manager)
inline constexpr std::size_t kInputLockCall1 = 33;   // call lockChannel(input, channel, player)
inline constexpr std::size_t kInputLockInput2 = 47;
inline constexpr std::size_t kInputLockCall2 = 61;
inline constexpr std::size_t kInputLockArea = 66;    // mov rax, [rip+rel32] (sMhArea)
inline constexpr std::size_t kAreaMenuFlags = 0xD31C;
inline constexpr std::uint16_t kAreaMenuFlagsOn = 0x0101;
inline constexpr std::int32_t kInputChannelMenu = 1;
inline constexpr std::int32_t kInputChannelMove = 2;  // only when the Start menu opened the page

// Start menu item ids.
inline constexpr std::int32_t kItemOptions = 71;  // opens uGUIMenuOption
inline constexpr std::int32_t kItemMods = 77;     // ours: 76 is the end marker

// Text ids for our entry: above the end of message files 11 (225 texts) and
// 18 (1219 texts), and at most 10000 (GetGameMessage sends larger ids to
// file 23). Without our hook they resolve to empty text.
inline constexpr std::uint32_t kModsLabelId = 9000;  // file 11
inline constexpr std::uint32_t kModsHelpId = 9001;   // file 18

// Every row table the Options pages read: OptionTabTables[0..6], the mode 2
// and 3 tables, TitleOptionTabTables[0..5]. Their kind-1 rows use setting
// ids 0-34; CMFM's rows use kind 1 from id 100, checked at start.
inline constexpr std::size_t kOptionTabTableCount = 7;
inline constexpr std::uintptr_t kRvaOptionRowsMode2 = 0x31BE438;
inline constexpr std::uintptr_t kRvaOptionRowsMode3 = 0x31BE848;
inline constexpr std::size_t kMaxGameTableRows = 128;  // the longest has 24

// ---- The title screen (docs/research/native-menu.md, "The title screen") ----

// uGUITitle::play (vtable slot 90) at RVA 0x1EFFDF0: labels the menu rows,
// hides the rows after Quit and records the Options and Quit rows.
inline constexpr char kTitlePlayPattern[] =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 81 EC 40 02 00 00 BA 07 00 00 00";
// The menu list's cursor callback (title, list, row) at RVA 0x1EFF690: row ->
// item id (equal on PC).
inline constexpr char kTitleCursorPattern[] = "40 53 48 83 EC 20 80 B9 C8 2D 00 00 00 48 8B D9";
// The menu list's selected callback (title, row) at RVA 0x1EFF480: the Quit
// row opens a Yes/No, any other row closes the title, and the title scene
// acts on the item id (Options: flow 9 opens uGUIOption).
inline constexpr char kTitleDecidedPattern[] =
    "48 89 5C 24 10 55 48 8D 6C 24 D0 48 81 EC 30 01 00 00 48 8B D9 3B 91 2C 30 00 00";
// uGUIOption (the title Options) on-open (slot 90) at RVA 0x1E8AF60 and close
// (slot 91) at RVA 0x1E8AB40. Not hooked: they confirm the vtable slots CMFM's
// page class overrides.
inline constexpr char kTitleOptionsOpenPattern[] =
    "48 89 5C 24 18 55 56 57 41 56 41 57 48 8D 6C 24 C9 48 81 EC E0 00 00 00 48 8B F9";
inline constexpr char kTitleOptionsClosePattern[] = "40 55 53 57 41 57 48 8D 6C 24 98 48 81 EC 68 01 00 00 48 8B F9";
// uGUIOption's showCategory(categories, category) at RVA 0x1E8C8F0: shows the
// category's rows and help line, and the VRAM panel for category 1 (Display).
inline constexpr char kTitleShowCategoryPattern[] =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 30 44 8B F2";

// Game functions CMFM calls (found with al.scanner, not hooked).
// cGUIInstButtonList::setRowCount(list, count) at RVA 0xB7FED0: rebuilds the
// row copies; keeps the cursor index.
inline constexpr char kListSetRowCountPattern[] =
    "40 53 48 83 EC 20 48 8B D9 E8 ?? ?? ?? ?? 48 8B 03 48 8B CB 48 83 C4 20 5B 48 FF A0 30 01 00 00";
// cGUIInstButtonList::setCursor(list, row) at RVA 0xB76190; reads the row
// under the old cursor without a range check.
inline constexpr char kListSetCursorPattern[] = "48 89 5C 24 08 57 48 83 EC 20 48 63 FA 48 8B D9 3B B9 D8 01 00 00";
// cGUIObjRoot::getObjectFromName(root, name) at RVA 0x2340E20.
inline constexpr char kGetObjectFromNamePattern[] = "48 89 5C 24 08 48 89 7C 24 10 48 8B 81 C8 00 00 00";
// cGUIObjSizeAdjustMessage::formatText(obj, text) at RVA 0x18C48A0.
inline constexpr char kFormatTextPattern[] = "48 85 C9 74 34 48 85 D2 74 25 0F B6 02 3A 05";
// uGUI::getInstanceFromName(unit, name) at RVA 0x23537A0.
inline constexpr char kGetInstanceFromNamePattern[] = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 81 90 02 00 00";
// cGUIInstance::setPlay(instance, bool) at RVA 0x18BAEA0.
inline constexpr char kSetPlayPattern[] = "48 85 C9 74 50 48 89 5C 24 08 57 48 83 EC 20 0F B6 FA";
// cGUIObject::setHidden(object, bool) at RVA 0x2343310 (object +88 bit
// 0x4000000).
inline constexpr char kSetHiddenPattern[] = "48 83 EC 28 44 8B 41 58 41 8B C0 C1 E8 1A";

// cGUIInstButtonList::moveCursor(list, row) at RVA 0xB74060: the list's cursor
// move from directional input (vtable slot 28 via a thunk, slot 94); calls
// the owner's cursor callback. With wrap on, Down on the last row asks for
// row 0 and Up on row 0 for the last row.
inline constexpr char kListMovePattern[] = "48 89 5C 24 18 57 48 83 EC 30 48 63 81 74 02 00 00 48 8B D9 48 63 FA";
// uGUIOption::update (vtable slot 81) at RVA 0x1E8EF50, every frame while the
// title Options page runs; +10480 is 0 while its category list has the focus.
// Not hooked (confirms slot 81).
inline constexpr char kTitleOptionsUpdatePattern[] =
    "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 41 56 41 57 48 83 EC 30 48 8B F9";
inline constexpr std::size_t kOptFocus = 0x28F0;  // 0: the category list, 1: the panel under its cursor
// uGUIOption::initCategory(tab object, unit, tab, name, title) at RVA
// 0x1E8DFE0: finds the tab's layout objects and fills its rows from
// TitleOptionTabTables[tab] (buildCategories calls it per category).
inline constexpr char kInitCategoryPattern[] =
    "4C 8B DC 49 89 7B 20 55 41 56 41 57 49 8D 6B A9 48 81 EC A0 00 00 00 41 8D 40 FB";

// uGUIOption::buildCategories(categories, unit) at RVA 0x1E8D5A0: slot 5 of
// the categories object's vtable; builds the category list and every tab's
// rows from TitleOptionTabTables. Called through that slot after checking it.
inline constexpr std::uintptr_t kRvaBuildCategories = 0x1E8D5A0;
inline constexpr std::size_t kBuildCategoriesSlot = 5;

// TitleOptionTabTables in .data (writable): 6 row-table pointers, [0..4] the
// title Options tabs Interface, Display, Audio, Language, Privacy.
inline constexpr std::uintptr_t kRvaTitleOptionTabTables = 0x3FC0480;
inline constexpr std::size_t kTitleOptionTabTableCount = 6;
inline constexpr std::uintptr_t kRvaTitleOptionRows[kTitleOptionTabTableCount] = {
    0x31BEEE0, 0x31BE870, 0x31BEB70, 0x31BED00, 0x31BF000, 0x31BE190};
inline constexpr std::size_t kTitleViewTabs = 5;
// The tab objects' layout names, as buildCategories passes them to initCategory.
inline constexpr const char* kTitleTabNames[kTitleViewTabs] = {"settings", "display", "audio", "language",
                                                               "display_G"};
// The category labels for tabs 0-4 (file 12), asked for by buildCategories,
// and the help line while a category is highlighted (file 18, set by
// sub_141E8C8F0; run 20261011-native-m3 showed the game's).
inline constexpr std::uint32_t kTitleCategoryLabelIds[kTitleViewTabs] = {392, 347, 348, 349, 395};
inline constexpr std::uint32_t kTitleCategoryHelpIds[kTitleViewTabs] = {852, 782, 783, 784, 859};

// Message files for the title rows CMFM moves down: Credits 13/0x66,
// Support 13/0x5F, Quit 23/0x170 (as uGUITitle::play writes them).
inline constexpr std::uintptr_t kRvaMessages13 = 0x5012E28;
inline constexpr std::uintptr_t kRvaMessages23 = 0x5013458;
inline constexpr std::uint32_t kTitleCreditsId = 0x66;
inline constexpr std::uint32_t kTitleSupportId = 0x5F;
inline constexpr std::uint32_t kTitleQuitId = 0x170;

// uGUITitle fields.
inline constexpr std::size_t kTitleList = 0x2900;        // btnlist_menuList
inline constexpr std::size_t kTitleOptionsRow = 0x2FD8;  // set by play()
inline constexpr std::size_t kTitleQuitRow = 0x302C;     // set by play()
inline constexpr std::uint32_t kTitleRows = 7;           // the list's DuplicateNum, set at setup

// cGUIInstButtonList fields.
inline constexpr std::size_t kListRowCount = 0x1D8;
inline constexpr std::size_t kListRows = 0x1E8;
inline constexpr std::size_t kListDuplicateNum = 0x298;
inline constexpr std::size_t kListCursor = 0x274;
inline constexpr std::size_t kListWrap = 656;  // byte: Down on the last row goes to row 0
inline constexpr std::size_t kListInactive = 371;  // byte: 1 = takes no input (tab setActive writes it)
// The row the cursor goes to when the list's opening animation ends (the
// list's update, RVA 0xB755C0, state 1: the first enabled row from here).
inline constexpr std::size_t kListStartRow = 560;
// List row (cGUIInstance) fields.
inline constexpr std::size_t kInstRoot = 0x1C0;          // cGUIObjRoot*
inline constexpr std::size_t kInstFlags = 0x628;
inline constexpr std::uint32_t kInstFlagActive = 0x2;    // cleared by play() for the rows after Quit
inline constexpr std::uint32_t kInstFlagEnabled = 0x10;  // cleared for greyed-out rows

// ---- A page of CMFM's own (docs/research/native-menu.md, "Pages and their lifetime") ----

// The title scene (aTitle) slot 7, TitleScene::updateFlows(scene, ...) at RVA
// 0x25E0D0, every frame of the title scene: runs its sub-flows; flow 9
// (Options) opens the page at scene+312, waits until it closes and checks
// it. Hooked: CMFM points flow 9 at its own page when "Mods" started it.
inline constexpr char kTitleSceneFlowsPattern[] =
    "48 83 EC 68 8B 41 74 48 89 5C 24 78 48 89 74 24 60 33 F6 48 89 7C 24 58 48 8B F9 0F 29 74 24 40 0F 57 F6 83 F8 19";
inline constexpr std::size_t kSceneFlow = 116;          // the running sub-flow
inline constexpr std::int32_t kSceneFlowOptions = 9;
inline constexpr std::size_t kSceneOptionsPage = 312;   // tracked reference to the title's uGUIOption

// sMhUnit::addBottom(units, move line, unit, group bits, 0) at RVA 0x2219070
// (addTop shares its first 64 bytes). The title scene puts its pages on line
// 20 with group bits 0x40000000.
inline constexpr char kAddBottomPattern[] =
    "48 89 5C 24 20 55 57 41 57 48 83 EC 30 48 63 EA 4D 8B F9 49 8B D8 48 8B F9 4D 85 C0 75 10 32 C0 48 8B 5C 24 68 48 83 C4 30 41 5F 5F 5D C3 80 79 30 00 48 89 74 24 50 4C 89 74 24 58 75 09 80 3D ?? ?? ?? ?? 00 74 0A 48 83 C1 08 FF 15 ?? ?? ?? ?? 48 69 F5 F8 00 00 00 4C 8B F5";
inline constexpr std::int32_t kGuiMoveLine = 20;
inline constexpr std::uint64_t kGuiGroupBits = 0x40000000;
// sMhUnit::Instance (pointer in .data) and sMhUnit's vtable.
inline constexpr std::uintptr_t kRvaUnitManager = 0x51238C8;
inline constexpr std::uintptr_t kRvaUnitManagerVtable = 0x33FCDE8;

// Tracked unit references: a 24-byte slot {unit, next, prev} in the unit's
// list (+208); the unit's destructor clears every slot. setRef(lock, unit,
// slot) at RVA 0x2229160, clearRef(lock, slot) at RVA 0x22291D0; the lock
// comes from a function that uMhGUI::requestOpen (RVA 0x1FB7CF0) calls.
inline constexpr char kSetRefPattern[] =
    "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 80 79 2C 00 49 8B F8 48 8B F2 48 8B D9 75 35";
inline constexpr char kClearRefPattern[] =
    "48 89 5C 24 08 57 48 83 EC 20 80 79 2C 00 48 8B FA 48 8B D9 75 22 FF 15 ?? ?? ?? ?? 48 8B 0F 48 85 C9 74 0B 48 8B D7 E8 ?? ?? ?? ?? FF 4B 28 48 8B CB FF 15";
inline constexpr char kRequestOpenPattern[] =
    "48 89 5C 24 08 57 48 83 EC 20 83 89 98 22 00 00 10 48 8D B9 00 21 00 00 48 8B DA 48 39 17 74 13 E8 ?? ?? ?? ?? 48 8B C8 4C 8B C7 48 8B D3 E8";
inline constexpr std::size_t kRequestOpenLockCall = 32;    // E8 rel32: the reference lock
inline constexpr std::size_t kRequestOpenSetRefCall = 46;  // E8 rel32: setRef
// uMhGUI::requestClose(unit, now) at RVA 0x1FB7CC0.
inline constexpr char kRequestClosePattern[] =
    "44 8B 81 98 22 00 00 41 83 C8 20 41 8B C0 41 0F BA F0 07 0F BA E8 07 84 D2 44 0F 45 C0 44 89 81 98 22 00 00 C3";

// uMhGUI state (+8856): any of these bits while open or opening.
inline constexpr std::size_t kGuiState = 0x2298;
inline constexpr std::uint32_t kGuiStateOpen = 0x80000011;

// cUnit fields.
inline constexpr std::size_t kUnitState = 12;  // byte: & 0xE while alive (2 new, 8 running; 16 killed)
inline constexpr std::size_t kUnitFlags = 20;
inline constexpr std::uint32_t kUnitFlagKeep = 8;  // sUnit::kill refuses the unit (group 0 pages, scene units)

// uGUIOption's class description (MtDTI in .data, filled by MtDti::ctor
// before WinMain) and vtable (93 slots in .rdata; no RTTI before it).
inline constexpr std::uintptr_t kRvaOptionPageDti = 0x502B370;
inline constexpr std::uintptr_t kRvaOptionPageVtable = 0x347DA20;
inline constexpr std::size_t kOptionPageVtableSlots = 93;
inline constexpr std::size_t kSlotGetDti = 4;    // lea rax, [DTI]; ret
inline constexpr std::size_t kSlotUpdate = 81;   // kTitleOptionsUpdatePattern
inline constexpr std::size_t kSlotOpen = 90;     // kTitleOptionsOpenPattern
inline constexpr std::size_t kSlotClose = 91;    // kTitleOptionsClosePattern
inline constexpr std::size_t kSlotGetGuiPath = 71;  // lea rax, [layout path]; ret
inline constexpr char kOptionPageLayout[] = "ui\\title\\options";

// CMFM's own page layout (decisions.md item 10): assets/nativePC/ui/cmfm/
// mods.gui, built by tools/gui/make_mods_gui.py and loaded by the game from
// nativePC (CMFM's mod folder under MO2).
inline constexpr char kModsLayout[] = "ui\\cmfm\\mods";
// sMhResource::loadResource(resources, class, path, flags) at RVA 0x22228D0:
// slot 11 of sMhResource's vtable; returns nullptr if the file is not found.
// Flag 1 loads from file, as uMhGUI's layout load does.
inline constexpr char kLoadResourcePattern[] =
    "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 83 EC 30 80 B9 7B 80 20 00 00 45 8B F9 49 8B F8 4C 8B E2";
inline constexpr std::size_t kLoadResourceSlot = 11;
inline constexpr std::uint32_t kLoadFromFile = 1;
inline constexpr std::uintptr_t kRvaResourceManager = 0x5123A00;  // sMhResource::Instance (pointer)
inline constexpr std::uintptr_t kRvaResourceManagerVtable = 0x33FC610;
inline constexpr std::uintptr_t kRvaGuiResourceClass = 0x51C4608;  // rGUI::DTI::Instance
inline constexpr char kGuiResourceClassName[] = "rGUI";
// In CMFM's layout: the mod list (the spare camera list, styled like the
// category list) and the panel that shows the highlighted mod (audio: its
// rows carry a slider part).
inline constexpr char kModListName[] = "btnlist_camera";
inline constexpr std::size_t kSplitPanel = 2;
// The split panel's title (initCategory writes the tab title into it) and
// CMFM's restart line (the camera panel's title, moved above the mod list).
inline constexpr char kSplitPanelTitle[] = "a_title_menu_audio";
inline constexpr char kRestartLineName[] = "a_title_menu_camera";
inline constexpr char kTitleText[] = "msg_sub_title";
// A panel row's parts (the row filler, RVA 0x1E8D890): the label text and the
// star icon the title page never shows (CMFM's restart mark).
inline constexpr char kRowLabelText[] = "msg_config";
inline constexpr char kRowValueText[] = "text_config_L";  // the value box
inline constexpr char kRowStar[] = "null_star";
// The help line: showCategory asks for it with sub_141AE0E60(sMhGUI, unit,
// message id, file 18, 0.0f, 0) at RVA 0x1AE0E60; this is that call (the
// manager at +6, rel32 at +9; the call at +33). The help bar keeps its text
// while the id stays the same, and in split mode every mod's help is the
// audio panel's id (783), so CMFM asks with ids of its own, alternating.
inline constexpr char kHelpRequestPattern[] =
    "8B 55 08 0F 57 C0 48 8B 0D ?? ?? ?? ?? 41 B9 12 00 00 00 C6 44 24 28 00 45 8B C7 F3 0F 11 44 24 20 E8 ?? ?? ?? ??";
inline constexpr std::size_t kHelpRequestManager = 6;  // 48 8B 0D rel32: sMhGUI::Instance (RVA 0x51C4640)
inline constexpr std::size_t kHelpRequestCall = 33;    // E8 rel32
inline constexpr std::uint32_t kHelpFile = 18;
inline constexpr std::uint32_t kSplitHelpIds[2] = {9996, 9997};

// The mouse on uGUIOption (docs/research/native-menu.md, "Split mode and the
// mouse"). sub_141E8A510 at RVA 0x1E8A510 asks whether a part of the page was
// clicked; it calls the three helpers CMFM uses:
//   mouse position(gui, float out[4], list) at RVA 0x1FB5D30 (call at +31),
//   hit test(gui, instance, position, int* out) at RVA 0x1FB6190 (call at +53),
//   left click this frame(gui, list) at RVA 0x1FB6560 (call at +82).
// They answer only while the page has the input (gui +8897/+8898).
inline constexpr char kOptionClickPattern[] =
    "40 53 48 83 EC 30 48 83 B9 A0 00 00 00 00 48 8B D9 74 4E 48 8B 49 08 48 8D 54 24 20 45 33 C0 E8 ?? ?? ?? ?? 48 8B 93 A0 00 00 00 4C 8B C0 48 8B 4B 08 45 33 C9 E8 ?? ?? ?? ?? 3C 01 75 23 48 8B 05 ?? ?? ?? ?? 33 D2 C6 80 85 47 01 00 01 48 8B 4B 08 E8 ?? ?? ?? ??";
inline constexpr std::size_t kOptionClickPosCall = 31;    // E8 rel32
inline constexpr std::size_t kOptionClickHitCall = 53;    // E8 rel32
inline constexpr std::size_t kOptionClickPressCall = 82;  // E8 rel32
// The mouse moved this frame(gui, list) at RVA 0x1FB65A0.
inline constexpr char kMouseMovedPattern[] =
    "48 85 D2 74 09 80 BA 72 01 00 00 00 74 0D 8B 81 9C 22 00 00 C1 E8 0A A8 01 75 1E 80 B9 C2 22 00 00 00 75 09 80 B9 C1 22 00 00 00 74 0C 48 8B 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? 32 C0";
// The page's own focus changes, called through the objects' vtables after
// checking the slot: the category list's Confirm, slot 8 of the categories
// object (RVA 0x1E89C40, enters the panel under the category cursor; the
// page's update calls it for a click on the panel), and a panel list's Back,
// slot 9 of a tab object (RVA 0x1E89AB0, focus back to the category list).
inline constexpr std::uintptr_t kRvaEnterPanel = 0x1E89C40;
inline constexpr std::size_t kEnterPanelSlot = 8;
inline constexpr std::uintptr_t kRvaLeavePanel = 0x1E89AB0;
inline constexpr std::size_t kLeavePanelSlot = 9;
inline constexpr std::size_t kTabWindow = 48;  // a_window_<tab>: the panel, as the page's update hit-tests it
inline constexpr char kOptionPageClassName[] = "uGUIOption";
inline constexpr std::uint32_t kOptionPageClassId = 0x165BDEB6;  // (~crc32(name)) & 0x7FFFFFFF
// MtDTI as MtDti::ctor fills it; its vtable has 4 slots (1: new object).
inline constexpr std::size_t kDtiVtableSlots = 4;

// uGUIOption fields.
inline constexpr std::size_t kOptCategories = 0x28F8;    // object with buildCategories in slot 5
inline constexpr std::size_t kCategoriesUnit = 8;        // categories object -> its uGUIOption
inline constexpr std::size_t kOptVramPanel = 0x7230;     // null_vram, shown only by showCategory
inline constexpr std::size_t kOptCategoryList = 0x2910;  // btnlist_menu (categories + 24)
inline constexpr std::size_t kOptTabs = 0x2918;          // 7 tab objects
inline constexpr std::size_t kOptTabSize = 0x968;
inline constexpr std::size_t kTabList = 24;              // its row list
inline constexpr std::size_t kTabIndex = 80;
inline constexpr std::size_t kTabDisplayRow = 84;        // -1 when unset
// Each tab object holds 24 row objects of 96 bytes from +96. A row object
// as constructed: vtable, zeros up to +80, tab 7 at +80, 0 at +84, -1 at +88.
inline constexpr std::size_t kTabRowObjects = 24;
inline constexpr std::size_t kRowObjectSize = 96;

}  // namespace cmfm::native::game
