// The Start menu's item tables: 16-byte entries {id, label text id, help text
// id, extra}, ended by an entry with id 76. Building a copy with one more
// entry is pure logic, tested in tests/test_system_items.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cmfm::native {

struct StartMenuItem {
    std::int32_t id;
    std::int32_t label;
    std::int32_t help;
    std::int32_t extra;
};
static_assert(sizeof(StartMenuItem) == 16);

inline constexpr std::int32_t kStartMenuEndMarker = 76;

enum class ItemTableError {
    none,
    no_end_marker,   // no end marker within max_read entries
    anchor_missing,  // anchor_id not in the table
    id_taken,        // the new item's id is already in the table
    too_long,        // the copy does not fit into out_capacity
};

struct ItemTableResult {
    ItemTableError error;
    std::size_t count;  // entries written, end marker included; 0 on error
};

// Copies `original` (read up to max_read entries, through its end marker)
// into `out` with `item` inserted right after the entry whose id is
// anchor_id. Writes nothing to `out` on error.
ItemTableResult insert_item_after(const StartMenuItem* original, std::size_t max_read, std::int32_t anchor_id,
                                  const StartMenuItem& item, StartMenuItem* out, std::size_t out_capacity);

const char* to_string(ItemTableError error);

}  // namespace cmfm::native
