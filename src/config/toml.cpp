#include "config/toml.hpp"

#include <cstdint>
#include <cstdio>
#include <map>

namespace cmfm::config {

namespace {

constexpr int kMaxDepth = 32;

bool is_digit(char c) { return c >= '0' && c <= '9'; }
bool is_bare(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || is_digit(c) || c == '_' || c == '-';
}

// Digits of `base`, single underscores only between digits.
bool digits(std::string_view s, int base) {
    if (s.empty() || s.front() == '_' || s.back() == '_') {
        return false;
    }
    char last = 0;
    for (const char c : s) {
        if (c == '_') {
            if (last == '_') {
                return false;
            }
        } else {
            const bool ok = base == 16 ? (is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
                            : base == 8 ? (c >= '0' && c <= '7')
                            : base == 2 ? (c == '0' || c == '1')
                                        : is_digit(c);
            if (!ok) {
                return false;
            }
        }
        last = c;
    }
    return true;
}

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

class Parser {
public:
    Parser(std::string_view text, std::vector<TomlEntry>& out) : t_(text), out_(out) {}

    bool run(TextError& error) {
        if (t_.substr(0, 3) == "\xEF\xBB\xBF") {
            p_ = 3;
        }
        while (statement()) {
        }
        if (!why_.empty()) {
            error = position_of(t_, p_, why_);
            return false;
        }
        return true;
    }

private:
    bool fail(const char* what) {
        why_ = what;
        return false;
    }

    char at(std::size_t i = 0) const { return p_ + i < t_.size() ? t_[p_ + i] : '\0'; }
    bool ahead(std::string_view s) const { return t_.substr(p_, s.size()) == s; }
    void ws() {
        while (at() == ' ' || at() == '\t') {
            ++p_;
        }
    }

    // After a statement: an optional comment, then a line end or the end.
    bool end_of_line() {
        ws();
        if (at() == '#') {
            while (p_ < t_.size() && at() != '\n') {
                ++p_;
            }
        }
        if (p_ >= t_.size()) {
            return true;
        }
        if (at() == '\r' && at(1) == '\n') {
            p_ += 2;
            return true;
        }
        if (at() == '\n') {
            ++p_;
            return true;
        }
        return fail("something after the value on the same line");
    }

    // Whitespace, line ends and comments (inside arrays).
    void blank_lines() {
        for (;;) {
            ws();
            if (at() == '#') {
                while (p_ < t_.size() && at() != '\n') {
                    ++p_;
                }
            } else if (at() == '\n') {
                ++p_;
            } else if (at() == '\r' && at(1) == '\n') {
                p_ += 2;
            } else {
                return;
            }
        }
    }

    // One line: a table header, a key = value pair, a comment or a blank
    // line; false at the end of the text (or on an error, why_ set).
    bool statement() {
        ws();
        if (p_ >= t_.size()) {
            return false;
        }
        if (at() == '\n' || (at() == '\r' && at(1) == '\n') || at() == '#') {
            return end_of_line();
        }
        if (at() == '[') {
            const bool array = at(1) == '[';
            p_ += array ? 2 : 1;
            ws();
            Path raw;
            if (!key(raw)) {
                return false;
            }
            ws();
            if (!ahead(array ? "]]" : "]")) {
                return fail(array ? "a [[table]] header without its ]]" : "a [table] header without its ]");
            }
            p_ += array ? 2 : 1;
            if (array) {
                Path parent(raw.begin(), raw.end() - 1);
                Path path = resolve(parent);
                path.push_back(raw.back());
                const std::size_t index = arrays_[toml_path_text(path)]++;
                path.push_back(PathPart{std::string(), true, index});
                table_ = std::move(path);
            } else {
                table_ = resolve(raw);
            }
            return end_of_line();
        }
        Path k;
        if (!key(k)) {
            return false;
        }
        ws();
        if (at() != '=') {
            return fail("a = after the key expected");
        }
        ++p_;
        ws();
        Path full = table_;
        full.insert(full.end(), k.begin(), k.end());
        return value(full, 0) && end_of_line();
    }

    // Prefixes that name an array of tables refer to its last element.
    Path resolve(const Path& raw) const {
        Path out;
        for (const PathPart& part : raw) {
            out.push_back(part);
            const auto a = arrays_.find(toml_path_text(out));
            if (a != arrays_.end() && a->second > 0) {
                out.push_back(PathPart{std::string(), true, a->second - 1});
            }
        }
        return out;
    }

    bool key(Path& out) {
        out.clear();
        for (;;) {
            std::string part;
            if (at() == '"') {
                if (!basic(part)) {
                    return false;
                }
            } else if (at() == '\'') {
                if (!literal(part)) {
                    return false;
                }
            } else {
                const std::size_t from = p_;
                while (is_bare(at())) {
                    ++p_;
                }
                if (p_ == from) {
                    return fail("a key expected");
                }
                part = std::string(t_.substr(from, p_ - from));
            }
            out.push_back(PathPart{std::move(part), false, 0});
            ws();
            if (at() != '.') {
                return true;
            }
            ++p_;
            ws();
        }
    }

    bool hex(int count, std::uint32_t& cp) {
        cp = 0;
        for (int i = 0; i < count; ++i, ++p_) {
            const char c = at();
            const int d = is_digit(c) ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) {
                return fail("a \\u escape with a character that is not hex");
            }
            cp = cp * 16 + static_cast<std::uint32_t>(d);
        }
        return true;
    }

    bool escape(std::string& out) {
        const char e = at();
        ++p_;
        switch (e) {
            case 'b': out += '\b'; return true;
            case 't': out += '\t'; return true;
            case 'n': out += '\n'; return true;
            case 'f': out += '\f'; return true;
            case 'r': out += '\r'; return true;
            case '"': out += '"'; return true;
            case '\\': out += '\\'; return true;
            case 'u':
            case 'U': {
                std::uint32_t cp = 0;
                if (!hex(e == 'u' ? 4 : 8, cp)) {
                    return false;
                }
                append_utf8(out, cp);
                return true;
            }
            default: return fail("an unknown escape in a string");
        }
    }

    bool basic(std::string& out) {
        ++p_;
        out.clear();
        while (p_ < t_.size()) {
            const char c = at();
            if (c == '"') {
                ++p_;
                return true;
            }
            if (c == '\n' || c == '\r') {
                return fail("a string is not closed on its line");
            }
            if (c == '\\') {
                ++p_;
                if (!escape(out)) {
                    return false;
                }
                continue;
            }
            out += c;
            ++p_;
        }
        return fail("a string is not closed");
    }

    bool literal(std::string& out) {
        ++p_;
        const std::size_t from = p_;
        while (p_ < t_.size() && at() != '\'') {
            if (at() == '\n' || at() == '\r') {
                return fail("a literal string is not closed on its line");
            }
            ++p_;
        }
        if (p_ >= t_.size()) {
            return fail("a literal string is not closed");
        }
        out = std::string(t_.substr(from, p_ - from));
        ++p_;
        return true;
    }

    // """...""" and '''...''': a line end right after the opening is not part
    // of the text; up to two more quotes may end it.
    bool multiline(char q, std::string& out) {
        p_ += 3;
        if (at() == '\n') {
            ++p_;
        } else if (at() == '\r' && at(1) == '\n') {
            p_ += 2;
        }
        out.clear();
        const std::string close(3, q);
        while (p_ < t_.size()) {
            if (ahead(close)) {
                p_ += 3;
                for (int extra = 0; extra < 2 && at() == q; ++extra) {
                    out += q;
                    ++p_;
                }
                return true;
            }
            if (q == '"' && at() == '\\') {
                ++p_;
                std::size_t look = p_;
                while (look < t_.size() && (t_[look] == ' ' || t_[look] == '\t')) {
                    ++look;
                }
                if (look < t_.size() && (t_[look] == '\n' || t_[look] == '\r')) {
                    p_ = look;  // a line-ending backslash: the break and the indent go
                    while (at() == ' ' || at() == '\t' || at() == '\n' || at() == '\r') {
                        ++p_;
                    }
                    continue;
                }
                if (!escape(out)) {
                    return false;
                }
                continue;
            }
            out += at();
            ++p_;
        }
        return fail("a multi-line string is not closed");
    }

    bool value(const Path& path, int depth) {
        if (depth > kMaxDepth) {
            return fail("nested too deeply");
        }
        Scalar s;
        s.begin = p_;
        const char c = at();
        if (ahead("\"\"\"") || ahead("'''")) {
            s.quote = c;
            s.multiline = true;
            if (!multiline(c, s.value)) {
                return false;
            }
        } else if (c == '"') {
            s.quote = '"';
            if (!basic(s.value)) {
                return false;
            }
        } else if (c == '\'') {
            s.quote = '\'';
            if (!literal(s.value)) {
                return false;
            }
        } else if (c == '[') {
            ++p_;
            for (std::size_t i = 0;; ++i) {
                blank_lines();
                if (at() == ']') {
                    ++p_;
                    return true;
                }
                Path item = path;
                item.push_back(PathPart{std::string(), true, i});
                if (!value(item, depth + 1)) {
                    return false;
                }
                blank_lines();
                if (at() == ',') {
                    ++p_;
                } else if (at() == ']') {
                    ++p_;
                    return true;
                } else {
                    return fail("a , or ] expected in an array");
                }
            }
        } else if (c == '{') {
            ++p_;
            ws();
            if (at() == '}') {
                ++p_;
                return true;
            }
            for (;;) {
                Path k;
                if (!key(k)) {
                    return false;
                }
                ws();
                if (at() != '=') {
                    return fail("a = after the key expected");
                }
                ++p_;
                ws();
                Path member = path;
                member.insert(member.end(), k.begin(), k.end());
                if (!value(member, depth + 1)) {
                    return false;
                }
                ws();
                if (at() == ',') {
                    ++p_;
                    ws();
                } else if (at() == '}') {
                    ++p_;
                    return true;
                } else {
                    return fail("a , or } expected in an inline table");
                }
            }
        } else {
            const std::size_t from = p_;
            while (p_ < t_.size()) {
                const char d = at();
                if (d == ' ' || d == '\t' || d == ',' || d == ']' || d == '}' || d == '#' || d == '\n' || d == '\r') {
                    break;
                }
                ++p_;
            }
            std::string token(t_.substr(from, p_ - from));
            // A date and a time with a space between them ("1979-05-27 07:32:00").
            if (token.size() == 10 && token[4] == '-' && token[7] == '-' && at() == ' ' && is_digit(at(1)) &&
                is_digit(at(2)) && at(3) == ':') {
                ++p_;
                while (p_ < t_.size() && !(at() == ' ' || at() == '\t' || at() == ',' || at() == ']' || at() == '}' ||
                                           at() == '#' || at() == '\n' || at() == '\r')) {
                    ++p_;
                }
                token = std::string(t_.substr(from, p_ - from));
            }
            if (token == "true" || token == "false") {
                s.kind = Scalar::Kind::Bool;
            } else if (toml_number(token)) {
                s.kind = Scalar::Kind::Number;
            } else if (!token.empty() && is_digit(token[0]) &&
                       token.find_first_of("-:") != std::string::npos) {
                s.kind = Scalar::Kind::Other;  // a date or a time
            } else {
                return fail(token.empty() ? "a value is missing" : "not a TOML value");
            }
            s.value = std::move(token);
        }
        s.end = p_;
        out_.push_back({path, std::move(s)});
        return true;
    }

    std::string_view t_;
    std::size_t p_ = 0;
    std::vector<TomlEntry>& out_;
    std::string why_;
    Path table_;
    std::map<std::string, std::size_t> arrays_;  // array of tables (its resolved path) -> elements
};

}  // namespace

bool parse_toml(std::string_view text, std::vector<TomlEntry>& out, TextError& error) {
    out.clear();
    Parser parser(text, out);
    if (!parser.run(error)) {
        out.clear();
        return false;
    }
    return true;
}

bool toml_number(std::string_view s) {
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'o' || s[1] == 'b')) {
        return digits(s.substr(2), s[1] == 'x' ? 16 : s[1] == 'o' ? 8 : 2);
    }
    if (!s.empty() && (s[0] == '+' || s[0] == '-')) {
        s.remove_prefix(1);
    }
    if (s == "inf" || s == "nan") {
        return true;
    }
    const std::size_t e = s.find_first_of("eE");
    const std::string_view mantissa = s.substr(0, e);
    const std::size_t dot = mantissa.find('.');
    const std::string_view whole = mantissa.substr(0, dot);
    if (!digits(whole, 10) || (whole.size() > 1 && whole[0] == '0')) {
        return false;
    }
    if (dot != std::string_view::npos && !digits(mantissa.substr(dot + 1), 10)) {
        return false;
    }
    if (e != std::string_view::npos) {
        std::string_view exp = s.substr(e + 1);
        if (!exp.empty() && (exp[0] == '+' || exp[0] == '-')) {
            exp.remove_prefix(1);
        }
        return digits(exp, 10);
    }
    return true;
}

std::string toml_quote(std::string_view s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04X", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += b;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

std::string toml_path_text(const Path& path) {
    std::string s;
    for (const PathPart& p : path) {
        if (p.is_index) {
            s += "[" + std::to_string(p.index) + "]";
            continue;
        }
        bool bare = !p.key.empty();
        for (const char c : p.key) {
            bare = bare && is_bare(c);
        }
        s += (s.empty() ? "" : ".") + (bare ? p.key : toml_quote(p.key));
    }
    return s;
}

}  // namespace cmfm::config
