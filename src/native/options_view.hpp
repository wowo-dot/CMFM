// The CMFM view of the Options page (prototype 2, docs/research/native-menu.md):
// "Mods" opens uGUIMenuOption in mode 0 with CMFM's own tab tables. Hooks on
// the option getter and setter answer CMFM's rows (kind 7) and pass every
// other row on; a hook on the page's close puts the game's tables back.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

// al_plugin_init: builds the test content and registers the getter, setter
// and close hooks.
bool options_view_register(const Env& env, std::string& why);
// WIN_MAIN: checks the hooks and the tab table pointers; false (with `why`)
// leaves the view off ("Mods" then opens the plain Options page).
bool options_view_check(const Env& env, std::string& why);
bool options_view_ready();

// Game thread, from the Start menu action hook.
bool options_view_enter();  // swaps in CMFM's tables; false if it could not
void options_view_exit();   // puts the game's tables back (no-op when off)

// From the text hook: CMFM's texts, or nullptr to let the game answer.
const char* options_view_text(void* message_file, std::uint32_t id);

// SHUTDOWN.
void options_view_restore(const Env& env);

}  // namespace cmfm::native
