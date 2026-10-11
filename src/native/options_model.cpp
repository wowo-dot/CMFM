#include "native/options_model.hpp"

namespace cmfm::native {

namespace {

bool valid(const OptionSpec& s) {
    if (s.label == nullptr || s.values == nullptr || s.value_count == 0 || s.value_count > kMaxValues ||
        s.initial >= s.value_count) {
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

bool OptionsModel::build(const char* header, const char* const* tab_names, const OptionSpec* specs,
                         std::size_t count, const OptionSpec& placeholder, const char*& why) {
    if (!valid(placeholder)) {
        why = "invalid placeholder row";
        return false;
    }
    std::size_t per_tab[kTabCount] = {};
    for (std::size_t i = 0; i < count; ++i) {
        if (specs[i].tab >= kTabCount || !valid(specs[i])) {
            why = "invalid row spec";
            return false;
        }
        ++per_tab[specs[i].tab];
    }
    std::size_t empty = 0;
    for (const std::size_t n : per_tab) {
        empty += n == 0 ? 1 : 0;
    }
    if (count + empty > kMaxRows) {
        why = "too many rows";
        return false;
    }

    header_ = header ? header : "";
    for (std::size_t t = 0; t < kTabCount; ++t) {
        tab_names_[t] = tab_names && tab_names[t] ? tab_names[t] : "";
        per_tab[t] = 0;
    }
    std::uint32_t label = kLabelIdFirst;
    std::uint32_t help = kHelpIdFirst;
    std::size_t row = 0;
    const auto add = [&](const OptionSpec& s, std::size_t tab) {
        // Label then its values in file 12; help then one per value in file 18.
        if (label + 1 + s.value_count > kTextIdLimit || help + 1 + s.value_count > kTextIdLimit) {
            return false;
        }
        specs_[row] = s;
        specs_[row].tab = static_cast<std::uint32_t>(tab);
        values_[row] = s.initial;
        label_ids_[row] = label;
        help_ids_[row] = help;
        tables_[tab][per_tab[tab]++] = GameOptionRow{
            kCmfmRowKind, static_cast<std::int32_t>(label), 1, kHelpCategory, static_cast<std::int32_t>(help),
            static_cast<std::int32_t>(row), static_cast<std::int32_t>(s.value_count), 0, nullptr};
        label += 1 + s.value_count;
        help += 1 + s.value_count;
        ++row;
        return true;
    };
    for (std::size_t i = 0; i < count; ++i) {
        if (!add(specs[i], specs[i].tab)) {
            why = "text ids exhausted";
            return false;
        }
    }
    for (std::size_t t = 0; t < kTabCount; ++t) {
        if (per_tab[t] == 0 && !add(placeholder, t)) {
            why = "text ids exhausted";
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

void OptionsModel::reset() {
    for (std::size_t i = 0; i < count_; ++i) {
        values_[i] = specs_[i].initial;
    }
}

bool OptionsModel::set(std::uint32_t setting_id, std::uint32_t value) {
    if (setting_id >= count_ || value >= specs_[setting_id].value_count) {
        return false;
    }
    values_[setting_id] = value;
    return true;
}

}  // namespace cmfm::native
