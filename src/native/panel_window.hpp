// The Mods page's panel window: a tab with more rows than the panel shows
// is shown as a window over its rows that the cursor moves (roadmap step 5c;
// run 20261011-longpage-1: the game draws all rows and never scrolls the
// panel). The game's list steps its cursor over the rows it holds
// (sub_140B75BD0 next, sub_140B75C80 previous: selectable rows only, wrapping
// when the list wraps); window_step does the same over the whole tab and
// says where the window has to be. Pure logic, tested in
// tests/test_panel_window.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cmfm::native {

// Rows the panel shows at most: 14 rows of 52 (design pixels) from the
// panel's top end above the help line (run 20261011-longpage-1: the 15th row
// touches it).
inline constexpr std::size_t kPanelRows = 14;

struct WindowStep {
    std::size_t top = 0;  // the window's first row in the tab
    std::size_t row = 0;  // the cursor's row in the window
    bool wrapped = false;
};

// The cursor at `cursor` (in a window at `top` showing `visible` of `count`
// rows) moves `steps` selectable rows down (or up), wrapping past the ends
// when `wrap` is set and stopping at the last selectable row otherwise, as
// the game's list does. The window then moves as little as possible to
// show the new row, and also shows the headings (unselectable rows) right
// above it while the row stays in view. `selectable` has `count` entries;
// a tab without a selectable row keeps the cursor where it is.
WindowStep window_step(const bool* selectable, std::size_t count, std::size_t visible, std::size_t top,
                       std::size_t cursor, std::uint32_t steps, bool down, bool wrap);

// The window at `top` for the row `index`: as little movement as possible,
// headings right above it in view (window_step's rule, for a cursor placed
// directly).
std::size_t window_top_for(const bool* selectable, std::size_t count, std::size_t visible, std::size_t top,
                           std::size_t index);

}  // namespace cmfm::native
