#include "native/panel_window.hpp"

namespace cmfm::native {

std::size_t window_top_for(const bool* selectable, std::size_t count, std::size_t visible, std::size_t top,
                           std::size_t index) {
    if (visible == 0 || count <= visible) {
        return 0;
    }
    const std::size_t last_top = count - visible;
    if (index < top) {
        top = index;
    } else if (index >= top + visible) {
        top = index - visible + 1;
    }
    // The headings right above the row, as long as the row stays in view.
    while (top > 0 && !selectable[top - 1] && index - (top - 1) < visible) {
        --top;
    }
    return top > last_top ? last_top : top;
}

WindowStep window_step(const bool* selectable, std::size_t count, std::size_t visible, std::size_t top,
                       std::size_t cursor, std::uint32_t steps, bool down, bool wrap) {
    WindowStep result{top, cursor, false};
    const std::size_t from = top + cursor;
    if (count == 0 || from >= count) {
        return result;
    }
    // As sub_140B75BD0 / sub_140B75C80 over the whole tab: count selectable
    // rows from the cursor; past an end, go on from the other end when the
    // list wraps, otherwise stop there; at most one full turn.
    std::size_t found = from;
    std::uint32_t counted = 0;
    std::size_t i = from;
    for (std::size_t turn = 0; turn < count && counted < (steps == 0 ? 1u : steps); ++turn) {
        if (down) {
            if (i + 1 >= count) {
                if (!wrap) {
                    break;
                }
                result.wrapped = true;
                i = 0;
            } else {
                ++i;
            }
        } else {
            if (i == 0) {
                if (!wrap) {
                    break;
                }
                result.wrapped = true;
                i = count - 1;
            } else {
                --i;
            }
        }
        if (selectable[i]) {
            found = i;
            ++counted;
        }
    }
    if (found == from) {
        result.wrapped = false;
    }
    // (Wrapped to the start, the heading above the first selectable row comes
    // into view by the same rule.)
    result.top = window_top_for(selectable, count, visible, top, found);
    result.row = found - result.top;
    return result;
}

}  // namespace cmfm::native
