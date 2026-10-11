#include "native/options_model.hpp"

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
    std::size_t per_tab[kMaxTabs] = {};
    std::size_t empty = 0;
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
    for (std::size_t t = 0; t < tab_count; ++t) {
        empty += per_tab[t] == 0 ? 1 : 0;
    }
    if (count + empty > kMaxRows) {
        why = "too many rows";
        return false;
    }

    header_ = header ? header : "";
    tab_count_ = tab_count;
    for (std::size_t t = 0; t < tab_count; ++t) {
        tabs_[t].name = tabs && tabs[t].name ? tabs[t].name : "";
        tabs_[t].help = tabs && tabs[t].help ? tabs[t].help : "";
        per_tab[t] = 0;
    }
    std::uint32_t label = kLabelIdFirst;
    std::uint32_t help = kHelpIdFirst;
    std::size_t row = 0;
    std::size_t shared = 0;
    for (std::size_t i = 0; i < count; ++i) {
        shared += rows[i].shared ? 1 : 0;
    }
    shared_values_.assign(shared * kMaxSharedValues, GameValueText{});
    std::size_t shared_used = 0;
    const auto add = [&](const RowSpec& s, std::size_t tab, std::int32_t source) {
        // Label then its value texts in file 12; help then one per value in
        // file 18 (a shared row's values all use the row's help).
        const std::uint32_t value_helps = s.shared ? 0 : s.value_count;
        if (label + 1 + text_count(s) > kTextIdLimit || help + 1 + value_helps > kHelpIdLimit) {
            return false;
        }
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
        tables_[tab][per_tab[tab]++] = GameOptionRow{
            kCmfmRowKind, static_cast<std::int32_t>(label), 1, kHelpCategory, static_cast<std::int32_t>(help),
            static_cast<std::int32_t>(kSettingIdFirst + row), static_cast<std::int32_t>(s.value_count), 0,
            values};
        label += 1 + text_count(s);
        help += 1 + value_helps;
        ++row;
        return true;
    };
    for (std::size_t i = 0; i < count; ++i) {
        if (!add(rows[i], rows[i].tab, static_cast<std::int32_t>(i))) {
            why = "text ids exhausted";
            count_ = 0;
            return false;
        }
    }
    for (std::size_t t = 0; t < tab_count; ++t) {
        if (per_tab[t] == 0 && !add(placeholder, t, kPlaceholder)) {
            why = "text ids exhausted";
            count_ = 0;
            return false;
        }
        tables_[t][per_tab[t]] = GameOptionRow{kRowTableEnd, 0, 0, 0, 0, 0, 0, 0, nullptr};
    }
    count_ = row;
    label_end_ = label;
    help_end_ = help;
    return true;
}

const char* OptionsModel::label_text(std::uint32_t id) const {
    if (id < kLabelIdFirst || id >= label_end_) {
        return nullptr;
    }
    for (std::size_t i = 0; i < count_; ++i) {
        const std::uint32_t first = label_ids_[i];
        if (id == first) {
            return specs_[i].label;
        }
        if (id > first && id <= first + text_count(specs_[i])) {
            return specs_[i].values[id - first - 1];
        }
    }
    return nullptr;
}

const char* OptionsModel::help_text(std::uint32_t id) const {
    if (id < kHelpIdFirst || id >= help_end_) {
        return nullptr;
    }
    for (std::size_t i = 0; i < count_; ++i) {
        const std::uint32_t first = help_ids_[i];
        if (id >= first && id <= first + (specs_[i].shared ? 0 : specs_[i].value_count)) {
            return specs_[i].help;  // the row's help, for every value too
        }
    }
    return nullptr;
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
