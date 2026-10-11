// The title screen's "Mods" entry and page (decisions.md item 8, prototype 3;
// docs/research/native-menu.md, "The title screen"):
// - entry: an 8th row in the title menu, right below Options. Hooks on the
//   menu's cursor and selected callbacks shift the rows below it by one, so
//   the game's own row numbers still hold; "Mods" acts as Options.
// - page: when the title Options page opens after "Mods", CMFM swaps in its
//   tab tables, rebuilds the page's lists with the game's own builder, keeps
//   five categories (Key Config and Default, a full reset, out of reach) and
//   renames the header and categories. The next plain open rebuilds the
//   game's lists.
// Exe 421810 only; any unexpected value leaves the title as the game made it.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

// al_plugin_init: registers the five hooks and finds the game functions CMFM
// calls. False (with `why`) if anything is missing; nothing is changed then.
bool title_menu_register(const Env& env, std::string& why);
// WIN_MAIN, after options_view_check: checks the hooks, the functions and the
// title tables. False (with `why`) leaves the title untouched.
bool title_menu_check(const Env& env, std::string& why);

// From the text hook: the category labels while the title Mods page is on,
// or nullptr to let the game answer.
const char* title_menu_text(void* message_file, std::uint32_t id);

// SHUTDOWN: puts the game's title tables back.
void title_menu_restore(const Env& env);

}  // namespace cmfm::native
