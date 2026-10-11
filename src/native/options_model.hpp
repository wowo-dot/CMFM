// The layout of a CMFM page in the game's own row format (40-byte rows,
// docs/research/native-menu.md): 5 tabs of rows, each row a label, its value
// names and a help line, with text ids CMFM answers through its text hook.
// Values are not kept here (menu/menu.hpp reads them from the plugins).
// Pure logic, tested in tests/test_options_model.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmfm::native {

// A value's texts in the game's per-value table (sub_140EEA620: the text id
// at +4, or +0 if that is -1; sub_140EEB160: the help id and file).
struct GameValueText {
    std::int32_t text;
    std::int32_t text_again;
    std::int32_t help_id;
    std::int32_t help_cat;
};
static_assert(sizeof(GameValueText) == 16);

// The game's Options row (cmfm_OptionRow in IDA).
struct GameOptionRow {
    std::int32_t kind;  // -1 ends a table
    std::int32_t label_id;
    std::int32_t flag;  // 1: value texts follow the label id
    std::int32_t help_cat;
    std::int32_t help_id;
    std::int32_t setting_id;
    std::int32_t value_count;
    std::int32_t pad;
    // One entry per value, read instead of the ids that follow the label and
    // help ids; nullptr: those.
    const GameValueText* values;
};
static_assert(sizeof(GameOptionRow) == 40);
static_assert(offsetof(GameOptionRow, values) == 32);

// CMFM rows are kind 1 (system settings) with setting ids from 100: both
// Options pages keep kind-1 rows (the title page drops unknown kinds), and
// the game's own kind-1 rows use ids 0-34. The getter and setter hooks answer
// these ids, so they never reach the game (a missed one would index sUserData
// out of range); every other use of a row's id in the game compares it with
// fixed numbers (docs/research/native-menu.md, "No caps"), so the range can
// be large.
inline constexpr std::int32_t kCmfmRowKind = 1;
inline constexpr std::uint32_t kSettingIdFirst = 100;
inline constexpr std::size_t kMaxRows = std::size_t{1} << 20;  // rows in the model, over all tabs
inline constexpr std::uint32_t kSettingIdLimit = kSettingIdFirst + static_cast<std::uint32_t>(kMaxRows);
inline constexpr std::int32_t kRowTableEnd = -1;
inline constexpr std::int32_t kHelpCategory = 18;

// Text numbers. The game sends any text number above 10000 to its message
// file 23, as the number minus 10000: row labels and values
// (sub_14044A280) and help lines (the help bar, sub_141D91740) alike. CMFM's
// row texts live there, far above the game's own few thousand: labels and
// value names from kLabelFirst, help lines from kHelpFirst (row numbers, as
// the rows hold them; the text hook gets them minus kFile23Offset).
inline constexpr std::uint32_t kFile23Offset = 10000;
inline constexpr std::uint32_t kTextSpan = 0x10000000;  // numbers per range
inline constexpr std::uint32_t kLabelFirst = kFile23Offset + kTextSpan;
inline constexpr std::uint32_t kHelpFirst = kFile23Offset + 2 * kTextSpan;
static_assert(kHelpFirst + kTextSpan <= 0x7FFFFFFFu);  // the help bar takes signed numbers

inline constexpr std::size_t kTabCount = 5;  // the in-game Options page has 5 fixed tabs
// The game's tab object holds 24 rows: what the views that show a tab whole
// (the game's Options page, CMFM's page without its own layout) can show.
inline constexpr std::size_t kGameRowsPerTab = 24;
// A tab in CMFM's model: a page of 1024 settings (CMFM_MAX_SETTINGS) and its
// reset row, or the Overview's rows. CMFM's Mods page shows them through its
// panel window (native/panel_window.hpp), 14 at a time.
inline constexpr std::size_t kMaxRowsPerTab = 1025;
inline constexpr std::size_t kMaxTabs = 1025;  // the Overview and 1024 pages
inline constexpr std::size_t kMaxValues = 32;
// Rows whose values share one text (numbers, as CMFM writes the current
// value) hold up to 101 values, the game slider's positions 0-100. Their
// per-value table has an entry for each position: the game reads a slider
// row's text at its 0-100 value (the getter on the slider's scale).
inline constexpr std::size_t kMaxSharedValues = 101;

// True for a row CMFM answers (kind 1, ids from 100).
constexpr bool is_cmfm_row(std::int32_t kind, std::uint32_t setting_id) {
    return kind == kCmfmRowKind && setting_id >= kSettingIdFirst && setting_id < kSettingIdLimit;
}

// All pointers must stay valid while the model is in use; the texts they
// point to may change (CMFM rewrites help lines and status texts).
struct TabSpec {
    const char* name;
    const char* help;  // the title page's category description
};

struct RowSpec {
    const char* label;
    const char* help;
    const char* const* values;  // value_count names, or one if shared
    std::uint32_t value_count;  // 1..kMaxValues, or 1..kMaxSharedValues if shared
    std::uint32_t tab;          // 0..kTabCount-1
    // The values share values[0], a text its owner rewrites as the value
    // changes (numbers): one text id and one help id for all of them.
    bool shared = false;
};

class OptionsModel {
public:
    // Builds the model with `tab_count` tabs (kTabCount..kMaxTabs; `tabs` has
    // that many); false (with `why`) if a row is invalid, a tab holds more
    // than kMaxRowsPerTab rows or there are more than kMaxRows rows. A tab without rows gets `placeholder` (its `tab` is ignored):
    // the game's Options page reads the row under the cursor without a null
    // check, so a tab must never be empty (run 20261011-native-m2 crashed on
    // one).
    bool build(const char* header, const TabSpec* tabs, std::size_t tab_count, const RowSpec* rows,
               std::size_t count, const RowSpec& placeholder, const char*& why);

    // The game-format table for a tab, ended by a row with kind -1 (valid
    // until the next build).
    const GameOptionRow* table(std::size_t tab) const { return tables_[tab].data(); }
    // The same, cut after kGameRowsPerTab rows, for views that show a tab
    // whole; and how many rows the tab has.
    const GameOptionRow* game_table(std::size_t tab) const { return game_tables_[tab].data(); }
    std::size_t tab_rows(std::size_t tab) const { return tab < tab_count_ ? tables_[tab].size() - 1 : 0; }

    // Texts by the number a row holds (kLabelFirst.., kHelpFirst..); nullptr
    // if the number is not ours.
    const char* label_text(std::uint32_t id) const;  // labels and values
    const char* help_text(std::uint32_t id) const;   // row and value help
    const char* header() const { return header_; }
    std::size_t tab_count() const { return tab_count_; }
    const char* tab_name(std::size_t tab) const { return tab < tab_count_ ? tabs_[tab].name : nullptr; }
    const char* tab_help(std::size_t tab) const { return tab < tab_count_ ? tabs_[tab].help : nullptr; }

    // Rows in the model, placeholders included.
    std::size_t row_count() const { return count_; }
    // Which input row a setting id shows: its index in `rows`, kPlaceholder,
    // or kNotOurs.
    static constexpr std::int32_t kPlaceholder = -1;
    static constexpr std::int32_t kNotOurs = -2;
    std::int32_t source(std::uint32_t setting_id) const;
    // The row's value count (0 if the id is not ours).
    std::uint32_t value_count(std::uint32_t setting_id) const;

private:
    // The row whose numbers (in `firsts`, ascending) hold `id`, or count_.
    std::size_t row_of(const std::vector<std::uint32_t>& firsts, std::uint32_t id) const;

    const char* header_ = "";
    std::vector<TabSpec> tabs_;
    std::size_t tab_count_ = 0;
    // Per row, in setting-id order (their text numbers ascending).
    std::vector<RowSpec> specs_;
    std::vector<std::int32_t> sources_;
    std::vector<std::uint32_t> label_ids_;
    std::vector<std::uint32_t> help_ids_;
    std::size_t count_ = 0;
    std::uint32_t label_end_ = kLabelFirst;
    std::uint32_t help_end_ = kHelpFirst;
    // Per tab: its rows plus the end row; the first kGameRowsPerTab of them
    // plus the end row.
    std::vector<std::vector<GameOptionRow>> tables_;
    std::vector<std::vector<GameOptionRow>> game_tables_;
    // The shared rows' per-value tables, kMaxSharedValues entries each
    // (sized once per build: the tables point into it).
    std::vector<GameValueText> shared_values_;
};

}  // namespace cmfm::native
