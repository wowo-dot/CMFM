// Unit tests for cmfm::native::ModList (the Mods page's mod list and its
// dropdowns).
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
    check(l.rows().size() == 4 && l.open() == ModList::kNone, "four entries, nothing open");
    check(l.shown_mod() == 0 && l.shown_page() == 0 && l.shown_row() == 0, "the Overview shows");
    check(l.row_of(1, -1) == 1 && l.row_of(1, 0) == ModList::kNone, "pages of a folded mod are not listed");
}

void previews_without_moving_rows() {
    ModList l = sample();
    ModList::Result r = l.preview(1);  // DLSS highlighted
    check(r.changed && !r.enter, "DLSS highlighted: its default page shows");
    check(l.shown_mod() == 1 && l.shown_page() == 0 && l.shown_row() == 1, "marked on its entry");
    check(l.open() == ModList::kNone && l.rows().size() == 4, "no dropdown opens");
    r = l.preview(1);
    check(!r.changed, "the same row again: nothing");
    r = l.preview(2);  // HUD
    check(r.changed && l.shown_mod() == 2 && l.rows().size() == 4, "HUD shows; still no row moved");
    r = l.preview(0);
    check(r.changed && l.shown_mod() == 0, "back to the Overview");
    check(!l.preview(99).changed, "a row past the end does nothing");
}

void opens_a_dropdown_on_activate() {
    ModList l = sample();
    ModList::Result r = l.activate(1);  // DLSS
    check(r.changed && r.enter, "DLSS activated: its page shows, into the panel");
    check(l.open() == 1 && l.rows().size() == 7, "its dropdown: all three pages listed");
    check(l.rows()[2].mod == 1 && l.rows()[2].page == 0 && l.rows()[4].page == 2, "the default page first");
    check(l.shown_mod() == 1 && l.shown_page() == 0 && l.shown_row() == 2, "the default page shows, marked on its row");
    r = l.preview(4);  // DLSS page 3 highlighted
    check(r.changed && l.shown_page() == 2 && l.shown_row() == 4 && l.open() == 1, "a page of the open dropdown shows");
    r = l.activate(4);
    check(!r.changed && r.enter && l.open() == 1, "activated: into the panel, the dropdown stays");
    r = l.preview(1);  // DLSS's entry
    check(!r.changed && l.shown_page() == 2, "the shown mod's entry: its shown page stays");
    r = l.activate(1);
    check(!r.changed && r.enter && l.open() == 1 && l.shown_page() == 2, "activated: still that page, never folds");
}

void keeps_one_dropdown_open() {
    ModList l = sample();
    l.activate(1);  // DLSS open
    const std::size_t demo = l.row_of(3, -1);
    ModList::Result r = l.preview(demo);  // Demo highlighted
    check(r.changed && l.shown_mod() == 3 && l.open() == 1 && l.row_of(3, -1) == demo, "Demo shows; DLSS stays open");
    check(l.shown_row() == demo, "Demo's entry marks it (its dropdown is folded)");
    r = l.activate(demo);
    check(!r.changed && r.enter && l.open() == 3 && l.shown_page() == 0, "Demo activated: its dropdown opens");
    check(l.row_of(1, 0) == ModList::kNone && l.rows().size() == 6, "DLSS folded");
    r = l.activate(l.row_of(2, -1));  // HUD: one page
    check(r.changed && r.enter && l.open() == ModList::kNone && l.shown_mod() == 2, "a single page: dropdowns fold");
    r = l.activate(l.row_of(1, -1));
    check(r.changed && l.shown_mod() == 1 && l.shown_page() == 0 && l.open() == 1, "back to DLSS: its default page");
}

void steps_through_pages() {
    ModList l = sample();
    check(!l.step_page(1).changed, "the Overview has one page");
    l.preview(1);  // DLSS shows, folded
    ModList::Result r = l.step_page(1);
    check(r.changed && l.shown_page() == 1 && l.open() == 1, "E: the next page, the dropdown opens");
    l.step_page(1);
    l.step_page(1);
    check(l.shown_page() == 0, "wraps past the last page");
    l.step_page(-1);
    check(l.shown_page() == 2 && l.shown_row() == 4, "Q: back to the last page");
    check(!l.activate(99).changed, "a row past the end does nothing");
}

void selects_directly() {
    ModList l = sample();
    check(l.select(3, 1) && l.open() == 3 && l.shown_row() == l.row_of(3, 1), "Demo's second page, its dropdown open");
    check(l.select(2, 0) && l.open() == ModList::kNone && l.shown_row() == l.row_of(2, -1), "a single page: none open");
    check(!l.select(1, 3) && !l.select(9, 0) && l.shown_mod() == 2, "no such page: nothing changes");
}

}  // namespace

int main() {
    lists_one_entry_per_mod();
    previews_without_moving_rows();
    opens_a_dropdown_on_activate();
    keeps_one_dropdown_open();
    steps_through_pages();
    selects_directly();
    if (g_failures == 0) {
        std::printf("test_mod_list: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
