// Small helpers over the game's GUI objects (exe 421810), shared by the title
// menu's entry and CMFM's page: the game functions CMFM calls (found with
// al.scanner) and field access.
#pragma once

#include "native/start_menu.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cmfm::native {

template <class T>
T& field(void* base, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

// One game function by its pattern (exactly one match), or false with `why`.
bool find_game_function(const Env& env, const char* name, const char* pattern, void** out, std::string& why);

// al_plugin_init: finds the list and text functions below.
bool gui_register(const Env& env, std::string& why);

// cGUIInstButtonList::setRowCount (keeps the cursor) and setCursor (reads the
// row under the old cursor unchecked).
void gui_set_row_count(void* list, std::uint64_t count);
void gui_set_cursor(void* list, std::uint32_t row);
// uGUI::getInstanceFromName, cGUIObjRoot::getObjectFromName,
// cGUIObjSizeAdjustMessage::formatText, cGUIInstance::setPlay,
// cGUIObject::setHidden.
void* gui_get_instance(void* unit, const char* name);
void* gui_get_object(void* root, const char* name);
void gui_format_text(void* object, const char* text);
void gui_set_play(void* instance, bool play);
void gui_set_hidden(void* object, bool hidden);
// Writes `text` into a named text object of a named instance of the unit's
// layout. False if either is missing.
bool set_instance_text(void* unit, const char* instance_name, const char* object_name, const char* text);

// A list's row instance, or nullptr past the row count.
void* list_row(void* list, std::uint32_t row);
// Writes `text` into the named text objects of a list row (`name2` may be
// nullptr). False if the row has none of them.
bool set_row_text(void* row, const char* name, const char* name2, const char* text);

}  // namespace cmfm::native
