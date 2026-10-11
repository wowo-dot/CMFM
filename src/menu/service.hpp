// The cmfm.menu service: the table CMFM publishes through Astera Loader, and
// the bridge between plugins (add_page from any thread) and the Mods pages
// (the game's menu thread). Pages added since the last opening are taken in
// when a Mods page opens; the menu core and its model are touched only on
// the game's menu thread after that.
#pragma once

#include "al/al_plugin.h"
#include "native/options_model.hpp"

#include <cstdint>
#include <string>

namespace cmfm::menu {

// al_plugin_init: publishes "cmfm.menu". False (with `why`) if Astera Loader
// refused the name; plugins then run without the menu.
bool service_publish(const al_api_t* api, al_plugin_t* self, const char* version, std::string& why);

// Game thread, when a Mods page is about to open (no page open): takes in the
// pages added since, rebuilds the layout if needed, sends CMFM_EVENT_OPENED,
// asks TEXT rows for their texts and turns CMFM's row texts on. nullptr
// (logged) if no usable model.
const native::OptionsModel* service_prepare();
// The page closed: texts off, CMFM_EVENT_CLOSED.
void service_closed();
// Opening failed after service_prepare: the same (the plugins heard OPENED).
void service_abort();

// CMFM's row texts are on (a Mods page is opening, open or closing).
bool service_texts_on();
const native::OptionsModel& service_model();
std::size_t service_used_tabs();

// From the option getter and setter hooks and Set To Default.
std::uint64_t service_get(std::uint32_t setting_id);
void service_set(std::uint32_t setting_id, std::int32_t value);
void service_reset();

// For the Mods page's drawing (game thread; false / 0 without a layout).
bool service_is_section(std::uint32_t setting_id);
bool service_restart_pending(std::uint32_t setting_id);
bool service_tab_restart_pending(std::size_t tab);
std::size_t service_restart_count();
// Tab t's header (Menu::tab_header); "" without a menu. Valid until the next
// call.
const char* service_tab_header(std::size_t tab);
// A number row's value text (Menu::value_text); nullptr for other rows.
const char* service_value_text(std::uint32_t setting_id, std::uint32_t index);
std::uint32_t service_restart_revision();
// Sliders: on while CMFM's own page shows its menu (its number rows are game
// sliders then, and the option hooks speak the slider's 0-100 scale for
// them); off for the in-game Options view, whose rows keep their arrows.
void service_set_sliders(bool on);
std::uint32_t service_slider_steps(std::uint32_t setting_id);  // 0: not a slider (now)
std::uint32_t service_value_revision();
// CHOICE rows that open the game's pick list on CMFM's page: their choices,
// or 0.
std::uint32_t service_list_choices(std::uint32_t setting_id);
// BUTTON rows and locks (Menu::press, Menu::is_locked).
bool service_is_button(std::uint32_t setting_id);
const char* service_button_question(std::uint32_t setting_id);  // nullptr: none
bool service_press(std::uint32_t setting_id);
bool service_locked(std::uint32_t setting_id);
bool service_hidden(std::uint32_t setting_id);  // Menu::is_hidden
std::uint32_t service_lock_revision();

}  // namespace cmfm::menu
