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

// GetGameMessage's loaded message files (rGUIMessage* globals).
inline constexpr std::uintptr_t kRvaMessages11 = 0x5012E18;  // Start menu labels
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
