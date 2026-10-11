// The "Mods" entry in the Start menu's System tab (decisions.md item 8),
// prototype 1: the entry is shown below Options with our label and help
// line, and selecting it runs the game's own Options action until the CMFM
// page exists. Exe 421810 only.
#pragma once

#include "al/al_plugin.h"

#include <cstdint>
#include <string>

namespace cmfm::native {

struct Env {
    const al_api_t* api;
    al_plugin_t* self;
    const al_hooks_t* hooks;
    std::uintptr_t base;  // the game exe's base
    const al_scanner_t* scanner;  // may be NULL
    const al_input_t* input;      // may be NULL (no mouse wheel then)
};

// The entry's label in the Start menu and on the title screen.
inline constexpr char kModsLabel[] = "Mods";

// al_plugin_init: registers the two hooks. False (with `why`) if either
// cannot be registered; the entry is then never shown.
bool start_menu_register(const Env& env, std::string& why);
// WIN_MAIN: once both hooks are active, swaps in the System table with our
// entry. False (with `why`) if anything differs from what is expected;
// nothing is changed then.
bool start_menu_activate(const Env& env, std::string& why);
// SHUTDOWN: puts the original System table back.
void start_menu_restore(const Env& env);

// The game's own text (message file global at `file_rva`, id), past CMFM's
// text hook; nullptr if the file is not loaded or the hook is not in place.
const char* game_text(std::uintptr_t file_rva, std::uint32_t id);

}  // namespace cmfm::native
