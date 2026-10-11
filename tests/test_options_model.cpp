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

// The numbers a model's rows hold: labels and values from L, help from H
// (the game's text file 23, plus 10000).
constexpr std::uint32_t L = kLabelFirst;
constexpr std::uint32_t H = kHelpFirst;
constexpr std::int32_t iL(std::uint32_t n) { return static_cast<std::int32_t>(L + n); }
constexpr std::int32_t iH(std::uint32_t n) { return static_cast<std::int32_t>(H + n); }

void builds_rows_in_game_format() {
    static OptionsModel m;
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kTabCount, kRows, 3, kFree, why), "build succeeds");
    const GameOptionRow* t0 = m.table(0);
    check(t0[0].kind == 1 && t0[0].setting_id == 100 && t0[0].value_count == 2, "row 0: kind 1, id 100");
    check(t0[0].label_id == iL(0) && t0[0].flag == 1 && t0[0].help_cat == 18 && t0[0].help_id == iH(0),
          "row 0 text numbers: file 23, far above the game's");
    check(t0[1].label_id == iL(3) && t0[1].help_id == iH(3) && t0[1].setting_id == 101,
          "row 1 follows row 0's values");
    check(L > kFile23Offset && L - kFile23Offset >= 0x10000000u && H + kTextSpan <= 0x7FFFFFFFu,
          "the numbers the game sends to file 23, within a signed int");
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
    check(same(m.label_text(L + 0), "Test toggle"), "label");
    check(same(m.label_text(L + 1), "Off") && same(m.label_text(L + 2), "On"), "values follow the label");
    check(same(m.label_text(L + 3), "Test choice") && same(m.label_text(L + 6), "Gamma"), "second row");
    check(same(m.label_text(L + 7), "Other tab") && same(m.label_text(L + 9), "On"), "third row on another tab");
    check(same(m.label_text(L + 10), "Free tab") && same(m.label_text(L + 11), "-"), "placeholder texts");
    check(m.label_text(L - 1) == nullptr && m.label_text(L + 16) == nullptr, "outside our ids");
    check(same(m.help_text(H + 0), "A test switch.") && same(m.help_text(H + 2), "A test switch."), "help and value help");
    check(m.help_text(H - 1) == nullptr && m.help_text(9002) == nullptr && m.label_text(9100) == nullptr,
          "numbers below the model's (the old file 12 and 18 ones too)");
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
    check(same(m.help_text(H + 0), "After.") && same(m.label_text(L + 1), "2 frames"), "texts read at lookup");
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
    // A tab holds a page of 1024 settings and its reset row; the views that
    // show a tab whole get its first 24 rows.
    static RowSpec many[kMaxRowsPerTab + 1];
    static const char* const labels[] = {"first", "x", "24th", "25th"};
    for (std::size_t r = 0; r <= kMaxRowsPerTab; ++r) {
        many[r] = {r == 0 ? labels[0] : r == kGameRowsPerTab - 1 ? labels[2] : r == kGameRowsPerTab ? labels[3] : labels[1],
                   "", kOnOff, 2, 0};
    }
    check(m.build("Mods", kTabs, kTabCount, many, kMaxRowsPerTab, kFree, why) && m.tab_rows(0) == kMaxRowsPerTab,
          "1025 rows on one tab");
    check(m.table(0)[kGameRowsPerTab].kind == kCmfmRowKind && m.table(0)[kMaxRowsPerTab].kind == kRowTableEnd,
          "the whole tab in its table");
    check(m.game_table(0)[kGameRowsPerTab].kind == kRowTableEnd &&
              same(m.label_text(static_cast<std::uint32_t>(m.game_table(0)[kGameRowsPerTab - 1].label_id)), "24th") &&
              m.game_table(0)[0].setting_id == m.table(0)[0].setting_id,
          "the game's table: the first 24 rows");
    check(m.game_table(1)[1].kind == kRowTableEnd && m.tab_rows(1) == 1, "a short tab whole in both");
    check(!m.build("Mods", kTabs, kTabCount, many, kMaxRowsPerTab + 1, kFree, why), "1026 rows on one tab refused");
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
    check(m.build("Mods", tabs, kMaxTabs, rows, 1, kFree, why) && m.tab_count() == kMaxTabs, "1025 tabs");
    check(m.table(kMaxTabs - 1)[0].value_count == 2 && m.table(kMaxTabs - 2)[0].value_count == 1,
          "the last tab's row, placeholders elsewhere");
    check(!m.build("Mods", tabs, kMaxTabs + 1, rows, 1, kFree, why), "1026 tabs refused");
    check(!m.build("Mods", tabs, 4, rows, 0, kFree, why), "fewer than 5 tabs refused");
    const RowSpec beyond[] = {{"x", "", kOnOff, 2, 7}};
    check(!m.build("Mods", tabs, kTabCount, beyond, 1, kFree, why), "a row beyond the tab count");
}

void holds_thousands_of_rows() {
    // Text numbers do not run out: 4 tabs of 1025 rows of 32 values each (the
    // old file 12 range held 900 texts).
    static OptionsModel m;
    static const char* values[kMaxValues];
    static const char* const names[] = {"v", "last value"};
    for (std::size_t v = 0; v < kMaxValues; ++v) {
        values[v] = v + 1 == kMaxValues ? names[1] : names[0];
    }
    constexpr std::size_t kRowsEach = kMaxRowsPerTab;
    static RowSpec rows[4 * kRowsEach];
    static const char* const labels[] = {"x", "the last row"};
    for (std::size_t i = 0; i < 4 * kRowsEach; ++i) {
        rows[i] = {i + 1 == 4 * kRowsEach ? labels[1] : labels[0], "", values, static_cast<std::uint32_t>(kMaxValues),
                   static_cast<std::uint32_t>(1 + i / kRowsEach)};
    }
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kTabCount, rows, 4 * kRowsEach, kFree, why) && m.row_count() == 4 * kRowsEach + 1,
          "4100 rows of 33 texts each");
    const GameOptionRow& last = m.table(4)[kRowsEach - 1];
    check(last.setting_id == static_cast<std::int32_t>(kSettingIdFirst + 4 * kRowsEach - 1) &&
              same(m.label_text(static_cast<std::uint32_t>(last.label_id)), "the last row") &&
              same(m.label_text(static_cast<std::uint32_t>(last.label_id) + kMaxValues), "last value") &&
              same(m.label_text(static_cast<std::uint32_t>(last.label_id) + kMaxValues + 1), "Free tab"),
          "the last row's texts found, then the placeholder's");
    check(is_cmfm_row(kCmfmRowKind, static_cast<std::uint32_t>(last.setting_id)), "its id is CMFM's");
    const RowSpec bad_tab[] = {{"x", "", kOnOff, 2, 9}};
    check(!m.build("Mods", kTabs, kTabCount, bad_tab, 1, kFree, why) && m.row_count() == 0 &&
              m.source(100) == OptionsModel::kNotOurs,
          "a failed build leaves no rows");
}

void knows_its_ids() {
    check(is_cmfm_row(1, 100) && is_cmfm_row(1, 299) && is_cmfm_row(1, kSettingIdLimit - 1),
          "kind 1, ids from 100 on");
    check(!is_cmfm_row(1, 34) && !is_cmfm_row(1, 99) && !is_cmfm_row(1, kSettingIdLimit), "game kind-1 ids pass on");
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
    check(t0[0].values[0].text == iL(1) && t0[0].values[100].text == iL(1) && t0[0].values[100].text_again == iL(1) &&
              t0[0].values[7].help_id == t0[0].help_id && t0[0].values[7].help_cat == 18,
          "every entry: the one value text and the row's help");
    check(same(m.label_text(L + 1), "50%"), "the shared text");
    now[0] = "51%";
    check(same(m.label_text(L + 1), "51%"), "read through the owner's pointer");
    check(t0[1].label_id == iL(2) && t0[1].help_id == iH(1), "the next row after two label ids and one help id");
    check(m.help_text(H + 0) != nullptr && same(m.help_text(H + 1), "A test switch."), "help ids");
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
    holds_thousands_of_rows();
    knows_its_ids();
    shares_number_texts();
    if (g_failures == 0) {
        std::printf("test_options_model: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
