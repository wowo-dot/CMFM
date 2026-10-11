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
// Called.
using InitCategoryFn = std::uint64_t (*)(void* tab, void* unit, std::int32_t index, const char* name,
                                         const char* title);
using BuildCategoriesFn = void (*)(void* categories, void* unit);
using LockChannelFn = std::uint64_t (*)(void* input, std::int32_t channel, std::uint32_t player);
using LoadResourceFn = void* (*)(void* resources, const void* resource_class, const char* path, std::uint32_t flags);

ShowCategoryFn g_show_category_next = nullptr;
ListMoveFn g_list_move_next = nullptr;
InitCategoryFn g_init_category = nullptr;
// From uGUIMenuOption::update: the input manager and area manager globals and
// the input lock.
void* const* g_input_manager = nullptr;
void* const* g_area_manager = nullptr;
LockChannelFn g_lock_channel = nullptr;
LoadResourceFn g_load_resource = nullptr;
// CMFM's layout as loaded once (kept for the session, like the page).
void* g_layout_resource = nullptr;

enum Hook { kShowCategory, kListMove, kHookCount };
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

// Runs `build` with TitleOptionTabTables[t] pointing at CMFM's tab top + t:
// the game reads those tables only while it fills a category's rows
// (fillCategoryRows), so the game's own Options pages never see CMFM's.
template <class Build>
bool with_tables(const OptionsModel& model, std::size_t top, Build build) {
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        std::string why;
        const std::uint64_t expected = g_env.base + game::kRvaTitleOptionRows[t];
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(model.table(top + t));
        if (!g_tabs[t].apply("title Options tab", title_tab_slot(t), expected, ours, why)) {
            logf(AL_LOG_WARNING, "Mods page: could not point tab %zu at CMFM's tab %zu: %s", t, top + t,
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
    const bool filled = with_tables(model, top, [&] {
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
    if (message != kMouseWheel || !scrolling()) {
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

// CMFM's page, vtable slot 81: every frame while it is open (state 2; the
// menu's thread).
std::uint64_t page_update(void* unit) {
    if (g_hold_player.load(std::memory_order_relaxed)) {
        hold_player(unit);
    }
    if (g_wheel.load(std::memory_order_relaxed) != 0) {
        const int delta = g_wheel.exchange(0, std::memory_order_relaxed) + g_wheel_rest;
        constexpr int kNotch = 120;  // WHEEL_DELTA
        const int steps = delta / kNotch;
        g_wheel_rest = delta % kNotch;
        if (steps != 0 && scrolling() && unit == g_view_unit.load(std::memory_order_acquire)) {
            const std::size_t top = g_top.load(std::memory_order_relaxed);
            const std::size_t last_top = g_entries.load(std::memory_order_relaxed) - game::kTitleViewTabs;
            // Wheel up (positive) shows earlier tabs; no wrap.
            std::int64_t next = static_cast<std::int64_t>(top) - steps;
            next = next < 0 ? 0 : next > static_cast<std::int64_t>(last_top) ? static_cast<std::int64_t>(last_top) : next;
            scroll_to(unit, static_cast<std::size_t>(next));
        }
    }
    return page_next(game::kSlotUpdate)(unit);
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
    void* instance = gui_get_instance(unit, "a_title_options");
    void* root = instance != nullptr ? field<void*>(instance, game::kInstRoot) : nullptr;
    void* object = root != nullptr ? gui_get_object(root, "msg_title") : nullptr;
    if (object != nullptr) {
        gui_format_text(object, text);
    }
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
    if (!prepare_rebuild(unit)) {
        menu::service_abort();
        return false;
    }
    g_view.store(true, std::memory_order_release);  // the text hook answers the category labels
    if (!with_tables(*model, 0, [&] { build(static_cast<char*>(unit) + game::kOptCategories, unit); })) {
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
    if (entries > game::kTitleViewTabs) {
        start_window(unit, categories);
    }
    set_header(unit, model->header());
    menu::service_opened();
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
    logf(AL_LOG_INFO, "Mods page: hooks registered (Options show category; list cursor moves)");
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
    logf(AL_LOG_INFO, "Mods page: layout %s loaded (CMFM's own)", game::kModsLayout);
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
    const std::size_t top = g_top.load(std::memory_order_relaxed);
    if (message_file_ptr == message_file(game::kRvaMessages12)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryLabelIds[t]) {
                return menu::service_model().tab_name(top + t);
            }
        }
    } else if (message_file_ptr == message_file(game::kRvaMessages18)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryHelpIds[t]) {
                return menu::service_model().tab_help(top + t);
            }
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
