// A mod's config file as CMFM edits it (docs/design/config-pages.md, "Editing
// a config file"): plain values found by path, each with its place and its
// token's form, and surgical edits (only the value's characters change, in
// the old token's style), verified by parsing the result again. JSON and INI
// now, TOML later. Pure logic, tested in tests/test_config.cpp.
#pragma once

#include "config/json.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cmfm::config {

enum class Syntax { Json, Ini };

// "json" / "ini", or from a file name's extension; false if unknown.
bool syntax_from_name(std::string_view name, Syntax& out);

// One plain value in a config file.
struct Scalar {
    enum class Kind { Text, Number, Bool, Null };
    Kind kind = Kind::Text;  // JSON: from the token; INI: always Text
    std::size_t begin = 0;   // the token in the text: [begin, end), quotes included
    std::size_t end = 0;
    std::string value;       // the value's text, without quotes or escapes
    char quote = 0;          // '"' or '\'' for a quoted token, 0 for a bare one
};

class Document {
public:
    // Parses `text`; false with where and what (the document is empty then).
    bool parse(Syntax syntax, std::string text, TextError& error);

    Syntax syntax() const { return syntax_; }
    const std::string& text() const { return text_; }

    // The plain value at `path` (JSON: keys and indices; INI: [section, key]
    // or [key] for keys before any section, matched without regard to case).
    // False with the reason: not in the file, appears twice, not a plain
    // value.
    bool lookup(const Path& path, Scalar& out, std::string& why) const;

    // Every plain value: its path and its kind and text (verification).
    std::vector<std::pair<std::string, std::string>> values() const;

private:
    struct IniEntry {
        std::string section;
        std::string key;
        Scalar scalar;
    };

    Syntax syntax_ = Syntax::Json;
    std::string text_;
    JsonValue json_;
    std::vector<IniEntry> ini_;
};

// A new value for the value at `path`, in its meaning ("false", "64", "2x"):
// the token keeps the old token's form (its quotes; a JSON number or literal
// stays one).
struct Edit {
    Path path;
    std::string value;
};

// `text` with the edits applied surgically, then verified: the result
// parses, the edited values read as their new values, every other value is
// unchanged. False with the reason (nothing applied then).
bool apply_edits(Syntax syntax, const std::string& text, const std::vector<Edit>& edits, std::string& out,
                 std::string& why);

}  // namespace cmfm::config
