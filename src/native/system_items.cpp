#include "native/system_items.hpp"

namespace cmfm::native {

ItemTableResult insert_item_after(const StartMenuItem* original, std::size_t max_read, std::int32_t anchor_id,
                                  const StartMenuItem& item, StartMenuItem* out, std::size_t out_capacity) {
    std::size_t length = 0;  // entries before the end marker
    bool ended = false;
    bool anchor = false;
    for (; length < max_read; ++length) {
        const std::int32_t id = original[length].id;
        if (id == kStartMenuEndMarker) {
            ended = true;
            break;
        }
        if (id == item.id) {
            return {ItemTableError::id_taken, 0};
        }
        anchor = anchor || id == anchor_id;
    }
    if (!ended) {
        return {ItemTableError::no_end_marker, 0};
    }
    if (!anchor) {
        return {ItemTableError::anchor_missing, 0};
    }
    const std::size_t count = length + 2;  // the new item and the end marker
    if (count > out_capacity) {
        return {ItemTableError::too_long, 0};
    }
    std::size_t w = 0;
    for (std::size_t r = 0; r <= length; ++r) {
        out[w++] = original[r];
        if (r < length && original[r].id == anchor_id) {
            out[w++] = item;
        }
    }
    return {ItemTableError::none, count};
}

const char* to_string(ItemTableError error) {
    switch (error) {
    case ItemTableError::none:
        return "ok";
    case ItemTableError::no_end_marker:
        return "no end marker";
    case ItemTableError::anchor_missing:
        return "anchor item missing";
    case ItemTableError::id_taken:
        return "item id already in the table";
    case ItemTableError::too_long:
        return "table too long";
    }
    return "unknown";
}

}  // namespace cmfm::native
