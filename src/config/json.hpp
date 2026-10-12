// JSON with positions (docs/design/config-pages.md): CMFM's description files
// and the JSON configs it edits. Every value keeps where it stands in the
// text, so a value can be replaced without touching anything else. Comments
// (// and /* */) and trailing commas are accepted (descriptions are written
// by hand; a mod's own parser may accept them too). Pure logic, tested in
// tests/test_config.cpp.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cmfm::config {

struct JsonValue {
    enum class Kind { Null, Bool, Number, String, Array, Object };
    Kind kind = Kind::Null;
    std::size_t begin = 0;  // the value in the text: [begin, end), quotes and brackets included
    std::size_t end = 0;
    bool boolean = false;
    double number = 0;
    std::string text;       // String: decoded; Number, Bool, Null: the token as written
    std::vector<std::pair<std::string, JsonValue>> members;  // Object, in order (keys decoded)
    std::vector<JsonValue> items;                            // Array
};

struct TextError {
    std::size_t line = 0;    // 1-based
    std::size_t column = 0;  // 1-based, in bytes
    std::string what;
};

// The whole text as one value (a UTF-8 byte-order mark is skipped); false
// with where and what went wrong.
bool parse_json(std::string_view text, JsonValue& out, TextError& error);

// Line and column of byte `offset`.
TextError position_of(std::string_view text, std::size_t offset, std::string what);

// A path into a document: object keys and array indices, e.g. "a.b[0].c".
struct PathPart {
    std::string key;
    bool is_index = false;
    std::size_t index = 0;
};
using Path = std::vector<PathPart>;
// "a.b[0].c"; false for an empty path or a malformed index.
bool parse_path(std::string_view text, Path& out);
std::string path_text(const Path& path);

// The value at `path`, or nullptr with the reason (missing, a key that
// appears twice on the way, not an object or array).
const JsonValue* find(const JsonValue& root, const Path& path, std::string& why);

// A string as a JSON string token, quotes included.
std::string json_quote(std::string_view s);

}  // namespace cmfm::config
