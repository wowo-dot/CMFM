// CMFM's description files (docs/design/config-pages.md): a JSON file in
// CMFM's pages folder that maps menu rows to plain values in a mod's own
// config file. parse_description reads and checks one; every mistake is
// reported with its line. Pure logic, tested in tests/test_config.cpp.
#pragma once

#include "config/document.hpp"
#include "config/json.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cmfm::config {

struct SettingSpec {
    enum class Type { Section, Bool, Int, Float, Choice, Key, Text };
    Type type = Type::Section;
    std::string id;     // optional; names the setting in conditions
    std::string label;  // the section's heading for a section
    std::string help;
    Path path;
    std::string path_text;
    // Choice: stored value -> label, in order.
    std::vector<std::pair<std::string, std::string>> choices;
    // Int and Float.
    double min = 0;
    double max = 0;
    double step = 1;
    std::uint32_t decimals = 0;
    std::string unit;
    // Key: how the file writes a key: "vk" (a Windows key code, decimal or
    // 0x hex), "vk-hex" (the same in hex digits without 0x, as Fexty's Mod
    // writes them: "62" is Numpad 2), "dik" (a DirectInput scan code),
    // "names" ("Ctrl+H").
    std::string keys;
    // "restart" / "live" for this setting; empty: the file's `applies`.
    std::string applies;
    // What "Reset to defaults" writes, in the setting's terms (bool "true" or
    // "false", a number, a choice's stored value, a key's name).
    bool has_default = false;
    std::string default_value;
    // Conditions: the id of another setting and the value it must have.
    std::vector<std::pair<std::string, JsonValue>> hide_unless;
    std::vector<std::pair<std::string, JsonValue>> lock_unless;
    std::size_t line = 0;
};

struct PageSpec {
    std::string title;
    std::string help;
    std::vector<SettingSpec> settings;
};

struct Description {
    std::string file;  // the description's own file name (messages)
    std::string mod;
    std::string version;
    std::string made_by;
    bool author = false;      // source: "author" (else "community")
    std::string config_file;  // relative to the game folder, with '/' or '\'
    Syntax syntax = Syntax::Json;
    std::vector<Path> expect;
    bool live = false;  // applies: "live" (else "restart")
    std::vector<PageSpec> pages;
};

// Reads and checks a description; false with "line N: what" in `why`.
bool parse_description(std::string_view text, std::string_view file_name, Description& out, std::string& why);

// A config path a description may name: relative, inside the game folder
// (no drive, no leading slash, no "..").
bool safe_relative_path(std::string_view path);

}  // namespace cmfm::config
