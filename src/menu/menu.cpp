#include "menu/menu.hpp"

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

namespace cmfm::menu {

namespace {

constexpr char kRestartNote[] = "Takes effect after restarting the game.";
constexpr char kButtonText[] = "Run";
// The structs as cmfm.menu 1.0 defined them: anything a plugin passes must be
// at least this big; later fields are read only when `size` covers them.
constexpr std::size_t kPageSize10 = offsetof(cmfm_page_t, press);
constexpr std::size_t kSettingSize10 = offsetof(cmfm_setting_t, button);
// Section headings in the game's own text colour tags (as its message files
// write them).
constexpr char kSectionStart[] = "<STYL MOJI_YELLOW_DEFAULT>";
constexpr char kSectionEnd[] = "</STYL>";
const char* const kOffOn[] = {"Off", "On"};
const char* const kDash[] = {"-"};
const char* const kNoValue[] = {""};
const native::RowSpec kEmptyRow = {"Empty", "No mod uses this tab.", kDash, 1, 0};

// A string of 1..limit bytes (or 0..limit when empty is allowed).
bool text_ok(const char* s, std::uint32_t limit, bool may_be_empty, std::string& out) {
    if (s == nullptr) {
        if (!may_be_empty) {
            return false;
        }
        out.clear();
        return true;
    }
    const std::size_t n = strnlen(s, limit + 1);
    if (n > limit || (n == 0 && !may_be_empty)) {
        return false;
    }
    out.assign(s, n);
    return true;
}

bool whole(double v) { return std::isfinite(v) && v == std::floor(v) && std::fabs(v) <= 2147483647.0; }

// Values from min to max in steps of step.
std::uint32_t range_count(double min, double max, double step) {
    return static_cast<std::uint32_t>(std::floor((max - min) / step + 1e-9)) + 1;
}

std::string number(const Setting& s, std::uint32_t k) {
    char b[64];
    const double v = s.min + k * s.step;
    if (s.type == CMFM_TYPE_INT) {
        std::snprintf(b, sizeof b, "%lld", static_cast<long long>(std::llround(v)));
    } else {
        std::snprintf(b, sizeof b, "%.*f", static_cast<int>(s.decimals), v > s.max ? s.max : v);
    }
    return b + s.unit;
}

std::uint32_t index_of(const Setting& s, const cmfm_value_t& v) {
    double k = 0;
    switch (s.type) {
        case CMFM_TYPE_BOOL:
            return v.i != 0 ? 1 : 0;
        case CMFM_TYPE_CHOICE:
            return v.i >= 0 && static_cast<std::uint32_t>(v.i) < s.value_count ? static_cast<std::uint32_t>(v.i) : 0;
        case CMFM_TYPE_INT:
            k = std::round((v.i - s.min) / s.step);
            break;
        case CMFM_TYPE_FLOAT:
            k = std::round((v.f - s.min) / s.step);
            break;
        default:
            return 0;
    }
    if (!(k >= 0)) {  // also NaN
        return 0;
    }
    return k >= s.value_count ? s.value_count - 1 : static_cast<std::uint32_t>(k);
}

cmfm_value_t value_at(const Setting& s, std::uint32_t k) {
    cmfm_value_t v = {};
    if (s.type == CMFM_TYPE_INT) {
        v.i = static_cast<std::int32_t>(std::llround(s.min + k * s.step));
    } else if (s.type == CMFM_TYPE_FLOAT) {
        v.f = s.min + k * s.step;
        if (v.f > s.max) {
            v.f = s.max;
        }
    } else {
        v.i = static_cast<std::int32_t>(k);
    }
    return v;
}

bool copy_setting(const cmfm_setting_t& in, std::size_t size, Setting& s, std::string& why) {
    char b[160];
    const auto fail = [&](const char* what) {
        std::snprintf(b, sizeof b, "setting %u: %s", in.id, what);
        why = b;
        return false;
    };
    s.id = in.id;
    s.type = in.type;
    s.flags = in.flags;
    if (!text_ok(in.label, kMaxLabel, false, s.label)) {
        return fail("label missing or longer than 64 bytes");
    }
    if (!text_ok(in.help, kMaxHelp, true, s.help)) {
        return fail("help longer than 200 bytes");
    }
    if ((in.flags &
         ~(CMFM_FLAG_RESTART | CMFM_FLAG_LIST | CMFM_FLAG_ARROWS | CMFM_FLAG_HIDE_LOCKED | CMFM_FLAG_LIVE)) != 0) {
        return fail("unknown flags");
    }
    if ((in.flags & CMFM_FLAG_LIVE) != 0 && in.type != CMFM_TYPE_TEXT) {
        return fail("LIVE is for TEXT settings");
    }
    if ((in.flags & (CMFM_FLAG_LIST | CMFM_FLAG_ARROWS)) != 0 && in.type != CMFM_TYPE_CHOICE) {
        return fail("LIST and ARROWS are for CHOICE settings");
    }
    if ((in.flags & CMFM_FLAG_LIST) != 0 && (in.flags & CMFM_FLAG_ARROWS) != 0) {
        return fail("LIST and ARROWS together");
    }
    s.default_value = in.default_value;
    switch (in.type) {
        case CMFM_TYPE_BOOL:
            s.value_count = 2;
            if (in.default_value.i != 0 && in.default_value.i != 1) {
                return fail("default is not 0 or 1");
            }
            return true;
        case CMFM_TYPE_CHOICE:
            if (in.choices == nullptr || in.choice_count == 0 || in.choice_count > CMFM_MAX_CHOICES) {
                return fail("needs 1 to 32 choices");
            }
            s.choices.resize(in.choice_count);
            for (std::uint32_t c = 0; c < in.choice_count; ++c) {
                if (!text_ok(in.choices[c], kMaxChoice, false, s.choices[c])) {
                    return fail("a choice name is missing or longer than 32 bytes");
                }
            }
            s.value_count = in.choice_count;
            if (in.default_value.i < 0 || static_cast<std::uint32_t>(in.default_value.i) >= in.choice_count) {
                return fail("default is not one of the choices");
            }
            return true;
        case CMFM_TYPE_INT:
        case CMFM_TYPE_FLOAT: {
            const bool is_int = in.type == CMFM_TYPE_INT;
            if (!std::isfinite(in.min) || !std::isfinite(in.max) || !std::isfinite(in.step) || in.step <= 0 ||
                in.min > in.max) {
                return fail("needs min <= max and step > 0");
            }
            if (is_int && (!whole(in.min) || !whole(in.max) || !whole(in.step))) {
                return fail("INT needs whole min, max and step");
            }
            if (!is_int && in.decimals > 3) {
                return fail("decimals above 3");
            }
            if ((in.max - in.min) / in.step + 1 > CMFM_MAX_STEPS + 0.5) {
                return fail("more than 101 values (the slider has positions 0 to 100)");
            }
            if (!text_ok(in.unit, kMaxUnit, true, s.unit)) {
                return fail("unit longer than 8 bytes");
            }
            s.min = in.min;
            s.max = in.max;
            s.step = in.step;
            s.decimals = is_int ? 0 : in.decimals;
            s.value_count = range_count(in.min, in.max, in.step);
            const double d = is_int ? in.default_value.i : in.default_value.f;
            if (!(d >= in.min - 1e-9 && d <= in.max + 1e-9)) {
                return fail("default outside min..max");
            }
            return true;
        }
        case CMFM_TYPE_TEXT:
            s.value_count = 1;
            return true;
        case CMFM_TYPE_SECTION:
            if ((in.flags & ~CMFM_FLAG_HIDE_LOCKED) != 0) {
                return fail("a SECTION takes no flags but HIDE_LOCKED");
            }
            s.value_count = 1;
            s.help.clear();
            return true;
        case CMFM_TYPE_BUTTON: {
            if ((in.flags & ~CMFM_FLAG_HIDE_LOCKED) != 0) {
                return fail("a BUTTON takes no flags but HIDE_LOCKED");
            }
            const bool sized = size >= offsetof(cmfm_setting_t, confirm) + sizeof in.confirm;
            if (!text_ok(sized ? in.button : nullptr, kMaxChoice, true, s.button)) {
                return fail("button text longer than 32 bytes");
            }
            if (s.button.empty()) {
                s.button = kButtonText;
            }
            if (!text_ok(sized ? in.confirm : nullptr, kMaxHelp, true, s.confirm)) {
                return fail("confirm question longer than 200 bytes");
            }
            s.value_count = 1;
            return true;
        }
        default:
            return fail("unknown type");
    }
}

}  // namespace

bool copy_page(const cmfm_page_t* page, Page& out, std::string& why) {
    if (page == nullptr || page->size < kPageSize10) {
        why = "no page, or a page smaller than cmfm.menu 1.0's";
        return false;
    }
    if (!text_ok(page->plugin, kMaxPlugin, false, out.plugin)) {
        why = "plugin name missing or longer than 64 bytes";
        return false;
    }
    if (!text_ok(page->title, kMaxTitle, false, out.title)) {
        why = "title missing or longer than 24 bytes";
        return false;
    }
    if (!text_ok(page->help, kMaxHelp, true, out.help)) {
        why = "page help longer than 200 bytes";
        return false;
    }
    if (page->settings == nullptr || page->setting_count == 0 || page->setting_count > CMFM_MAX_SETTINGS) {
        why = "needs 1 to 24 settings";
        return false;
    }
    const std::uint32_t stride = page->settings[0].size;
    if (stride < kSettingSize10) {
        why = "settings smaller than cmfm.menu 1.0's";
        return false;
    }
    const char* base = reinterpret_cast<const char*>(page->settings);
    bool any_value = false;
    bool any_text = false;
    bool any_button = false;
    out.settings.resize(page->setting_count);
    for (std::uint32_t i = 0; i < page->setting_count; ++i) {
        const auto* in = reinterpret_cast<const cmfm_setting_t*>(base + std::size_t{i} * stride);
        if (in->size != stride) {
            why = "settings of different sizes in one array";
            return false;
        }
        if (!copy_setting(*in, stride, out.settings[i], why)) {
            return false;
        }
        if (in->id >= CMFM_RESERVED_IDS) {
            why = "setting " + std::to_string(in->id) + ": ids from 0xFFFFFF00 are CMFM's";
            return false;
        }
        for (std::uint32_t j = 0; j < i; ++j) {
            if (out.settings[j].id == in->id) {
                why = "setting " + std::to_string(in->id) + ": id used twice";
                return false;
            }
        }
        if (in->type == CMFM_TYPE_TEXT) {
            any_text = true;
        } else if (in->type == CMFM_TYPE_BUTTON) {
            any_button = true;
        } else if (in->type != CMFM_TYPE_SECTION) {
            any_value = true;
        }
    }
    if (!any_value && !any_text && !any_button) {
        why = "needs a setting besides its sections";
        return false;
    }
    if (any_value && (page->get == nullptr || page->set == nullptr)) {
        why = "get and set callbacks needed";
        return false;
    }
    if (any_text && page->get_text == nullptr) {
        why = "get_text callback needed for TEXT settings";
        return false;
    }
    const std::uint32_t flags = page->size >= offsetof(cmfm_page_t, flags) + sizeof page->flags ? page->flags : 0;
    if ((flags & ~CMFM_PAGE_NO_RESET) != 0) {
        why = "unknown page flags";
        return false;
    }
    out.flags = flags;
    const char* version =
        page->size >= offsetof(cmfm_page_t, version) + sizeof page->version ? page->version : nullptr;
    if (!text_ok(version, kMaxVersion, true, out.version)) {
        why = "version longer than 24 bytes";
        return false;
    }
    const cmfm_press_fn press = page->size >= offsetof(cmfm_page_t, press) + sizeof page->press ? page->press : nullptr;
    const cmfm_locked_fn locked =
        page->size >= offsetof(cmfm_page_t, locked) + sizeof page->locked ? page->locked : nullptr;
    if (any_button && press == nullptr) {
        why = "press callback needed for BUTTON settings";
        return false;
    }
    out.user = page->user;
    out.get = page->get;
    out.set = page->set;
    out.validate = page->validate;
    out.get_text = page->get_text;
    out.on_event = page->on_event;
    out.press = press;
    out.locked = locked;
    return true;
}

void Menu::logf(int level, const char* format, ...) const {
    if (log_ == nullptr) {
        return;
    }
    char line[400];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    log_(level, line);
}

void Menu::set_help(Row& row, const char* text) {
    std::snprintf(row.help, sizeof row.help, "%s", text);
}

void Menu::add(Page page) {
    bool resettable = false;
    for (const Setting& s : page.settings) {
        resettable = resettable || s.type == CMFM_TYPE_BOOL || s.type == CMFM_TYPE_CHOICE ||
                     s.type == CMFM_TYPE_INT || s.type == CMFM_TYPE_FLOAT;
    }
    if (resettable && (page.flags & CMFM_PAGE_NO_RESET) == 0 && page.settings.size() < CMFM_MAX_SETTINGS) {
        Setting reset;
        reset.id = kResetId;
        reset.type = CMFM_TYPE_BUTTON;
        reset.label = "Reset to defaults";
        reset.help = "Puts every setting of this mod back to its default.";
        reset.button = "Reset";
        reset.confirm = "Reset every " + page.title + " setting to its default?";
        reset.value_count = 1;
        page.settings.push_back(std::move(reset));
    }
    entries_.push_back({true, pages_.size()});
    pages_.push_back(std::move(page));
    not_shown_why_.emplace_back();
    dirty_ = true;
}

void Menu::add_refused(std::string plugin, std::string why) {
    entries_.push_back({false, refused_.size()});
    refused_.emplace_back(std::move(plugin), std::move(why));
    dirty_ = true;
}

void Menu::compose(std::size_t shown, std::vector<native::RowSpec>& specs) {
    // Status rows: CMFM itself, one per add_page call (at most 23 rows).
    constexpr std::size_t kStatusRows = native::kMaxRowsPerTab;
    std::size_t entry_rows = entries_.empty() ? 1 : entries_.size();
    const bool more = entry_rows > kStatusRows - 1;
    if (more) {
        entry_rows = kStatusRows - 1;  // the last one says how many more
    }
    std::size_t setting_rows = 0;
    for (std::size_t p = 0; p < shown; ++p) {
        setting_rows += pages_[p].settings.size();
    }
    rows_.clear();
    rows_.reserve(1 + entry_rows + setting_rows);  // stable addresses for the model

    const auto status = [&](const std::string& label, const char* value, const std::string& help) {
        Row& r = rows_.emplace_back();
        r.status = true;
        r.label = label;
        std::snprintf(r.text, sizeof r.text, "%s", value);
        r.value_ptrs = {r.text};
        r.base_help = help;
        set_help(r, help.c_str());
    };
    status("CMFM", version_.empty() ? "-" : version_.c_str(),
           "Configuration Menu For Mods. Each mod that supports it has an entry in this list.");
    if (entries_.empty()) {
        status("No mod pages yet", "-", "Mods that support CMFM add their settings here.");
    }
    for (std::size_t e = 0; e < entries_.size() && e < entry_rows; ++e) {
        if (more && e == entry_rows - 1) {
            status("More", std::to_string(entries_.size() - e).c_str(),
                   "More mods than this list holds; see cmfm.log.");
            break;
        }
        const Entry& en = entries_[e];
        if (!en.page) {
            const auto& [plugin, why] = refused_[en.index];
            status(plugin.empty() ? std::string("(unnamed)") : plugin, "Refused", "Refused: " + why);
        } else if (en.index < shown) {
            const Page& p = pages_[en.index];
            std::size_t count = 0;
            for (const Setting& s : p.settings) {
                count += s.type != CMFM_TYPE_SECTION && s.id != kResetId;
            }
            status(p.title, "Active", "From " + p.plugin + ": " + std::to_string(count) + " setting(s).");
            rows_.back().status_page = static_cast<std::int32_t>(en.index);
        } else {
            const Page& p = pages_[en.index];
            status(p.title, "Not shown", "From " + p.plugin + ". " + not_shown_why_[en.index]);
        }
    }

    // The shown pages' settings, page p on tab 1 + p.
    for (std::size_t p = 0; p < shown; ++p) {
        for (std::size_t i = 0; i < pages_[p].settings.size(); ++i) {
            const Setting& s = pages_[p].settings[i];
            Row& r = rows_.emplace_back();
            r.page = p;
            r.setting = i;
            r.label = s.label;
            if (s.type == CMFM_TYPE_SECTION) {
                r.label = kSectionStart + s.label + kSectionEnd;
                r.value_ptrs = {kNoValue[0]};
            } else if (s.type == CMFM_TYPE_TEXT) {
                r.value_ptrs = {r.text};
            } else if (s.type == CMFM_TYPE_BUTTON) {
                r.values = {s.button};
                r.value_ptrs = {r.values[0].c_str()};
            } else if (s.type == CMFM_TYPE_BOOL) {
                r.value_ptrs = {kOffOn[0], kOffOn[1]};
            } else if (s.type == CMFM_TYPE_CHOICE) {
                r.values = s.choices;
                for (const std::string& v : r.values) {
                    r.value_ptrs.push_back(v.c_str());
                }
            } else {
                // Numbers: one text for all values, the current one (written
                // on every read and change), so a range needs no text per value.
                r.shared = true;
                r.value_count = s.value_count;
                std::snprintf(r.text, sizeof r.text, "%s", number(s, 0).c_str());
                r.value_ptrs = {r.text};
            }
            r.base_help = s.help;
            if ((s.flags & CMFM_FLAG_RESTART) != 0) {
                r.base_help += (r.base_help.empty() ? "" : " ") + std::string(kRestartNote);
            }
            set_help(r, r.base_help.c_str());
        }
    }

    specs.clear();
    for (const Row& r : rows_) {
        const std::uint32_t tab = r.status ? 0 : static_cast<std::uint32_t>(1 + r.page);
        specs.push_back({r.label.c_str(), r.help, r.value_ptrs.data(),
                         r.shared ? r.value_count : static_cast<std::uint32_t>(r.value_ptrs.size()), tab, r.shared});
    }

    tab_count_ = 1 + shown < native::kTabCount ? native::kTabCount : 1 + shown;
    tab_names_[0] = "Overview";
    tab_helps_[0] = "CMFM's version and every mod: active, waiting for a restart, or refused.";
    for (std::size_t t = 1; t < tab_count_; ++t) {
        if (t - 1 < shown) {
            const Page& p = pages_[t - 1];
            tab_names_[t] = p.title;
            tab_helps_[t] = p.help.empty() ? "Settings from " + p.plugin + "." : p.help;
        } else {
            tab_names_[t] = "-";
            tab_helps_[t] = "No mod uses this tab.";
        }
    }
}

bool Menu::build(native::OptionsModel& model, std::string& why) {
    for (std::size_t p = 0; p < pages_.size(); ++p) {
        not_shown_why_[p] = p < kMaxPages ? "" : "Not shown: only 24 mod pages fit.";
    }
    std::size_t shown = pages_.size() < kMaxPages ? pages_.size() : kMaxPages;
    native::TabSpec tabs[native::kMaxTabs];
    std::vector<native::RowSpec> specs;
    for (;;) {
        compose(shown, specs);
        for (std::size_t t = 0; t < tab_count_; ++t) {
            tabs[t] = {tab_names_[t].c_str(), tab_helps_[t].c_str()};
        }
        const char* fail = nullptr;
        if (model.build("Mods", tabs, tab_count_, specs.data(), specs.size(), kEmptyRow, fail)) {
            break;
        }
        if (shown == 0) {
            why = std::string("the status page does not fit: ") + fail;
            model_ = nullptr;
            shown_ = 0;
            return false;
        }
        --shown;
        not_shown_why_[shown] = std::string("Not shown: ") + fail + " (too many rows or texts).";
        logf(1, "page \"%s\" (%s) not shown: %s", pages_[shown].title.c_str(), pages_[shown].plugin.c_str(), fail);
    }
    model_ = &model;
    shown_ = shown;
    dirty_ = false;
    update_status_texts();
    logf(0, "menu built: %zu page(s) shown, %zu added, %zu refused, %zu rows", shown, pages_.size(),
         refused_.size(), model.row_count());
    return true;
}

std::size_t Menu::row_index(std::uint32_t setting_id) const {
    if (model_ == nullptr) {
        return rows_.size();
    }
    const std::int32_t src = model_->source(setting_id);
    if (src < 0 || static_cast<std::size_t>(src) >= rows_.size() || rows_[static_cast<std::size_t>(src)].status) {
        return rows_.size();
    }
    return static_cast<std::size_t>(src);
}

const Menu::Row* Menu::any_setting_row(std::uint32_t setting_id) const {
    const std::size_t i = row_index(setting_id);
    return i < rows_.size() ? &rows_[i] : nullptr;
}

Menu::Row* Menu::setting_row(std::uint32_t setting_id) {
    const std::size_t i = row_index(setting_id);
    if (i == rows_.size()) {
        return nullptr;
    }
    const cmfm_type_t type = pages_[rows_[i].page].settings[rows_[i].setting].type;
    return type == CMFM_TYPE_TEXT || type == CMFM_TYPE_SECTION || type == CMFM_TYPE_BUTTON ? nullptr : &rows_[i];
}

bool Menu::is_button(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].type == CMFM_TYPE_BUTTON;
}

const char* Menu::button_question(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return nullptr;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    return s.type == CMFM_TYPE_BUTTON && !s.confirm.empty() ? s.confirm.c_str() : nullptr;
}

bool Menu::press(std::uint32_t setting_id) {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return false;
    }
    const Page& p = pages_[r->page];
    const Setting& s = p.settings[r->setting];
    if (s.id == kResetId) {
        reset_page(r->page);
        return true;
    }
    if (s.type != CMFM_TYPE_BUTTON || p.press == nullptr) {
        return false;
    }
    if (s.locked) {
        logf(0, "%s: \"%s\" is locked: %s", p.plugin.c_str(), s.label.c_str(), s.locked_why.c_str());
        return false;
    }
    const bool ok = p.press(p.user, s.id) == CMFM_OK;
    logf(ok ? 0 : 1, "%s: \"%s\" pressed%s", p.plugin.c_str(), s.label.c_str(), ok ? "" : ": the mod reported a failure");
    ++value_revision_;  // the mod may have changed anything
    refresh_texts();
    refresh_locks();
    return ok;
}

bool Menu::is_locked(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].locked;
}

bool Menu::is_hidden(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return false;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    return s.locked && (s.flags & CMFM_FLAG_HIDE_LOCKED) != 0;
}

void Menu::refresh_locks() {
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        Page& p = pages_[r.page];
        Setting& s = p.settings[r.setting];
        const bool hideable = (s.flags & CMFM_FLAG_HIDE_LOCKED) != 0;
        if (p.locked == nullptr || s.id == kResetId ||
            ((s.type == CMFM_TYPE_SECTION || s.type == CMFM_TYPE_TEXT) && !hideable)) {
            continue;
        }
        char why[kMaxHelp + 1] = {};
        const bool locked = p.locked(p.user, s.id, why, sizeof why) != 0;
        why[sizeof why - 1] = '\0';
        if (locked != s.locked || (locked && s.locked_why != why)) {
            s.locked = locked;
            s.locked_why = locked ? why : "";
            ++lock_revision_;
        }
        if (locked) {
            char line[sizeof r.help];
            std::snprintf(line, sizeof line, "Locked: %s", why[0] ? why : "the mod does not allow a change now.");
            set_help(r, line);
        } else if (std::strncmp(r.help, "Locked: ", 8) == 0) {
            set_help(r, r.base_help.c_str());
        }
    }
}

bool Menu::is_live(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return false;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    return s.type == CMFM_TYPE_TEXT && (s.flags & CMFM_FLAG_LIVE) != 0;
}

bool Menu::reread_text(std::uint32_t setting_id) {
    const std::size_t i = row_index(setting_id);
    if (i >= rows_.size() || !is_live(setting_id)) {
        return false;
    }
    Row& r = rows_[i];
    const Page& p = pages_[r.page];
    char text[CMFM_MAX_TEXT] = {};
    p.get_text(p.user, p.settings[r.setting].id, text, sizeof text);
    text[sizeof text - 1] = '\0';
    if (std::strcmp(text, r.text) == 0) {
        return false;
    }
    std::memcpy(r.text, text, sizeof text);
    return true;
}

void Menu::refresh_texts() {
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        const Page& p = pages_[r.page];
        const Setting& s = p.settings[r.setting];
        if (s.type == CMFM_TYPE_TEXT) {
            r.text[0] = '\0';
            p.get_text(p.user, s.id, r.text, sizeof r.text);
            r.text[sizeof r.text - 1] = '\0';
        }
    }
}

void Menu::note_value(Row& row, std::uint32_t index) {
    Setting& s = pages_[row.page].settings[row.setting];
    if ((s.flags & CMFM_FLAG_RESTART) == 0) {
        return;
    }
    const bool pending = s.first_index < 0 || static_cast<std::uint32_t>(s.first_index) != index;
    if (pending != s.restart_pending) {
        s.restart_pending = pending;
        ++restart_revision_;
        update_status_texts();
        logf(0, "%s: \"%s\" %s", pages_[row.page].plugin.c_str(), s.label.c_str(),
             pending ? "waits for a restart" : "back to the value the game started with");
    }
}

void Menu::update_status_texts() {
    for (Row& r : rows_) {
        if (r.status_page >= 0) {
            const bool pending = tab_restart_pending(1 + static_cast<std::size_t>(r.status_page));
            std::snprintf(r.text, sizeof r.text, "%s", pending ? "Restart needed" : "Active");
        }
    }
}

bool Menu::is_section(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].type == CMFM_TYPE_SECTION;
}

bool Menu::restart_pending(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].restart_pending;
}

bool Menu::tab_restart_pending(std::size_t tab) const {
    if (tab == 0 || tab - 1 >= shown_) {
        return false;
    }
    for (const Setting& s : pages_[tab - 1].settings) {
        if (s.restart_pending) {
            return true;
        }
    }
    return false;
}

const char* Menu::tab_header(std::size_t tab) {
    header_.clear();
    if (tab == 0) {
        header_ = "Overview";
    } else if (tab - 1 < shown_) {
        const Page& p = pages_[tab - 1];
        header_ = p.title;
        if (!p.version.empty()) {
            header_ += " \xC2\xB7 ";  // middle dot
            if (p.version[0] >= '0' && p.version[0] <= '9') {
                header_ += 'v';
            }
            header_ += p.version;
        }
        if (tab_restart_pending(tab)) {
            header_ += " \xC2\xB7 <STYL MOJI_YELLOW_DEFAULT>Restart needed</STYL>";
        }
    }
    return header_.c_str();
}

const char* Menu::value_text(std::uint32_t setting_id, std::uint32_t index) {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr || !r->shared) {
        return nullptr;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    value_text_ = number(s, index < s.value_count ? index : s.value_count - 1);
    return value_text_.c_str();
}

std::uint32_t Menu::slider_steps(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return 0;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    const bool number = s.type == CMFM_TYPE_INT || s.type == CMFM_TYPE_FLOAT;
    // A locked number row shows its value as text, greyed like any locked row
    // (a greyed slider would still be switched on by the game's row look).
    return number && s.value_count >= 3 && !s.locked ? s.value_count - 1 : 0;
}

std::uint32_t Menu::list_choices(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return 0;
    }
    const Setting& s = pages_[r->page].settings[r->setting];
    if (s.type != CMFM_TYPE_CHOICE || s.value_count > kMaxListChoices || (s.flags & CMFM_FLAG_ARROWS) != 0) {
        return 0;
    }
    return (s.flags & CMFM_FLAG_LIST) != 0 || s.value_count > kArrowChoices ? s.value_count : 0;
}

std::size_t Menu::restart_count() const {
    std::size_t n = 0;
    for (std::size_t p = 0; p < shown_; ++p) {
        for (const Setting& s : pages_[p].settings) {
            n += s.restart_pending;
        }
    }
    return n;
}

void Menu::refresh() {
    for (Row& r : rows_) {
        r.get_failed = false;
        r.lock_logged = false;
        set_help(r, r.base_help.c_str());
    }
    refresh_texts();
    refresh_locks();
}

void Menu::send(cmfm_event_t event) {
    for (std::size_t p = 0; p < shown_; ++p) {
        if (pages_[p].on_event != nullptr) {
            pages_[p].on_event(pages_[p].user, event);
        }
    }
}

std::uint32_t Menu::get(std::uint32_t setting_id) {
    Row* r = setting_row(setting_id);
    if (r == nullptr) {
        return 0;
    }
    Page& p = pages_[r->page];
    Setting& s = p.settings[r->setting];
    cmfm_value_t v = {};
    if (p.get(p.user, s.id, &v) != CMFM_OK) {
        if (!r->get_failed) {
            r->get_failed = true;
            logf(1, "%s: reading \"%s\" failed", p.plugin.c_str(), s.label.c_str());
        }
        return 0;
    }
    const std::uint32_t index = index_of(s, v);
    if (s.first_index < 0) {
        s.first_index = static_cast<std::int32_t>(index);  // the value the session started with
    }
    if (r->shared) {
        std::snprintf(r->text, sizeof r->text, "%s", number(s, index).c_str());  // read right after this
    }
    return index;
}

bool Menu::set(std::uint32_t setting_id, std::int32_t index) {
    Row* r = setting_row(setting_id);
    if (r == nullptr) {
        return false;
    }
    const Page& p = pages_[r->page];
    const Setting& s = p.settings[r->setting];
    if (index < 0 || static_cast<std::uint32_t>(index) >= s.value_count) {
        logf(1, "%s: \"%s\" got value index %d of %u; ignored", p.plugin.c_str(), s.label.c_str(), index,
             s.value_count);
        return false;
    }
    const cmfm_value_t v = value_at(s, static_cast<std::uint32_t>(index));
    const std::string number_name = r->shared ? number(s, static_cast<std::uint32_t>(index)) : std::string();
    const char* name = r->shared ? number_name.c_str() : r->value_ptrs[static_cast<std::size_t>(index)];
    if (s.locked) {
        if (!r->lock_logged) {
            r->lock_logged = true;
            logf(0, "%s: \"%s\" = %s refused: locked (%s)", p.plugin.c_str(), s.label.c_str(), name,
                 s.locked_why.c_str());
        }
        return false;
    }
    if ((s.flags & CMFM_FLAG_RESTART) != 0 && s.first_index < 0) {
        get(setting_id);  // the value before the first change
    }
    if (p.validate != nullptr) {
        char why[kMaxHelp + 1] = {};
        if (p.validate(p.user, s.id, &v, why, sizeof why) != CMFM_OK) {
            why[sizeof why - 1] = '\0';
            char line[sizeof r->help];
            std::snprintf(line, sizeof line, "Not changed: %s", why[0] ? why : "the mod refused this value.");
            set_help(*r, line);
            logf(0, "%s: \"%s\" = %s refused: %s", p.plugin.c_str(), s.label.c_str(), name, why);
            return false;
        }
    }
    if (p.set(p.user, s.id, &v) != CMFM_OK) {
        set_help(*r, "Not changed: the mod could not apply this value.");
        logf(1, "%s: setting \"%s\" = %s failed", p.plugin.c_str(), s.label.c_str(), name);
        return false;
    }
    set_help(*r, r->base_help.c_str());
    logf(0, "%s: \"%s\" = %s", p.plugin.c_str(), s.label.c_str(), name);
    if (r->shared) {
        std::snprintf(r->text, sizeof r->text, "%s", name);
    }
    ++value_revision_;
    note_value(*r, static_cast<std::uint32_t>(index));
    refresh_locks();  // a lock may depend on this value
    return true;
}

void Menu::reset_defaults() {
    std::size_t done = 0;
    std::size_t refused = 0;
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        Page& p = pages_[r.page];
        Setting& s = p.settings[r.setting];
        if (s.type == CMFM_TYPE_TEXT || s.type == CMFM_TYPE_SECTION || s.type == CMFM_TYPE_BUTTON) {
            continue;
        }
        if (reset_setting(r, false)) {
            ++done;
        } else {
            ++refused;
        }
    }
    refresh_locks();
    logf(0, "Set To Default: %zu setting(s) back to their defaults, %zu refused", done, refused);
}

bool Menu::reset_setting(Row& r, bool even_locked) {
    Page& p = pages_[r.page];
    Setting& s = p.settings[r.setting];
    if (s.locked && !even_locked) {
        return false;
    }
    cmfm_value_t current = {};
    if ((s.flags & CMFM_FLAG_RESTART) != 0 && s.first_index < 0 && p.get(p.user, s.id, &current) == CMFM_OK) {
        s.first_index = static_cast<std::int32_t>(index_of(s, current));  // the value before the reset
    }
    const std::uint32_t index = index_of(s, s.default_value);
    const cmfm_value_t v = value_at(s, index);
    char why[kMaxHelp + 1] = {};
    if ((p.validate != nullptr && p.validate(p.user, s.id, &v, why, sizeof why) != CMFM_OK) ||
        p.set(p.user, s.id, &v) != CMFM_OK) {
        return false;
    }
    set_help(r, r.base_help.c_str());
    ++value_revision_;
    note_value(r, index);
    if (r.shared) {
        std::snprintf(r.text, sizeof r.text, "%s", number(s, index).c_str());
    }
    return true;
}

void Menu::reset_page(std::size_t page) {
    std::size_t done = 0;
    std::size_t kept = 0;
    // Every setting, locked and hidden ones too (the player asked for the
    // defaults everywhere; `validate` may still refuse one).
    for (Row& r : rows_) {
        if (r.status || r.page != page) {
            continue;
        }
        const Setting& s = pages_[page].settings[r.setting];
        if (s.type != CMFM_TYPE_BOOL && s.type != CMFM_TYPE_CHOICE && s.type != CMFM_TYPE_INT &&
            s.type != CMFM_TYPE_FLOAT) {
            continue;
        }
        if (reset_setting(r, true)) {
            ++done;
        } else {
            ++kept;
        }
    }
    ++value_revision_;
    refresh_texts();
    refresh_locks();
    logf(0, "%s: Reset to defaults: %zu setting(s) back to their defaults, %zu refused", pages_[page].plugin.c_str(),
         done, kept);
}

}  // namespace cmfm::menu
