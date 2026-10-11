// Addresses, patterns and expected values for MonsterHunterWorld.exe 421810
// (SHA-256 c2ebbbd2...d90ea), from docs/research/native-menu.md. The native
// menu runs only on this exe; every value is checked before it is used.
#pragma once

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

}  // namespace cmfm::native::game
