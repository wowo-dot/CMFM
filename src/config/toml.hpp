// TOML with positions (docs/design/config-pages.md): every plain value of a
// TOML config file with its path and its place in the text, so a value can be
// replaced without touching anything else. Tables, dotted keys, arrays of
// tables (paths like trigger[0].name), inline tables, arrays, comments; all
// string kinds, integers (decimal, 0x, 0o, 0b, underscores), floats,
// booleans, dates and times. Pure logic, tested in tests/test_config.cpp.
#pragma once

#include "config/json.hpp"
#include "config/scalar.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace cmfm::config {

struct TomlEntry {
    Path path;
    Scalar scalar;
};

// Every plain value in document order; false with where and what.
bool parse_toml(std::string_view text, std::vector<TomlEntry>& out, TextError& error);

// A TOML number token: the whole of `s` reads as an integer or a float.
bool toml_number(std::string_view s);

// A string as a TOML basic string token, quotes included.
std::string toml_quote(std::string_view s);

// A path as text with no two paths alike: keys that are not bare in quotes
// (a."b.c"[0].d).
std::string toml_path_text(const Path& path);

}  // namespace cmfm::config
