#include "native/options_model.hpp"

#include <algorithm>

namespace cmfm::native {

namespace {

// The texts a row has after its label: one if its values share it.
std::uint32_t text_count(const RowSpec& s) {
    return s.shared ? 1 : s.value_count;
}

bool valid(const RowSpec& s) {
    if (s.label == nullptr || s.help == nullptr || s.values == nullptr || s.value_count == 0 ||
        s.value_count > (s.shared ? kMaxSharedValues : kMaxValues)) {
        return false;
    }
    for (std::uint32_t v = 0; v < text_count(s); ++v) {
        if (s.values[v] == nullptr) {
            return false;
        }
    }
    return true;
}

const GameOptionRow kEndRow = {kRowTableEnd, 0, 0, 0, 0, 0, 0, 0, nullptr};

}  // namespace

bool OptionsModel::build(const char* header, const TabSpec* tabs, std::size_t tab_count, const RowSpec* rows,
                         std::size_t count, const RowSpec& placeholder, const char*& why) {
    count_ = 0;
    if (!valid(placeholder)) {
        why = "invalid placeholder row";
        return false;
    }
    if (tab_count < kTabCount || tab_count > kMaxTabs) {
        why = "tab count out of range";
        return false;
    }
    std::vector<std::size_t> per_tab(tab_count, 0);
    for (std::size_t i = 0; i < count; ++i) {
        if (rows[i].tab >= tab_count || !valid(rows[i])) {
            why = "invalid row";
            return false;
        }
        if (++per_tab[rows[i].tab] > kMaxRowsPerTab) {
            why = "too many rows on one tab";
            return false;
        }
    }
    const auto empty = static_cast<std::size_t>(std::count(per_tab.begin(), per_tab.end(), std::size_t{0}));
    if (count + empty > kMaxRows) {
        why = "too many rows";
        return false;
    }

    header_ = header ? header : "";
    tab_count_ = tab_count;
    tabs_.assign(tab_count, TabSpec{});
    tables_.assign(tab_count, {});
    game_tables_.assign(tab_count, {});
    for (std::size_t t = 0; t < tab_count; ++t) {
        tabs_[t].name = tabs && tabs[t].name ? tabs[t].name : "";
        tabs_[t].help = tabs && tabs[t].help ? tabs[t].help : "";
        tables_[t].reserve((per_tab[t] == 0 ? 1 : per_tab[t]) + 1);
    }
    const std::size_t total = count + empty;
    specs_.assign(total, RowSpec{});
    sources_.assign(total, 0);
    label_ids_.assign(total, 0);
    help_ids_.assign(total, 0);
    std::uint32_t label = kLabelFirst;
    std::uint32_t help = kHelpFirst;
    std::size_t row = 0;
    std::size_t shared = 0;
    for (std::size_t i = 0; i < count; ++i) {
        shared += rows[i].shared ? 1 : 0;
    }
    shared_values_.assign(shared * kMaxSharedValues, GameValueText{});
    std::size_t shared_used = 0;
    // kMaxRows rows of at most 1 + kMaxValues texts each stay within a text
    // span: numbers never run out.
    static_assert(kMaxRows * (1 + kMaxValues) < kTextSpan);
    const auto add = [&](const RowSpec& s, std::size_t tab, std::int32_t source) {
        // Label then its value texts; help then one per value (a shared row's
        // values all use the row's help).
        const std::uint32_t value_helps = s.shared ? 0 : s.value_count;
        specs_[row] = s;
        specs_[row].tab = static_cast<std::uint32_t>(tab);
        sources_[row] = source;
        label_ids_[row] = label;
        help_ids_[row] = help;
        const GameValueText* values = nullptr;
        if (s.shared) {
            GameValueText* entries = &shared_values_[shared_used * kMaxSharedValues];
            for (std::size_t k = 0; k < kMaxSharedValues; ++k) {
                entries[k] = GameValueText{static_cast<std::int32_t>(label + 1), static_cast<std::int32_t>(label + 1),
                                           static_cast<std::int32_t>(help), kHelpCategory};
            }
            values = entries;
            ++shared_used;
        }
        tables_[tab].push_back(GameOptionRow{
            kCmfmRowKind, static_cast<std::int32_t>(label), 1, kHelpCategory, static_cast<std::int32_t>(help),
            static_cast<std::int32_t>(kSettingIdFirst + row), static_cast<std::int32_t>(s.value_count), 0,
            values});
        label += 1 + text_count(s);
        help += 1 + value_helps;
        ++row;
    };
    for (std::size_t i = 0; i < count; ++i) {
        add(rows[i], rows[i].tab, static_cast<std::int32_t>(i));
    }
    for (std::size_t t = 0; t < tab_count; ++t) {
        if (tables_[t].empty()) {
            add(placeholder, t, kPlaceholder);
        }
        const std::size_t game_rows = std::min(tables_[t].size(), kGameRowsPerTab);
        game_tables_[t].assign(tables_[t].begin(), tables_[t].begin() + static_cast<std::ptrdiff_t>(game_rows));
        game_tables_[t].push_back(kEndRow);
        tables_[t].push_back(kEndRow);
    }
    count_ = row;
    label_end_ = label;
    help_end_ = help;
    return true;
}

std::size_t OptionsModel::row_of(const std::vector<std::uint32_t>& firsts, std::uint32_t id) const {
    // The last row whose first number is at most `id` (numbers ascend with
    // the rows).
    const auto end = firsts.begin() + static_cast<std::ptrdiff_t>(count_);
    const auto next = std::upper_bound(firsts.begin(), end, id);
    return next == firsts.begin() ? count_ : static_cast<std::size_t>(next - firsts.begin()) - 1;
}

const char* OptionsModel::label_text(std::uint32_t id) const {
    if (id < kLabelFirst || id >= label_end_) {
        return nullptr;
    }
    const std::size_t i = row_of(label_ids_, id);
    if (i == count_) {
        return nullptr;
    }
    const std::uint32_t first = label_ids_[i];
    if (id == first) {
        return specs_[i].label;
    }
    return id <= first + text_count(specs_[i]) ? specs_[i].values[id - first - 1] : nullptr;
}

const char* OptionsModel::help_text(std::uint32_t id) const {
    if (id < kHelpFirst || id >= help_end_) {
        return nullptr;
    }
    const std::size_t i = row_of(help_ids_, id);
    if (i == count_) {
        return nullptr;
    }
    // The row's help, for every value too.
    return id <= help_ids_[i] + (specs_[i].shared ? 0 : specs_[i].value_count) ? specs_[i].help : nullptr;
}

std::int32_t OptionsModel::source(std::uint32_t setting_id) const {
    const std::uint32_t row = setting_id - kSettingIdFirst;  // wraps for ids below ours
    return row < count_ ? sources_[row] : kNotOurs;
}

std::uint32_t OptionsModel::value_count(std::uint32_t setting_id) const {
    const std::uint32_t row = setting_id - kSettingIdFirst;
    return row < count_ ? specs_[row].value_count : 0;
}

}  // namespace cmfm::native
