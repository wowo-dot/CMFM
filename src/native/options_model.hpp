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
// out of range).
inline constexpr std::int32_t kCmfmRowKind = 1;
inline constexpr std::uint32_t kSettingIdFirst = 100;
inline constexpr std::uint32_t kSettingIdLimit = 300;
inline constexpr std::int32_t kRowTableEnd = -1;
inline constexpr std::int32_t kHelpCategory = 18;
inline constexpr std::uint32_t kLabelIdFirst = 9100;  // file 12
inline constexpr std::uint32_t kHelpIdFirst = 9002;   // file 18
inline constexpr std::uint32_t kTextIdLimit = 10000;  // GetGameMessage sends larger ids to file 23
// Rows' help ids stay below this; file 18 ids 9996-9999 are the Mods page's
// own help lines (native/mods_page.cpp).
inline constexpr std::uint32_t kHelpIdLimit = 9996;

inline constexpr std::size_t kTabCount = 5;        // the in-game Options page has 5 fixed tabs
inline constexpr std::size_t kMaxTabs = 25;        // the title page scrolls through more
// The game's tab object holds 24 rows: what the views that show a tab whole
// (the game's Options page, CMFM's page without its own layout) can show.
inline constexpr std::size_t kGameRowsPerTab = 24;
// A tab in CMFM's model: a page of 64 settings (CMFM_MAX_SETTINGS) and its
// reset row. CMFM's Mods page shows them through its panel window
// (native/panel_window.hpp), 14 at a time.
inline constexpr std::size_t kMaxRowsPerTab = 65;
inline constexpr std::size_t kMaxRows = 200;       // setting ids 100-299
inline constexpr std::size_t kMaxValues = 32;
// Rows whose values share one text (numbers, as CMFM writes the current
// value) hold up to 101 values, the game slider's positions 0-100. Their
// per-value table has an entry for each position: the game reads a slider
// row's text at its 0-100 value (the getter on the slider's scale).
inline constexpr std::size_t kMaxSharedValues = 101;
static_assert(kSettingIdFirst + kMaxRows <= kSettingIdLimit);

// True for a row CMFM answers (kind 1, ids 100-299).
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
    // than kMaxRowsPerTab rows, there are more than kMaxRows rows or text ids
    // run out. A tab without rows gets `placeholder` (its `tab` is ignored):
    // the game's Options page reads the row under the cursor without a null
    // check, so a tab must never be empty (run 20261011-native-m2 crashed on
    // one).
    bool build(const char* header, const TabSpec* tabs, std::size_t tab_count, const RowSpec* rows,
               std::size_t count, const RowSpec& placeholder, const char*& why);

    // The game-format table for a tab, ended by a row with kind -1.
    const GameOptionRow* table(std::size_t tab) const { return tables_[tab]; }
    // The same, cut after kGameRowsPerTab rows, for views that show a tab
    // whole; and how many rows the tab has.
    const GameOptionRow* game_table(std::size_t tab) const { return game_tables_[tab]; }
    std::size_t tab_rows(std::size_t tab) const { return tab < tab_count_ ? tab_rows_[tab] : 0; }

    // Texts by message file and id; nullptr if the id is not ours.
    const char* label_text(std::uint32_t id) const;  // file 12, labels and values
    const char* help_text(std::uint32_t id) const;   // file 18, row and value help
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
    const char* header_ = "";
    TabSpec tabs_[kMaxTabs] = {};
    std::size_t tab_count_ = 0;
    RowSpec specs_[kMaxRows] = {};
    std::int32_t sources_[kMaxRows] = {};
    std::uint32_t label_ids_[kMaxRows] = {};
    std::uint32_t help_ids_[kMaxRows] = {};
    std::size_t count_ = 0;
    std::uint32_t label_end_ = kLabelIdFirst;
    std::uint32_t help_end_ = kHelpIdFirst;
    // Per tab: its rows plus the end row; the first kGameRowsPerTab of them
    // plus the end row; the row count.
    GameOptionRow tables_[kMaxTabs][kMaxRowsPerTab + 1] = {};
    GameOptionRow game_tables_[kMaxTabs][kGameRowsPerTab + 1] = {};
    std::size_t tab_rows_[kMaxTabs] = {};
    // The shared rows' per-value tables, kMaxSharedValues entries each
    // (sized once per build: the tables point into it).
    std::vector<GameValueText> shared_values_;
};

}  // namespace cmfm::native
