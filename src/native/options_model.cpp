#include "native/options_model.hpp"

namespace cmfm::native {

bool OptionsModel::build(const char* header, const char* const* tab_names, const OptionSpec* specs,
                         std::size_t count, const char*& why) {
    if (count > kMaxRows) {
        why = "too many rows";
        return false;
    }
    header_ = header ? header : "";
    for (std::size_t t = 0; t < kTabCount; ++t) {
        tab_names_[t] = tab_names && tab_names[t] ? tab_names[t] : "";
    }
    std::size_t per_tab[kTabCount] = {};
    std::uint32_t label = kLabelIdFirst;
    std::uint32_t help = kHelpIdFirst;
    for (std::size_t i = 0; i < count; ++i) {
        const OptionSpec& s = specs[i];
        if (s.tab >= kTabCount || s.label == nullptr || s.values == nullptr || s.value_count == 0 ||
            s.value_count > kMaxValues || s.initial >= s.value_count) {
            why = "invalid row spec";
            return false;
        }
        for (std::uint32_t v = 0; v < s.value_count; ++v) {
            if (s.values[v] == nullptr) {
                why = "missing value name";
                return false;
            }
        }
        // Label then its values in file 12; help then one per value in file 18.
        if (label + 1 + s.value_count > kTextIdLimit || help + 1 + s.value_count > kTextIdLimit) {
            why = "text ids exhausted";
            return false;
        }
        specs_[i] = s;
        values_[i] = s.initial;
        label_ids_[i] = label;
        help_ids_[i] = help;
        tables_[s.tab][per_tab[s.tab]++] = GameOptionRow{
            kCmfmRowKind, static_cast<std::int32_t>(label), 1, kHelpCategory, static_cast<std::int32_t>(help),
            static_cast<std::int32_t>(i), static_cast<std::int32_t>(s.value_count), 0, nullptr};
        label += 1 + s.value_count;
        help += 1 + s.value_count;
    }
    for (std::size_t t = 0; t < kTabCount; ++t) {
        tables_[t][per_tab[t]] = GameOptionRow{kRowTableEnd, 0, 0, 0, 0, 0, 0, 0, nullptr};
    }
    count_ = count;
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
            // Row help and, for now, the same text for every value.
            return specs_[i].help ? specs_[i].help : "";
        }
    }
    return nullptr;
}

bool OptionsModel::get(std::uint32_t setting_id, std::uint32_t& value) const {
    if (setting_id >= count_) {
        return false;
    }
    value = values_[setting_id];
    return true;
}

bool OptionsModel::set(std::uint32_t setting_id, std::uint32_t value) {
    if (setting_id >= count_ || value >= specs_[setting_id].value_count) {
        return false;
    }
    values_[setting_id] = value;
    return true;
}

}  // namespace cmfm::native
