// The "Mods" entry in the Start menu's System tab (decisions.md item 8): the
// entry is shown below Options with our label and help line, and opens
// CMFM's page (mods_page.hpp); without it, the game's Options page with
// CMFM's rows (options_view.hpp). Exe 421810 only.
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
    const al_patches_t* patches;  // al.patch (API 1.8); may be NULL (no Start menu entry then)
};

// The entry's label in the Start menu and on the title screen.
inline constexpr char kModsLabel[] = "Mods";

// al_plugin_init: registers the two hooks. False (with `why`) if either
// cannot be registered; the entry is then never shown.
bool start_menu_register(const Env& env, std::string& why);
// WIN_MAIN: once both hooks are active, points the two tab lists (constant
// tables in the game's read-only data) at a copy of the System tab's table
// with our entry, through Astera Loader's al.patch. False (with `why`) if
// anything differs from what is expected; nothing is changed then.
bool start_menu_activate(const Env& env, std::string& why);
// SHUTDOWN: puts the original System table back (al.patch would too, after
// every plugin's SHUTDOWN).
void start_menu_restore(const Env& env);

// The game's own text (message file global at `file_rva`, id), past CMFM's
// text hook; nullptr if the file is not loaded or the hook is not in place.
const char* game_text(std::uintptr_t file_rva, std::uint32_t id);

}  // namespace cmfm::native
