#include "config/config_pages.hpp"

#include "menu/keys.hpp"

#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace cmfm::config {

namespace {

std::string lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// The pairs a switch may be written as in a config file.
constexpr const char* kSwitches[][2] = {
    {"true", "false"}, {"1", "0"}, {"on", "off"}, {"yes", "no"}, {"enabled", "disabled"}, {"enable", "disable"},
};

bool parse_int(std::string_view s, long long& out) {
    if (!s.empty() && s.front() == '+') {
        s.remove_prefix(1);
    }
    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.remove_prefix(2);
        base = 16;
    }
    const auto [rest, ec] = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return ec == std::errc() && rest == s.data() + s.size() && !s.empty();
}

bool parse_double(std::string_view s, double& out) {
    if (!s.empty() && s.front() == '+') {
        s.remove_prefix(1);
    }
    const auto [rest, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && rest == s.data() + s.size() && !s.empty() && std::isfinite(out);
}

// A number in the old token's form: hex (with its x and digits' case) if it
// was hex, else decimal.
std::string number_like(long long v, const std::string& old) {
    if (old.size() > 2 && old[0] == '0' && (old[1] == 'x' || old[1] == 'X')) {
        bool upper = false;
        for (std::size_t i = 2; i < old.size(); ++i) {
            upper = upper || (old[i] >= 'A' && old[i] <= 'F');
        }
        char b[32];
        std::snprintf(b, sizeof b, upper ? "%c%llX" : "%c%llx", old[1], static_cast<unsigned long long>(v));
        return std::string("0") + b;
    }
    return std::to_string(v);
}

// Hex digits without a prefix ("62").
bool parse_hex(std::string_view s, long long& out) {
    const auto [rest, ec] = std::from_chars(s.data(), s.data() + s.size(), out, 16);
    return ec == std::errc() && rest == s.data() + s.size() && !s.empty();
}

// Hex digits without a prefix, in the old token's case and width (a leading
// zero kept: "0A").
std::string hex_like(long long v, const std::string& old) {
    bool upper = true;
    for (const char c : old) {
        upper = upper && !(c >= 'a' && c <= 'f');
    }
    char b[32];
    std::snprintf(b, sizeof b, upper ? "%0*llX" : "%0*llx", static_cast<int>(old.size() > 1 && old[0] == '0' ? old.size() : 1),
                  static_cast<unsigned long long>(v));
    return b;
}

void copy_text(char* out, std::uint32_t size, const std::string& text) {
    if (out != nullptr && size > 0) {
        std::snprintf(out, size, "%s", text.c_str());
    }
}

}  // namespace

ConfigPages::ConfigPages(Description description, Files& files, LogFn log)
    : d_(std::move(description)), files_(files), log_(log) {
    build();
}

void ConfigPages::build() {
    plugin_ = "pages/" + d_.file;
    if (!d_.author) {
        plugin_ += d_.made_by.empty() ? " (community page)" : " (community page by " + d_.made_by + ")";
    }
    if (plugin_.size() > 64) {
        plugin_.resize(64);
    }
    std::size_t choice_settings = 0;
    for (const PageSpec& p : d_.pages) {
        for (const SettingSpec& s : p.settings) {
            choice_settings += s.type == SettingSpec::Type::Choice;
        }
    }
    choice_labels_.reserve(choice_settings);
    settings_.resize(d_.pages.size());
    std::uint32_t next_id = 1;
    for (std::size_t p = 0; p < d_.pages.size(); ++p) {
        for (const SettingSpec& spec : d_.pages[p].settings) {
            rows_.push_back({&spec, next_id});
            cmfm_setting_t s = {};
            s.size = sizeof s;
            s.id = next_id++;
            s.label = spec.label.c_str();
            s.help = spec.help.empty() ? nullptr : spec.help.c_str();
            const bool value = spec.type != SettingSpec::Type::Section && spec.type != SettingSpec::Type::Text;
            const bool live = spec.applies.empty() ? d_.live : spec.applies == "live";
            if (value && !live) {
                s.flags |= CMFM_FLAG_RESTART;
            }
            if (!spec.hide_unless.empty()) {
                s.flags |= CMFM_FLAG_HIDE_LOCKED;
            }
            switch (spec.type) {
                case SettingSpec::Type::Section: s.type = CMFM_TYPE_SECTION; break;
                case SettingSpec::Type::Text: s.type = CMFM_TYPE_TEXT; break;
                case SettingSpec::Type::Bool:
                    s.type = CMFM_TYPE_BOOL;
                    s.default_value.i = spec.default_value == "true" ? 1 : 0;
                    break;
                case SettingSpec::Type::Int:
                case SettingSpec::Type::Float: {
                    const bool is_int = spec.type == SettingSpec::Type::Int;
                    s.type = is_int ? CMFM_TYPE_INT : CMFM_TYPE_FLOAT;
                    s.min = spec.min;
                    s.max = spec.max;
                    s.step = spec.step;
                    s.decimals = spec.decimals;
                    s.unit = spec.unit.empty() ? nullptr : spec.unit.c_str();
                    double d = spec.min;
                    if (spec.has_default) {
                        parse_double(spec.default_value, d);
                    }
                    if (is_int) {
                        s.default_value.i = static_cast<std::int32_t>(std::llround(d));
                    } else {
                        s.default_value.f = d;
                    }
                    break;
                }
                case SettingSpec::Type::Choice: {
                    s.type = CMFM_TYPE_CHOICE;
                    std::vector<const char*>& labels = choice_labels_.emplace_back();
                    for (std::size_t c = 0; c < spec.choices.size(); ++c) {
                        labels.push_back(spec.choices[c].second.c_str());
                        if (spec.choices[c].first == spec.default_value) {
                            s.default_value.i = static_cast<std::int32_t>(c);
                        }
                    }
                    s.choices = labels.data();
                    s.choice_count = static_cast<std::uint32_t>(labels.size());
                    break;
                }
                case SettingSpec::Type::Key: {
                    s.type = CMFM_TYPE_KEY;
                    std::int32_t key = 0;
                    if (spec.has_default && menu::parse_key_name(spec.default_value, key)) {
                        s.default_value.i = key;
                    }
                    break;
                }
            }
            settings_[p].push_back(s);
        }
    }
    for (std::size_t p = 0; p < d_.pages.size(); ++p) {
        cmfm_page_t page = {};
        page.size = sizeof page;
        page.plugin = plugin_.c_str();
        page.title = d_.pages[p].title.c_str();
        page.help = d_.pages[p].help.empty() ? nullptr : d_.pages[p].help.c_str();
        page.settings = settings_[p].data();
        page.setting_count = static_cast<std::uint32_t>(settings_[p].size());
        page.user = this;
        page.get = &ConfigPages::get;
        page.set = &ConfigPages::set;
        page.validate = &ConfigPages::validate;
        page.get_text = &ConfigPages::get_text;
        page.on_event = &ConfigPages::on_event;
        page.locked = &ConfigPages::locked;
        page.version = d_.version.empty() ? nullptr : d_.version.c_str();
        page.mod = d_.mod.c_str();
        // "Reset to defaults" only if every value on the page has a default.
        for (const SettingSpec& spec : d_.pages[p].settings) {
            if (spec.type != SettingSpec::Type::Section && spec.type != SettingSpec::Type::Text && !spec.has_default) {
                page.flags |= CMFM_PAGE_NO_RESET;
            }
        }
        pages_.push_back(page);
    }
}

bool ConfigPages::read_file(Document& out, std::string& why) const {
    std::string text;
    if (!files_.read(d_.config_file, text, why)) {
        why = "The config file " + d_.config_file + " cannot be read (" + why + ").";
        return false;
    }
    TextError e;
    if (!out.parse(d_.syntax, std::move(text), e)) {
        why = "The config file " + d_.config_file + " does not parse (line " + std::to_string(e.line) + ": " + e.what +
              ").";
        return false;
    }
    return true;
}

bool ConfigPages::check(std::string& why) {
    Document doc;
    if (!read_file(doc, why)) {
        return false;
    }
    for (const Path& p : d_.expect) {
        Scalar s;
        std::string missing;
        if (!doc.lookup(p, s, missing) && missing.find("not a plain value") == std::string::npos) {
            why = "the config file has no " + path_text(p) + " (another version of the mod?)";
            return false;
        }
    }
    for (const Row& r : rows_) {
        std::int32_t key = 0;
        if (r.spec->type == SettingSpec::Type::Key && r.spec->has_default &&
            !menu::parse_key_name(r.spec->default_value, key)) {
            why = "line " + std::to_string(r.spec->line) + ": \"" + r.spec->default_value + "\" is not a key's name";
            return false;
        }
    }
    return true;
}

const ConfigPages::Row* ConfigPages::row(std::uint32_t id) const {
    return id >= 1 && id <= rows_.size() ? &rows_[id - 1] : nullptr;
}

const ConfigPages::Row* ConfigPages::row_by_name(const std::string& id) const {
    for (const Row& r : rows_) {
        if (r.spec->id == id) {
            return &r;
        }
    }
    return nullptr;
}

bool ConfigPages::current(const Row& r, std::string& text, std::string& why) const {
    const auto change = changes_.find(r.id);
    if (change != changes_.end()) {
        text = change->second;
        return true;
    }
    if (!doc_ok_) {
        why = doc_why_;
        return false;
    }
    Scalar s;
    if (!doc_.lookup(r.spec->path, s, why)) {
        why = "Not in the config file: " + why + ".";
        return false;
    }
    text = s.value;
    return true;
}

bool ConfigPages::decode(const Row& r, const std::string& text, cmfm_value_t& out) const {
    const SettingSpec& s = *r.spec;
    out = {};
    switch (s.type) {
        case SettingSpec::Type::Bool: {
            const std::string l = lower(text);
            for (const auto& pair : kSwitches) {
                if (l == pair[0] || l == pair[1]) {
                    out.i = l == pair[0] ? 1 : 0;
                    return true;
                }
            }
            return false;
        }
        case SettingSpec::Type::Int: {
            long long v = 0;
            if (!parse_int(text, v) || v < s.min || v > s.max) {
                return false;
            }
            const auto k = static_cast<long long>(std::llround(s.step));
            if (k > 0 && (v - static_cast<long long>(std::llround(s.min))) % k != 0) {
                return false;
            }
            out.i = static_cast<std::int32_t>(v);
            return true;
        }
        case SettingSpec::Type::Float: {
            double v = 0;
            if (!parse_double(text, v) || v < s.min - 1e-9 || v > s.max + 1e-9) {
                return false;
            }
            out.f = v;
            return true;
        }
        case SettingSpec::Type::Choice: {
            for (int pass = 0; pass < 2; ++pass) {
                for (std::size_t c = 0; c < s.choices.size(); ++c) {
                    if (pass == 0 ? s.choices[c].first == text : lower(s.choices[c].first) == lower(text)) {
                        out.i = static_cast<std::int32_t>(c);
                        return true;
                    }
                }
            }
            return false;
        }
        case SettingSpec::Type::Key: {
            if (s.keys == "names") {
                std::int32_t key = 0;
                if (!menu::parse_key_name(text, key)) {
                    return false;
                }
                out.i = key;
                return true;
            }
            long long code = 0;
            const bool parsed = s.keys == "vk-hex" ? parse_hex(text, code) : parse_int(text, code);
            if (!parsed || code < 0 || code > 0xFF) {
                return false;
            }
            const std::uint32_t vk = s.keys == "dik" ? menu::vk_from_dik(static_cast<std::uint32_t>(code))
                                                     : static_cast<std::uint32_t>(code);
            if (code != 0 && (vk == 0 || !menu::key_valid(static_cast<std::int32_t>(vk)))) {
                return false;
            }
            out.i = code == 0 ? 0 : static_cast<std::int32_t>(vk);
            return true;
        }
        default:
            return false;
    }
}

bool ConfigPages::encode(const Row& r, const cmfm_value_t& value, std::string& out, std::string& why) const {
    const SettingSpec& s = *r.spec;
    std::string old;  // the file's own token, for its form
    Scalar scalar;
    std::string ignored;
    if (doc_ok_ && doc_.lookup(s.path, scalar, ignored)) {
        old = scalar.value;
    }
    switch (s.type) {
        case SettingSpec::Type::Bool: {
            const std::string l = lower(old);
            const char* const* pair = kSwitches[0];
            for (const auto& p : kSwitches) {
                if (l == p[0] || l == p[1]) {
                    pair = p;
                }
            }
            out = pair[value.i != 0 ? 0 : 1];
            // The old token's case: TRUE, True, true.
            bool all_upper = !old.empty();
            for (const char c : old) {
                all_upper = all_upper && !std::islower(static_cast<unsigned char>(c));
            }
            if (all_upper && std::isalpha(static_cast<unsigned char>(old.front()))) {
                for (char& c : out) {
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
            } else if (!old.empty() && std::isupper(static_cast<unsigned char>(old.front()))) {
                out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
            }
            return true;
        }
        case SettingSpec::Type::Int: out = number_like(value.i, old); return true;
        case SettingSpec::Type::Float: {
            char b[64];
            std::snprintf(b, sizeof b, "%.*f", static_cast<int>(s.decimals), value.f);
            out = b;
            return true;
        }
        case SettingSpec::Type::Choice:
            if (value.i < 0 || static_cast<std::size_t>(value.i) >= s.choices.size()) {
                why = "no such choice";
                return false;
            }
            out = s.choices[static_cast<std::size_t>(value.i)].first;
            return true;
        case SettingSpec::Type::Key: {
            const auto key = static_cast<std::uint32_t>(value.i);
            if (s.keys == "names") {
                if (key == 0) {
                    why = "this mod's file has no way to hold no key";
                    return false;
                }
                out = menu::key_name(value.i);
                return true;
            }
            if (CMFM_KEY_MODS(key) != 0) {
                why = "this mod's file holds a key without Ctrl, Shift or Alt";
                return false;
            }
            std::uint32_t code = CMFM_KEY_VK(key);
            if (s.keys == "dik" && code != 0) {
                code = menu::dik_from_vk(code);
                if (code == 0) {
                    why = "this mod's file cannot hold " + menu::key_name(value.i);
                    return false;
                }
            }
            out = s.keys == "vk-hex" ? hex_like(code, old) : number_like(code, old);
            return true;
        }
        default:
            why = "not a value";
            return false;
    }
}

bool ConfigPages::holds(const std::string& id, const JsonValue& want) const {
    const Row* r = row_by_name(id);
    std::string text;
    std::string why;
    cmfm_value_t v = {};
    if (r == nullptr || !current(*r, text, why) || !decode(*r, text, v)) {
        return false;
    }
    switch (r->spec->type) {
        case SettingSpec::Type::Bool: return (v.i != 0) == want.boolean;
        case SettingSpec::Type::Int: return v.i == want.number;
        case SettingSpec::Type::Float: return std::fabs(v.f - want.number) < 1e-9;
        case SettingSpec::Type::Choice: return r->spec->choices[static_cast<std::size_t>(v.i)].first == want.text;
        default: return false;
    }
}

void ConfigPages::opened() {
    changes_.clear();
    doc_ok_ = read_file(doc_, doc_why_);
    if (!doc_ok_ && log_ != nullptr) {
        log_(1, (plugin_ + ": " + doc_why_).c_str());
    }
}

void ConfigPages::closed() {
    if (changes_.empty()) {
        return;
    }
    std::map<std::uint32_t, std::string> changes;
    changes.swap(changes_);
    Document fresh;
    std::string why;
    if (!read_file(fresh, why)) {
        if (log_ != nullptr) {
            log_(1, (plugin_ + ": " + std::to_string(changes.size()) + " change(s) not written: " + why).c_str());
        }
        return;
    }
    std::vector<Edit> edits;
    for (const auto& [id, text] : changes) {
        const Row* r = row(id);
        Scalar then;
        Scalar now;
        std::string ignored;
        const bool had = doc_ok_ && doc_.lookup(r->spec->path, then, ignored);
        if (!fresh.lookup(r->spec->path, now, ignored) || (had && now.value != then.value)) {
            if (log_ != nullptr) {
                log_(1, (plugin_ + ": \"" + r->spec->label + "\" changed in the file while the page was open; the "
                         "file's value stays").c_str());
            }
            continue;
        }
        edits.push_back({r->spec->path, text});
    }
    std::string out;
    if (edits.empty()) {
        return;
    }
    if (!apply_edits(d_.syntax, fresh.text(), edits, out, why) || !files_.write(d_.config_file, out, why)) {
        if (log_ != nullptr) {
            log_(1, (plugin_ + ": " + std::to_string(edits.size()) + " change(s) not written to " + d_.config_file +
                     ": " + why).c_str());
        }
        return;
    }
    if (log_ != nullptr) {
        log_(0, (plugin_ + ": " + std::to_string(edits.size()) + " change(s) written to " + d_.config_file).c_str());
    }
}

cmfm_result_t CMFM_CALL ConfigPages::get(void* user, std::uint32_t id, cmfm_value_t* value) {
    const auto* self = static_cast<ConfigPages*>(user);
    const Row* r = self->row(id);
    std::string text;
    std::string why;
    return r != nullptr && self->current(*r, text, why) && self->decode(*r, text, *value) ? CMFM_OK : CMFM_ERROR;
}

cmfm_result_t CMFM_CALL ConfigPages::validate(void* user, std::uint32_t id, const cmfm_value_t* value, char* why,
                                              std::uint32_t why_size) {
    auto* self = static_cast<ConfigPages*>(user);
    const Row* r = self->row(id);
    std::string text;
    std::string reason;
    if (r == nullptr || !self->doc_ok_) {
        copy_text(why, why_size, r == nullptr ? "unknown setting" : self->doc_why_);
        return CMFM_ERROR;
    }
    if (!self->encode(*r, *value, text, reason)) {
        copy_text(why, why_size, reason + ".");
        return CMFM_ERROR;
    }
    // A dry run: the file must take this change with the others.
    std::vector<Edit> edits{{r->spec->path, text}};
    for (const auto& [other, t] : self->changes_) {
        if (other != id) {
            edits.push_back({self->row(other)->spec->path, t});
        }
    }
    std::string out;
    if (!apply_edits(self->d_.syntax, self->doc_.text(), edits, out, reason)) {
        copy_text(why, why_size, reason + ".");
        return CMFM_ERROR;
    }
    return CMFM_OK;
}

cmfm_result_t CMFM_CALL ConfigPages::set(void* user, std::uint32_t id, const cmfm_value_t* value) {
    auto* self = static_cast<ConfigPages*>(user);
    const Row* r = self->row(id);
    std::string text;
    std::string why;
    if (r == nullptr || !self->encode(*r, *value, text, why)) {
        return CMFM_ERROR;
    }
    Scalar s;
    if (self->doc_ok_ && self->doc_.lookup(r->spec->path, s, why) && s.value == text) {
        self->changes_.erase(id);  // back to what the file holds
    } else {
        self->changes_[id] = text;
    }
    return CMFM_OK;
}

void CMFM_CALL ConfigPages::get_text(void* user, std::uint32_t id, char* text, std::uint32_t text_size) {
    const auto* self = static_cast<ConfigPages*>(user);
    const Row* r = self->row(id);
    std::string value;
    std::string why;
    if (r != nullptr && self->current(*r, value, why)) {
        copy_text(text, text_size, value);
    } else {
        copy_text(text, text_size, "-");
    }
}

void CMFM_CALL ConfigPages::on_event(void* user, cmfm_event_t event) {
    auto* self = static_cast<ConfigPages*>(user);
    if (event == CMFM_EVENT_OPENED) {
        self->opened();
    } else if (event == CMFM_EVENT_CLOSED) {
        self->closed();
    }
}

std::uint32_t CMFM_CALL ConfigPages::locked(void* user, std::uint32_t id, char* why, std::uint32_t why_size) {
    const auto* self = static_cast<ConfigPages*>(user);
    const Row* r = self->row(id);
    if (r == nullptr) {
        return 0;
    }
    const SettingSpec& s = *r->spec;
    std::string text;
    std::string reason;
    if (s.type != SettingSpec::Type::Section && !self->current(*r, text, reason)) {
        copy_text(why, why_size, reason);
        return 1;
    }
    cmfm_value_t v = {};
    if (s.type != SettingSpec::Type::Section && s.type != SettingSpec::Type::Text && !self->decode(*r, text, v)) {
        copy_text(why, why_size, "The file holds \"" + text + "\", which this page does not know as a value.");
        return 1;
    }
    for (const auto* list : {&s.hide_unless, &s.lock_unless}) {
        for (const auto& [other, want] : *list) {
            if (self->holds(other, want)) {
                continue;
            }
            const Row* o = self->row_by_name(other);
            std::string value = want.kind == JsonValue::Kind::Bool ? (want.boolean ? "On" : "Off") : want.text;
            if (o != nullptr && o->spec->type == SettingSpec::Type::Choice) {
                for (const auto& [stored, label] : o->spec->choices) {
                    if (stored == want.text) {
                        value = label;
                    }
                }
            }
            copy_text(why, why_size, "Needs " + (o != nullptr ? o->spec->label : other) + ": " + value + ".");
            return 1;
        }
    }
    return 0;
}

}  // namespace cmfm::config
