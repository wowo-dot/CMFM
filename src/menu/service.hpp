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
// pages added since, rebuilds the layout if needed, asks TEXT rows for their
// texts and turns CMFM's row texts on. nullptr (logged) if no usable model.
const native::OptionsModel* service_prepare();
// The page is open: CMFM_EVENT_OPENED to the plugins.
void service_opened();
// The page closed: texts off, CMFM_EVENT_CLOSED.
void service_closed();
// Opening failed after service_prepare: texts off, no events.
void service_abort();

// CMFM's row texts are on (a Mods page is opening, open or closing).
bool service_texts_on();
const native::OptionsModel& service_model();
std::size_t service_used_tabs();

// From the option getter and setter hooks and Set To Default.
std::uint64_t service_get(std::uint32_t setting_id);
void service_set(std::uint32_t setting_id, std::int32_t value);
void service_reset();

}  // namespace cmfm::menu
