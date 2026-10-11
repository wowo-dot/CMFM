// A GUI page of CMFM's own (docs/research/native-menu.md, "A page of CMFM's
// own"): a uGUIOption object, built by the game's own code, that carries
// CMFM's class description (uGUICmfmPage, a child of uGUIOption) and a copy
// of uGUIOption's vtable with CMFM's open, close and update handlers. The
// game's own Options pages are never touched.
//
// CMFM creates the page the way the title scene creates its pages (new
// object, move line 20) and follows it with one of the game's tracked
// references, which the game clears when the page is destroyed (the title
// scene kills its units when it ends). Exe 421810 only.
#pragma once

#include "native/start_menu.hpp"

#include <cstdint>
#include <string>

namespace cmfm::native {

using UnitFn = std::uint64_t (*)(void* unit);

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
// Creates the page on the game thread (a game unit's update) and follows it.
// nullptr (with `why`) on failure; do not retry then (a page may exist
// unfollowed).
void* page_create(std::string& why);
// The game's own function behind one of the overridden slots.
UnitFn page_next(std::size_t slot);
// Asks the page to close (uMhGUI::requestClose).
void page_request_close(void* unit);
// SHUTDOWN: stops following the page.
void page_release();

}  // namespace cmfm::native
