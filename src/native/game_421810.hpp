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
// (slot 91) at RVA 0x1E8AB40.
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

// cGUIFixedScrollList::enable(scroll, owner, callback) at RVA 0xB65B20. Every
// button list holds one at +736 (attached at list init). Switched on, it
// turns moves past the last visible row into scrolls (the list's wrap must
// be on) and calls callback(owner, from, to) after each scroll; a game menu
// sets it up like this at 0x140556030.
inline constexpr char kScrollEnablePattern[] =
    "80 79 09 00 48 89 91 A0 00 00 00 4C 89 81 88 00 00 00 4C 89 81 90 00 00 00";
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
inline constexpr std::size_t kListWrap = 656;             // byte
inline constexpr std::size_t kListScroll = 736;           // cGUIFixedScrollList
inline constexpr std::size_t kListScrollOn = 744;         // byte (scroll +8)
inline constexpr std::size_t kListScrollKeepWrap = 745;   // byte (scroll +9)
inline constexpr std::size_t kListScrollVisible = 852;    // scroll +116
inline constexpr std::size_t kListScrollTotal = 856;      // scroll +120
inline constexpr std::size_t kListScrollSelected = 868;   // scroll +132: the selected item's index
inline constexpr std::size_t kListScrollCallback = 872;   // scroll +136
inline constexpr std::size_t kListScrollCallback2 = 880;  // scroll +144
inline constexpr std::size_t kListScrollOwner = 896;      // scroll +160
// List row (cGUIInstance) fields.
inline constexpr std::size_t kInstRoot = 0x1C0;          // cGUIObjRoot*
inline constexpr std::size_t kInstFlags = 0x628;
inline constexpr std::uint32_t kInstFlagActive = 0x2;    // cleared by play() for the rows after Quit
inline constexpr std::uint32_t kInstFlagEnabled = 0x10;  // cleared for greyed-out rows

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
