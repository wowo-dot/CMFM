// Unit tests for cmfm::native::insert_item_after (no game, no framework).
#include "native/system_items.hpp"

#include <cstdio>

using cmfm::native::insert_item_after;
using cmfm::native::ItemTableError;
using cmfm::native::kStartMenuEndMarker;
using cmfm::native::StartMenuItem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// The System tab as read from exe 421810 (run 20261011-gui-inspect).
const StartMenuItem kSystem[] = {
    {71, 35, 259, 0},  {10, 188, 988, 989},       {72, 8, 9, 10},     {73, 37, 261, 773},
    {74, 370, 10371, 10372}, {75, 375, 10376, 10377}, {kStartMenuEndMarker, 0, 0, 0},
    {39, 121, 694, 761},  // the next table in memory (View Tutorials)
};
const StartMenuItem kMods = {77, 9000, 9001, 0};

void inserts_after_options() {
    StartMenuItem out[24] = {};
    const auto r = insert_item_after(kSystem, 16, 71, kMods, out, 24);
    check(r.error == ItemTableError::none, "insert succeeds");
    check(r.count == 8, "6 items + Mods + end marker");
    const int ids[] = {71, 77, 10, 72, 73, 74, 75, kStartMenuEndMarker};
    for (int i = 0; i < 8; ++i) {
        check(out[i].id == ids[i], "order Options, Mods, View Mode, ...");
    }
    check(out[1].label == 9000 && out[1].help == 9001, "Mods keeps its text ids");
    check(out[2].label == 188 && out[2].extra == 989, "other entries copied whole");
}

void anchor_missing() {
    StartMenuItem out[24] = {};
    const auto r = insert_item_after(kSystem, 16, 999, kMods, out, 24);
    check(r.error == ItemTableError::anchor_missing && r.count == 0, "missing anchor is refused");
    check(out[0].id == 0, "nothing written on error");
}

void id_taken() {
    const StartMenuItem table[] = {{71, 1, 2, 0}, {77, 3, 4, 0}, {kStartMenuEndMarker, 0, 0, 0}};
    StartMenuItem out[24] = {};
    const auto r = insert_item_after(table, 16, 71, kMods, out, 24);
    check(r.error == ItemTableError::id_taken, "an id already present is refused");
}

void no_end_marker() {
    const StartMenuItem table[] = {{71, 1, 2, 0}, {10, 3, 4, 0}, {72, 5, 6, 0}};
    StartMenuItem out[24] = {};
    const auto r = insert_item_after(table, 3, 71, kMods, out, 24);
    check(r.error == ItemTableError::no_end_marker, "a table without end marker is refused");
}

void too_long() {
    StartMenuItem out[7] = {};
    const auto r = insert_item_after(kSystem, 16, 71, kMods, out, 7);
    check(r.error == ItemTableError::too_long, "a copy that does not fit is refused");
}

void anchor_last() {
    const StartMenuItem table[] = {{10, 1, 2, 0}, {71, 3, 4, 0}, {kStartMenuEndMarker, 0, 0, 0}};
    StartMenuItem out[24] = {};
    const auto r = insert_item_after(table, 16, 71, kMods, out, 24);
    check(r.error == ItemTableError::none && r.count == 4, "anchor as the last item");
    check(out[2].id == 77 && out[3].id == kStartMenuEndMarker, "Mods before the end marker");
}

}  // namespace

int main() {
    inserts_after_options();
    anchor_missing();
    id_taken();
    no_end_marker();
    too_long();
    anchor_last();
    if (g_failures == 0) {
        std::printf("test_system_items: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
