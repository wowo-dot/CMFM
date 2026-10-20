#include "menu/menu.hpp"

#include "cmfm/cmfm_check.h"
#include "menu/keys.hpp"

#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

namespace cmfm::menu {

// A page's settings and CMFM's reset row fit one tab of the model.
static_assert(CMFM_MAX_SETTINGS + 1 <= native::kMaxRowsPerTab);

namespace {

constexpr char kRestartNote[] = "Takes effect after restarting the game.";
constexpr char kButtonText[] = "Run";
// The structs as cmfm.menu 1.0 defined them: anything a plugin passes must be
// at least this big; later fields are read only when `size` covers them.
// Section headings in the game's own text colour tags (as its message files
// write them).
constexpr char kSectionStart[] = "<STYL MOJI_YELLOW_DEFAULT>";
constexpr char kSectionEnd[] = "</STYL>";
// Plain buttons (Menu::is_plain_button): the label in its own colour.
constexpr char kPlainButtonStart[] = "<STYL MOJI_RED_DEFAULT>";
constexpr char kKeyConflictStart[] = "<STYL MOJI_RED_DEFAULT>";
constexpr char kKeyWaitHelp[] =
    "Press the new key, with Ctrl, Shift or Alt if you like. Esc or a click cancels, Backspace sets no key.";
const char* const kOffOn[] = {"Off", "On"};

bool resettable_type(cmfm_type_t t) {
    return t == CMFM_TYPE_BOOL || t == CMFM_TYPE_CHOICE || t == CMFM_TYPE_INT || t == CMFM_TYPE_FLOAT ||
           t == CMFM_TYPE_KEY;
}
const char* const kDash[] = {"-"};
const char* const kNoValue[] = {""};
const native::RowSpec kEmptyRow = {"Empty", "No mod uses this tab.", kDash, 1, 0};


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

// A setting cmfm_check_setting accepted, copied; `size` is the array's stride.
void copy_setting(const cmfm_setting_t& in, std::size_t size, Setting& s) {
    s.id = in.id;
    s.type = in.type;
    s.flags = in.flags;
    s.label = in.label;
    s.help = in.help != nullptr ? in.help : "";
    s.default_value = in.default_value;
    s.value_count = 1;
    switch (in.type) {
        case CMFM_TYPE_BOOL:
            s.value_count = 2;
            break;
        case CMFM_TYPE_CHOICE:
            s.choices.assign(in.choices, in.choices + in.choice_count);
            s.value_count = in.choice_count;
            break;
        case CMFM_TYPE_INT:
        case CMFM_TYPE_FLOAT:
            s.min = in.min;
            s.max = in.max;
            s.step = in.step;
            s.decimals = in.type == CMFM_TYPE_INT ? 0 : in.decimals;
            s.unit = in.unit != nullptr ? in.unit : "";
            s.value_count = range_count(in.min, in.max, in.step);
            break;
        case CMFM_TYPE_SECTION:
            s.help.clear();
            break;
        case CMFM_TYPE_BUTTON: {
            const bool sized = size >= offsetof(cmfm_setting_t, confirm) + sizeof in.confirm;
            s.button = sized && in.button != nullptr && in.button[0] != '\0' ? in.button : kButtonText;
            s.confirm = sized && in.confirm != nullptr ? in.confirm : "";
            break;
        }
        default:  // TEXT, KEY
            break;
    }
}

}  // namespace

bool copy_page(const cmfm_page_t* page, Page& out, std::string& why) {
    // The rules are cmfm_check.h's, the header plugins use to check their
    // pages outside the game, so the two cannot drift apart.
    char reason[256] = {};
    if (cmfm_check_page(page, reason, sizeof reason) != CMFM_OK) {
        why = reason;
        return false;
    }
    out.plugin = page->plugin;
    out.title = page->title;
    out.help = page->help != nullptr ? page->help : "";
    const std::uint32_t stride = page->settings[0].size;
    const char* base = reinterpret_cast<const char*>(page->settings);
    out.settings.resize(page->setting_count);
    for (std::uint32_t i = 0; i < page->setting_count; ++i) {
        copy_setting(*reinterpret_cast<const cmfm_setting_t*>(base + std::size_t{i} * stride), stride, out.settings[i]);
    }
    const auto has = [page](std::size_t offset, std::size_t field_size) { return page->size >= offset + field_size; };
    out.flags = has(offsetof(cmfm_page_t, flags), sizeof page->flags) ? page->flags : 0;
    const char* mod = has(offsetof(cmfm_page_t, mod), sizeof page->mod) ? page->mod : nullptr;
    out.mod = mod != nullptr ? mod : "";
    const char* version = has(offsetof(cmfm_page_t, version), sizeof page->version) ? page->version : nullptr;
    out.version = version != nullptr ? version : "";
    out.user = page->user;
    out.get = page->get;
    out.set = page->set;
    out.validate = page->validate;
    out.get_text = page->get_text;
    out.on_event = page->on_event;
    out.press = has(offsetof(cmfm_page_t, press), sizeof page->press) ? page->press : nullptr;
    out.locked = has(offsetof(cmfm_page_t, locked), sizeof page->locked) ? page->locked : nullptr;
    out.question = has(offsetof(cmfm_page_t, question), sizeof page->question) ? page->question : nullptr;
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

void Menu::log_set(std::uint32_t setting_id, const std::string& line) {
    if (setting_id == log_setting_) {
        log_pending_ = line;
        ++log_steps_;
        return;
    }
    flush_set_log();
    logf(0, "%s", line.c_str());
    log_setting_ = setting_id;
}

void Menu::flush_set_log() {
    if (!log_pending_.empty()) {
        logf(0, "%s (after %u more step%s)", log_pending_.c_str(), log_steps_, log_steps_ == 1 ? "" : "s");
    }
    log_pending_.clear();
    log_steps_ = 0;
    log_setting_ = 0;
}

void Menu::set_help(Row& row, const char* text) {
    std::snprintf(row.help, sizeof row.help, "%s", text);
}

void Menu::add(Page page) {
    bool resettable = false;
    for (const Setting& s : page.settings) {
        resettable = resettable || resettable_type(s.type);
    }
    if (resettable && (page.flags & CMFM_PAGE_NO_RESET) == 0) {
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

void Menu::set_problems(std::string text) {
    if (text != problems_) {
        problems_ = std::move(text);
        dirty_ = true;
    }
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
    group_mods(shown);
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
           problems_.empty() ? "Configuration Menu For Mods. Each mod that supports it has an entry in this list."
                             : "Configuration Menu For Mods. " + problems_);
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
            // One row per mod, at its first page.
            const std::size_t m = tab_mod_[1 + en.index];
            const Mod& mod = mods_[m];
            if (mod.tabs.front() != 1 + en.index) {
                continue;
            }
            std::size_t count = 0;
            for (const std::size_t t : mod.tabs) {
                for (const Setting& s : pages_[t - 1].settings) {
                    count += s.type != CMFM_TYPE_SECTION && s.id != kResetId;
                }
            }
            std::string help = "From " + mod.plugin + ": " + std::to_string(count) + " setting(s)";
            if (mod.tabs.size() > 1) {
                help += " on " + std::to_string(mod.tabs.size()) + " pages";
            }
            status(mod.name, "Active", help + ".");
            rows_.back().status_mod = static_cast<std::int32_t>(m);
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
            } else if (s.type == CMFM_TYPE_KEY) {
                // One text, the key's name (written on every read and change).
                std::snprintf(r.text, sizeof r.text, "%s", key_name(s.key > 0 ? s.key : 0).c_str());
                r.value_ptrs = {r.text};
            } else if (s.type == CMFM_TYPE_BUTTON) {
                if (s.id == kResetId) {
                    r.label = kPlainButtonStart + s.label + kSectionEnd;
                }
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
    tab_names_.assign(tab_count_, std::string());
    tab_helps_.assign(tab_count_, std::string());
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
        not_shown_why_[p] = p < kMaxPages ? "" : "Not shown: only 1024 mod pages fit.";
    }
    std::size_t shown = pages_.size() < kMaxPages ? pages_.size() : kMaxPages;
    std::vector<native::TabSpec> tabs;
    std::vector<native::RowSpec> specs;
    for (;;) {
        compose(shown, specs);
        tabs.assign(tab_count_, native::TabSpec{});
        for (std::size_t t = 0; t < tab_count_; ++t) {
            tabs[t] = {tab_names_[t].c_str(), tab_helps_[t].c_str()};
        }
        const char* fail = nullptr;
        if (model.build("Mods", tabs.data(), tab_count_, specs.data(), specs.size(), kEmptyRow, fail)) {
            break;
        }
        if (shown == 0) {
            why = std::string("the status page does not fit: ") + fail;
            model_ = nullptr;
            shown_ = 0;
            return false;
        }
        --shown;
        not_shown_why_[shown] = std::string("Not shown: ") + fail + ".";
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

bool Menu::is_plain_button(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].id == kResetId;
}

const char* Menu::button_question(std::uint32_t setting_id) {
    const Row* r = any_setting_row(setting_id);
    if (r == nullptr) {
        return nullptr;
    }
    const Page& p = pages_[r->page];
    const Setting& s = p.settings[r->setting];
    if (s.type != CMFM_TYPE_BUTTON || s.confirm.empty()) {
        return nullptr;
    }
    // Since 1.3 the page may word the question now.
    if (p.question != nullptr) {
        char text[CMFM_MAX_QUESTION + 64] = {};
        if (p.question(p.user, s.id, text, sizeof text) == CMFM_OK) {
            text[sizeof text - 1] = '\0';
            std::size_t n = strnlen(text, sizeof text);
            if (n > CMFM_MAX_QUESTION) {
                n = CMFM_MAX_QUESTION;
                while (n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) {
                    --n;  // not inside a UTF-8 character
                }
            }
            if (n > 0) {
                question_.assign(text, n);
                return question_.c_str();
            }
        }
    }
    return s.confirm.c_str();
}

bool Menu::press(std::uint32_t setting_id) {
    flush_set_log();
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
    update_keys();  // an unlocked KEY row gets its conflict note back
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
        if (r.status_mod >= 0) {
            const bool pending = mod_restart_pending(static_cast<std::size_t>(r.status_mod));
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

void Menu::group_mods(std::size_t shown) {
    mods_.clear();
    tab_mod_.assign(1 + shown, 0);
    mods_.push_back({"Overview", "", {0}});
    for (std::size_t p = 0; p < shown; ++p) {
        const Page& page = pages_[p];
        std::size_t m = 1;
        for (; m < mods_.size(); ++m) {
            const Page& first = pages_[mods_[m].tabs.front() - 1];
            if (first.plugin == page.plugin && first.mod == page.mod) {
                break;
            }
        }
        if (m == mods_.size()) {
            mods_.push_back({page.mod.empty() ? page.title : page.mod, page.plugin, {}});
        }
        mods_[m].tabs.push_back(1 + p);
        tab_mod_[1 + p] = m;
    }
}

bool Menu::mod_restart_pending(std::size_t mod) const {
    if (mod >= mods_.size()) {
        return false;
    }
    for (const std::size_t t : mods_[mod].tabs) {
        if (tab_restart_pending(t)) {
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
        const Mod& mod = mods_[mod_of_tab(tab)];
        header_ = mod.tabs.size() > 1 ? mod.name : p.title;  // several pages: the tabs name the page
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
    flush_set_log();
    key_waiting_ = 0;
    for (Row& r : rows_) {
        r.get_failed = false;
        r.lock_logged = false;
        set_help(r, r.base_help.c_str());
    }
    refresh_texts();
    refresh_locks();
    read_keys();
}

void Menu::read_keys() {
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        Page& p = pages_[r.page];
        Setting& s = p.settings[r.setting];
        cmfm_value_t v = {};
        if (s.type == CMFM_TYPE_KEY && p.get(p.user, s.id, &v) == CMFM_OK) {
            s.key = key_valid(v.i) ? v.i : 0;
            if (s.first_index < 0) {
                s.first_index = s.key;  // the key the session started with
            }
        }
    }
    update_keys();
}

void Menu::update_keys() {
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        const Setting& s = pages_[r.page].settings[r.setting];
        if (s.type != CMFM_TYPE_KEY || s.id == key_waiting_) {
            continue;
        }
        // A lock's reason and a refusal keep the help line.
        const bool keep_help = s.locked || std::strncmp(r.help, "Not changed: ", 13) == 0;
        // The first other KEY setting it overlaps, and how many there are.
        const Setting* other = nullptr;
        std::size_t other_page = 0;
        std::size_t others = 0;
        for (std::size_t p = 0; p < shown_ && s.key > 0; ++p) {
            for (const Setting& o : pages_[p].settings) {
                if (&o != &s && o.type == CMFM_TYPE_KEY && keys_overlap(o.key > 0 ? o.key : 0, s.key)) {
                    if (other == nullptr) {
                        other = &o;
                        other_page = p;
                    }
                    ++others;
                }
            }
        }
        const std::string name = key_name(s.key > 0 ? s.key : 0);
        if (other == nullptr) {
            std::snprintf(r.text, sizeof r.text, "%s", name.c_str());
            if (!keep_help) {
                set_help(r, r.base_help.c_str());
            }
            continue;
        }
        std::snprintf(r.text, sizeof r.text, "%s%s%s", kKeyConflictStart, name.c_str(), kSectionEnd);
        if (keep_help) {
            continue;
        }
        std::string note = std::string(kKeyConflictStart) + key_name(other->key) + " is also " + other->label;
        if (other_page != r.page) {
            note += " (" + pages_[other_page].title + ")";
        }
        if (others > 1) {
            note += " and " + std::to_string(others - 1) + " more";
        }
        note += std::string(".") + kSectionEnd;
        if (!r.base_help.empty()) {
            note += " " + r.base_help;
        }
        set_help(r, note.c_str());
    }
}

bool Menu::is_key(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr && pages_[r->page].settings[r->setting].type == CMFM_TYPE_KEY;
}

bool Menu::begin_key(std::uint32_t setting_id) {
    const std::size_t i = row_index(setting_id);
    if (i >= rows_.size() || !is_key(setting_id)) {
        return false;
    }
    Row& r = rows_[i];
    const Page& p = pages_[r.page];
    const Setting& s = p.settings[r.setting];
    if (s.locked) {
        logf(0, "%s: \"%s\" is locked: %s", p.plugin.c_str(), s.label.c_str(), s.locked_why.c_str());
        return false;
    }
    key_waiting_ = s.id;
    key_wait(setting_id, 10);
    set_help(r, kKeyWaitHelp);
    return true;
}

void Menu::key_wait(std::uint32_t setting_id, std::uint32_t seconds) {
    const std::size_t i = row_index(setting_id);
    if (i < rows_.size() && is_key(setting_id) && pages_[rows_[i].page].settings[rows_[i].setting].id == key_waiting_) {
        std::snprintf(rows_[i].text, sizeof rows_[i].text, "Press a key (%u)", seconds);
    }
}

void Menu::cancel_key(std::uint32_t setting_id) {
    const std::size_t i = row_index(setting_id);
    if (i >= rows_.size() || !is_key(setting_id)) {
        return;
    }
    key_waiting_ = 0;
    set_help(rows_[i], rows_[i].base_help.c_str());
    update_keys();
}

bool Menu::set_key(std::uint32_t setting_id, std::int32_t key) {
    flush_set_log();
    const std::size_t i = row_index(setting_id);
    if (i >= rows_.size() || !is_key(setting_id)) {
        return false;
    }
    key_waiting_ = 0;
    Row& r = rows_[i];
    const Page& p = pages_[r.page];
    Setting& s = pages_[r.page].settings[r.setting];
    const std::string name = key_name(key_valid(key) ? key : 0);
    const auto refuse = [&](const char* why, bool warn) {
        char line[sizeof r.help];
        std::snprintf(line, sizeof line, "Not changed: %s", why);
        set_help(r, line);
        logf(warn ? 1 : 0, "%s: \"%s\" = %s refused: %s", p.plugin.c_str(), s.label.c_str(), name.c_str(), why);
        update_keys();
        return false;
    };
    if (!key_valid(key)) {
        return refuse("this key cannot be bound.", false);
    }
    if (s.locked) {
        return refuse(s.locked_why.empty() ? "the mod does not allow a change now." : s.locked_why.c_str(), false);
    }
    if ((s.flags & CMFM_FLAG_RESTART) != 0 && s.first_index < 0) {
        cmfm_value_t before = {};
        if (p.get(p.user, s.id, &before) == CMFM_OK) {
            s.first_index = key_valid(before.i) ? before.i : 0;  // the key before the first change
        }
    }
    cmfm_value_t v = {};
    v.i = key;
    if (p.validate != nullptr) {
        char why[kMaxHelp + 1] = {};
        if (p.validate(p.user, s.id, &v, why, sizeof why) != CMFM_OK) {
            why[sizeof why - 1] = '\0';
            return refuse(why[0] ? why : "the mod refused this key.", false);
        }
    }
    if (p.set(p.user, s.id, &v) != CMFM_OK) {
        return refuse("the mod could not apply this key.", true);
    }
    set_help(r, r.base_help.c_str());
    logf(0, "%s: \"%s\" = %s", p.plugin.c_str(), s.label.c_str(), name.c_str());
    s.key = key;
    ++value_revision_;
    note_value(r, static_cast<std::uint32_t>(key));
    refresh_locks();
    update_keys();
    return true;
}

const char* Menu::row_help(std::uint32_t setting_id) const {
    const Row* r = any_setting_row(setting_id);
    return r != nullptr ? r->help : nullptr;
}

void Menu::send(cmfm_event_t event) {
    flush_set_log();
    // Once per on_event and user pair: the pages of one mod that share them
    // hear an event once.
    for (std::size_t p = 0; p < shown_; ++p) {
        const Page& page = pages_[p];
        bool sent = page.on_event == nullptr;
        for (std::size_t q = 0; q < p && !sent; ++q) {
            sent = pages_[q].on_event == page.on_event && pages_[q].user == page.user;
        }
        if (!sent) {
            page.on_event(page.user, event);
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
    if (s.type == CMFM_TYPE_KEY) {
        const std::int32_t key = key_valid(v.i) ? v.i : 0;
        if (s.first_index < 0) {
            s.first_index = key;  // the key the session started with
        }
        if (key != s.key) {
            s.key = key;  // changed by the mod itself
            update_keys();
        }
        return 0;  // the row's one text, the key's name
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
    if (s.type == CMFM_TYPE_KEY) {
        return false;  // keys are set by set_key only
    }
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
    log_set(setting_id, p.plugin + ": \"" + s.label + "\" = " + name);
    if (r->shared) {
        std::snprintf(r->text, sizeof r->text, "%s", name);
    }
    ++value_revision_;
    note_value(*r, static_cast<std::uint32_t>(index));
    refresh_locks();  // a lock may depend on this value
    return true;
}

void Menu::reset_defaults() {
    flush_set_log();
    std::size_t done = 0;
    std::size_t refused = 0;
    for (Row& r : rows_) {
        if (r.status) {
            continue;
        }
        Page& p = pages_[r.page];
        Setting& s = p.settings[r.setting];
        if (!resettable_type(s.type)) {
            continue;
        }
        if (reset_setting(r, false)) {
            ++done;
        } else {
            ++refused;
        }
    }
    refresh_locks();
    update_keys();
    logf(0, "Set To Default: %zu setting(s) back to their defaults, %zu refused", done, refused);
}

bool Menu::reset_setting(Row& r, bool even_locked) {
    Page& p = pages_[r.page];
    Setting& s = p.settings[r.setting];
    if (s.locked && !even_locked) {
        return false;
    }
    const bool key = s.type == CMFM_TYPE_KEY;
    cmfm_value_t current = {};
    if ((s.flags & CMFM_FLAG_RESTART) != 0 && s.first_index < 0 && p.get(p.user, s.id, &current) == CMFM_OK) {
        // The value before the reset.
        s.first_index = key ? (key_valid(current.i) ? current.i : 0) : static_cast<std::int32_t>(index_of(s, current));
    }
    const std::uint32_t index = key ? static_cast<std::uint32_t>(s.default_value.i) : index_of(s, s.default_value);
    cmfm_value_t v = value_at(s, index);
    if (key) {
        v = {};
        v.i = s.default_value.i;
    }
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
    if (key) {
        s.key = v.i;
    }
    return true;
}

void Menu::reset_page(std::size_t page) {
    flush_set_log();
    std::size_t done = 0;
    std::size_t kept = 0;
    // Every setting, locked and hidden ones too (the player asked for the
    // defaults everywhere; `validate` may still refuse one).
    for (Row& r : rows_) {
        if (r.status || r.page != page) {
            continue;
        }
        const Setting& s = pages_[page].settings[r.setting];
        if (!resettable_type(s.type)) {
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
    update_keys();
    logf(0, "%s: Reset to defaults: %zu setting(s) back to their defaults, %zu refused", pages_[page].plugin.c_str(),
         done, kept);
}

}  // namespace cmfm::menu
