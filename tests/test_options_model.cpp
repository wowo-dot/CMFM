// Unit tests for cmfm::native::OptionsModel (no game, no framework).
#include "native/options_model.hpp"

#include <cstdio>
#include <cstring>

using namespace cmfm::native;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

bool same(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

const char* const kOnOff[] = {"Off", "On"};
const char* const kAbc[] = {"Alpha", "Beta", "Gamma"};
const TabSpec kTabs[kTabCount] = {{"CMFM", "CMFM's rows."}, {"", nullptr}, {"", ""}, {"", ""}, {"", ""}};
const RowSpec kRows[] = {
    {"Test toggle", "A test switch.", kOnOff, 2, 0},
    {"Test choice", "A test choice.", kAbc, 3, 0},
    {"Other tab", "On tab 2.", kOnOff, 2, 2},
};
const char* const kDash[] = {"-"};
const RowSpec kFree = {"Free tab", "Nothing here.", kDash, 1, 0};

void builds_rows_in_game_format() {
    static OptionsModel m;
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kTabCount, kRows, 3, kFree, why), "build succeeds");
    const GameOptionRow* t0 = m.table(0);
    check(t0[0].kind == 1 && t0[0].setting_id == 100 && t0[0].value_count == 2, "row 0: kind 1, id 100");
    check(t0[0].label_id == 9100 && t0[0].flag == 1 && t0[0].help_cat == 18 && t0[0].help_id == 9002,
          "row 0 text ids");
    check(t0[1].label_id == 9103 && t0[1].help_id == 9005 && t0[1].setting_id == 101, "row 1 follows row 0's values");
    check(t0[2].kind == kRowTableEnd, "tab 0 ends after 2 rows");
    check(m.table(2)[0].setting_id == 102 && m.table(2)[1].kind == kRowTableEnd, "tab 2 has its row");
    // No tab may be empty: the game reads the row under the cursor unchecked.
    for (std::size_t t = 0; t < kTabCount; ++t) {
        check(m.table(t)[0].kind == kCmfmRowKind, "every tab has a first row");
    }
    check(m.table(1)[0].value_count == 1 && m.table(1)[1].kind == kRowTableEnd, "tab 1 gets the placeholder");
    check(m.row_count() == 6, "3 rows + 3 placeholders (tabs 1, 3, 4)");
}

void maps_ids_to_rows() {
    static OptionsModel m;
    const char* why = nullptr;
    m.build("Mods", kTabs, kTabCount, kRows, 3, kFree, why);
    check(m.source(100) == 0 && m.source(101) == 1 && m.source(102) == 2, "input rows");
    check(m.source(103) == OptionsModel::kPlaceholder, "placeholder");
    check(m.source(106) == OptionsModel::kNotOurs && m.source(99) == OptionsModel::kNotOurs &&
              m.source(0) == OptionsModel::kNotOurs,
          "outside the model");
    check(m.value_count(101) == 3 && m.value_count(103) == 1 && m.value_count(5) == 0, "value counts");
}

void resolves_texts() {
    static OptionsModel m;
    const char* why = nullptr;
    m.build("Mods", kTabs, kTabCount, kRows, 3, kFree, why);
    check(same(m.label_text(9100), "Test toggle"), "label");
    check(same(m.label_text(9101), "Off") && same(m.label_text(9102), "On"), "values follow the label");
    check(same(m.label_text(9103), "Test choice") && same(m.label_text(9106), "Gamma"), "second row");
    check(same(m.label_text(9107), "Other tab") && same(m.label_text(9109), "On"), "third row on another tab");
    check(same(m.label_text(9110), "Free tab") && same(m.label_text(9111), "-"), "placeholder texts");
    check(m.label_text(9099) == nullptr && m.label_text(9116) == nullptr, "outside our ids");
    check(same(m.help_text(9002), "A test switch.") && same(m.help_text(9004), "A test switch."), "help and value help");
    check(m.help_text(9001) == nullptr, "9001 is not the model's");
    check(same(m.header(), "Mods") && same(m.tab_name(0), "CMFM") && same(m.tab_name(1), ""), "header and tabs");
    check(same(m.tab_help(0), "CMFM's rows.") && same(m.tab_help(1), "") && m.tab_help(5) == nullptr, "tab help");
}

void follows_rewritten_texts() {
    // CMFM rewrites help lines and status texts in place.
    static OptionsModel m;
    static char help[32] = "Before.";
    static char value[32] = "1 frame";
    static const char* values[] = {value};
    const RowSpec rows[] = {{"Status", help, values, 1, 0}};
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kTabCount, rows, 1, kFree, why), "build");
    std::snprintf(help, sizeof help, "After.");
    std::snprintf(value, sizeof value, "2 frames");
    check(same(m.help_text(9002), "After.") && same(m.label_text(9101), "2 frames"), "texts read at lookup");
}

void rejects_bad_rows() {
    static OptionsModel m;
    const char* why = nullptr;
    const RowSpec bad_tab[] = {{"x", "", kOnOff, 2, 5}};
    check(!m.build("Mods", kTabs, kTabCount, bad_tab, 1, kFree, why), "tab out of range");
    const RowSpec no_values[] = {{"x", "", kOnOff, 0, 0}};
    check(!m.build("Mods", kTabs, kTabCount, no_values, 1, kFree, why), "no values");
    const RowSpec no_help[] = {{"x", nullptr, kOnOff, 2, 0}};
    check(!m.build("Mods", kTabs, kTabCount, no_help, 1, kFree, why), "help must be a string");
    const RowSpec bad_free = {"x", "", kOnOff, 0, 0};
    check(!m.build("Mods", kTabs, kTabCount, kRows, 3, bad_free, why), "invalid placeholder");
    check(m.build("Mods", kTabs, kTabCount, nullptr, 0, kFree, why) && m.row_count() == 5, "no rows: 5 placeholders");
    // The title Options page holds 24 rows per tab.
    RowSpec many[kMaxRowsPerTab + 1];
    for (RowSpec& s : many) {
        s = {"x", "", kOnOff, 2, 0};
    }
    check(m.build("Mods", kTabs, kTabCount, many, kMaxRowsPerTab, kFree, why), "24 rows on one tab");
    check(!m.build("Mods", kTabs, kTabCount, many, kMaxRowsPerTab + 1, kFree, why), "25 rows on one tab refused");
}

void builds_more_tabs() {
    // The title page scrolls: up to kMaxTabs tabs, each with a row.
    static OptionsModel m;
    static TabSpec tabs[kMaxTabs];
    for (std::size_t t = 0; t < kMaxTabs; ++t) {
        tabs[t] = {"T", "H"};
    }
    const RowSpec rows[] = {{"Last", "On the last tab.", kOnOff, 2, kMaxTabs - 1}};
    const char* why = nullptr;
    check(m.build("Mods", tabs, kMaxTabs, rows, 1, kFree, why) && m.tab_count() == kMaxTabs, "25 tabs");
    check(m.table(kMaxTabs - 1)[0].value_count == 2 && m.table(kMaxTabs - 2)[0].value_count == 1,
          "the last tab's row, placeholders elsewhere");
    check(!m.build("Mods", tabs, kMaxTabs + 1, rows, 1, kFree, why), "26 tabs refused");
    check(!m.build("Mods", tabs, 4, rows, 0, kFree, why), "fewer than 5 tabs refused");
    const RowSpec beyond[] = {{"x", "", kOnOff, 2, 7}};
    check(!m.build("Mods", tabs, kTabCount, beyond, 1, kFree, why), "a row beyond the tab count");
}

void runs_out_of_text_ids() {
    // 900 label ids: 4 tabs x 24 rows x (1 + 32 values) do not fit.
    static OptionsModel m;
    static const char* values[kMaxValues];
    for (const char*& v : values) {
        v = "v";
    }
    static RowSpec rows[4 * kMaxRowsPerTab];
    for (std::size_t i = 0; i < 4 * kMaxRowsPerTab; ++i) {
        rows[i] = {"x", "", values, static_cast<std::uint32_t>(kMaxValues), static_cast<std::uint32_t>(1 + i / kMaxRowsPerTab)};
    }
    const char* why = nullptr;
    check(!m.build("Mods", kTabs, kTabCount, rows, 4 * kMaxRowsPerTab, kFree, why) && same(why, "text ids exhausted"),
          "text ids exhausted");
    check(m.row_count() == 0 && m.source(100) == OptionsModel::kNotOurs, "a failed build leaves no rows");
}

void knows_its_ids() {
    check(is_cmfm_row(1, 100) && is_cmfm_row(1, 299), "kind 1, ids 100-299");
    check(!is_cmfm_row(1, 34) && !is_cmfm_row(1, 99) && !is_cmfm_row(1, 300), "game kind-1 ids pass on");
    check(!is_cmfm_row(0, 100) && !is_cmfm_row(2, 100) && !is_cmfm_row(7, 100), "other kinds pass on");
}

void shares_number_texts() {
    static OptionsModel m;
    static const char* now[] = {"50%"};
    const RowSpec rows[] = {
        {"Fine", "A fine number.", now, 101, 0, true},
        {"Test toggle", "A test switch.", kOnOff, 2, 0},
    };
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kTabCount, rows, 2, kFree, why), "builds with a shared row");
    const GameOptionRow* t0 = m.table(0);
    check(t0[0].value_count == 101 && t0[0].values != nullptr, "101 values with a per-value table");
    check(t0[0].values[0].text == 9101 && t0[0].values[100].text == 9101 && t0[0].values[100].text_again == 9101 &&
              t0[0].values[7].help_id == t0[0].help_id && t0[0].values[7].help_cat == 18,
          "every entry: the one value text and the row's help");
    check(same(m.label_text(9101), "50%"), "the shared text");
    now[0] = "51%";
    check(same(m.label_text(9101), "51%"), "read through the owner's pointer");
    check(t0[1].label_id == 9102 && t0[1].help_id == 9003, "the next row after two label ids and one help id");
    check(m.help_text(9002) != nullptr && same(m.help_text(9003), "A test switch."), "help ids");
    check(t0[1].values == nullptr, "other rows have no table");
    const RowSpec too_many[] = {{"Fine", "A fine number.", now, 102, 0, true}};
    check(!m.build("Mods", kTabs, kTabCount, too_many, 1, kFree, why), "102 shared values refused");
    const RowSpec plain_many[] = {{"Plain", "Not shared.", now, 33, 0}};
    check(!m.build("Mods", kTabs, kTabCount, plain_many, 1, kFree, why), "33 own values refused");
}

}  // namespace

int main() {
    builds_rows_in_game_format();
    maps_ids_to_rows();
    resolves_texts();
    follows_rewritten_texts();
    rejects_bad_rows();
    builds_more_tabs();
    runs_out_of_text_ids();
    knows_its_ids();
    shares_number_texts();
    if (g_failures == 0) {
        std::printf("test_options_model: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
