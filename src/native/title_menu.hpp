// The title screen's "Mods" entry (decisions.md item 8; docs/research/
// native-menu.md, "The title screen"):
// - entry: an 8th row in the title menu, right below Options. Hooks on the
//   menu's cursor and selected callbacks shift the rows below it by one, so
//   the game's own row numbers still hold; "Mods" acts as Options.
// - page: CMFM's own page (mods_page.hpp), made in the title scene's update.
//   "Mods" starts the scene's Options flow, which CMFM points at its page.
// Exe 421810 only; any unexpected value leaves the title as the game made it.
#pragma once

#include "native/start_menu.hpp"

#include <string>

namespace cmfm::native {

// al_plugin_init: registers the four hooks. False (with `why`) if one is
// refused; nothing is changed then.
bool title_menu_register(const Env& env, std::string& why);
// WIN_MAIN, after mods_page_check: checks the hooks. False (with `why`)
// leaves the title untouched.
bool title_menu_check(const Env& env, std::string& why);
// SHUTDOWN.
void title_menu_restore(const Env& env);

}  // namespace cmfm::native
