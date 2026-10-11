// CMFM's page (page_class.hpp), wherever it opens: the title menu's Mods
// entry (title_menu.hpp) and the Start menu's (start_menu.hpp).
// - content: on open, CMFM fills the page's lists with its menu's tabs
//   (menu/service.hpp) through the game's own builder, keeps one category per
//   tab in use (Key Config and Default, a full reset, out of reach), scrolls
//   more than five, renames the header and categories and hides the Display
//   category's VRAM panel. The game's title tab tables point at CMFM's only
//   while CMFM's page builds; the game's Options pages are never touched.
// - lifetime: one page, made in the title scene's update and kept for the
//   session (scene changes do not kill it).
// - from the Start menu: opened the way the menu opens its Options page, and
//   the menu opens again when the page closes.
// Exe 421810 only.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

// al_plugin_init: finds the game functions (with the page class's) and
// registers the two hooks. False (with `why`) if anything is missing.
bool mods_page_register(const Env& env, std::string& why);
// WIN_MAIN, after options_view_check: checks the hooks, the title tables and
// the page class. False (with `why`): no page this session.
bool mods_page_check(const Env& env, std::string& why);
bool mods_page_ready();

// The page, made if there is none yet (once per session; call from a game
// unit's update on the game thread). nullptr if it cannot be made.
void* mods_page_ensure();
// From the Start menu's action for "Mods": opens the page in place of the
// game's Options page. False if there is no page ready (the caller falls back).
bool mods_page_open_from_start_menu(void* start_menu);

// From the text hook: the category labels and descriptions while the page
// shows CMFM's menu, or nullptr to let the game answer.
const char* mods_page_text(void* message_file, std::uint32_t id);

// From the option setter (the game's menu thread): the value a slider row of
// the shown panel stands on, from its knob's position: the track in `steps` +
// 1 equal zones, one per value, the ends included (the game's own 0-100 value
// rounds up, one step too far for some positions with 100 steps). False if no
// shown slider has it.
bool mods_page_slider_index(std::uint32_t setting_id, std::uint32_t steps, std::uint32_t* index);

// SHUTDOWN: puts the title tab tables back and stops following the page.
void mods_page_restore(const Env& env);

}  // namespace cmfm::native
