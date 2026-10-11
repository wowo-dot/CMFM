// The title screen's "Mods" entry and page (decisions.md item 8, prototype 3;
// docs/research/native-menu.md, "The title screen"):
// - entry: an 8th row in the title menu, right below Options. Hooks on the
//   menu's cursor and selected callbacks shift the rows below it by one, so
//   the game's own row numbers still hold; "Mods" acts as Options.
// - page: CMFM's own page (page_class.hpp), made in the title scene's update
//   and gone with the scene. "Mods" starts the scene's Options flow, which
//   CMFM points at its page; on open CMFM fills the page's lists with its
//   menu's tabs (menu/service.hpp) through the game's own builder, keeps one
//   category per tab in use (Key Config and Default, a full reset, out of
//   reach), scrolls more than five, renames the header and categories and
//   hides the Display category's VRAM panel. The game's tab tables point at
//   CMFM's only while CMFM's page builds; the game's Options page is never
//   touched.
// Exe 421810 only; any unexpected value leaves the title as the game made it.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

// al_plugin_init: registers the six hooks and finds the game functions CMFM
// calls (with the page class's). False (with `why`) if anything is missing;
// nothing is changed then.
bool title_menu_register(const Env& env, std::string& why);
// WIN_MAIN, after options_view_check: checks the hooks, the title tables and
// the page class. False (with `why`) leaves the title untouched.
bool title_menu_check(const Env& env, std::string& why);

// From the text hook: the category labels and descriptions while the title
// Mods page is on, or nullptr to let the game answer.
const char* title_menu_text(void* message_file, std::uint32_t id);

// SHUTDOWN: puts the game's title tables back and stops following the page.
void title_menu_restore(const Env& env);

}  // namespace cmfm::native
