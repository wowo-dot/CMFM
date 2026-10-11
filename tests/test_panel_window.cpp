// Unit tests for cmfm::native::window_step (no game, no framework).
#include "native/panel_window.hpp"

#include <cstdio>

using namespace cmfm::native;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

bool is(const WindowStep& s, std::size_t top, std::size_t row, bool wrapped) {
    return s.top == top && s.row == row && s.wrapped == wrapped;
}

// 24 rows, all selectable: the demo's long page.
bool g_all[24];

void moves_inside_the_window() {
    check(is(window_step(g_all, 24, 14, 0, 0, 1, true, true), 0, 1, false), "down inside");
    check(is(window_step(g_all, 24, 14, 0, 5, 1, false, true), 0, 4, false), "up inside");
    check(is(window_step(g_all, 24, 14, 3, 13, 1, false, true), 3, 12, false), "up inside a moved window");
}

void scrolls_at_the_edges() {
    check(is(window_step(g_all, 24, 14, 0, 13, 1, true, true), 1, 13, false), "down on the bottom row scrolls");
    check(is(window_step(g_all, 24, 14, 10, 13, 1, true, false), 10, 13, false), "no wrap: stays on the last row");
    check(is(window_step(g_all, 24, 14, 5, 0, 1, false, true), 4, 0, false), "up on the top row scrolls");
}

void wraps_like_the_list() {
    check(is(window_step(g_all, 24, 14, 10, 13, 1, true, true), 0, 0, true), "down on the last row wraps to the top");
    check(is(window_step(g_all, 24, 14, 0, 0, 1, false, true), 10, 13, true), "up on the first row wraps to the end");
    check(is(window_step(g_all, 24, 14, 0, 0, 1, false, false), 0, 0, false), "no wrap: stays on the first row");
}

void skips_headings_and_shows_them() {
    // Rows: 0 heading, 1-9 rows, 10 heading, 11-19 rows.
    bool rows[20];
    for (bool& r : rows) {
        r = true;
    }
    rows[0] = false;
    rows[10] = false;
    // From row 9 (window 0-13) down: row 11, in view.
    check(is(window_step(rows, 20, 14, 0, 9, 1, true, true), 0, 11, false), "down over a heading");
    // From row 13 (window 0-13) down: row 14; the window moves by one.
    check(is(window_step(rows, 20, 14, 0, 13, 1, true, true), 1, 13, false), "down past the bottom");
    // Window 6-19, cursor on row 11 (window row 5), up: row 9, and the
    // window keeps row 10's heading in view (no move needed).
    check(is(window_step(rows, 20, 14, 6, 5, 1, false, true), 6, 3, false), "up over a heading inside");
    // Window 1-14 (heading 0 out of view), cursor on row 1, up, no wrap:
    // stays, and the window shows the heading above it.
    check(is(window_step(rows, 20, 14, 1, 0, 1, false, false), 0, 1, false), "first row: its heading comes into view");
    // Window 6-19, cursor on row 19, down with wrap: row 1, window at 0 for
    // the heading.
    check(is(window_step(rows, 20, 14, 6, 13, 1, true, true), 0, 1, true), "wrap to the top shows the heading");
    // A heading just above the window while the cursor goes up to the row
    // under it: the window moves up to show it (30 rows, headings at 0, 10
    // and 20; window 11-24, cursor on row 12).
    bool more[30];
    for (bool& r : more) {
        r = true;
    }
    more[0] = more[10] = more[20] = false;
    check(is(window_step(more, 30, 14, 11, 1, 1, false, true), 10, 1, false), "row under a heading shows it");
}

void page_steps() {
    check(is(window_step(g_all, 24, 14, 0, 0, 13, true, false), 0, 13, false), "13 down");
    check(is(window_step(g_all, 24, 14, 0, 13, 13, true, false), 10, 13, false), "13 more: the last row");
    check(is(window_step(g_all, 24, 14, 10, 13, 13, true, false), 10, 13, false), "at the end: stays");
}

void small_tabs_need_no_window() {
    check(is(window_step(g_all, 5, 14, 0, 4, 1, true, true), 0, 0, true), "5 rows wrap");
    check(is(window_step(g_all, 5, 14, 0, 2, 1, true, true), 0, 3, false), "5 rows step");
    const bool none[3] = {false, false, false};
    check(is(window_step(none, 3, 14, 0, 1, 1, true, true), 0, 1, false), "no selectable row: stays");
    check(window_top_for(g_all, 5, 14, 0, 4) == 0, "small tab: top 0");
    check(window_top_for(g_all, 24, 14, 0, 20) == 7, "row 20 at the bottom");
    check(window_top_for(g_all, 24, 14, 7, 3) == 3, "row 3 at the top");
}

}  // namespace

int main() {
    for (bool& r : g_all) {
        r = true;
    }
    moves_inside_the_window();
    scrolls_at_the_edges();
    wraps_like_the_list();
    skips_headings_and_shows_them();
    page_steps();
    small_tabs_need_no_window();
    if (g_failures != 0) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("panel window: all tests passed\n");
    return 0;
}
