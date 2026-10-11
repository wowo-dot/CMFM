#include "native/options_model.hpp"

namespace cmfm::native {

namespace {

bool valid(const RowSpec& s) {
    if (s.label == nullptr || s.help == nullptr || s.values == nullptr || s.value_count == 0 ||
        s.value_count > kMaxValues) {
        return false;
    }
    for (std::uint32_t v = 0; v < s.value_count; ++v) {
        if (s.values[v] == nullptr) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool OptionsModel::build(const char* header, const TabSpec* tabs, const RowSpec* rows, std::size_t count,
                         const RowSpec& placeholder, const char*& why) {
    if (!valid(placeholder)) {
        why = "invalid placeholder row";
        return false;
    }
    std::size_t per_tab[kTabCount] = {};
    for (std::size_t i = 0; i < count; ++i) {
        if (rows[i].tab >= kTabCount || !valid(rows[i])) {
            why = "invalid row";
            return false;
        }
        if (++per_tab[rows[i].tab] > kMaxRowsPerTab) {
            why = "too many rows on one tab";
            return false;
        }
    }

    header_ = header ? header : "";
    for (std::size_t t = 0; t < kTabCount; ++t) {
        tabs_[t].name = tabs && tabs[t].name ? tabs[t].name : "";
        tabs_[t].help = tabs && tabs[t].help ? tabs[t].help : "";
        per_tab[t] = 0;
    }
    std::uint32_t label = kLabelIdFirst;
    std::uint32_t help = kHelpIdFirst;
    std::size_t row = 0;
    const auto add = [&](const RowSpec& s, std::size_t tab, std::int32_t source) {
        // Label then its values in file 12; help then one per value in file 18.
        if (label + 1 + s.value_count > kTextIdLimit || help + 1 + s.value_count > kTextIdLimit) {
            return false;
        }
        specs_[row] = s;
        specs_[row].tab = static_cast<std::uint32_t>(tab);
        sources_[row] = source;
        label_ids_[row] = label;
        help_ids_[row] = help;
        tables_[tab][per_tab[tab]++] = GameOptionRow{
            kCmfmRowKind, static_cast<std::int32_t>(label), 1, kHelpCategory, static_cast<std::int32_t>(help),
            static_cast<std::int32_t>(kSettingIdFirst + row), static_cast<std::int32_t>(s.value_count), 0,
            nullptr};
        label += 1 + s.value_count;
        help += 1 + s.value_count;
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
    for (std::size_t t = 0; t < kTabCount; ++t) {
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
        if (id > first && id <= first + specs_[i].value_count) {
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
        if (id >= first && id <= first + specs_[i].value_count) {
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
