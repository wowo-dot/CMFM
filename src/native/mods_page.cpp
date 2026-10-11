#include "native/mods_page.hpp"

#include "menu/service.hpp"
#include "native/data_patch.hpp"
#include "native/game_421810.hpp"
#include "native/gui.hpp"
#include "native/options_model.hpp"
#include "native/options_view.hpp"
#include "native/page_class.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace cmfm::native {
namespace {

// Hooked.
using ShowCategoryFn = std::uint64_t (*)(void* categories, std::int32_t category);
using ListMoveFn = std::uint64_t (*)(void* list, std::int32_t row);
using RowLooksFn = std::uint64_t (*)(void* tab, std::uint32_t cursor);
// Called.
using InitCategoryFn = std::uint64_t (*)(void* tab, void* unit, std::int32_t index, const char* name,
                                         const char* title);
using BuildCategoriesFn = void (*)(void* categories, void* unit);
using LockChannelFn = std::uint64_t (*)(void* input, std::int32_t channel, std::uint32_t player);
using LoadResourceFn = void* (*)(void* resources, const void* resource_class, const char* path, std::uint32_t flags);
using MousePositionFn = void* (*)(void* gui, float* out, void* list);
using MouseHitFn = std::uint8_t (*)(void* gui, void* instance, const float* position, std::int32_t* index);
using MouseFlagFn = std::uint8_t (*)(void* gui, void* list);
using PanelFocusFn = std::uint64_t (*)(void* object, std::int32_t index);
using HelpRequestFn = void (*)(void* manager, void* unit, std::int32_t message, std::int32_t file, float seconds,
                               std::uint8_t flag);
using SliderFromSettingFn = void (*)(void* slider, void* text, const GameOptionRow* row);
using SliderSetPositionFn = void (*)(void* slider, float position);
using RowDecideFn = void (*)(void* row);
using SelectDecideFn = std::uint64_t (*)(void* select, std::uint32_t index);
using SetFocusFn = std::uint64_t (*)(void* unit, std::int32_t focus);
using SetScaleYFn = void (*)(void* instance, float scale);
using SetCurrentFrameFn = void (*)(void* object, float frame, bool apply);

ShowCategoryFn g_show_category_next = nullptr;
ListMoveFn g_list_move_next = nullptr;
RowLooksFn g_row_looks_next = nullptr;
RowDecideFn g_row_decide_next = nullptr;
SelectDecideFn g_select_decide_next = nullptr;
InitCategoryFn g_init_category = nullptr;
// From uGUIMenuOption::update: the input manager and area manager globals and
// the input lock.
void* const* g_input_manager = nullptr;
void* const* g_area_manager = nullptr;
LockChannelFn g_lock_channel = nullptr;
LoadResourceFn g_load_resource = nullptr;
// The page's mouse helpers (split mode); without them the mod list follows
// only the keys and the wheel.
MousePositionFn g_mouse_position = nullptr;
MouseHitFn g_mouse_hit = nullptr;
MouseFlagFn g_mouse_click = nullptr;
MouseFlagFn g_mouse_moved = nullptr;
bool g_mouse_off = false;       // a focus function was not where expected (logged once)
bool g_cursor_warned = false;   // the category cursor left the panel (logged once per open)
// The help line request (optional: without it the help line may keep the
// previous mod's text).
HelpRequestFn g_help_request = nullptr;
void* const* g_gui_manager = nullptr;
std::uint32_t g_help_flip = 0;
// Sliders for number rows (optional: without these the rows keep arrows).
SliderFromSettingFn g_slider_from_setting = nullptr;
SliderSetPositionFn g_slider_set_position = nullptr;
// The split panel's slider rows as the last decoration found them.
struct SliderRow {
    std::uint32_t row;
    std::uint32_t steps;
    std::uint32_t index;  // the value shown
};
SliderRow g_slider_rows[game::kTabRowObjects];
std::size_t g_slider_count = 0;
std::uint32_t g_value_seen = 0;  // the menu's value revision the sliders show
// The pick list for long CHOICE rows (optional: without these the rows keep
// arrows). Its check marks stay out of the select object's own array, whose
// update hides every mark of a setting that is not a language.
SetFocusFn g_set_focus = nullptr;
SetScaleYFn g_set_scale_y = nullptr;
SetCurrentFrameFn g_set_current_frame = nullptr;
void* g_pick_checks[game::kSelectRows] = {};
std::uint32_t g_pick_count = 0;
std::int32_t g_pick_filled = -1;  // the setting the pick list holds
// CMFM's layout as loaded once (kept for the session, like the page).
void* g_layout_resource = nullptr;

enum Hook { kShowCategory, kListMove, kRowLooks, kRowDecide, kSelectDecide, kHookCount };
std::uint32_t g_hooks[kHookCount] = {};

Env g_env = {};
std::atomic<bool> g_ready{false};
std::uint32_t g_pages_created = 0;
bool g_create_failed = false;  // logged once; no page this session then
// CMFM's page is open with CMFM's rows (the text hook answers).
std::atomic<bool> g_view{false};
// The Start menu that opened the page: it opens again when the page closes.
GameRef g_return_menu = {};
// The page is open from the Start menu: it holds the hunter as the game's
// Options page does.
std::atomic<bool> g_hold_player{false};
std::uint32_t g_opened_in_game = 0;
// TitleOptionTabTables, pointed at CMFM's tables only while CMFM's page builds.
QwordPatch g_tabs[game::kTitleViewTabs];
// The category list's window over the menu's tabs: physical row r shows tab
// g_top + r. The game's code takes the row as the tab object's index, and
// only five tab objects have row lists, so more than five tabs scroll.
std::atomic<std::size_t> g_top{0};
std::atomic<std::size_t> g_entries{0};
std::atomic<void*> g_window_list{nullptr};  // the category list while it scrolls
std::atomic<void*> g_view_unit{nullptr};
std::atomic<int> g_wheel{0};  // wheel delta from the window thread
int g_wheel_rest = 0;
// The category list's wrap flag before CMFM turned it on (it relies on it).
struct SavedWrap {
    bool on = false;
    void* list = nullptr;
    std::uint8_t wrap = 0;
} g_saved;
// Edge rows with an arrow when more tabs lie beyond them.
char g_label_up[96];
char g_label_down[96];
// Split mode (CMFM's own layout, docs/research/native-menu.md "Layout
// study"): the mod list (the layout's spare camera list, styled like the
// category list) shows every menu tab; the page's category list, off screen,
// keeps its cursor on the audio panel, which shows the highlighted tab.
std::atomic<bool> g_split{false};
std::atomic<void*> g_mod_list{nullptr};  // while the page shows CMFM's menu
std::atomic<std::size_t> g_selected{0};  // the highlighted menu tab
std::size_t g_mod_top = 0;               // the first tab in the mod list's window
std::uint32_t g_restart_seen = 0;        // the menu's restart revision the page shows
std::int32_t g_decorated_focus = -1;     // the page's focus when the panel was last decorated
char g_restart_line[96];

// Short lines on user actions only (no allocation).
void logf(al_log_level_t level, const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    g_env.api->log(g_env.self, level, line);
}

void* message_file(std::uintptr_t rva) {
    return *reinterpret_cast<void* const*>(g_env.base + rva);
}

std::uintptr_t title_tab_slot(std::size_t tab) {
    return g_env.base + game::kRvaTitleOptionTabTables + 8 * tab;
}

// The page's lists are rebuilt on every open. Before that: every
// cursor to row 0 (setRowCount keeps the cursor, and the page reads the row
// under it unchecked) and the row objects as the constructor leaves them
// (the game iterates all 24 and would use stale rows beyond the new count).
void cursor_to_top(void* list) {
    if (list == nullptr || field<std::uint32_t>(list, game::kListRowCount) == 0) {
        return;
    }
    if (field<std::uint32_t>(list, game::kListCursor) < field<std::uint32_t>(list, game::kListRowCount)) {
        gui_set_cursor(list, 0);
    } else {
        field<std::uint32_t>(list, game::kListCursor) = 0;
    }
}

char* tab_object(void* unit, std::size_t t) {
    return static_cast<char*>(unit) + game::kOptTabs + t * game::kOptTabSize;
}

void prepare_tab(void* unit, std::size_t t) {
    char* tab = tab_object(unit, t);
    cursor_to_top(field<void*>(tab, game::kTabList));
    field<std::int32_t>(tab, game::kTabDisplayRow) = -1;
    for (std::size_t r = 0; r < game::kTabRowObjects; ++r) {
        char* row = tab + (r + 1) * game::kRowObjectSize;
        std::memset(row + 8, 0, 72);
        field<std::int32_t>(row, 80) = 7;
        field<std::int32_t>(row, 84) = 0;
        field<std::int32_t>(row, 88) = -1;
    }
}

bool prepare_rebuild(void* unit) {
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        if (field<std::int32_t>(tab_object(unit, t), game::kTabIndex) != static_cast<std::int32_t>(t)) {
            logf(AL_LOG_WARNING, "Mods page: tab object %zu is tab %d", t,
                 field<std::int32_t>(tab_object(unit, t), game::kTabIndex));
            return false;
        }
    }
    cursor_to_top(field<void*>(unit, game::kOptCategoryList));
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        prepare_tab(unit, t);
    }
    return true;
}

void restore_tables() {
    for (QwordPatch& p : g_tabs) {
        std::string why;
        if (p.applied() && !p.restore(why)) {
            logf(AL_LOG_WARNING, "Mods page: could not restore a tab table: %s", why.c_str());
        }
    }
}

// The menu tab shown by tab object t: in split mode every panel holds the
// highlighted tab (a panel must never be empty, and only the audio panel is
// shown); otherwise the window's tab.
std::size_t menu_tab(std::size_t t) {
    return g_split.load(std::memory_order_relaxed) ? g_selected.load(std::memory_order_relaxed)
                                                    : g_top.load(std::memory_order_relaxed) + t;
}

// Runs `build` with TitleOptionTabTables[t] pointing at CMFM's menu_tab(t):
// the game reads those tables only while it fills a category's rows
// (fillCategoryRows), so the game's own Options pages never see CMFM's.
template <class Build>
bool with_tables(const OptionsModel& model, Build build) {
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        std::string why;
        const std::uint64_t expected = g_env.base + game::kRvaTitleOptionRows[t];
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(model.table(menu_tab(t)));
        if (!g_tabs[t].apply("title Options tab", title_tab_slot(t), expected, ours, why)) {
            logf(AL_LOG_WARNING, "Mods page: could not point tab %zu at CMFM's tab %zu: %s", t, menu_tab(t),
                 why.c_str());
            restore_tables();
            return false;
        }
    }
    build();
    restore_tables();
    return true;
}

// ---- Scrolling the category list (more than five tabs) ----

std::uint64_t show_category_detour(void* categories, std::int32_t category);
bool split_view();
void select_tab(void* unit, std::size_t index);
void decorate_panel(void* unit);
void show_restart_line(void* unit);
void request_split_help(void* unit);
void update_sliders(void* unit);
void show_pick_list(void* unit, const GameOptionRow& entry);

bool scrolling() {
    return g_view.load(std::memory_order_acquire) && g_entries.load(std::memory_order_relaxed) > game::kTitleViewTabs;
}

// The window moved: point the five tab objects at their new tabs and fill
// them again, the game's way (initCategory), relabel the rows (arrows on the
// edge rows when more lie beyond) and show the highlighted tab again.
void refill_window(void* unit) {
    const OptionsModel& model = menu::service_model();
    void* categories = field<void*>(unit, game::kOptCategoryList);
    const std::size_t top = g_top.load(std::memory_order_relaxed);
    const std::size_t entries = g_entries.load(std::memory_order_relaxed);
    const bool filled = with_tables(model, [&] {
        for (std::size_t r = 0; r < game::kTitleViewTabs; ++r) {
            prepare_tab(unit, r);
            g_init_category(tab_object(unit, r), unit, static_cast<std::int32_t>(r), game::kTitleTabNames[r],
                            model.tab_name(top + r));
        }
    });
    if (!filled) {
        return;
    }
    for (std::size_t r = 0; r < game::kTitleViewTabs; ++r) {
        const char* label = model.tab_name(top + r);
        if (r == 0 && top > 0) {
            std::snprintf(g_label_up, sizeof g_label_up, "\xE2\x96\xB2 %s", label);  // up-pointing triangle
            label = g_label_up;
        } else if (r == game::kTitleViewTabs - 1 && top + game::kTitleViewTabs < entries) {
            std::snprintf(g_label_down, sizeof g_label_down, "%s \xE2\x96\xBC", label);  // down-pointing triangle
            label = g_label_down;
        }
        set_row_text(list_row(categories, static_cast<std::uint32_t>(r)), "msg_1", nullptr, label);
    }
    show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                         static_cast<std::int32_t>(field<std::uint32_t>(categories, game::kListCursor)));
}

void scroll_to(void* unit, std::size_t top) {
    if (top == g_top.load(std::memory_order_relaxed)) {
        return;
    }
    g_top.store(top, std::memory_order_relaxed);
    refill_window(unit);
}

// Directional moves on the category list: past the bottom or top row the
// window moves instead of the cursor; at the very end and start it wraps.
std::uint64_t list_move_detour(void* list, std::int32_t row) {
    if (list == g_window_list.load(std::memory_order_acquire) && split_view() &&
        g_mod_list.load(std::memory_order_acquire) != nullptr) {
        // Split mode: the category cursor stays on the audio panel; the move
        // goes to the mod list (wrapping at both ends).
        const std::int32_t old = field<std::int32_t>(list, game::kListCursor);
        const std::size_t entries = g_entries.load(std::memory_order_relaxed);
        if (row != old && entries > 1) {
            const std::size_t selected = g_selected.load(std::memory_order_relaxed);
            select_tab(g_view_unit.load(std::memory_order_acquire),
                       row > old ? (selected + 1) % entries : (selected + entries - 1) % entries);
        }
        return 0;
    }
    if (list == g_window_list.load(std::memory_order_acquire) && scrolling()) {
        void* unit = g_view_unit.load(std::memory_order_acquire);
        const std::int32_t old = field<std::int32_t>(list, game::kListCursor);
        const std::int32_t last = static_cast<std::int32_t>(game::kTitleViewTabs) - 1;
        const std::size_t top = g_top.load(std::memory_order_relaxed);
        const std::size_t last_top = g_entries.load(std::memory_order_relaxed) - game::kTitleViewTabs;
        if (old == last && row == 0) {  // Down on the bottom row
            if (top < last_top) {
                scroll_to(unit, top + 1);
                return 0;  // the cursor stays
            }
            scroll_to(unit, 0);
        } else if (old == 0 && row == last) {  // Up on the top row
            if (top > 0) {
                scroll_to(unit, top - 1);
                return 0;
            }
            scroll_to(unit, last_top);
        }
    }
    return g_list_move_next(list, row);
}

// The mouse wheel (window thread): kept for the page's next update while the
// category list has the focus.
std::uint32_t AL_CALL message_callback(al_plugin_t*, void*, std::uint32_t message, std::uintptr_t wparam,
                                       std::intptr_t, void*) {
    constexpr std::uint32_t kMouseWheel = 0x020A;  // WM_MOUSEWHEEL
    if (message != kMouseWheel || !(scrolling() || split_view())) {
        return 0;
    }
    void* unit = g_view_unit.load(std::memory_order_acquire);
    if (unit == nullptr || field<std::int32_t>(unit, game::kOptFocus) != 0) {
        return 0;  // inside a tab: the game handles the wheel
    }
    g_wheel.fetch_add(static_cast<std::int16_t>((wparam >> 16) & 0xFFFF), std::memory_order_relaxed);
    return 1;
}

// What uGUIMenuOption::update does first in its open state when the Start
// menu opened it: lock the player's menu and movement input channels and set
// the area manager's menu bytes, every frame.
void hold_player(void* unit) {
    if ((field<std::uint32_t>(unit, game::kGuiState) & game::kGuiStateOpen) == 0) {
        return;
    }
    if (void* input = *g_input_manager) {
        g_lock_channel(input, game::kInputChannelMenu, 0);
        g_lock_channel(input, game::kInputChannelMove, 0);
    }
    if (void* area = *g_area_manager) {
        field<std::uint16_t>(area, game::kAreaMenuFlags) = game::kAreaMenuFlagsOn;
    }
}

// Split mode: the page enters the panel under the category cursor, and only
// the split panel is shown, so the cursor stays there (a Confirm on any other
// row would enter a hidden panel and keep the input there; run
// 20261011-split-3). The layout's start row covers the list's opening; this
// catches anything else, visibly.
void keep_panel_cursor(void* unit) {
    void* categories = field<void*>(unit, game::kOptCategoryList);
    if (categories == nullptr || field<std::int32_t>(unit, game::kOptFocus) != 0) {
        return;
    }
    const std::uint32_t cursor = field<std::uint32_t>(categories, game::kListCursor);
    if (cursor == game::kSplitPanel) {
        return;
    }
    if (!g_cursor_warned) {
        g_cursor_warned = true;
        logf(AL_LOG_WARNING, "Mods page: the category cursor moved to row %u; back on the panel", cursor);
    }
    if (cursor < field<std::uint32_t>(categories, game::kListRowCount)) {
        gui_set_cursor(categories, static_cast<std::uint32_t>(game::kSplitPanel));
    } else {
        field<std::uint32_t>(categories, game::kListCursor) = static_cast<std::uint32_t>(game::kSplitPanel);
    }
    show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                         static_cast<std::int32_t>(game::kSplitPanel));
}

// One of the page's own focus changes, if the object's vtable slot holds it.
PanelFocusFn focus_function(void* object, std::size_t slot, std::uintptr_t rva, const char* what) {
    void* const* vtable = field<void* const*>(object, 0);
    void* fn = vtable != nullptr ? vtable[slot] : nullptr;
    if (reinterpret_cast<std::uintptr_t>(fn) != g_env.base + rva) {
        g_mouse_off = true;
        logf(AL_LOG_WARNING, "Mods page: %s is not where expected; the mouse no longer moves the focus", what);
        return nullptr;
    }
    return reinterpret_cast<PanelFocusFn>(fn);
}

// Split mode, the mouse with both sides live, as on the game's page: over the
// mod list it highlights the entry under the pointer (and takes the focus
// back from the panel, as Back does); moving onto the panel gives it the
// focus (the game's page waits for a click there; the page's update still
// does that too). Only on mouse activity, and only while the page has the
// input (the helpers answer nothing otherwise).
void track_mouse(void* unit) {
    void* list = g_mod_list.load(std::memory_order_acquire);
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    if (g_mouse_moved == nullptr || g_mouse_off || list == nullptr || (focus != 0 && focus != 1)) {
        return;
    }
    const bool moved = g_mouse_moved(unit, nullptr) != 0;
    if (!moved && g_mouse_click(unit, nullptr) == 0) {
        return;
    }
    alignas(16) float position[4] = {};
    g_mouse_position(unit, position, nullptr);
    const std::uint32_t rows = field<std::uint32_t>(list, game::kListRowCount);
    for (std::uint32_t r = 0; r < rows; ++r) {
        void* row = list_row(list, r);
        if (row == nullptr || g_mouse_hit(unit, row, position, nullptr) == 0) {
            continue;
        }
        if (focus == 1) {
            char* tab = tab_object(unit, game::kSplitPanel);
            if (PanelFocusFn leave = focus_function(tab, game::kLeavePanelSlot, game::kRvaLeavePanel, "a panel's Back")) {
                leave(tab, 0);
            }
        }
        select_tab(unit, g_mod_top + r);
        return;
    }
    void* categories = field<void*>(unit, game::kOptCategoryList);
    void* window = field<void*>(tab_object(unit, game::kSplitPanel), game::kTabWindow);
    if (focus != 0 || !moved || categories == nullptr || window == nullptr ||
        field<std::uint32_t>(categories, game::kListCursor) != game::kSplitPanel ||
        g_mouse_hit(unit, window, position, nullptr) == 0) {
        return;
    }
    void* owner = static_cast<char*>(unit) + game::kOptCategories;
    if (PanelFocusFn enter = focus_function(owner, game::kEnterPanelSlot, game::kRvaEnterPanel, "the category Confirm")) {
        enter(owner, static_cast<std::int32_t>(game::kSplitPanel));
    }
}

// CMFM's page, vtable slot 81: every frame while it is open (state 2; the
// menu's thread).
std::uint64_t page_update(void* unit) {
    if (split_view() && unit == g_view_unit.load(std::memory_order_acquire)) {
        keep_panel_cursor(unit);
        track_mouse(unit);
        const std::uint32_t revision = menu::service_restart_revision();
        if (revision != g_restart_seen) {
            g_restart_seen = revision;
            decorate_panel(unit);
            show_restart_line(unit);
        }
    }
    if (g_hold_player.load(std::memory_order_relaxed)) {
        hold_player(unit);
    }
    if (g_wheel.load(std::memory_order_relaxed) != 0) {
        const int delta = g_wheel.exchange(0, std::memory_order_relaxed) + g_wheel_rest;
        constexpr int kNotch = 120;  // WHEEL_DELTA
        const int steps = delta / kNotch;
        g_wheel_rest = delta % kNotch;
        if (steps != 0 && split_view() && unit == g_view_unit.load(std::memory_order_acquire)) {
            // Wheel up (positive) selects earlier tabs; no wrap.
            std::int64_t next = static_cast<std::int64_t>(g_selected.load(std::memory_order_relaxed)) - steps;
            const std::int64_t last = static_cast<std::int64_t>(g_entries.load(std::memory_order_relaxed)) - 1;
            next = next < 0 ? 0 : next > last ? last : next;
            select_tab(unit, static_cast<std::size_t>(next));
        } else if (steps != 0 && scrolling() && unit == g_view_unit.load(std::memory_order_acquire)) {
            const std::size_t top = g_top.load(std::memory_order_relaxed);
            const std::size_t last_top = g_entries.load(std::memory_order_relaxed) - game::kTitleViewTabs;
            // Wheel up (positive) shows earlier tabs; no wrap.
            std::int64_t next = static_cast<std::int64_t>(top) - steps;
            next = next < 0 ? 0 : next > static_cast<std::int64_t>(last_top) ? static_cast<std::int64_t>(last_top) : next;
            scroll_to(unit, static_cast<std::size_t>(next));
        }
    }
    const std::uint64_t result = page_next(game::kSlotUpdate)(unit);
    // A focus change activates or deactivates the panel, and the tab's
    // setActive (RVA 0x1E8C530) hides the rows' stars again by the game's own
    // rule; keys, clicks and the mouse above all change the focus before this
    // point, so the marks are back before the frame is drawn. (The value
    // boxes come back through the row looks, handled in their hook.)
    if (split_view() && unit == g_view_unit.load(std::memory_order_acquire)) {
        const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
        if (focus != g_decorated_focus) {
            g_decorated_focus = focus;
            decorate_panel(unit);
        }
        update_sliders(unit);  // the game moved them in this update (keys, mouse, setter)
    }
    return result;
}

void start_window(void* unit, void* categories) {
    char* list = static_cast<char*>(categories);
    g_saved = {true, list, field<std::uint8_t>(list, game::kListWrap)};
    field<std::uint8_t>(list, game::kListWrap) = 1;
    g_wheel.store(0, std::memory_order_relaxed);
    g_wheel_rest = 0;
    g_view_unit.store(unit, std::memory_order_release);
    g_window_list.store(categories, std::memory_order_release);
    // The bottom row's arrow: more tabs below.
    const OptionsModel& model = menu::service_model();
    std::snprintf(g_label_down, sizeof g_label_down, "%s \xE2\x96\xBC", model.tab_name(game::kTitleViewTabs - 1));
    set_row_text(list_row(categories, game::kTitleViewTabs - 1), "msg_1", nullptr, g_label_down);
    logf(AL_LOG_INFO, "Mods page: category list scrolls (%zu tabs, 5 rows; wrap was %u)",
         g_entries.load(std::memory_order_relaxed), g_saved.wrap);
}

// Only on the list CMFM changed (a new page starts with the game's values).
void stop_window(void* unit) {
    g_window_list.store(nullptr, std::memory_order_release);
    g_view_unit.store(nullptr, std::memory_order_release);
    if (!g_saved.on) {
        return;
    }
    if (field<void*>(unit, game::kOptCategoryList) == g_saved.list) {
        field<std::uint8_t>(static_cast<char*>(g_saved.list), game::kListWrap) = g_saved.wrap;
    }
    g_saved.on = false;
}

BuildCategoriesFn build_categories(void* unit) {
    void* categories = static_cast<char*>(unit) + game::kOptCategories;
    void* const* vtable = field<void* const*>(categories, 0);
    void* fn = vtable != nullptr ? vtable[game::kBuildCategoriesSlot] : nullptr;
    if (reinterpret_cast<std::uintptr_t>(fn) != g_env.base + game::kRvaBuildCategories) {
        logf(AL_LOG_WARNING, "Mods page: the category builder is not where expected");
        return nullptr;
    }
    return reinterpret_cast<BuildCategoriesFn>(fn);
}

void set_header(void* unit, const char* text) {
    set_instance_text(unit, "a_title_options", "msg_title", text);
}

// ---- Split mode: the mod list and the panel of the highlighted tab ----

bool split_view() {
    return g_split.load(std::memory_order_relaxed) && g_view.load(std::memory_order_acquire);
}

// The mod list's rows: the tabs in its window, arrows on the edge rows when
// more lie beyond, the highlight on the selected tab.
void show_mod_list() {
    void* list = g_mod_list.load(std::memory_order_acquire);
    if (list == nullptr) {
        return;
    }
    const OptionsModel& model = menu::service_model();
    const std::size_t entries = g_entries.load(std::memory_order_relaxed);
    const std::size_t visible = field<std::uint32_t>(list, game::kListDuplicateNum);
    const std::size_t rows = entries < visible ? entries : visible;
    if (field<std::uint32_t>(list, game::kListRowCount) != rows) {
        cursor_to_top(list);
        gui_set_row_count(list, rows);
    }
    for (std::size_t r = 0; r < rows; ++r) {
        void* row = list_row(list, static_cast<std::uint32_t>(r));
        if (row == nullptr) {
            continue;
        }
        const char* label = model.tab_name(g_mod_top + r);
        if (r == 0 && g_mod_top > 0) {
            std::snprintf(g_label_up, sizeof g_label_up, "\xE2\x96\xB2 %s", label);  // up-pointing triangle
            label = g_label_up;
        } else if (r == rows - 1 && g_mod_top + rows < entries) {
            std::snprintf(g_label_down, sizeof g_label_down, "%s \xE2\x96\xBC", label);  // down-pointing triangle
            label = g_label_down;
        }
        set_row_text(row, "msg_1", nullptr, label);
        field<std::uint32_t>(row, game::kInstFlags) |= game::kInstFlagActive | game::kInstFlagEnabled;
        gui_set_play(row, true);
    }
    const std::size_t cursor = g_selected.load(std::memory_order_relaxed) - g_mod_top;
    if (cursor < rows) {
        gui_set_cursor(list, static_cast<std::uint32_t>(cursor));
        // Where the list's opening animation leaves the cursor.
        field<std::int32_t>(list, game::kListStartRow) = static_cast<std::int32_t>(cursor);
    }
}

// The panels show the selected tab again, the game's way (initCategory), and
// the audio panel is shown with its help line.
void fill_split_panel(void* unit) {
    const OptionsModel& model = menu::service_model();
    const std::size_t selected = g_selected.load(std::memory_order_relaxed);
    with_tables(model, [&] {
        prepare_tab(unit, game::kSplitPanel);
        g_init_category(tab_object(unit, game::kSplitPanel), unit, static_cast<std::int32_t>(game::kSplitPanel),
                        game::kTitleTabNames[game::kSplitPanel], model.tab_name(selected));
    });
    show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                         static_cast<std::int32_t>(game::kSplitPanel));
    decorate_panel(unit);
    request_split_help(unit);
}

// The help line for the highlighted mod, with one of CMFM's two ids in turn
// (mods_page_text answers both with the mod's description): the help bar
// redraws only when the id changes.
void request_split_help(void* unit) {
    void* manager = g_gui_manager != nullptr ? *g_gui_manager : nullptr;
    if (g_help_request == nullptr || manager == nullptr) {
        return;
    }
    g_help_flip ^= 1;
    g_help_request(manager, unit, static_cast<std::int32_t>(game::kSplitHelpIds[g_help_flip]),
                   static_cast<std::int32_t>(game::kHelpFile), 0.0f, 0);
}

// A number row of the split panel as a slider, the way the row filler makes
// the game's own volume rows one (sub_141E8E410): steps set first, the row
// marked so the game drives the slider every frame (keys, mouse, the setter
// through CMFM's hook on the 0-100 scale), the knob placed from the getter.
// CMFM's value label goes into the slider's text; the row's value box stays
// hidden. False if the row is no slider (not a number row, sliders off, or
// the panel's rows have none).
bool show_slider(char* row_object, const GameOptionRow& entry, std::uint32_t r, bool active) {
    const auto id = static_cast<std::uint32_t>(entry.setting_id);
    const std::uint32_t steps = menu::service_slider_steps(id);
    void* slider = field<void*>(row_object, game::kRowSlider);
    if (steps == 0 || slider == nullptr || g_slider_from_setting == nullptr) {
        return false;
    }
    if (field<std::uint8_t>(row_object, game::kRowIsSlider) == 0) {
        field<std::int32_t>(slider, game::kSliderSteps) = static_cast<std::int32_t>(steps);
        field<std::uint8_t>(row_object, game::kRowIsSlider) = 1;
        g_slider_from_setting(slider, nullptr, &entry);
        field<std::uint32_t>(slider, game::kSliderMode) = 2;
        // Set from the getter marks the slider active; the row look keeps it
        // to the panel's cursor row (run 20261011-sliders-1: green knobs while
        // the list had the focus).
        field<std::uint8_t>(slider, game::kSliderActive) = active ? 1 : 0;
        gui_set_play(slider, true);
    }
    const auto index = static_cast<std::uint32_t>(menu::service_get(id));
    g_slider_rows[g_slider_count++] = SliderRow{r, steps, index};
    if (void* end = field<void*>(row_object, game::kRowSliderEnd)) {
        gui_set_hidden(end, true);
    }
    if (void* text = field<void*>(row_object, game::kRowSliderText)) {
        const char* label =
            menu::service_model().label_text(static_cast<std::uint32_t>(entry.label_id) + 1 + index);
        gui_format_text(text, label != nullptr ? label : "");
        gui_set_hidden(text, false);
    }
    return true;
}

// The slider's knob as a fraction of its track (sub_140B8A190).
float slider_position(void* slider) {
    const float min = field<float>(slider, game::kSliderMin);
    const float range = field<float>(slider, game::kSliderMax) - min;
    const char* knob = field<const char*>(slider, game::kSliderKnob);
    if (!(range > 1e-6f) || knob == nullptr) {
        return 0.0f;
    }
    const float at = field<std::int32_t>(slider, game::kSliderOrientation) == 0 ? *reinterpret_cast<const float*>(knob + 160)
                                                                                 : *reinterpret_cast<const float*>(knob + 164);
    return (at - min) / range;
}

// After the value changed (labels follow) and after the mouse lets go of a
// knob between two steps (it snaps to the value it set; the keys move it by
// steps already).
void update_sliders(void* unit) {
    if (g_slider_count == 0) {
        return;
    }
    if (menu::service_value_revision() != g_value_seen) {
        decorate_panel(unit);
    }
    char* tab = tab_object(unit, game::kSplitPanel);
    for (std::size_t i = 0; i < g_slider_count; ++i) {
        const SliderRow& s = g_slider_rows[i];
        char* row_object = tab + (s.row + 1) * game::kRowObjectSize;
        void* slider = field<void*>(row_object, game::kRowSlider);
        if (slider == nullptr || field<std::uint8_t>(row_object, game::kRowIsSlider) == 0 ||
            field<std::uint8_t>(slider, game::kSliderDragging) != 0) {
            continue;
        }
        const float target = static_cast<float>(s.index) / static_cast<float>(s.steps);
        const float off = slider_position(slider) - target;
        if (off > 0.002f || off < -0.002f) {
            g_slider_set_position(slider, target);
        }
    }
}

// What the game's row filler does not know about CMFM's rows, after every
// fill of the split panel, focus change and restart state change: section
// headings the cursor skips (the row's "selectable" flag off, as the list's
// moves and hover test it) and without a value box, and the restart mark
// (the row's star icon) on settings waiting for a restart.
void decorate_panel(void* unit) {
    char* tab = tab_object(unit, game::kSplitPanel);
    void* list = field<void*>(tab, game::kTabList);
    if (list == nullptr) {
        return;
    }
    const OptionsModel& model = menu::service_model();
    const std::size_t selected = g_selected.load(std::memory_order_relaxed);
    const GameOptionRow* rows = model.table(selected);
    const std::uint32_t count = field<std::uint32_t>(list, game::kListRowCount);
    std::int32_t first = -1;  // the first row the cursor may take
    std::uint32_t known = 0;  // rows with a table entry
    // The row the panel's cursor is on, while the panel has the focus.
    const std::uint32_t active_row = field<std::int32_t>(unit, game::kOptFocus) == 1
                                         ? field<std::uint32_t>(list, game::kListCursor)
                                         : 0xFFFFFFFFu;
    g_slider_count = 0;
    g_value_seen = menu::service_value_revision();
    for (std::uint32_t r = 0; r < count && rows[r].kind != kRowTableEnd; ++r) {
        known = r + 1;
        void* row = list_row(list, r);
        if (row == nullptr) {
            continue;
        }
        const auto id = static_cast<std::uint32_t>(rows[r].setting_id);
        const bool section = menu::service_is_section(id);
        std::uint32_t& flags = field<std::uint32_t>(row, game::kInstFlags);
        flags = section ? flags & ~game::kInstFlagActive : flags | game::kInstFlagActive;
        if (!section && first < 0) {
            first = static_cast<std::int32_t>(r);
        }
        char* row_object = tab + (r + 1) * game::kRowObjectSize;
        const bool slider = show_slider(row_object, rows[r], r, r == active_row);
        if (!slider && g_set_focus != nullptr && menu::service_list_choices(id) != 0) {
            field<std::uint8_t>(row_object, game::kRowOpensList) = 1;  // Confirm opens the pick list
        }
        void* root = field<void*>(row, game::kInstRoot);
        if (void* star = root != nullptr ? gui_get_object(root, game::kRowStar) : nullptr) {
            gui_set_hidden(star, !menu::service_restart_pending(id));
        }
        if (void* value = root != nullptr ? gui_get_object(root, game::kRowValueText) : nullptr) {
            gui_set_hidden(value, section || slider);  // a heading has no value box, a slider its own text
        }
    }
    if (first >= 0) {
        const std::uint32_t cursor = field<std::uint32_t>(list, game::kListCursor);
        if (cursor < known && menu::service_is_section(static_cast<std::uint32_t>(rows[cursor].setting_id))) {
            gui_set_cursor(list, static_cast<std::uint32_t>(first));
        }
        field<std::int32_t>(list, game::kListStartRow) = first;
    }
    // The row looks' rule for a cursor row that opens a list (sub_141E8E600;
    // on a fresh fill they ran before the pick rows were flagged): Confirm on,
    // arrows off, and its list shown while the panel or the list has the focus.
    const std::uint32_t cursor = field<std::uint32_t>(list, game::kListCursor);
    if (cursor < known && field<std::uint8_t>(tab + (cursor + 1) * game::kRowObjectSize, game::kRowOpensList) != 0) {
        field<std::uint8_t>(list, game::kListTakesConfirm) = 1;
        if (void* arrows = field<void*>(tab, game::kTabNumInput)) {
            gui_set_play(arrows, false);
        }
        const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
        if (focus == 1 || focus == game::kFocusPickList) {
            show_pick_list(unit, rows[cursor]);
        }
    }
}

// The line above the mod list: empty unless changes wait for a restart.
void show_restart_line(void* unit) {
    const std::size_t n = menu::service_restart_count();
    if (n == 0) {
        g_restart_line[0] = '\0';
    } else {
        std::snprintf(g_restart_line, sizeof g_restart_line,
                      "<STYL MOJI_YELLOW_DEFAULT>Restart the game to apply %zu change%s</STYL>", n, n == 1 ? "" : "s");
    }
    set_instance_text(unit, game::kRestartLineName, game::kTitleText, g_restart_line);
}

void select_tab(void* unit, std::size_t index) {
    void* list = g_mod_list.load(std::memory_order_acquire);
    const std::size_t entries = g_entries.load(std::memory_order_relaxed);
    if (list == nullptr || index >= entries || index == g_selected.load(std::memory_order_relaxed)) {
        return;
    }
    const std::size_t visible = field<std::uint32_t>(list, game::kListDuplicateNum);
    if (index < g_mod_top) {
        g_mod_top = index;
    } else if (index >= g_mod_top + visible) {
        g_mod_top = index - visible + 1;
    }
    g_selected.store(index, std::memory_order_relaxed);
    show_mod_list();
    fill_split_panel(unit);
}

// Fills CMFM's page with CMFM's menu: the status tab and the plugin pages,
// at most five category rows (more scroll), so Key Config and Default (a
// full reset) stay out of reach. False if anything is off; the caller closes
// the page then.
bool enter_view(void* unit) {
    BuildCategoriesFn build = build_categories(unit);
    void* categories = field<void*>(unit, game::kOptCategoryList);
    if (build == nullptr || categories == nullptr) {
        return false;
    }
    const OptionsModel* model = menu::service_prepare();  // CMFM's texts on from here
    if (model == nullptr) {
        return false;
    }
    stop_window(unit);
    g_top.store(0, std::memory_order_relaxed);
    g_entries.store(menu::service_used_tabs(), std::memory_order_relaxed);
    void* mod_list = nullptr;
    if (g_split.load(std::memory_order_relaxed)) {
        mod_list = gui_get_instance(unit, game::kModListName);
        if (mod_list == nullptr || field<void*>(mod_list, 0) != field<void*>(categories, 0) ||
            field<std::uint32_t>(mod_list, game::kListDuplicateNum) == 0) {
            logf(AL_LOG_WARNING, "Mods page: the layout has no mod list (%s)", game::kModListName);
            menu::service_abort();
            return false;
        }
        // The last selection again, if it still exists.
        if (g_selected.load(std::memory_order_relaxed) >= g_entries.load(std::memory_order_relaxed)) {
            g_selected.store(0, std::memory_order_relaxed);
            g_mod_top = 0;
        }
    }
    if (!prepare_rebuild(unit)) {
        menu::service_abort();
        return false;
    }
    g_view.store(true, std::memory_order_release);  // the text hook answers the category labels
    if (!with_tables(*model, [&] { build(static_cast<char*>(unit) + game::kOptCategories, unit); })) {
        g_view.store(false, std::memory_order_release);
        menu::service_abort();
        return false;
    }
    const std::size_t entries = g_entries.load(std::memory_order_relaxed);
    const std::uint32_t used =
        static_cast<std::uint32_t>(entries < game::kTitleViewTabs ? entries : game::kTitleViewTabs);
    if (field<std::uint32_t>(categories, game::kListRowCount) < game::kTitleViewTabs) {
        logf(AL_LOG_WARNING, "Mods page: only %u categories", field<std::uint32_t>(categories, game::kListRowCount));
        g_view.store(false, std::memory_order_release);
        menu::service_abort();
        return false;
    }
    gui_set_row_count(categories, used);
    for (std::uint32_t t = 0; t < used; ++t) {
        set_row_text(list_row(categories, t), "msg_1", nullptr, model->tab_name(t));
    }
    if (field<std::uint32_t>(categories, game::kListRowCount) != used) {
        logf(AL_LOG_WARNING, "Mods page: the category list did not shrink");
        g_view.store(false, std::memory_order_release);
        menu::service_abort();
        return false;
    }
    if (mod_list != nullptr) {
        // The category list keeps its cursor on the audio panel (set after the
        // game's open handler, and where the list's opening animation leaves
        // it); Up/Down on it move the mod list instead.
        field<std::int32_t>(categories, game::kListStartRow) = static_cast<std::int32_t>(game::kSplitPanel);
        g_cursor_warned = false;
        g_wheel.store(0, std::memory_order_relaxed);
        g_wheel_rest = 0;
        g_view_unit.store(unit, std::memory_order_release);
        g_window_list.store(categories, std::memory_order_release);
        g_mod_list.store(mod_list, std::memory_order_release);
        // CMFM drives the mod list (the keys through the category list, the
        // mouse in track_mouse); the game never set it up, so it must not take
        // input itself.
        field<std::uint8_t>(mod_list, game::kListInactive) = 1;
        options_view_sliders(g_slider_from_setting != nullptr);  // number rows as sliders
        g_pick_count = 0;
        g_pick_filled = -1;
        const std::size_t visible = field<std::uint32_t>(mod_list, game::kListDuplicateNum);
        if (g_selected.load(std::memory_order_relaxed) >= g_mod_top + visible) {
            g_mod_top = g_selected.load(std::memory_order_relaxed) - visible + 1;
        }
        show_mod_list();
        logf(AL_LOG_INFO, "Mods page: mod list on (%zu tabs, %zu rows visible)", entries, visible);
    } else if (entries > game::kTitleViewTabs) {
        start_window(unit, categories);
    }
    set_header(unit, model->header());
    logf(AL_LOG_INFO, "Mods page: on (%zu categories, %zu rows)", entries, model->row_count());
    return true;
}

// CMFM's page, vtable slot 90: the open handler.
std::uint64_t page_open(void* unit) {
    if (g_view.exchange(false, std::memory_order_acq_rel)) {
        menu::service_closed();  // the close handler did not run
    }
    const bool on = enter_view(unit);
    const std::uint64_t result = page_next(game::kSlotOpen)(unit);
    if (on && g_mod_list.load(std::memory_order_acquire) != nullptr) {
        // The game's open put the category cursor on row 0.
        gui_set_cursor(field<void*>(unit, game::kOptCategoryList), static_cast<std::uint32_t>(game::kSplitPanel));
        show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                             static_cast<std::int32_t>(game::kSplitPanel));
        show_mod_list();
        g_restart_seen = menu::service_restart_revision();
        g_decorated_focus = field<std::int32_t>(unit, game::kOptFocus);
        decorate_panel(unit);
        show_restart_line(unit);
        request_split_help(unit);
    }
    if (!on) {
        logf(AL_LOG_WARNING, "Mods page: CMFM's page could not be filled; closing it");
        gui_request_close(unit);
    }
    return result;
}

// CMFM's page, vtable slot 91: the close handler, on the first frame of
// closing (also when the game forces pages shut at a scene change). The rows
// stay on screen while the page closes; the next open fills it again.
std::uint64_t page_close(void* unit) {
    const std::uint64_t result = page_next(game::kSlotClose)(unit);
    g_hold_player.store(false, std::memory_order_relaxed);  // slot 81 stops running here too
    g_mod_list.store(nullptr, std::memory_order_release);
    g_slider_count = 0;
    g_pick_count = 0;
    g_pick_filled = -1;
    options_view_sliders(false);
    if (g_view.exchange(false, std::memory_order_acq_rel)) {
        menu::service_closed();
        logf(AL_LOG_INFO, "Mods page: closed");
    }
    // Back to the Start menu, as the game's return stack does on the same
    // frame after its own Options; only while the menu still counts a page of
    // its own as open (a scene change kills the menu first).
    void* menu = game_ref_alive(g_return_menu);
    game_ref_set(g_return_menu, nullptr);
    if (menu != nullptr && field<std::uint8_t>(menu, game::kStartMenuChildOpen) != 0) {
        gui_request_open(menu, nullptr);
        logf(AL_LOG_INFO, "Mods page: back to the Start menu");
    }
    return result;
}

// The game shows its VRAM panel with the Display category (category 1); the
// Mods page has no use for it.
std::uint64_t show_category_detour(void* categories, std::int32_t category) {
    const std::uint64_t result = g_show_category_next(categories, category);
    void* unit = field<void*>(categories, game::kCategoriesUnit);
    if (g_view.load(std::memory_order_acquire) && unit != nullptr && unit == page_alive()) {
        void* vram = field<void*>(unit, game::kOptVramPanel);
        if (vram != nullptr) {
            gui_set_play(vram, false);
        }
    }
    return result;
}

// The row looks end every change of the panel's rows (after the tab refresh
// has shown the value boxes again, and they hide the pick list of any row
// that is not a language row): on the split panel of CMFM's page the
// headings, marks, sliders and the pick list are redone before anything is
// drawn.
std::uint64_t row_looks_detour(void* tab, std::uint32_t cursor) {
    const std::uint64_t result = g_row_looks_next(tab, cursor);
    void* unit = g_view_unit.load(std::memory_order_acquire);
    if (unit != nullptr && split_view() && tab == tab_object(unit, game::kSplitPanel)) {
        decorate_panel(unit);
    }
    return result;
}

// The pick list for a CHOICE row of CMFM's page, filled as the language fill
// (RVA 0x1E8CFA0) fills it: the rows, their texts, the window and its line
// sized to them; the check mark on the current choice. Filled again only for
// another row. Returns the current choice, or -1 if the row has no pick list.
std::int32_t fill_pick_list(void* unit, const GameOptionRow& entry) {
    const auto id = static_cast<std::uint32_t>(entry.setting_id);
    const std::uint32_t n = menu::service_list_choices(id);
    char* select = static_cast<char*>(unit) + game::kOptSelect;
    void* list = field<void*>(select, game::kSelectList);
    if (n == 0 || n > game::kSelectRows || list == nullptr || g_set_focus == nullptr) {
        return -1;
    }
    const auto current = static_cast<std::uint32_t>(menu::service_get(id));
    if (g_pick_filled != static_cast<std::int32_t>(id) || field<std::uint32_t>(list, game::kListRowCount) != n) {
        field<std::int32_t>(select, game::kSelectSetting) = static_cast<std::int32_t>(id);
        cursor_to_top(list);
        gui_set_row_count(list, n);
        if (void* window = field<void*>(select, game::kSelectWindow)) {
            g_set_scale_y(window, static_cast<float>(n) * game::kWindowRowScale);
        }
        if (void* line = field<void*>(select, game::kSelectLine)) {
            g_set_current_frame(line, static_cast<float>(n), true);
        }
        std::memset(select + game::kSelectChecks, 0, game::kSelectRows * sizeof(void*));
        const OptionsModel& model = menu::service_model();
        for (std::uint32_t r = 0; r < n; ++r) {
            void* row = list_row(list, r);
            void* root = row != nullptr ? field<void*>(row, game::kInstRoot) : nullptr;
            if (void* text = root != nullptr ? gui_get_object(root, game::kPickText) : nullptr) {
                const char* label = model.label_text(static_cast<std::uint32_t>(entry.label_id) + 1 + r);
                gui_format_text(text, label != nullptr ? label : "");
            }
            g_pick_checks[r] = root != nullptr ? gui_get_object(root, game::kPickCheck) : nullptr;
        }
        g_pick_count = n;
        g_pick_filled = static_cast<std::int32_t>(id);
    }
    for (std::uint32_t r = 0; r < g_pick_count; ++r) {
        if (g_pick_checks[r] != nullptr) {
            gui_set_hidden(g_pick_checks[r], r != current);
        }
    }
    return static_cast<std::int32_t>(current);
}

// Confirm on a pick row: the focus on its list (shown already while the
// cursor is on the row), the cursor on the current choice.
bool open_pick_list(void* unit, const GameOptionRow& entry) {
    const std::int32_t current = fill_pick_list(unit, entry);
    if (current < 0) {
        return false;
    }
    g_set_focus(unit, game::kFocusPickList);  // puts the cursor on row 0
    void* list = field<void*>(static_cast<char*>(unit) + game::kOptSelect, game::kSelectList);
    if (static_cast<std::uint32_t>(current) < g_pick_count) {
        gui_set_cursor(list, static_cast<std::uint32_t>(current));
    }
    logf(AL_LOG_INFO, "Mods page: pick list opened (setting %d, %u choices)", entry.setting_id, g_pick_count);
    return true;
}

// While the panel's cursor is on a pick row its list shows next to the panel,
// as the game's language rows show theirs (the row looks play the same two
// parts for a language row); Confirm then gives it the focus.
void show_pick_list(void* unit, const GameOptionRow& entry) {
    if (fill_pick_list(unit, entry) < 0) {
        return;
    }
    char* select = static_cast<char*>(unit) + game::kOptSelect;
    if (void* shown = field<void*>(select, game::kSelectShown)) {
        gui_set_play(shown, true);
    }
    if (void* list = field<void*>(select, game::kSelectList)) {
        gui_set_play(list, true);
    }
}

// Confirm on a panel row: the pick list for CMFM's long CHOICE rows (the
// game's own decide does something only for its language and dialog rows).
void row_decide_detour(void* row) {
    const auto* entry = field<const GameOptionRow*>(row, game::kRowDef);
    void* unit = field<void*>(row, game::kRowObjectUnit);
    if (entry != nullptr && unit != nullptr && split_view() && unit == g_view_unit.load(std::memory_order_acquire) &&
        is_cmfm_row(entry->kind, static_cast<std::uint32_t>(entry->setting_id)) && open_pick_list(unit, *entry)) {
        return;
    }
    g_row_decide_next(row);
}

// A pick in the pick list: CMFM's settings get the value; the game's handler
// then closes the list as for any setting that is not a language.
std::uint64_t select_decide_detour(void* select, std::uint32_t index) {
    const std::int32_t id = field<std::int32_t>(select, game::kSelectSetting);
    if (g_view.load(std::memory_order_acquire) && id >= 0 &&
        is_cmfm_row(kCmfmRowKind, static_cast<std::uint32_t>(id)) && index < g_pick_count) {
        logf(AL_LOG_INFO, "Mods page: picked choice %u for setting %d", index, id);
        menu::service_set(static_cast<std::uint32_t>(id), static_cast<std::int32_t>(index));
        for (std::uint32_t r = 0; r < g_pick_count; ++r) {
            if (g_pick_checks[r] != nullptr) {
                gui_set_hidden(g_pick_checks[r], r != index);
            }
        }
    }
    return g_select_decide_next(select, index);
}

// The target of a rip-relative operand whose rel32 ends `end` bytes into
// `code` (rel32 at end - 4).
std::uintptr_t rip_target(const std::uint8_t* code, std::size_t end) {
    std::int32_t rel = 0;
    std::memcpy(&rel, code + end - 4, sizeof rel);
    return reinterpret_cast<std::uintptr_t>(code) + end + rel;
}

bool find_input_lock(const Env& env, std::string& why) {
    void* match = nullptr;
    if (!find_game_function(env, "uGUIMenuOption input lock", game::kOptionsInputLockPattern, &match, why)) {
        return false;
    }
    const auto* code = static_cast<const std::uint8_t*>(match);
    const std::uintptr_t input = rip_target(code, game::kInputLockInput1 + 7);
    const std::uintptr_t lock = rip_target(code, game::kInputLockCall1 + 5);
    if (rip_target(code, game::kInputLockInput2 + 7) != input || rip_target(code, game::kInputLockCall2 + 5) != lock) {
        why = "uGUIMenuOption's input lock differs";
        return false;
    }
    g_input_manager = reinterpret_cast<void* const*>(input);
    g_lock_channel = reinterpret_cast<LockChannelFn>(lock);
    g_area_manager = reinterpret_cast<void* const*>(rip_target(code, game::kInputLockArea + 7));
    return true;
}

// The page's mouse helpers, from the calls in uGUIOption's click check.
bool find_mouse(const Env& env, std::string& why) {
    void* match = nullptr;
    if (!find_game_function(env, "uGUIOption click check", game::kOptionClickPattern, &match, why) ||
        !find_game_function(env, "mouse moved", game::kMouseMovedPattern, reinterpret_cast<void**>(&g_mouse_moved),
                            why)) {
        g_mouse_moved = nullptr;
        return false;
    }
    const auto* code = static_cast<const std::uint8_t*>(match);
    g_mouse_position = reinterpret_cast<MousePositionFn>(rip_target(code, game::kOptionClickPosCall + 5));
    g_mouse_hit = reinterpret_cast<MouseHitFn>(rip_target(code, game::kOptionClickHitCall + 5));
    g_mouse_click = reinterpret_cast<MouseFlagFn>(rip_target(code, game::kOptionClickPressCall + 5));
    return true;
}

// The two slider functions CMFM calls; both or neither.
bool find_sliders(const Env& env, std::string& why) {
    if (!find_game_function(env, "slider from setting", game::kSliderFromSettingPattern,
                            reinterpret_cast<void**>(&g_slider_from_setting), why) ||
        !find_game_function(env, "slider position", game::kSliderSetPositionPattern,
                            reinterpret_cast<void**>(&g_slider_set_position), why)) {
        g_slider_from_setting = nullptr;
        g_slider_set_position = nullptr;
        return false;
    }
    return true;
}

// The help line request and the GUI manager, from showCategory's call.
bool find_help_request(const Env& env, std::string& why) {
    void* match = nullptr;
    if (!find_game_function(env, "showCategory help request", game::kHelpRequestPattern, &match, why)) {
        return false;
    }
    const auto* code = static_cast<const std::uint8_t*>(match);
    g_gui_manager = reinterpret_cast<void* const*>(rip_target(code, game::kHelpRequestManager + 7));
    g_help_request = reinterpret_cast<HelpRequestFn>(rip_target(code, game::kHelpRequestCall + 5));
    return true;
}

}  // namespace

bool mods_page_register(const Env& env, std::string& why) {
    g_env = env;
    if (!gui_register(env, why) ||
        !find_game_function(env, "initCategory", game::kInitCategoryPattern,
                            reinterpret_cast<void**>(&g_init_category), why) ||
        !page_class_register(env, why) || !find_input_lock(env, why) ||
        !find_game_function(env, "sMhResource::loadResource", game::kLoadResourcePattern,
                            reinterpret_cast<void**>(&g_load_resource), why)) {
        return false;
    }
    struct Spec {
        const char* name;
        const char* pattern;
        void* detour;
        void** next;
    };
    const Spec specs[kHookCount] = {
        {"CMFM:uGUIOption::showCategory", game::kTitleShowCategoryPattern,
         reinterpret_cast<void*>(&show_category_detour), reinterpret_cast<void**>(&g_show_category_next)},
        {"CMFM:cGUIInstButtonList::moveCursor", game::kListMovePattern, reinterpret_cast<void*>(&list_move_detour),
         reinterpret_cast<void**>(&g_list_move_next)},
        {"CMFM:uGUIOption::rowLooks", game::kRowLooksPattern, reinterpret_cast<void*>(&row_looks_detour),
         reinterpret_cast<void**>(&g_row_looks_next)},
        {"CMFM:uGUIOption::decideRow", game::kRowDecidePattern, reinterpret_cast<void*>(&row_decide_detour),
         reinterpret_cast<void**>(&g_row_decide_next)},
        {"CMFM:uGUIOption::decidePick", game::kSelectDecidePattern, reinterpret_cast<void*>(&select_decide_detour),
         reinterpret_cast<void**>(&g_select_decide_next)},
    };
    for (int i = 0; i < kHookCount; ++i) {
        al_hook_t h = {sizeof(al_hook_t)};
        h.name = specs[i].name;
        h.pattern = specs[i].pattern;
        h.detour = specs[i].detour;
        h.next = specs[i].next;
        g_hooks[i] = env.hooks->hook(env.self, &h);
        if (g_hooks[i] == 0) {
            for (int j = 0; j < i; ++j) {
                env.hooks->set_enabled(env.self, g_hooks[j], 0);
            }
            why = std::string("the hook ") + specs[i].name + " was refused (see the lines above)";
            return false;
        }
    }
    logf(AL_LOG_INFO,
         "Mods page: hooks registered (Options show category; list cursor moves; row looks; row and pick decide)");
    std::string mouse_why;
    if (!find_mouse(env, mouse_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; the mod list will not follow the mouse", mouse_why.c_str());
    }
    std::string help_why;
    if (!find_help_request(env, help_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; the help line may keep the previous mod's text", help_why.c_str());
    }
    std::string slider_why;
    if (!find_sliders(env, slider_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; number rows keep their arrows", slider_why.c_str());
    }
    std::string pick_why;
    if (!find_game_function(env, "page focus", game::kSetFocusPattern, reinterpret_cast<void**>(&g_set_focus),
                            pick_why) ||
        !find_game_function(env, "setScaleY", game::kSetScaleYPattern, reinterpret_cast<void**>(&g_set_scale_y),
                            pick_why) ||
        !find_game_function(env, "setCurrentFrame", game::kSetCurrentFramePattern,
                            reinterpret_cast<void**>(&g_set_current_frame), pick_why)) {
        g_set_focus = nullptr;
        logf(AL_LOG_WARNING, "Mods page: %s; long choices keep their arrows", pick_why.c_str());
    }
    // The wheel scrolls a long category list; without al.input it does not.
    if (env.input == nullptr || env.input->subscribe_messages(env.self, 0, &message_callback, nullptr) != AL_OK) {
        logf(AL_LOG_WARNING, "Mods page: no window messages from al.input; the mouse wheel will not scroll the list");
    }
    return true;
}

bool mods_page_check(const Env& env, std::string& why) {
    for (const std::uint32_t h : g_hooks) {
        if (env.hooks->state(env.self, h) != AL_HOOK_ACTIVE) {
            why = "the Mods page hooks are not active";
            return false;
        }
    }
    if (!options_view_ready()) {
        why = "the Mods rows are not ready (see the options view lines)";
        return false;
    }
    for (std::size_t t = 0; t < game::kTitleOptionTabTableCount; ++t) {
        const std::uint64_t found = *reinterpret_cast<const std::uint64_t*>(title_tab_slot(t));
        const std::uint64_t expected = env.base + game::kRvaTitleOptionRows[t];
        if (found != expected) {
            char b[128];
            std::snprintf(b, sizeof b, "title Options tab %zu table is 0x%llx, expected 0x%llx", t,
                          static_cast<unsigned long long>(found), static_cast<unsigned long long>(expected));
            why = b;
            return false;
        }
    }
    if (!page_class_check(env, PageHandlers{&page_update, &page_open, &page_close}, why)) {
        return false;
    }
    g_ready.store(true, std::memory_order_release);
    logf(AL_LOG_INFO, "Mods page: ready");
    return true;
}

namespace {

// Loads CMFM's own layout through the game's resource manager before the page
// exists; only if the game finds it does the page use it (a missing layout
// would leave the page without its parts).
void use_own_layout() {
    void* resources = *reinterpret_cast<void* const*>(g_env.base + game::kRvaResourceManager);
    const auto* resource_class = reinterpret_cast<const char* const*>(g_env.base + game::kRvaGuiResourceClass);
    if (resources == nullptr ||
        *static_cast<const std::uintptr_t*>(resources) != g_env.base + game::kRvaResourceManagerVtable ||
        static_cast<void* const*>(*static_cast<void* const* const*>(resources))[game::kLoadResourceSlot] !=
            reinterpret_cast<void*>(g_load_resource) ||
        resource_class[1] == nullptr || std::strcmp(resource_class[1], game::kGuiResourceClassName) != 0) {
        logf(AL_LOG_WARNING, "Mods page: the resource manager is not as expected; the page uses the game's layout");
        return;
    }
    g_layout_resource = g_load_resource(resources, resource_class, game::kModsLayout, game::kLoadFromFile);
    if (g_layout_resource == nullptr) {
        logf(AL_LOG_WARNING, "Mods page: layout %s not found (nativePC); the page uses the game's layout (%s)",
             game::kModsLayout, page_layout());
        return;
    }
    page_set_layout(game::kModsLayout);
    g_split.store(true, std::memory_order_relaxed);
    logf(AL_LOG_INFO, "Mods page: layout %s loaded (CMFM's own; mod list and one panel)", game::kModsLayout);
}

}  // namespace

bool mods_page_ready() {
    return g_ready.load(std::memory_order_acquire);
}

void* mods_page_ensure() {
    if (!g_ready.load(std::memory_order_acquire)) {
        return nullptr;
    }
    void* page = page_alive();
    if (page != nullptr || g_create_failed) {
        return page;
    }
    use_own_layout();
    std::string why;
    page = page_create(why);
    if (page == nullptr) {
        g_create_failed = true;
        logf(AL_LOG_WARNING, "Mods page: CMFM's page could not be made (%s); Mods opens the Options page",
             why.c_str());
        return nullptr;
    }
    ++g_pages_created;
    logf(AL_LOG_INFO, "Mods page: CMFM's page made (unit %p, page %u, kept for the session)", page, g_pages_created);
    return page;
}

bool mods_page_open_from_start_menu(void* menu) {
    if (!g_ready.load(std::memory_order_acquire)) {
        return false;
    }
    void* page = page_alive();
    if (page == nullptr || !unit_running(page) || menu == nullptr ||
        field<std::uintptr_t>(menu, 0) != g_env.base + game::kRvaStartMenuVtable) {
        return false;
    }
    // What the Start menu's own Options action (onItemAction case 71) does,
    // with CMFM's page for the game's Options page: open the page, mark a page
    // of the menu's own as open (the game stays in menu mode, so the hunter
    // does not move; sMhGUI+83664) and close the menu. The page's close
    // handler opens the menu again.
    field<std::int32_t>(menu, game::kStartMenuDialog) = game::kStartMenuNoDialog;
    g_hold_player.store(true, std::memory_order_relaxed);
    gui_request_open(page, nullptr);
    game_ref_set(g_return_menu, menu);
    field<std::uint8_t>(menu, game::kStartMenuChildOpen) = 1;
    gui_request_close(menu);
    field<std::int32_t>(menu, game::kStartMenuAction) = -1;
    ++g_opened_in_game;
    logf(AL_LOG_INFO, "Start menu: Mods opens CMFM's page");
    return true;
}

const char* mods_page_text(void* message_file_ptr, std::uint32_t id) {
    if (!g_view.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    if (message_file_ptr == message_file(game::kRvaMessages12)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryLabelIds[t]) {
                return menu::service_model().tab_name(menu_tab(t));
            }
        }
    } else if (message_file_ptr == message_file(game::kRvaMessages18)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryHelpIds[t]) {
                return menu::service_model().tab_help(menu_tab(t));
            }
        }
        if (id == game::kSplitHelpIds[0] || id == game::kSplitHelpIds[1]) {
            return menu::service_model().tab_help(g_selected.load(std::memory_order_relaxed));
        }
    }
    return nullptr;
}

void mods_page_restore(const Env&) {
    if (g_env.api == nullptr) {
        return;  // never registered
    }
    g_ready.store(false, std::memory_order_release);
    if (g_view.exchange(false, std::memory_order_acq_rel)) {
        menu::service_closed();
    }
    restore_tables();
    game_ref_set(g_return_menu, nullptr);
    page_release();
    logf(AL_LOG_INFO, "Mods page: done; %u page(s) made, opened from the Start menu %u time(s)", g_pages_created,
         g_opened_in_game);
}

}  // namespace cmfm::native
