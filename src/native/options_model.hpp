// What the CMFM view of the Options page shows: up to 5 tabs of rows, in
// the game's own row format (40-byte rows, docs/research/native-menu.md),
// with text ids CMFM answers through its text hook. Pure logic, tested in
// tests/test_options_model.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cmfm::native {

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
    const std::int32_t* value_labels;
};
static_assert(sizeof(GameOptionRow) == 40);
static_assert(offsetof(GameOptionRow, value_labels) == 32);

// CMFM rows are kind 1 (system settings) with setting ids from 100: both
// Options pages keep kind-1 rows (the title page drops unknown kinds), and
// the game's own kind-1 rows use ids 0-34. The getter and setter hooks answer
// these ids, so they never reach the game (a missed one would index sUserData
// out of range).
inline constexpr std::int32_t kCmfmRowKind = 1;
inline constexpr std::uint32_t kSettingIdFirst = 100;
inline constexpr std::uint32_t kSettingIdLimit = 200;
inline constexpr std::int32_t kRowTableEnd = -1;
inline constexpr std::int32_t kHelpCategory = 18;
inline constexpr std::uint32_t kLabelIdFirst = 9100;  // file 12
inline constexpr std::uint32_t kHelpIdFirst = 9002;   // file 18
inline constexpr std::uint32_t kTextIdLimit = 10000;  // GetGameMessage sends larger ids to file 23

inline constexpr std::size_t kTabCount = 5;  // the Options page has 5 fixed tabs
inline constexpr std::size_t kMaxRows = 64;
inline constexpr std::size_t kMaxRowsPerTab = 24;  // the title Options page holds 24 rows per tab
inline constexpr std::size_t kMaxValues = 16;
static_assert(kSettingIdFirst + kMaxRows <= kSettingIdLimit);

// True for a row CMFM answers (kind 1, ids 100-199).
constexpr bool is_cmfm_row(std::int32_t kind, std::uint32_t setting_id) {
    return kind == kCmfmRowKind && setting_id >= kSettingIdFirst && setting_id < kSettingIdLimit;
}

struct OptionSpec {
    const char* label;
    const char* help;
    const char* const* values;  // value_count names
    std::uint32_t value_count;  // 1..kMaxValues
    std::uint32_t initial;
    std::uint32_t tab;  // 0..kTabCount-1
};

class OptionsModel {
public:
    // Builds the model; false (with `why`) if a spec is invalid or ids run out.
    // A tab without rows gets `placeholder` (its `tab` is ignored): the game's
    // Options page reads the row under the cursor without a null check, so a
    // tab must never be empty (run 20261011-native-m2 crashed on one). A tab
    // takes at most kMaxRowsPerTab rows.
    // `tab_names` and `tab_helps` (each kTabCount entries, or NULL) name and
    // describe the tabs; the title Options page shows the description while
    // the tab's category is highlighted.
    bool build(const char* header, const char* const* tab_names, const char* const* tab_helps, const OptionSpec* specs,
               std::size_t count, const OptionSpec& placeholder, const char*& why);

    // The game-format table for a tab, ended by a row with kind -1.
    const GameOptionRow* table(std::size_t tab) const { return tables_[tab]; }

    // Texts by message file and id; nullptr if the id is not ours.
    const char* label_text(std::uint32_t id) const;  // file 12, labels and values
    const char* help_text(std::uint32_t id) const;   // file 18, row and value help
    const char* header() const { return header_; }
    const char* tab_name(std::size_t tab) const { return tab < kTabCount ? tab_names_[tab] : nullptr; }
    const char* tab_help(std::size_t tab) const { return tab < kTabCount ? tab_helps_[tab] : nullptr; }

    std::size_t row_count() const { return count_; }
    // By setting id (kSettingIdFirst + row); false if it is not one of ours.
    bool get(std::uint32_t setting_id, std::uint32_t& value) const;
    // False if the row or value is out of range; nothing changes then.
    bool set(std::uint32_t setting_id, std::uint32_t value);
    // Every row back to its initial value (the page's "Set To Default").
    void reset();
    const OptionSpec* spec(std::uint32_t setting_id) const;

private:
    const char* header_ = "";
    const char* tab_names_[kTabCount] = {};
    const char* tab_helps_[kTabCount] = {};
    OptionSpec specs_[kMaxRows] = {};
    std::uint32_t values_[kMaxRows] = {};
    std::uint32_t label_ids_[kMaxRows] = {};
    std::uint32_t help_ids_[kMaxRows] = {};
    std::size_t count_ = 0;
    std::uint32_t label_end_ = kLabelIdFirst;
    std::uint32_t help_end_ = kHelpIdFirst;
    // Per tab: its rows plus the end row.
    GameOptionRow tables_[kTabCount][kMaxRows + 1] = {};
};

}  // namespace cmfm::native
