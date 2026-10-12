#include "config/json.hpp"

#include <charconv>
#include <cstdio>
#include <cstdint>
#include <tuple>

namespace cmfm::config {

namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view text) : t_(text) {}

    bool run(JsonValue& out, TextError& error) {
        if (t_.substr(0, 3) == "\xEF\xBB\xBF") {
            p_ = 3;
        }
        if (!value(out, 0) || !skip() ) {
            error = position_of(t_, p_, why_);
            return false;
        }
        if (p_ != t_.size()) {
            error = position_of(t_, p_, "text after the end of the document");
            return false;
        }
        return true;
    }

private:
    bool fail(const char* what) {
        why_ = what;
        return false;
    }

    // Whitespace and comments; false on an unterminated comment.
    bool skip() {
        while (p_ < t_.size()) {
            const char c = t_[p_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++p_;
            } else if (c == '/' && p_ + 1 < t_.size() && t_[p_ + 1] == '/') {
                while (p_ < t_.size() && t_[p_] != '\n') {
                    ++p_;
                }
            } else if (c == '/' && p_ + 1 < t_.size() && t_[p_ + 1] == '*') {
                const std::size_t close = t_.find("*/", p_ + 2);
                if (close == std::string_view::npos) {
                    return fail("a /* comment is not closed");
                }
                p_ = close + 2;
            } else {
                break;
            }
        }
        return true;
    }

    bool value(JsonValue& v, int depth) {
        if (depth > kMaxDepth) {
            return fail("nested too deeply");
        }
        if (!skip()) {
            return false;
        }
        if (p_ >= t_.size()) {
            return fail("a value is missing");
        }
        v.begin = p_;
        const char c = t_[p_];
        bool ok = false;
        if (c == '{') {
            ok = object(v, depth);
        } else if (c == '[') {
            ok = array(v, depth);
        } else if (c == '"') {
            v.kind = JsonValue::Kind::String;
            ok = string(v.text);
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            ok = number(v);
        } else {
            ok = literal(v);
        }
        v.end = p_;
        return ok;
    }

    bool literal(JsonValue& v) {
        for (const auto& [word, kind, b] : {std::tuple<std::string_view, JsonValue::Kind, bool>{"true", JsonValue::Kind::Bool, true},
                                             {"false", JsonValue::Kind::Bool, false},
                                             {"null", JsonValue::Kind::Null, false}}) {
            if (t_.substr(p_, word.size()) == word) {
                v.kind = kind;
                v.boolean = b;
                v.text = std::string(word);
                p_ += word.size();
                return true;
            }
        }
        return fail("not a JSON value");
    }

    bool number(JsonValue& v) {
        const std::size_t start = p_;
        if (t_[p_] == '-') {
            ++p_;
        }
        const auto digits = [&] {
            const std::size_t from = p_;
            while (p_ < t_.size() && t_[p_] >= '0' && t_[p_] <= '9') {
                ++p_;
            }
            return p_ > from;
        };
        if (!digits()) {
            return fail("a number without digits");
        }
        if (p_ < t_.size() && t_[p_] == '.') {
            ++p_;
            if (!digits()) {
                return fail("a number without digits after the point");
            }
        }
        if (p_ < t_.size() && (t_[p_] == 'e' || t_[p_] == 'E')) {
            ++p_;
            if (p_ < t_.size() && (t_[p_] == '+' || t_[p_] == '-')) {
                ++p_;
            }
            if (!digits()) {
                return fail("a number without digits in its exponent");
            }
        }
        v.kind = JsonValue::Kind::Number;
        v.text = std::string(t_.substr(start, p_ - start));
        const auto [rest, ec] = std::from_chars(v.text.data(), v.text.data() + v.text.size(), v.number);
        if (ec != std::errc() || rest != v.text.data() + v.text.size()) {
            return fail("a number out of range");
        }
        return true;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
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

    bool hex4(std::uint32_t& out) {
        out = 0;
        for (int i = 0; i < 4; ++i, ++p_) {
            if (p_ >= t_.size()) {
                return fail("a \\u escape is cut short");
            }
            const char c = t_[p_];
            const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) {
                return fail("a \\u escape with a character that is not hex");
            }
            out = out * 16 + static_cast<std::uint32_t>(d);
        }
        return true;
    }

    bool string(std::string& out) {
        ++p_;  // the opening quote
        out.clear();
        while (p_ < t_.size()) {
            const char c = t_[p_];
            if (c == '"') {
                ++p_;
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("a string with a raw control character (a line break?)");
            }
            if (c != '\\') {
                out += c;
                ++p_;
                continue;
            }
            if (++p_ >= t_.size()) {
                break;
            }
            const char e = t_[p_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) {
                        return false;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF && t_.substr(p_, 2) == "\\u") {
                        p_ += 2;
                        std::uint32_t low = 0;
                        if (!hex4(low)) {
                            return false;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default:
                    return fail("an unknown escape in a string");
            }
        }
        return fail("a string is not closed");
    }

    bool object(JsonValue& v, int depth) {
        v.kind = JsonValue::Kind::Object;
        ++p_;
        for (;;) {
            if (!skip()) {
                return false;
            }
            if (p_ < t_.size() && t_[p_] == '}') {
                ++p_;
                return true;
            }
            if (p_ >= t_.size() || t_[p_] != '"') {
                return fail("a key (in quotes) or } expected");
            }
            std::string key;
            if (!string(key) || !skip()) {
                return false;
            }
            if (p_ >= t_.size() || t_[p_] != ':') {
                return fail("a : after the key expected");
            }
            ++p_;
            JsonValue member;
            if (!value(member, depth + 1) || !skip()) {
                return false;
            }
            v.members.emplace_back(std::move(key), std::move(member));
            if (p_ < t_.size() && t_[p_] == ',') {
                ++p_;
            } else if (p_ < t_.size() && t_[p_] == '}') {
                ++p_;
                return true;
            } else {
                return fail("a , or } expected");
            }
        }
    }

    bool array(JsonValue& v, int depth) {
        v.kind = JsonValue::Kind::Array;
        ++p_;
        for (;;) {
            if (!skip()) {
                return false;
            }
            if (p_ < t_.size() && t_[p_] == ']') {
                ++p_;
                return true;
            }
            JsonValue item;
            if (!value(item, depth + 1) || !skip()) {
                return false;
            }
            v.items.push_back(std::move(item));
            if (p_ < t_.size() && t_[p_] == ',') {
                ++p_;
            } else if (p_ < t_.size() && t_[p_] == ']') {
                ++p_;
                return true;
            } else {
                return fail("a , or ] expected");
            }
        }
    }

    std::string_view t_;
    std::size_t p_ = 0;
    std::string why_;
};

}  // namespace

bool parse_json(std::string_view text, JsonValue& out, TextError& error) {
    out = JsonValue{};
    return Parser(text).run(out, error);
}

TextError position_of(std::string_view text, std::size_t offset, std::string what) {
    TextError e;
    e.line = 1;
    e.column = 1;
    for (std::size_t i = 0; i < offset && i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++e.line;
            e.column = 1;
        } else {
            ++e.column;
        }
    }
    e.what = std::move(what);
    return e;
}

bool parse_path(std::string_view text, Path& out) {
    out.clear();
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '[') {
            const std::size_t close = text.find(']', i);
            if (close == std::string_view::npos || close == i + 1) {
                return false;
            }
            PathPart part;
            part.is_index = true;
            const auto [rest, ec] = std::from_chars(text.data() + i + 1, text.data() + close, part.index);
            if (ec != std::errc() || rest != text.data() + close) {
                return false;
            }
            out.push_back(part);
            i = close + 1;
            if (i < text.size() && text[i] == '.') {
                ++i;
            }
            continue;
        }
        std::size_t next = i;
        while (next < text.size() && text[next] != '.' && text[next] != '[') {
            ++next;
        }
        if (next == i) {
            return false;
        }
        out.push_back(PathPart{std::string(text.substr(i, next - i)), false, 0});
        i = next < text.size() && text[next] == '.' ? next + 1 : next;
        if (next < text.size() && text[next] == '.' && i == text.size()) {
            return false;  // a trailing dot
        }
    }
    return !out.empty();
}

std::string path_text(const Path& path) {
    std::string s;
    for (const PathPart& p : path) {
        if (p.is_index) {
            s += "[" + std::to_string(p.index) + "]";
        } else {
            s += (s.empty() ? "" : ".") + p.key;
        }
    }
    return s;
}

const JsonValue* find(const JsonValue& root, const Path& path, std::string& why) {
    const JsonValue* v = &root;
    std::string at;
    for (const PathPart& part : path) {
        if (part.is_index) {
            at += "[" + std::to_string(part.index) + "]";
            if (v->kind != JsonValue::Kind::Array) {
                why = at + " is not in a list";
                return nullptr;
            }
            if (part.index >= v->items.size()) {
                why = at + " is not in the file";
                return nullptr;
            }
            v = &v->items[part.index];
            continue;
        }
        at += (at.empty() ? "" : ".") + part.key;
        if (v->kind != JsonValue::Kind::Object) {
            why = at + ": its parent is not an object";
            return nullptr;
        }
        const JsonValue* next = nullptr;
        for (const auto& [key, member] : v->members) {
            if (key != part.key) {
                continue;
            }
            if (next != nullptr) {
                why = at + " appears twice";
                return nullptr;
            }
            next = &member;
        }
        if (next == nullptr) {
            why = at + " is not in the file";
            return nullptr;
        }
        v = next;
    }
    return v;
}

std::string json_quote(std::string_view s) {
    std::string out = "\"";
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += b;
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

}  // namespace cmfm::config
