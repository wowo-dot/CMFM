// A GUI page of CMFM's own (docs/research/native-menu.md, "A page of CMFM's
// own"): a uGUIOption object, built by the game's own code, that carries
// CMFM's class description (uGUICmfmPage, a child of uGUIOption) and a copy
// of uGUIOption's vtable with CMFM's open, close and update handlers. The
// game's own Options pages are never touched.
//
// CMFM creates the page the way the game creates its pages (new object,
// move line 20) and does not keep it: the title's page dies with the title
// scene, like the title's own pages, and in game the page lives only while it
// is open (mods_page.cpp). Every GUI unit sits in sMhGUI's unit list from its
// start to its end; a page kept across the change from title to game made
// every in-game unit sit one slot on, which broke tools that index that list
// (docs/research/native-menu.md, "The game's GUI unit list"). CMFM follows the
// page with one of the game's tracked references, which the game clears when
// the page is destroyed. Exe 421810 only.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

using UnitFn = std::uint64_t (*)(void* unit);

// One of the game's tracked references: linked into the unit's list, and
// zeroed by the game when the unit is destroyed. Static storage only (the
// game writes into it).
struct GameRef {
    void* unit;
    void* next;
    void* prev;
};

// A unit that exists and has not been killed / that has finished its setup.
bool unit_alive(const void* unit);
bool unit_running(const void* unit);
// Points `ref` at `unit` (nullptr: lets go); the unit or nullptr if alive.
void game_ref_set(GameRef& ref, void* unit);
void* game_ref_alive(const GameRef& ref);

// CMFM's handlers for vtable slots 81 (update), 90 (open) and 91 (close).
// Each calls page_next(slot) itself.
struct PageHandlers {
    UnitFn update;
    UnitFn open;
    UnitFn close;
};

// al_plugin_init: finds the game functions the page needs.
bool page_class_register(const Env& env, std::string& why);
// WIN_MAIN: checks uGUIOption's class description and vtable (the slots
// CMFM overrides hold the functions their patterns find) and builds CMFM's
// copies. False (with `why`) if anything differs; no page is made then.
bool page_class_check(const Env& env, const PageHandlers& handlers, std::string& why);

// The page if it exists and has not been killed, else nullptr.
void* page_alive();
// Creates the page on the game thread (a game unit's update) and follows it;
// not kept (see above). nullptr (with `why`) on failure; do not retry then (a
// page may exist unfollowed).
void* page_create(std::string& why);
// True if page_create registered the page tabs (game::kTabListName) as one of
// `unit`'s input lists, so they can take keys and pad.
bool page_tab_input(const void* unit);
// Marks the page kept (no scene change kills it; a kept page that is open is
// closed by the game at a scene change instead) or not.
void page_keep(void* unit, bool keep);
// The game's own function behind one of the overridden slots.
UnitFn page_next(std::size_t slot);
// The layout the page loads at its setup (vtable slot 71): uGUIOption's
// (ui/title/options) unless set before page_create. `path` must stay valid.
void page_set_layout(const char* path);
const char* page_layout();
// uMhGUI::requestOpen(unit, opener) and requestClose(unit, now): the open or
// close happens on the unit's next update.
void gui_request_open(void* unit, void* opener);
void gui_request_close(void* unit);
// SHUTDOWN: stops following the page.
void page_release();

}  // namespace cmfm::native
