// Unit tests for cmfm::native::ModList (the Mods page's mod list and its
// page tabs).
#include "native/mod_list.hpp"

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

// The Overview, DLSS (3 pages), HUD (1 page), Demo (2 pages).
ModList sample() {
    ModList l;
    l.reset({1, 3, 1, 2});
    return l;
}

void lists_one_entry_per_mod() {
    ModList l = sample();
    check(l.mod_count() == 4 && l.page_count(1) == 3 && l.page_count(9) == 0, "four mods, their page counts");
    check(l.shown_mod() == 0 && l.shown_page() == 0, "the Overview shows");
}

void previews_and_activates() {
    ModList l = sample();
    ModList::Result r = l.preview(1);  // DLSS highlighted
    check(r.changed && !r.enter && l.shown_mod() == 1 && l.shown_page() == 0, "DLSS shows on its first page");
    check(!l.preview(1).changed, "the same mod again: nothing");
    r = l.activate(1);
    check(!r.changed && r.enter, "Confirm on what shows: into the panel");
    r = l.activate(2);
    check(r.changed && r.enter && l.shown_mod() == 2, "Confirm on another mod: it shows, into the panel");
    check(!l.preview(99).changed && !l.activate(99).enter, "a row past the end does nothing");
}

void remembers_each_mods_page() {
    ModList l = sample();
    l.preview(1);
    ModList::Result r = l.show_page(2);  // DLSS's third tab
    check(r.changed && l.shown_page() == 2, "a tab: that page");
    check(!l.show_page(2).changed && !l.show_page(3).changed, "the shown tab, or none: nothing");
    l.preview(3);
    check(l.shown_page() == 0, "Demo on its first page");
    l.preview(1);
    check(l.shown_page() == 2 && l.page_of(3) == 0, "back to DLSS: the page it showed last");
    l.reset({1, 3, 1, 2});
    check(l.shown_mod() == 0 && l.page_of(1) == 2, "the page opens again: the same mods keep their pages");
    l.reset({1, 3, 2});
    check(l.page_of(1) == 0, "other mods: every mod on its first page");
}

void steps_through_pages() {
    ModList l = sample();
    check(!l.step_page(1).changed, "the Overview has one page");
    l.preview(1);
    ModList::Result r = l.step_page(1);
    check(r.changed && l.shown_page() == 1, "E: the next page");
    l.step_page(1);
    l.step_page(1);
    check(l.shown_page() == 0, "wraps past the last page");
    l.step_page(-1);
    check(l.shown_page() == 2, "Q: back to the last page");
}

void selects_directly() {
    ModList l = sample();
    check(l.select(3, 1) && l.shown_mod() == 3 && l.shown_page() == 1, "Demo's second page");
    check(!l.select(1, 3) && !l.select(9, 0) && l.shown_mod() == 3, "no such page: nothing changes");
}

void keeps_the_shown_tab_in_view() {
    check(tab_window_top(0, 2, 3, 4) == 0, "everything fits: from the first tab");
    check(tab_window_top(0, 3, 7, 4) == 0, "the fourth tab of seven: still the first window");
    check(tab_window_top(0, 4, 7, 4) == 1, "the fifth: the window moves by one");
    check(tab_window_top(3, 6, 7, 4) == 3, "the last: the last window");
    check(tab_window_top(3, 1, 7, 4) == 1, "back to the second: the window follows");
    check(tab_window_top(5, 5, 7, 4) == 3, "never past the last window");
}

}  // namespace

int main() {
    lists_one_entry_per_mod();
    previews_and_activates();
    remembers_each_mods_page();
    steps_through_pages();
    selects_directly();
    keeps_the_shown_tab_in_view();
    if (g_failures == 0) {
        std::printf("test_mod_list: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
