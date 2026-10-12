// One plain value in a config file, with its place (config/document.hpp).
#pragma once

#include <cstddef>
#include <string>

namespace cmfm::config {

struct Scalar {
    // Other: a value CMFM reads but never edits (a TOML date or time).
    enum class Kind { Text, Number, Bool, Null, Other };
    Kind kind = Kind::Text;  // JSON and TOML: from the token; INI: always Text
    std::size_t begin = 0;   // the token in the text: [begin, end), quotes included
    std::size_t end = 0;
    std::string value;       // the value's text, without quotes or escapes
    char quote = 0;          // '"' or '\'' for a quoted token, 0 for a bare one
    bool multiline = false;  // a TOML multi-line string (read, never edited)
};

}  // namespace cmfm::config
