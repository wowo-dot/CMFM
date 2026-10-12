#include "config/document.hpp"

#include <algorithm>
#include <cctype>

namespace cmfm::config {

namespace {

bool same_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool blank(char c) { return c == ' ' || c == '\t'; }

std::string kind_text(const Scalar& s) {
    const char* k = s.kind == Scalar::Kind::Number ? "n:" : s.kind == Scalar::Kind::Bool ? "b:"
                    : s.kind == Scalar::Kind::Null ? "z:" : "s:";
    return k + s.value;
}

bool json_scalar(const JsonValue& v, Scalar& out) {
    out = Scalar{};
    out.begin = v.begin;
    out.end = v.end;
    out.value = v.text;
    switch (v.kind) {
        case JsonValue::Kind::String: out.kind = Scalar::Kind::Text; out.quote = '"'; return true;
        case JsonValue::Kind::Number: out.kind = Scalar::Kind::Number; return true;
        case JsonValue::Kind::Bool: out.kind = Scalar::Kind::Bool; return true;
        case JsonValue::Kind::Null: out.kind = Scalar::Kind::Null; return true;
        default: return false;
    }
}

void json_values(const JsonValue& v, const std::string& at, std::vector<std::pair<std::string, std::string>>& out) {
    if (v.kind == JsonValue::Kind::Object) {
        for (const auto& [key, member] : v.members) {
            json_values(member, at.empty() ? key : at + "." + key, out);
        }
    } else if (v.kind == JsonValue::Kind::Array) {
        for (std::size_t i = 0; i < v.items.size(); ++i) {
            json_values(v.items[i], at + "[" + std::to_string(i) + "]", out);
        }
    } else {
        Scalar s;
        json_scalar(v, s);
        out.emplace_back(at, kind_text(s));
    }
}

// A JSON number token: the whole of `s` must read as one.
bool json_number(const std::string& s) {
    JsonValue v;
    TextError e;
    return parse_json(s, v, e) && v.kind == JsonValue::Kind::Number;
}

}  // namespace

bool syntax_from_name(std::string_view name, Syntax& out) {
    std::string n(name);
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto ends = [&](std::string_view tail) {
        return n.size() >= tail.size() && n.compare(n.size() - tail.size(), tail.size(), tail) == 0;
    };
    if (n == "json" || ends(".json")) {
        out = Syntax::Json;
        return true;
    }
    if (n == "ini" || ends(".ini") || ends(".cfg")) {
        out = Syntax::Ini;
        return true;
    }
    return false;
}

bool Document::parse(Syntax syntax, std::string text, TextError& error) {
    syntax_ = syntax;
    text_ = std::move(text);
    json_ = JsonValue{};
    ini_.clear();
    if (syntax == Syntax::Json) {
        if (!parse_json(text_, json_, error)) {
            text_.clear();
            return false;
        }
        return true;
    }
    // INI: [section] lines, key = value lines (a value bare or in quotes; a
    // bare value ends before whitespace followed by ; or #), comment lines
    // starting with ; or #.
    std::string section;
    std::size_t line_start = text_.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
    while (line_start <= text_.size()) {
        std::size_t line_end = text_.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = text_.size();
        }
        std::size_t end = line_end;
        if (end > line_start && text_[end - 1] == '\r') {
            --end;
        }
        std::size_t p = line_start;
        while (p < end && blank(text_[p])) {
            ++p;
        }
        if (p < end && text_[p] != ';' && text_[p] != '#') {
            if (text_[p] == '[') {
                const std::size_t close = text_.find(']', p);
                if (close == std::string::npos || close >= end) {
                    error = position_of(text_, p, "a [section without its ]");
                    text_.clear();
                    return false;
                }
                std::size_t a = p + 1;
                std::size_t b = close;
                while (a < b && blank(text_[a])) {
                    ++a;
                }
                while (b > a && blank(text_[b - 1])) {
                    --b;
                }
                section = text_.substr(a, b - a);
            } else {
                const std::size_t eq = text_.find('=', p);
                if (eq == std::string::npos || eq >= end) {
                    error = position_of(text_, p, "a line that is not key = value, [section] or a comment");
                    text_.clear();
                    return false;
                }
                std::size_t kb = eq;
                while (kb > p && blank(text_[kb - 1])) {
                    --kb;
                }
                IniEntry entry;
                entry.section = section;
                entry.key = text_.substr(p, kb - p);
                std::size_t v = eq + 1;
                while (v < end && blank(text_[v])) {
                    ++v;
                }
                Scalar& s = entry.scalar;
                s.kind = Scalar::Kind::Text;
                if (v < end && (text_[v] == '"' || text_[v] == '\'')) {
                    const std::size_t close = text_.find(text_[v], v + 1);
                    if (close == std::string::npos || close >= end) {
                        error = position_of(text_, v, "a quoted value is not closed");
                        text_.clear();
                        return false;
                    }
                    s.quote = text_[v];
                    s.begin = v;
                    s.end = close + 1;
                    s.value = text_.substr(v + 1, close - v - 1);
                } else {
                    std::size_t ve = v;
                    std::size_t stop = end;
                    for (std::size_t i = v + 1; i < end; ++i) {
                        if ((text_[i] == ';' || text_[i] == '#') && blank(text_[i - 1])) {
                            stop = i;
                            break;
                        }
                    }
                    ve = stop;
                    while (ve > v && blank(text_[ve - 1])) {
                        --ve;
                    }
                    s.begin = v;
                    s.end = ve;
                    s.value = text_.substr(v, ve - v);
                }
                ini_.push_back(std::move(entry));
            }
        }
        if (line_end == text_.size()) {
            break;
        }
        line_start = line_end + 1;
    }
    return true;
}

bool Document::lookup(const Path& path, Scalar& out, std::string& why) const {
    if (syntax_ == Syntax::Json) {
        const JsonValue* v = find(json_, path, why);
        if (v == nullptr) {
            return false;
        }
        if (!json_scalar(*v, out)) {
            why = path_text(path) + " is a list or an object, not a plain value";
            return false;
        }
        return true;
    }
    if (path.empty() || path.size() > 2 || path[0].is_index || (path.size() == 2 && path[1].is_index)) {
        why = "an INI path is Section.Key (or Key before any section)";
        return false;
    }
    const std::string section = path.size() == 2 ? path[0].key : std::string();
    const std::string& key = path.back().key;
    const IniEntry* found = nullptr;
    for (const IniEntry& e : ini_) {
        if (same_ci(e.section, section) && same_ci(e.key, key)) {
            if (found != nullptr) {
                why = path_text(path) + " appears twice";
                return false;
            }
            found = &e;
        }
    }
    if (found == nullptr) {
        why = path_text(path) + " is not in the file";
        return false;
    }
    out = found->scalar;
    return true;
}

std::vector<std::pair<std::string, std::string>> Document::values() const {
    std::vector<std::pair<std::string, std::string>> out;
    if (syntax_ == Syntax::Json) {
        json_values(json_, "", out);
    } else {
        for (const IniEntry& e : ini_) {
            out.emplace_back(e.section + "." + e.key, kind_text(e.scalar));
        }
    }
    return out;
}

bool apply_edits(Syntax syntax, const std::string& text, const std::vector<Edit>& edits, std::string& out,
                 std::string& why) {
    Document before;
    TextError error;
    if (!before.parse(syntax, text, error)) {
        why = "the file does not parse (line " + std::to_string(error.line) + ": " + error.what + ")";
        return false;
    }
    struct Splice {
        std::size_t begin;
        std::size_t end;
        std::string token;
        std::string path;
        std::string expect;  // the value's kind and text after the edit
    };
    std::vector<Splice> splices;
    for (const Edit& edit : edits) {
        Scalar s;
        if (!before.lookup(edit.path, s, why)) {
            return false;
        }
        const std::string at = path_text(edit.path);
        const std::string& v = edit.value;
        Splice sp{s.begin, s.end, {}, at, {}};
        if (syntax == Syntax::Json) {
            switch (s.kind) {
                case Scalar::Kind::Text: sp.token = json_quote(v); break;
                case Scalar::Kind::Number:
                    if (!json_number(v)) {
                        why = at + ": \"" + v + "\" is not a number";
                        return false;
                    }
                    sp.token = v;
                    break;
                case Scalar::Kind::Bool:
                    if (v != "true" && v != "false") {
                        why = at + ": a JSON true or false, not \"" + v + "\"";
                        return false;
                    }
                    sp.token = v;
                    break;
                case Scalar::Kind::Null:
                    why = at + " is null in the file: its type is unknown";
                    return false;
            }
        } else {
            const bool bad = v.find_first_of("\r\n") != std::string::npos || (s.quote != 0 && v.find(s.quote) != std::string::npos) ||
                             (s.quote == 0 && (v.empty() || blank(v.front()) || blank(v.back())));
            bool comment = false;
            for (std::size_t i = 1; s.quote == 0 && i < v.size(); ++i) {
                comment = comment || ((v[i] == ';' || v[i] == '#') && blank(v[i - 1]));
            }
            if (bad || comment) {
                why = at + ": \"" + v + "\" cannot be written in this file's form";
                return false;
            }
            sp.token = s.quote != 0 ? std::string(1, s.quote) + v + s.quote : v;
        }
        Scalar after_kind = s;
        after_kind.value = v;
        sp.expect = kind_text(after_kind);
        for (const Splice& other : splices) {
            if (other.begin == sp.begin) {
                why = at + " is edited twice";
                return false;
            }
        }
        splices.push_back(std::move(sp));
    }
    std::sort(splices.begin(), splices.end(), [](const Splice& a, const Splice& b) { return a.begin > b.begin; });
    out = text;
    for (const Splice& sp : splices) {
        const std::size_t old_len = sp.end - sp.begin;
        out.replace(sp.begin, old_len, sp.token);
        // INI: an inline comment after the value keeps its column.
        if (syntax == Syntax::Ini && sp.token.size() != old_len) {
            std::size_t p = sp.begin + sp.token.size();
            std::size_t spaces = 0;
            while (p + spaces < out.size() && out[p + spaces] == ' ') {
                ++spaces;
            }
            const std::size_t q = p + spaces;
            if (spaces > 0 && q < out.size() && (out[q] == ';' || out[q] == '#')) {
                const long long want = static_cast<long long>(spaces) + static_cast<long long>(old_len) -
                                       static_cast<long long>(sp.token.size());
                const std::size_t keep = want < 1 ? 1 : static_cast<std::size_t>(want);
                out.replace(p, spaces, keep, ' ');
            }
        }
    }
    Document after;
    if (!after.parse(syntax, out, error)) {
        why = "the edited file would not parse (line " + std::to_string(error.line) + ": " + error.what + ")";
        return false;
    }
    const auto old_values = before.values();
    const auto new_values = after.values();
    if (old_values.size() != new_values.size()) {
        why = "the edited file would hold a different number of values";
        return false;
    }
    for (std::size_t i = 0; i < old_values.size(); ++i) {
        if (old_values[i].first != new_values[i].first) {
            why = "the edited file would hold its values differently";
            return false;
        }
        const Splice* edited = nullptr;
        for (const Splice& sp : splices) {
            // Compare by path text; INI paths come as section.key.
            if (sp.path == old_values[i].first ||
                (syntax == Syntax::Ini && same_ci(sp.path, old_values[i].first)) ||
                (syntax == Syntax::Ini && same_ci("." + sp.path, old_values[i].first))) {
                edited = &sp;
            }
        }
        const std::string& want = edited != nullptr ? edited->expect : old_values[i].second;
        if (new_values[i].second != want) {
            why = "the edit of " + (edited != nullptr ? edited->path : old_values[i].first) + " would not read back as written";
            return false;
        }
    }
    return true;
}

}  // namespace cmfm::config
