// A mod's config file as CMFM edits it (docs/design/config-pages.md, "Editing
// a config file"): plain values found by path, each with its place and its
// token's form, and surgical edits (only the value's characters change, in
// the old token's style), verified by parsing the result again. JSON, INI and
// TOML. Pure logic, tested in tests/test_config.cpp.
#pragma once

#include "config/json.hpp"
#include "config/scalar.hpp"
#include "config/toml.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cmfm::config {

enum class Syntax { Json, Ini, Toml };

// "json" / "ini" / "toml", or from a file name's extension; false if unknown.
bool syntax_from_name(std::string_view name, Syntax& out);

class Document {
public:
    // Parses `text`; false with where and what (the document is empty then).
    bool parse(Syntax syntax, std::string text, TextError& error);

    Syntax syntax() const { return syntax_; }
    const std::string& text() const { return text_; }

    // The plain value at `path` (JSON and TOML: keys and indices, an array
    // of tables like a list; INI: [section, key] or [key] for keys before any
    // section, matched without regard to case).
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
    std::vector<TomlEntry> toml_;
};

// A new value for the value at `path`, in its meaning ("false", "64", "2x"):
// the token keeps the old token's form (its quotes; a JSON or TOML number or
// literal stays one, a TOML float stays a float).
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
