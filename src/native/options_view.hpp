// The CMFM view of the Options page (docs/research/native-menu.md): "Mods"
// opens uGUIMenuOption in mode 0 with the tab tables of CMFM's menu
// (menu/service.hpp). Hooks on the option getter and setter answer CMFM's
// rows (kind 1, ids 100-299) on every Options page and pass every other row
// on; a hook on the page's close puts the game's tables back.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

// al_plugin_init: registers the getter, setter, close and Set To Default
// hooks.
bool options_view_register(const Env& env, std::string& why);
// WIN_MAIN: checks the hooks, the tab table pointers and that no game row
// uses CMFM's ids; false (with `why`) leaves the view off ("Mods" then opens
// the plain Options page).
bool options_view_check(const Env& env, std::string& why);
bool options_view_ready();

// Game thread, from the Start menu action hook.
bool options_view_enter();  // swaps in CMFM's tables; false if it could not
void options_view_exit();   // puts the game's tables back (no-op when off)

// From the text hook: CMFM's texts, or nullptr to let the game answer.
const char* options_view_text(void* message_file, std::uint32_t id);

// CMFM's own page: its number rows become sliders while it shows the menu;
// the getter and setter hooks then answer them on the slider's 0-100 scale.
void options_view_sliders(bool on);

// SHUTDOWN.
void options_view_restore(const Env& env);

}  // namespace cmfm::native
