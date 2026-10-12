#include "config/description.hpp"

#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace cmfm::config {

namespace {

constexpr std::size_t kMaxMod = 24;      // the mod list's space (cmfm.h: title)
constexpr std::size_t kMaxTitle = 24;
constexpr std::size_t kMaxLabel = 64;
constexpr std::size_t kMaxHelp = 200;
constexpr std::size_t kMaxChoice = 32;
constexpr std::size_t kMaxChoices = 32;
constexpr std::size_t kMaxUnit = 8;
constexpr std::size_t kMaxMadeBy = 64;
constexpr std::size_t kMaxSettings = 1024;
constexpr double kMaxSteps = 101;

class Checker {
public:
    Checker(std::string_view text, std::string& why) : text_(text), why_(why) {}

    bool fail(const JsonValue& at, const std::string& what) {
        const TextError e = position_of(text_, at.begin, what);
        why_ = "line " + std::to_string(e.line) + ": " + what;
        return false;
    }

    bool object(const JsonValue& v, const char* what) {
        return v.kind == JsonValue::Kind::Object || fail(v, std::string(what) + " must be an object { ... }");
    }

    // Every member of `v` is one of `known`.
    bool only(const JsonValue& v, std::initializer_list<const char*> known, const char* what) {
        for (const auto& [key, member] : v.members) {
            bool ok = false;
            for (const char* k : known) {
                ok = ok || key == k;
            }
            if (!ok) {
                return fail(member, "unknown field \"" + key + "\" in " + what);
            }
        }
        return true;
    }

    const JsonValue* member(const JsonValue& v, const char* key) {
        for (const auto& [k, m] : v.members) {
            if (k == key) {
                return &m;
            }
        }
        return nullptr;
    }

    // A string field: required or not, 1..limit bytes.
    bool text(const JsonValue& v, const char* key, std::size_t limit, bool required, std::string& out) {
        const JsonValue* m = member(v, key);
        if (m == nullptr) {
            return !required || fail(v, std::string("\"") + key + "\" is missing");
        }
        if (m->kind != JsonValue::Kind::String) {
            return fail(*m, std::string("\"") + key + "\" must be a string in quotes");
        }
        if (m->text.empty() || m->text.size() > limit) {
            return fail(*m, std::string("\"") + key + "\" must be 1 to " + std::to_string(limit) + " bytes");
        }
        out = m->text;
        return true;
    }

    bool number(const JsonValue& v, const char* key, bool required, double& out) {
        const JsonValue* m = member(v, key);
        if (m == nullptr) {
            return !required || fail(v, std::string("\"") + key + "\" is missing");
        }
        if (m->kind != JsonValue::Kind::Number) {
            return fail(*m, std::string("\"") + key + "\" must be a number");
        }
        out = m->number;
        return true;
    }

    bool path(const JsonValue& v, SettingSpec& s) {
        const JsonValue* m = member(v, "path");
        if (m == nullptr) {
            return fail(v, "\"path\" is missing (where the value is in the config file)");
        }
        s.path.clear();
        if (m->kind == JsonValue::Kind::String) {
            if (!parse_path(m->text, s.path)) {
                return fail(*m, "\"path\" is not a path like Section.Key or a.b[0].c");
            }
        } else if (m->kind == JsonValue::Kind::Array && !m->items.empty()) {
            for (const JsonValue& part : m->items) {
                if (part.kind != JsonValue::Kind::String || part.text.empty()) {
                    return fail(part, "a \"path\" list holds the keys as strings");
                }
                s.path.push_back(PathPart{part.text, false, 0});
            }
        } else {
            return fail(*m, "\"path\" must be a string or a list of keys");
        }
        s.path_text = path_text(s.path);
        return true;
    }

    std::string_view text_;
    std::string& why_;
};

std::string number_text(double v, std::uint32_t decimals) {
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", static_cast<int>(decimals), v);
    return b;
}

bool condition(Checker& c, const JsonValue& v, const char* key, std::vector<std::pair<std::string, JsonValue>>& out) {
    const JsonValue* m = c.member(v, key);
    if (m == nullptr) {
        return true;
    }
    if (m->kind != JsonValue::Kind::Object || m->members.empty()) {
        return c.fail(*m, std::string("\"") + key + "\" must be an object like { \"other_id\": true }");
    }
    for (const auto& [id, want] : m->members) {
        if (want.kind != JsonValue::Kind::Bool && want.kind != JsonValue::Kind::Number &&
            want.kind != JsonValue::Kind::String) {
            return c.fail(want, std::string("\"") + key + "\": a value must be true/false, a number or a string");
        }
        out.emplace_back(id, want);
    }
    return true;
}

bool setting(Checker& c, const JsonValue& v, SettingSpec& s) {
    if (!c.object(v, "a setting")) {
        return false;
    }
    s.line = position_of(c.text_, v.begin, "").line;
    // Unknown fields first: a typo ("lable") explains a field that seems missing.
    if (!c.only(v, {"id", "section", "type", "label", "help", "path", "default", "min", "max", "step", "decimals",
                    "unit", "choices", "keys", "hide_unless", "lock_unless"},
                "a setting") ||
        !c.text(v, "id", kMaxLabel, false, s.id)) {
        return false;
    }
    if (c.member(v, "section") != nullptr) {
        s.type = SettingSpec::Type::Section;
        return c.only(v, {"section", "id", "hide_unless"}, "a section") &&
               c.text(v, "section", kMaxLabel, true, s.label) && condition(c, v, "hide_unless", s.hide_unless);
    }
    std::string type;
    if (!c.text(v, "type", 16, true, type) || !c.text(v, "label", kMaxLabel, true, s.label) ||
        !c.text(v, "help", kMaxHelp, false, s.help) || !c.path(v, s) ||
        !condition(c, v, "hide_unless", s.hide_unless) || !condition(c, v, "lock_unless", s.lock_unless)) {
        return false;
    }
    const JsonValue* def = c.member(v, "default");
    const JsonValue& at = *c.member(v, "type");
    if (type == "bool") {
        s.type = SettingSpec::Type::Bool;
        if (!c.only(v, {"id", "type", "label", "help", "path", "default", "hide_unless", "lock_unless"}, "a bool")) {
            return false;
        }
        if (def != nullptr) {
            if (def->kind != JsonValue::Kind::Bool) {
                return c.fail(*def, "a bool's \"default\" is true or false");
            }
            s.has_default = true;
            s.default_value = def->boolean ? "true" : "false";
        }
        return true;
    }
    if (type == "int" || type == "float") {
        const bool is_int = type == "int";
        s.type = is_int ? SettingSpec::Type::Int : SettingSpec::Type::Float;
        if (!c.only(v, {"id", "type", "label", "help", "path", "default", "min", "max", "step", "decimals", "unit",
                        "hide_unless", "lock_unless"},
                    is_int ? "an int" : "a float") ||
            !c.number(v, "min", true, s.min) || !c.number(v, "max", true, s.max) || !c.number(v, "step", false, s.step) ||
            !c.text(v, "unit", kMaxUnit, false, s.unit)) {
            return false;
        }
        double decimals = 0;
        if (!c.number(v, "decimals", false, decimals)) {
            return false;
        }
        if (is_int && c.member(v, "decimals") != nullptr) {
            return c.fail(*c.member(v, "decimals"), "an int has no decimals");
        }
        if (!(decimals >= 0 && decimals <= 3 && decimals == std::floor(decimals))) {
            return c.fail(v, "\"decimals\" is 0 to 3");
        }
        s.decimals = static_cast<std::uint32_t>(decimals);
        const auto whole = [](double x) { return std::isfinite(x) && x == std::floor(x); };
        if (!(s.step > 0) || !(s.min <= s.max) || (is_int && (!whole(s.min) || !whole(s.max) || !whole(s.step)))) {
            return c.fail(v, is_int ? "an int needs whole min <= max and a whole step > 0" : "needs min <= max and step > 0");
        }
        if ((s.max - s.min) / s.step + 1 > kMaxSteps + 0.5) {
            return c.fail(v, "more than 101 values from min to max (the slider has positions 0 to 100)");
        }
        if (def != nullptr) {
            if (def->kind != JsonValue::Kind::Number || def->number < s.min - 1e-9 || def->number > s.max + 1e-9) {
                return c.fail(*def, "\"default\" must be a number from min to max");
            }
            s.has_default = true;
            s.default_value = is_int ? number_text(def->number, 0) : number_text(def->number, s.decimals);
        }
        return true;
    }
    if (type == "choice") {
        s.type = SettingSpec::Type::Choice;
        if (!c.only(v, {"id", "type", "label", "help", "path", "default", "choices", "hide_unless", "lock_unless"},
                    "a choice")) {
            return false;
        }
        const JsonValue* choices = c.member(v, "choices");
        if (choices == nullptr) {
            return c.fail(v, "\"choices\" is missing");
        }
        if (choices->kind == JsonValue::Kind::Object) {
            for (const auto& [stored, label] : choices->members) {
                if (label.kind != JsonValue::Kind::String || label.text.empty() || label.text.size() > kMaxChoice) {
                    return c.fail(label, "a choice's label is a string of 1 to 32 bytes");
                }
                s.choices.emplace_back(stored, label.text);
            }
        } else if (choices->kind == JsonValue::Kind::Array) {
            for (const JsonValue& item : choices->items) {
                if (item.kind != JsonValue::Kind::String || item.text.empty() || item.text.size() > kMaxChoice) {
                    return c.fail(item, "a choice is a string of 1 to 32 bytes");
                }
                s.choices.emplace_back(item.text, item.text);
            }
        } else {
            return c.fail(*choices, "\"choices\" is an object { \"stored\": \"Label\" } or a list of strings");
        }
        if (s.choices.empty() || s.choices.size() > kMaxChoices) {
            return c.fail(*choices, "1 to 32 choices");
        }
        for (std::size_t i = 0; i < s.choices.size(); ++i) {
            for (std::size_t j = 0; j < i; ++j) {
                if (s.choices[i].first == s.choices[j].first) {
                    return c.fail(*choices, "the stored value \"" + s.choices[i].first + "\" is listed twice");
                }
            }
        }
        if (def != nullptr) {
            bool known = false;
            for (const auto& [stored, label] : s.choices) {
                known = known || (def->kind == JsonValue::Kind::String && def->text == stored);
            }
            if (!known) {
                return c.fail(*def, "\"default\" must be one of the choices' stored values");
            }
            s.has_default = true;
            s.default_value = def->text;
        }
        return true;
    }
    if (type == "key") {
        s.type = SettingSpec::Type::Key;
        if (!c.only(v, {"id", "type", "label", "help", "path", "default", "keys", "hide_unless", "lock_unless"},
                    "a key") ||
            !c.text(v, "keys", 8, true, s.keys)) {
            return false;
        }
        if (s.keys != "vk" && s.keys != "dik" && s.keys != "names") {
            return c.fail(*c.member(v, "keys"), "\"keys\" is \"vk\", \"dik\" or \"names\"");
        }
        if (def != nullptr) {
            if (def->kind != JsonValue::Kind::String) {
                return c.fail(*def, "a key's \"default\" is its name, like \"F4\" or \"Ctrl+H\"");
            }
            s.has_default = true;
            s.default_value = def->text;
        }
        return true;
    }
    if (type == "text") {
        s.type = SettingSpec::Type::Text;
        return c.only(v, {"id", "type", "label", "help", "path", "hide_unless"}, "a text line");
    }
    return c.fail(at, "\"type\" is bool, int, float, choice, key or text (or a { \"section\": ... })");
}

const SettingSpec* by_id(const Description& d, const std::string& id) {
    for (const PageSpec& p : d.pages) {
        for (const SettingSpec& s : p.settings) {
            if (!s.id.empty() && s.id == id) {
                return &s;
            }
        }
    }
    return nullptr;
}

// Conditions name settings that exist, hold a value, and fit their type.
bool check_conditions(const Description& d, std::string& why) {
    for (const PageSpec& p : d.pages) {
        for (const SettingSpec& s : p.settings) {
            for (const auto* list : {&s.hide_unless, &s.lock_unless}) {
                for (const auto& [id, want] : *list) {
                    const SettingSpec* other = by_id(d, id);
                    const std::string at = "line " + std::to_string(s.line) + ": ";
                    if (other == nullptr) {
                        why = at + "a condition names \"" + id + "\", which no setting has as its id";
                        return false;
                    }
                    if (other == &s) {
                        why = at + "a setting's condition names itself";
                        return false;
                    }
                    bool fits = false;
                    switch (other->type) {
                        case SettingSpec::Type::Bool: fits = want.kind == JsonValue::Kind::Bool; break;
                        case SettingSpec::Type::Int:
                        case SettingSpec::Type::Float: fits = want.kind == JsonValue::Kind::Number; break;
                        case SettingSpec::Type::Choice:
                            for (const auto& [stored, label] : other->choices) {
                                fits = fits || (want.kind == JsonValue::Kind::String && want.text == stored);
                            }
                            break;
                        default: break;
                    }
                    if (!fits) {
                        why = at + "a condition on \"" + id + "\" must use one of its values (a bool's true/false, "
                              "a number, a choice's stored value)";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

}  // namespace

bool safe_relative_path(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t start = 0;
    while (start <= path.size()) {
        std::size_t end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos) {
            end = path.size();
        }
        const std::string_view part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") {
            return false;
        }
        start = end + 1;
    }
    return true;
}

bool parse_description(std::string_view text, std::string_view file_name, Description& out, std::string& why) {
    out = Description{};
    out.file = std::string(file_name);
    JsonValue root;
    TextError error;
    if (!parse_json(text, root, error)) {
        why = "line " + std::to_string(error.line) + ": " + error.what;
        return false;
    }
    Checker c(text, why);
    if (!c.object(root, "the file") ||
        !c.only(root, {"cmfm", "mod", "version", "source", "made_by", "config", "applies", "pages"}, "the file")) {
        return false;
    }
    double format = 0;
    if (!c.number(root, "cmfm", true, format)) {
        return false;
    }
    if (format != 1) {
        return c.fail(*c.member(root, "cmfm"), "\"cmfm\" is 1 (this CMFM reads description format 1)");
    }
    std::string source;
    std::string applies = "restart";
    if (!c.text(root, "mod", kMaxMod, true, out.mod) || !c.text(root, "version", kMaxTitle, false, out.version) ||
        !c.text(root, "source", 16, true, source) || !c.text(root, "made_by", kMaxMadeBy, false, out.made_by) ||
        !c.text(root, "applies", 16, false, applies)) {
        return false;
    }
    if (source != "author" && source != "community") {
        return c.fail(*c.member(root, "source"), "\"source\" is \"author\" or \"community\"");
    }
    out.author = source == "author";
    if (applies != "restart" && applies != "live") {
        return c.fail(*c.member(root, "applies"), "\"applies\" is \"restart\" or \"live\"");
    }
    out.live = applies == "live";
    const JsonValue* config = c.member(root, "config");
    if (config == nullptr) {
        return c.fail(root, "\"config\" is missing (the mod's config file)");
    }
    if (!c.object(*config, "\"config\"") || !c.only(*config, {"file", "syntax", "expect"}, "\"config\"") ||
        !c.text(*config, "file", 260, true, out.config_file)) {
        return false;
    }
    if (!safe_relative_path(out.config_file)) {
        return c.fail(*c.member(*config, "file"),
                      "\"file\" must be a path inside the game folder, like nativePC/plugins/x.json (no drive, no ..)");
    }
    std::string syntax;
    if (!c.text(*config, "syntax", 8, false, syntax)) {
        return false;
    }
    if (!syntax_from_name(syntax.empty() ? out.config_file : syntax, out.syntax)) {
        return c.fail(*config, syntax.empty() ? "the config file's syntax is unknown: give \"syntax\": \"json\" or \"ini\""
                                              : "\"syntax\" is \"json\" or \"ini\"");
    }
    if (const JsonValue* expect = c.member(*config, "expect")) {
        if (expect->kind != JsonValue::Kind::Array) {
            return c.fail(*expect, "\"expect\" is a list of paths");
        }
        for (const JsonValue& item : expect->items) {
            Path p;
            if (item.kind != JsonValue::Kind::String || !parse_path(item.text, p)) {
                return c.fail(item, "\"expect\" holds paths like Section.Key or a.b[0].c");
            }
            out.expect.push_back(std::move(p));
        }
    }
    const JsonValue* pages = c.member(root, "pages");
    if (pages == nullptr || pages->kind != JsonValue::Kind::Array || pages->items.empty()) {
        return c.fail(pages != nullptr ? *pages : root, "\"pages\" is a list of at least one page");
    }
    for (const JsonValue& pv : pages->items) {
        PageSpec page;
        if (!c.object(pv, "a page") || !c.only(pv, {"title", "help", "settings"}, "a page") ||
            !c.text(pv, "title", kMaxTitle, true, page.title) || !c.text(pv, "help", kMaxHelp, false, page.help)) {
            return false;
        }
        const JsonValue* settings = c.member(pv, "settings");
        if (settings == nullptr || settings->kind != JsonValue::Kind::Array || settings->items.empty() ||
            settings->items.size() > kMaxSettings) {
            return c.fail(settings != nullptr ? *settings : pv, "\"settings\" is a list of 1 to 1024 settings");
        }
        bool any = false;
        for (const JsonValue& sv : settings->items) {
            SettingSpec s;
            if (!setting(c, sv, s)) {
                return false;
            }
            any = any || s.type != SettingSpec::Type::Section;
            page.settings.push_back(std::move(s));
        }
        if (!any) {
            return c.fail(pv, "a page needs a setting besides its sections");
        }
        out.pages.push_back(std::move(page));
    }
    // Ids are unique in the file.
    for (const PageSpec& p : out.pages) {
        for (const SettingSpec& s : p.settings) {
            if (!s.id.empty() && by_id(out, s.id) != &s) {
                why = "line " + std::to_string(s.line) + ": the id \"" + s.id + "\" is used twice";
                return false;
            }
        }
    }
    return check_conditions(out, why);
}

}  // namespace cmfm::config
