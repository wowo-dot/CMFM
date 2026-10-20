#include "native/mods_page.hpp"

#include "cmfm/cmfm.h"
#include "menu/keys.hpp"
#include "menu/service.hpp"
#include "native/data_patch.hpp"
#include "native/game_421810.hpp"
#include "native/gui.hpp"
#include "native/measure.hpp"
#include "native/mod_list.hpp"
#include "native/options_model.hpp"
#include "native/options_view.hpp"
#include "native/page_class.hpp"
#include "native/panel_window.hpp"

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace cmfm::native {
namespace {

// Hooked.
using ShowCategoryFn = std::uint64_t (*)(void* categories, std::int32_t category);
using ListMoveFn = std::uint64_t (*)(void* list, std::int32_t row);
using RowLooksFn = std::uint64_t (*)(void* tab, std::uint32_t cursor);
using ListStepFn = std::uint32_t (*)(void* list, std::int32_t cursor, std::int32_t steps, std::uint8_t wrap,
                                     std::uint8_t* wrapped);
// Called.
using InitCategoryFn = std::uint64_t (*)(void* tab, void* unit, std::int32_t index, const char* name,
                                         const char* title);
using BuildCategoriesFn = void (*)(void* categories, void* unit);
using LockChannelFn = std::uint64_t (*)(void* input, std::int32_t channel, std::uint32_t player);
using LoadResourceFn = void* (*)(void* resources, const void* resource_class, const char* path, std::uint32_t flags);
using MousePositionFn = void* (*)(void* gui, float* out, void* list);
using MouseHitFn = std::uint8_t (*)(void* gui, void* instance, const float* position, std::int32_t* index);
using MouseFlagFn = std::uint8_t (*)(void* gui, void* list);
using MenuKeyFn = std::uint8_t (*)(void* gui, std::uint32_t key, void* list);
using MenuDirectionFn = std::uint32_t (*)(void* gui, std::int32_t* sources, std::int32_t* mode, std::uint32_t* flags);
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
using EnabledCheckFn = std::uint8_t (*)(const GameOptionRow* row, std::int32_t* reason, std::uint8_t flag);
using TabRefreshFn = std::uint64_t (*)(void* tab);
using YesNoDialogFn = void (*)(void* manager, void* request);
using DefaultAnswerFn = void (*)(void* const* capture, std::uint32_t unknown, std::int32_t answer);
using RowTextFn = void (*)(void* row, void* label, void* value, const GameOptionRow* entry);
using ScrollMouseFn = std::uint8_t (*)(void* bar);
using TabCursorFn = std::uint64_t (*)(void* tab, void* list, std::int32_t row);
using ArrowValueFn = std::uint64_t (*)(void* arrows, std::int32_t value);

ShowCategoryFn g_show_category_next = nullptr;
ListMoveFn g_list_move_next = nullptr;
RowLooksFn g_row_looks_next = nullptr;
ListStepFn g_step_down_next = nullptr;
ListStepFn g_step_up_next = nullptr;
RowDecideFn g_row_decide_next = nullptr;
SelectDecideFn g_select_decide_next = nullptr;
EnabledCheckFn g_enabled_check_next = nullptr;
DefaultAnswerFn g_default_answer_next = nullptr;
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
// The game's menu input through the player's bindings (game_421810.hpp):
// keyboard and pad on the page tabs. Both or neither.
MenuKeyFn g_menu_key = nullptr;
MenuDirectionFn g_menu_direction = nullptr;
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
// Buttons: the refresh after a press, the Yes/No question (optional: a
// button with a question is not pressed without it).
TabRefreshFn g_tab_refresh = nullptr;
YesNoDialogFn g_yes_no_dialog = nullptr;
std::int32_t g_pending_button = -1;  // the button whose question is open
bool g_showing_category = false;      // inside the game's showCategory (show_category_detour)

// A KEY row waiting for the player's key (begin_key_wait): the game sees idle
// input meanwhile (al.input capture); CMFM reads the real keys every frame.
struct KeyWait {
    std::int32_t id = -1;  // the setting; -1: no wait
    std::chrono::steady_clock::time_point end;
    std::uint32_t seconds = 0;    // shown in the row
    std::uint64_t armed[4] = {};  // keys seen up since the wait began (a key held then counts after a release)
    std::uint32_t pad_armed[4] = {};  // the same for each pad's buttons
};
KeyWait g_key_wait;
constexpr std::uint32_t kKeyWaitSeconds = 10;
std::int32_t g_row_help = -1;  // the row whose help line kRowHelpIds show
std::uint32_t g_row_help_flip = 0;
char g_question[256];                // the text the open question shows
std::uint32_t g_lock_seen = 0;       // the menu's lock revision the panel shows
// CMFM's layout as loaded once (kept for the session, like the page).
void* g_layout_resource = nullptr;

enum Hook {
    kShowCategory,
    kListMove,
    kRowLooks,
    kRowDecide,
    kSelectDecide,
    kEnabledCheck,
    kDefaultAnswer,
    kHookCount
};
std::uint32_t g_hooks[kHookCount] = {};
// The panel window's cursor steps and the rows' text filler (optional:
// without them a long tab shows all its rows, past the bottom of the screen).
std::uint32_t g_step_hooks[2] = {};
bool g_panel_window_on = false;
RowTextFn g_row_text = nullptr;
// The scroll bars' mouse (optional: without it the bars only show).
ScrollMouseFn g_scroll_mouse = nullptr;
// The arrows' own value (optional: without it a press starts from the value
// the arrows last held).
ArrowValueFn g_arrow_value = nullptr;

// The split panel's window over the highlighted tab's rows
// (native/panel_window.hpp): the rows the panel holds (the game keeps
// pointers to these entries in its row objects, so a scroll rewrites them in
// place), plus the end row; which of the tab's rows the cursor may take.
GameOptionRow g_panel_rows[kMaxRowsPerTab + 1];
bool g_panel_selectable[kMaxRowsPerTab];
std::size_t g_panel_count = 0;  // the tab's rows the panel shows
std::size_t g_panel_top = 0;    // the first of them in the window
// The tab's rows the panel shows, by their index in the tab (rows hidden by
// their lock, CMFM_FLAG_HIDE_LOCKED, left out), and which were hidden when
// the panel was last filled (one bit per tab row).
std::uint16_t g_panel_shown[kMaxRowsPerTab];
using RowBits = std::bitset<kMaxRowsPerTab>;
RowBits g_panel_hidden;
static_assert(kMaxRowsPerTab <= 65536);  // g_panel_shown's entries

// The scrollbars of CMFM's layout (track and bar, the game's own), found by
// name on the page: one for the mod list, one for the panel, a horizontal one
// for the page tabs. Shown only while their list scrolls; the thumb can be
// dragged.
struct ScrollBar {
    void* root = nullptr;
    void* track = nullptr;
    void* bar = nullptr;
    bool shown = false;
};
ScrollBar g_list_bar;
ScrollBar g_panel_bar;
ScrollBar g_tab_bar;
void* g_bars_unit = nullptr;  // the page the bars were looked up on

Env g_env = {};
std::atomic<bool> g_ready{false};
std::uint32_t g_pages_created = 0;
bool g_create_failed = false;  // logged once; no page this session then
// CMFM's page is open with CMFM's rows (the text hook answers).
std::atomic<bool> g_view{false};
// The Start menu that opened the page: it opens again when the page closes.
GameRef g_return_menu = {};
// Mods was chosen in that menu: the page's open handler closes the menu (a
// page made on the spot opens a frame later, after its setup).
bool g_leave_menu = false;
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

// The mod list (roadmap 1.2 item 7, native/mod_list.hpp): one entry per mod
// and dropdowns of their pages. As on the game's own page: the highlight (the
// pointer, the arrow keys) shows its row in the panel; a click or Confirm goes
// into the panel; Q / E step through the shown mod's pages.
ModList g_list;
std::size_t g_highlight = 0;  // the list row the mod list's cursor is on
constexpr std::size_t kMaxListRows = 32;  // visible rows (the layout has 11)
char g_row_labels[kMaxListRows][128];
std::string g_highlight_help;  // the help line for the highlighted row
// CMFM's copy of the categories object's vtable, for CMFM's own page only:
// slot 8, the category list's Confirm (the keys; a click on the panel calls
// the function behind it directly), goes to the mod list first.
void* g_categories_vtable[game::kCategoriesVtableSlots] = {};
PanelFocusFn g_category_confirm_next = nullptr;
bool g_list_confirm = false;  // the keys' Confirm reaches the mod list first (else only the game's)
bool g_page_keys[2] = {};    // Q / LB, E / RB held in the last update
// Split mode (CMFM's own layout, docs/research/native-menu.md "Layout
// study"): the mod list (the layout's spare camera list, styled like the
// category list) shows every menu tab; the page's category list, off screen,
// keeps its cursor on the audio panel, which shows the highlighted tab.
std::atomic<bool> g_split{false};
std::atomic<void*> g_mod_list{nullptr};  // while the page shows CMFM's menu
// The page tabs above the panel (decisions item 17): the shown mod's pages, a
// window of them (the layout's DuplicateNum, read once per list) from
// g_tab_top, the shown page in light green under the list's cursor; hidden
// for a mod with one page. Without the layout's list, Q / E still switch.
std::atomic<void*> g_tab_list{nullptr};
void* g_tab_window_of = nullptr;
std::size_t g_tab_window = 0;
std::size_t g_tab_top = 0;
std::size_t g_tab_mod = 0;  // the mod the window was placed for
bool g_tabs_shown = false;
// The tab under the pointer (its row in the window), or -1: the list's cursor
// shows it (the tab a click would pick); otherwise the cursor is on the shown
// page, or hidden while the wheel or the scrollbar moved that out of view.
std::int64_t g_tab_hover = -1;
void* g_tab_cursor = nullptr;  // the list's cursor instance (setPlay hides it)
bool g_tab_cursor_shown = true;
// Keyboard and pad on the page tabs (tab_keys): Up from the panel's first row
// asks for them (the panel list's step, acted on in the page's update). While
// on, the page's focus is on the list side with the category list inactive,
// and the tab list is the one active registered list (the GUI then keeps the
// panels' side bright). CMFM reads the keys through the game's menu input,
// the stick included: Left / Right move the tabs' cursor (g_tab_mark, as the
// pointer does), Confirm shows that page and goes into it (as a click), Down
// goes back into the panel, Back to the mod list. The list's own steps are
// ignored.
bool g_tab_keys_wanted = false;
bool g_tab_keys = false;
std::size_t g_tab_mark = 0;  // the page the tabs' cursor is on while they have the keys
bool g_mod_hover = false;  // the pointer is over a mod row (the hand shows)
constexpr std::size_t kMaxTabs = 8;
char g_tab_labels[kMaxTabs][128];
// The mod list's window: how many rows its layout shows. Read from the list's
// DuplicateNum once per list, before CMFM first sets its row count: setRowCount
// stores the count into that field (rebuildRows, RVA 0xB7FEF0 + 0x1F8), so after
// a fill with 2 rows it would say 2 for good (found in the closed beta, with 2
// mods; the DLSS session's handoff-cmfm-20261014).
std::atomic<std::uint32_t> g_list_window{0};
void* g_list_window_of = nullptr;  // the list it was read from (game thread)
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

// Runs `build` with TitleOptionTabTables[t] pointing at CMFM's menu_tab(t)
// (in split mode at the panel window's rows): the game reads those tables
// only while it fills a category's rows (fillCategoryRows), so the game's own
// Options pages never see CMFM's.
template <class Build>
bool with_tables(const OptionsModel& model, Build build) {
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        std::string why;
        const std::uint64_t expected = g_env.base + game::kRvaTitleOptionRows[t];
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(
            g_split.load(std::memory_order_relaxed) ? g_panel_rows : model.game_table(menu_tab(t)));
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
void show_selected(void* unit, bool keep_focus);
void set_highlight(void* unit, std::size_t row);
void show_highlight(void* unit);
void show_tabs(bool follow = true);
void scroll_tabs(std::size_t top);
std::int64_t tab_at(void* unit, float* position);
bool hover_tabs(void* unit, float* position);
bool click_tab(void* unit, float* position, std::int32_t focus);
void show_scroll(ScrollBar& b, std::size_t total, std::size_t visible, std::size_t top);
void place_tab_cursor();
void tab_keys(void* unit);
void leave_tab_keys(void* unit, bool to_panel);
bool activate_row(void* unit, std::size_t row, bool keep_focus);
void enter_panel(void* unit);
void leave_panel(void* unit);
void page_keys(void* unit);
// looks_cursor: the cursor row the row looks were given (the list stores a
// moved cursor only after its cursor callback); -1: the list's.
void decorate_panel(void* unit, std::int32_t looks_cursor = -1);
void centre_label(void* label, bool centred);
void show_restart_line(void* unit);
void show_panel_header(void* unit);
void request_split_help(void* unit);
void update_sliders(void* unit);
// all: every slider row (the window moves), not only the locked ones.
void end_sliders(void* unit, bool all = false);
void show_pick_list(void* unit, const GameOptionRow& entry);
void update_scrollbars();
void drag_scrollbars(void* unit);
bool scroll_dragging();
void wheel_split(void* unit, int steps);
void refresh_live_rows(void* unit);
void poll_key_wait(void* unit);
void end_key_wait(void* unit, bool set, std::int32_t key, const char* how);
std::size_t panel_visible();
RowBits panel_hidden_now();
void rebuild_panel(void* unit);
void fill_split_panel(void* unit);

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

// Split mode: the mod list's highlight moves `steps` rows down or up
// (wrapping at both ends), and the panel shows the row it lands on.
void step_highlight(bool down, std::int32_t steps) {
    const std::size_t rows = g_list.mod_count();
    if (rows <= 1) {
        return;
    }
    void* unit = g_view_unit.load(std::memory_order_acquire);
    const std::size_t n = static_cast<std::size_t>(steps > 0 ? steps : 1) % rows;
    set_highlight(unit, down ? (g_highlight + n) % rows : (g_highlight + rows - n) % rows);
}

// Split mode: the hidden category list stands in for the mod list (its
// cursor stays on the audio panel), so its moves go to the mod list.
bool split_categories(void* list) {
    return list == g_window_list.load(std::memory_order_acquire) && split_view() &&
           g_mod_list.load(std::memory_order_acquire) != nullptr;
}

// Directional moves on the category list: past the bottom or top row the
// window moves instead of the cursor; at the very end and start it wraps.
std::uint64_t list_move_detour(void* list, std::int32_t row) {
    if (split_categories(list)) {
        // Keys, D-pad and stick reach the mod list through the step hooks
        // (panel_step), which keep this cursor, so a move that gets here came
        // from elsewhere. Its direction: a wrap from the last row to row 0 is
        // Down, the reverse Up (the list may hold only two or three rows).
        const std::int32_t old = field<std::int32_t>(list, game::kListCursor);
        const std::int32_t last = static_cast<std::int32_t>(field<std::uint32_t>(list, game::kListRowCount)) - 1;
        if (row != old) {
            const bool down = (old >= last && row == 0) ? true : (old == 0 && row >= last) ? false : row > old;
            step_highlight(down, 1);
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
// category list has the focus, and in split mode while the mod list or the
// panel has it (the game does not scroll a panel with the wheel; run
// 20261011-scrollbar-2).
std::uint32_t AL_CALL message_callback(al_plugin_t*, void*, std::uint32_t message, std::uintptr_t wparam,
                                       std::intptr_t, void*) {
    constexpr std::uint32_t kMouseWheel = 0x020A;  // WM_MOUSEWHEEL
    if (message != kMouseWheel || !(scrolling() || split_view())) {
        return 0;
    }
    void* unit = g_view_unit.load(std::memory_order_acquire);
    const std::int32_t focus = unit != nullptr ? field<std::int32_t>(unit, game::kOptFocus) : -1;
    if (focus != 0 && !(focus == 1 && split_view())) {
        return 0;  // inside a tab (or the pick list): the game handles the wheel
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

// Split mode, the mouse as on the game's page: with the list's focus the
// pointer highlights the row under it (the panel shows it) and a click goes
// into the panel; a click on the panel goes in too (the game's update does
// that one). With the panel's focus the pointer over the list does nothing
// (Back or a right click returns, the game's way); a click on a row shows it
// and the panel keeps the focus, as Q / E do. A click on a page tab works the
// same way (click_tab); over the tabs the tabs' cursor follows the pointer
// (hover_tabs), with either focus. Only on mouse activity, and only while the
// page has the input (the helpers answer nothing otherwise).
void track_mouse(void* unit) {
    void* list = g_mod_list.load(std::memory_order_acquire);
    std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    if (g_mouse_moved == nullptr || g_mouse_off || list == nullptr || (focus != 0 && focus != 1)) {
        return;
    }
    const bool moved = g_mouse_moved(unit, nullptr) != 0;
    const bool clicked = g_mouse_click(unit, nullptr) != 0;
    if (!moved && !clicked) {
        return;
    }
    if (g_tab_keys) {
        // The pointer takes over from the keys: the tabs give them back to
        // the panel (the tabs' own pointer handling as an active list would
        // fight CMFM's), and the pointer acts as usual from there.
        leave_tab_keys(unit, true);
        focus = field<std::int32_t>(unit, game::kOptFocus);
        if (focus != 0 && focus != 1) {
            return;
        }
    }
    alignas(16) float position[4] = {};
    g_mouse_position(unit, position, nullptr);
    if ((clicked && click_tab(unit, position, focus)) || hover_tabs(unit, position)) {
        g_mod_hover = false;
        return;
    }
    const std::uint32_t rows = field<std::uint32_t>(list, game::kListRowCount);
    for (std::uint32_t r = 0; r < rows; ++r) {
        void* row = list_row(list, r);
        if (row == nullptr || g_mouse_hit(unit, row, position, nullptr) == 0) {
            continue;
        }
        const std::size_t at = g_mod_top + r;
        g_mod_hover = true;
        if (focus == 1) {
            if (clicked) {
                activate_row(unit, at, true);
            }
            return;
        }
        set_highlight(unit, at);
        if (clicked && activate_row(unit, at, false)) {
            enter_panel(unit);
        }
        return;
    }
    g_mod_hover = false;
}

// CMFM's page, vtable slot 81: every frame while it is open (state 2; the
// menu's thread).
std::uint64_t page_update(void* unit) {
    if (g_key_wait.id >= 0) {
        g_wheel.store(0, std::memory_order_relaxed);  // the wait has the input
        if (split_view() && unit == g_view_unit.load(std::memory_order_acquire)) {
            poll_key_wait(unit);
        }
    }
    if (split_view() && unit == g_view_unit.load(std::memory_order_acquire)) {
        keep_panel_cursor(unit);
        drag_scrollbars(unit);
        if (!scroll_dragging()) {
            track_mouse(unit);  // the pointer may cross the mod list while it holds a thumb
        }
        page_keys(unit);
        tab_keys(unit);
        // The hand pointer over the page tabs and the mod rows, as the game's
        // pages show it over their own clickable parts.
        const std::int32_t pointer_focus = field<std::int32_t>(unit, game::kOptFocus);
        void* manager = g_gui_manager != nullptr ? *g_gui_manager : nullptr;
        if (manager != nullptr && (g_tab_hover >= 0 || g_mod_hover) && (pointer_focus == 0 || pointer_focus == 1)) {
            field<std::uint8_t>(manager, game::kGuiPointerHand) = 1;
        }
        const std::uint32_t revision = menu::service_restart_revision();
        if (revision != g_restart_seen) {
            g_restart_seen = revision;
            decorate_panel(unit);
            show_restart_line(unit);
            show_panel_header(unit);
        }
        // A lock changed (after any change CMFM made): rows that stop being
        // sliders (a locked number row shows its value as text) become plain
        // rows first, then the game reads and writes every row again and the
        // row looks grey the locked ones (enabled check), then CMFM's touches.
        const std::uint32_t locks = menu::service_lock_revision();
        if (locks != g_lock_seen && panel_hidden_now() != g_panel_hidden) {
            g_lock_seen = locks;
            rebuild_panel(unit);
        } else if (locks != g_lock_seen) {
            g_lock_seen = locks;
            end_sliders(unit);
            char* tab = tab_object(unit, game::kSplitPanel);
            if (g_tab_refresh != nullptr) {
                g_tab_refresh(tab);
            } else if (void* list = field<void*>(tab, game::kTabList)) {
                g_row_looks_next(tab, field<std::uint32_t>(list, game::kListCursor));
            }
            decorate_panel(unit);
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
            wheel_split(unit, steps);
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
        refresh_live_rows(unit);
    }
    return result;
}

// Live TEXT lines (CMFM_FLAG_LIVE) the panel shows, read again four times a
// second; a changed line gets its value text written the way the game's tab
// refresh writes it (the text filler without the label: no row looks, so
// nothing restarts its animation).
void refresh_live_rows(void* unit) {
    using clock = std::chrono::steady_clock;
    static clock::time_point next{};
    const clock::time_point now = clock::now();
    if (now < next) {
        return;
    }
    next = now + std::chrono::milliseconds(250);
    char* tab = tab_object(unit, game::kSplitPanel);
    bool refresh = false;
    for (std::size_t r = 0; r < panel_visible(); ++r) {
        const auto id = static_cast<std::uint32_t>(g_panel_rows[r].setting_id);
        if (!menu::service_is_live(id) || !menu::service_reread_text(id)) {
            continue;
        }
        char* row_object = tab + (r + 1) * game::kRowObjectSize;
        void* row = field<void*>(row_object, game::kRowInstance);
        void* value = field<void*>(row_object, game::kRowValueObject);
        if (g_row_text != nullptr && row != nullptr && value != nullptr) {
            g_row_text(row, nullptr, value, &g_panel_rows[r]);
        } else {
            refresh = true;
        }
    }
    if (refresh && g_tab_refresh != nullptr) {
        g_tab_refresh(tab);
        decorate_panel(unit);
    }
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

// The mod list's rows in its window: one entry per mod, the shown mod in
// light green, the cursor on the highlighted row. The scrollbar says when
// more rows lie beyond.
void show_mod_list() {
    void* list = g_mod_list.load(std::memory_order_acquire);
    if (list == nullptr) {
        return;
    }
    const std::vector<menu::Mod>& mods = menu::service_mods();
    const std::size_t total = g_list.mod_count() < mods.size() ? g_list.mod_count() : mods.size();
    std::size_t visible = g_list_window.load(std::memory_order_relaxed);
    visible = visible < kMaxListRows ? visible : kMaxListRows;
    const std::size_t count = total < visible ? total : visible;
    if (g_mod_top + count > total) {
        g_mod_top = total - count;
    }
    if (field<std::uint32_t>(list, game::kListRowCount) != count) {
        cursor_to_top(list);
        gui_set_row_count(list, count);
    }
    for (std::size_t r = 0; r < count; ++r) {
        void* row = list_row(list, static_cast<std::uint32_t>(r));
        const std::size_t m = g_mod_top + r;
        if (row == nullptr) {
            continue;
        }
        if (m == g_list.shown_mod()) {
            std::snprintf(g_row_labels[r], sizeof g_row_labels[r], "<STYL MOJI_LIGHTGREEN_DEFAULT>%s</STYL>",
                          mods[m].name.c_str());
        } else {
            std::snprintf(g_row_labels[r], sizeof g_row_labels[r], "%s", mods[m].name.c_str());
        }
        set_row_text(row, "msg_1", nullptr, g_row_labels[r]);
        field<std::uint32_t>(row, game::kInstFlags) |= game::kInstFlagActive | game::kInstFlagEnabled;
        gui_set_play(row, true);
    }
    if (g_highlight >= g_mod_top && g_highlight < g_mod_top + count) {
        const auto cursor = static_cast<std::uint32_t>(g_highlight - g_mod_top);
        gui_set_cursor(list, cursor);
        // Where the list's opening animation leaves the cursor.
        field<std::int32_t>(list, game::kListStartRow) = static_cast<std::int32_t>(cursor);
    }
    update_scrollbars();
}

// A tab's label centred in its tab (the title menu's row text is laid out
// left in a box of its own width) and shrunk when it is wider than the tab
// (AutoScale); what it was is logged once, for the layout's sake.
void centre_tab_label(void* row) {
    void* root = field<void*>(row, game::kInstRoot);
    void* label = root != nullptr ? gui_get_object(root, game::kTabText) : nullptr;
    if (label == nullptr) {
        return;
    }
    std::uint8_t& layout = field<std::uint8_t>(label, game::kMessageLayout);
    float& width = field<float>(label, game::kMessageWidth);
    static bool logged = false;
    if (!logged) {
        logged = true;
        logf(AL_LOG_INFO, "Mods page: a page tab's label was laid out 0x%02X, %.1f wide", static_cast<unsigned>(layout),
             static_cast<double>(width));
    }
    std::uint32_t& flags = field<std::uint32_t>(label, game::kMessageFlags);
    const auto centred = static_cast<std::uint8_t>((layout & 0xF0) | 0x01);
    if (layout != centred || width != game::kTabWidth || (flags & game::kMessageAutoScale) == 0) {
        layout = centred;
        width = game::kTabWidth;
        flags |= game::kMessageAutoScale | game::kMessageRelayout;
    }
}

// The list's cursor on tab row `row` (shown again if it was hidden), or
// hidden (row < 0).
void place_tab_cursor(void* list, std::int64_t row) {
    const bool show = row >= 0;
    if (g_tab_cursor != nullptr && show != g_tab_cursor_shown) {
        g_tab_cursor_shown = show;
        gui_set_play(g_tab_cursor, show);
    }
    if (show) {
        gui_set_cursor(list, static_cast<std::uint32_t>(row));
        field<std::int32_t>(list, game::kListStartRow) = static_cast<std::int32_t>(row);
    }
}

// Where the tabs' cursor belongs: the tab under the pointer, else the shown
// page's tab (while the tabs have the keys, the marked one) while it is in the
// window, else nowhere.
void place_tab_cursor() {
    void* list = g_tab_list.load(std::memory_order_acquire);
    if (list == nullptr || !g_tabs_shown) {
        return;
    }
    const std::size_t count = field<std::uint32_t>(list, game::kListRowCount);
    const std::size_t page = g_tab_keys ? g_tab_mark : g_list.shown_page();
    if (g_tab_hover >= 0 && static_cast<std::size_t>(g_tab_hover) < count) {
        place_tab_cursor(list, g_tab_hover);
    } else if (page >= g_tab_top && page < g_tab_top + count) {
        place_tab_cursor(list, static_cast<std::int64_t>(page - g_tab_top));
    } else {
        place_tab_cursor(list, -1);
    }
}

// The tabs as the shown mod's pages stand: its pages in the window (with
// `follow`, the shown one brought into view; the wheel and the scrollbar move
// the window without it), a « or » on an edge tab when more lie beyond, the
// shown page in light green, the cursor where place_tab_cursor puts it, the
// scrollbar under them while they scroll; hidden for a mod with one page.
void show_tabs(bool follow) {
    void* list = g_tab_list.load(std::memory_order_acquire);
    if (list == nullptr) {
        return;
    }
    const std::vector<menu::Mod>& mods = menu::service_mods();
    const OptionsModel& model = menu::service_model();
    const std::size_t m = g_list.shown_mod();
    const std::size_t pages = m < mods.size() ? mods[m].tabs.size() : 0;
    const std::size_t visible = g_tab_window < kMaxTabs ? g_tab_window : kMaxTabs;
    g_tabs_shown = pages > 1 && visible > 0;
    gui_set_play(list, g_tabs_shown);  // acts only on a change
    if (!g_tabs_shown) {
        g_tab_hover = -1;
        show_scroll(g_tab_bar, 0, 0, 0);
        return;
    }
    if (m != g_tab_mod) {
        g_tab_mod = m;
        g_tab_top = 0;
    }
    const std::size_t page = g_list.shown_page();
    const std::size_t count = pages < visible ? pages : visible;
    if (follow) {
        g_tab_top = tab_window_top(g_tab_top, page, pages, visible);
    } else if (g_tab_top > pages - count) {
        g_tab_top = pages - count;
    }
    if (field<std::uint32_t>(list, game::kListRowCount) != count) {
        cursor_to_top(list);
        gui_set_row_count(list, count);
    }
    for (std::size_t r = 0; r < count; ++r) {
        void* row = list_row(list, static_cast<std::uint32_t>(r));
        if (row == nullptr) {
            continue;
        }
        const std::size_t p = g_tab_top + r;
        const char* title = model.tab_name(mods[m].tabs[p]);
        const char* before = r == 0 && g_tab_top > 0 ? "\xC2\xAB " : "";
        const char* after = r + 1 == count && p + 1 < pages ? " \xC2\xBB" : "";
        std::snprintf(g_tab_labels[r], sizeof g_tab_labels[r],
                      p == page ? "<STYL MOJI_LIGHTGREEN_DEFAULT><SIZE %d>%s%s%s</SIZE></STYL>"
                                : "<SIZE %d>%s%s%s</SIZE>",
                      game::kTabTextSize, before, title != nullptr ? title : "", after);
        set_row_text(row, game::kTabText, nullptr, g_tab_labels[r]);
        centre_tab_label(row);
        field<std::uint32_t>(row, game::kInstFlags) |= game::kInstFlagActive | game::kInstFlagEnabled;
        gui_set_play(row, true);
    }
    place_tab_cursor();
    show_scroll(g_tab_bar, pages, count, g_tab_top);
}

// The tabs' window from `top` (the wheel, the scrollbar's thumb): the shown
// page stays; the cursor stays on the tab under the pointer.
void scroll_tabs(std::size_t top) {
    if (top != g_tab_top) {
        g_tab_top = top;
        show_tabs(false);
    }
}

// The tab row under the pointer, or -1. The tabs' hit areas may be wider
// than a tab: the last tab hit wins (its label starts right of the earlier
// one's).
std::int64_t tab_at(void* unit, float* position) {
    void* list = g_tab_list.load(std::memory_order_acquire);
    if (list == nullptr || !g_tabs_shown) {
        return -1;
    }
    const std::uint32_t count = field<std::uint32_t>(list, game::kListRowCount);
    std::int64_t hit = -1;
    for (std::uint32_t r = 0; r < count; ++r) {
        void* row = list_row(list, r);
        if (row != nullptr && g_mouse_hit(unit, row, position, nullptr) != 0) {
            hit = r;
        }
    }
    return hit;
}

// The pointer moved: the cursor follows it over the tabs and goes back to
// the shown page when it leaves them. True while it is over a tab.
bool hover_tabs(void* unit, float* position) {
    const std::int64_t hit = tab_at(unit, position);
    if (hit != g_tab_hover) {
        g_tab_hover = hit;
        place_tab_cursor();
    }
    return hit >= 0;
}

// A click on a page tab: the shown mod's page; with the list's focus the
// panel takes it (one click goes in, as on a mod), with the panel's it keeps
// it (as Q / E). True if the click was on a tab.
bool click_tab(void* unit, float* position, std::int32_t focus) {
    const std::int64_t hit = tab_at(unit, position);
    if (hit < 0) {
        return false;
    }
    g_tab_hover = hit;
    if (g_list.show_page(g_tab_top + static_cast<std::size_t>(hit)).changed) {
        show_selected(unit, focus == 1);
        request_split_help(unit);
    }
    if (focus == 0) {
        enter_panel(unit);
    }
    return true;
}

std::size_t list_visible() {
    void* list = g_mod_list.load(std::memory_order_acquire);
    return list != nullptr ? g_list_window.load(std::memory_order_relaxed) : 0;
}

// The window follows the highlight (and never runs past the last row).
void keep_highlight_in_view() {
    const std::size_t rows = g_list.mod_count();
    const std::size_t visible = list_visible();
    if (rows == 0 || visible == 0) {
        g_highlight = 0;
        g_mod_top = 0;
        return;
    }
    if (g_highlight >= rows) {
        g_highlight = rows - 1;
    }
    if (g_highlight < g_mod_top) {
        g_mod_top = g_highlight;
    } else if (g_highlight >= g_mod_top + visible) {
        g_mod_top = g_highlight - visible + 1;
    }
    const std::size_t last_top = rows > visible ? rows - visible : 0;
    if (g_mod_top > last_top) {
        g_mod_top = last_top;
    }
}

// The cursor to list row `row` (mod `row`): the list, the panel
// (ModList::preview) and the help line follow, as the game's category list
// shows the highlighted category.
void set_highlight(void* unit, std::size_t row) {
    if (row == g_highlight || row >= g_list.mod_count()) {
        return;
    }
    g_highlight = row;
    keep_highlight_in_view();
    show_highlight(unit);
}

// The panel shows the highlighted row (with the list's focus); the list and
// the help line follow.
void show_highlight(void* unit) {
    if (g_list.preview(g_highlight).changed) {
        show_selected(unit, false);
    } else {
        show_mod_list();
    }
    request_split_help(unit);
}

// A click or Confirm on list row `row` (ModList::activate): the panel shows
// that mod and the cursor goes to it. `keep_focus`: the panel has the focus
// and keeps it (a click on the list from inside the panel). True if there is
// such a row: the panel takes the focus then.
bool activate_row(void* unit, std::size_t row, bool keep_focus) {
    if (row >= g_list.mod_count()) {
        return false;
    }
    const ModList::Result result = g_list.activate(row);
    g_highlight = row;
    keep_highlight_in_view();
    if (result.changed) {
        show_selected(unit, keep_focus);
    } else {
        show_mod_list();
    }
    request_split_help(unit);
    return result.enter;
}

// ---- Split mode: the panel's window over a long tab ----

// Rows the panel holds: kPanelRows of the tab with the window, else the whole
// tab up to the game's 24 row objects (rows past them out of reach then).
std::size_t panel_visible() {
    const std::size_t limit = g_panel_window_on ? kPanelRows : kGameRowsPerTab;
    return g_panel_count > limit ? limit : g_panel_count;
}

// Which of the highlighted tab's rows are hidden now (their lock, with
// CMFM_FLAG_HIDE_LOCKED), one bit per row.
RowBits panel_hidden_now() {
    const GameOptionRow* rows = menu::service_model().table(g_selected.load(std::memory_order_relaxed));
    RowBits hidden;
    for (std::size_t r = 0; r < kMaxRowsPerTab && rows[r].kind != kRowTableEnd; ++r) {
        if (menu::service_hidden(static_cast<std::uint32_t>(rows[r].setting_id))) {
            hidden.set(r);
        }
    }
    return hidden;
}

// The panel window's rows from the highlighted tab, from shown row `top` on
// (kept in range), with the end row after them. `recheck`: ask which rows
// are hidden now; otherwise keep the last answer (a scroll between two fills
// must not change the rows the game holds). A tab never shows empty (the page
// reads the row under the cursor unchecked): if every row is hidden, its first
// row shows.
void load_panel_rows(std::size_t top, bool recheck = true) {
    const GameOptionRow* rows = menu::service_model().table(g_selected.load(std::memory_order_relaxed));
    if (recheck) {
        g_panel_hidden = panel_hidden_now();
    }
    std::size_t count = 0;
    std::size_t tab_rows = 0;
    for (; tab_rows < kMaxRowsPerTab && rows[tab_rows].kind != kRowTableEnd; ++tab_rows) {
        if (!g_panel_hidden.test(tab_rows)) {
            g_panel_shown[count] = static_cast<std::uint16_t>(tab_rows);
            g_panel_selectable[count] =
                !menu::service_is_section(static_cast<std::uint32_t>(rows[tab_rows].setting_id));
            ++count;
        }
    }
    if (count == 0 && tab_rows > 0) {
        g_panel_shown[0] = 0;
        g_panel_selectable[0] = true;
        count = 1;
    }
    g_panel_count = count;
    const std::size_t visible = panel_visible();
    if (top > count - visible) {
        top = count - visible;
    }
    g_panel_top = top;
    for (std::size_t r = 0; r < visible; ++r) {
        g_panel_rows[r] = rows[g_panel_shown[top + r]];
    }
    g_panel_rows[visible] = GameOptionRow{kRowTableEnd, 0, 0, 0, 0, 0, 0, 0, nullptr};
}

void* panel_list(void* unit) {
    return field<void*>(tab_object(unit, game::kSplitPanel), game::kTabList);
}

// The window moves to `top`: the rows that showed sliders or opened a list
// become plain rows (their entries are about to change; the game would move
// a slider's new setting to the old knob), then the window's entries are
// rewritten in place. The next cursor change (or `refresh`) has the game
// read and write every row again, and CMFM decorates them after the row
// looks.
void scroll_panel(void* unit, std::size_t top, bool refresh) {
    if (top == g_panel_top) {
        return;
    }
    end_sliders(unit, true);
    char* tab = tab_object(unit, game::kSplitPanel);
    for (std::size_t r = 0; r < game::kTabRowObjects; ++r) {
        field<std::uint8_t>(tab + (r + 1) * game::kRowObjectSize, game::kRowOpensList) = 0;
    }
    load_panel_rows(top, false);
    // The game's refresh rewrites only the values: the labels the way the
    // row filler writes them.
    for (std::size_t r = 0; r < panel_visible(); ++r) {
        char* row_object = tab + (r + 1) * game::kRowObjectSize;
        void* row = field<void*>(row_object, game::kRowInstance);
        void* value = field<void*>(row_object, game::kRowValueObject);
        void* root = row != nullptr ? field<void*>(row, game::kInstRoot) : nullptr;
        void* label = root != nullptr ? gui_get_object(root, game::kRowLabelText) : nullptr;
        if (label != nullptr && value != nullptr) {
            g_row_text(row, label, value, &g_panel_rows[r]);
        }
    }
    if (refresh && g_tab_refresh != nullptr) {
        g_tab_refresh(tab);
        decorate_panel(unit);
    }
}

// The panel list's cursor steps (keys, stick): over the whole tab, the
// window following (native/panel_window.hpp). When the window moves under a
// cursor that keeps its row, the list is told it came from the row the old
// setting moved to, so moveCursor makes it a real cursor change: the sound,
// the row looks, the arrows and the help line for the new setting.
std::uint32_t panel_step(bool down, ListStepFn next, void* list, std::int32_t cursor, std::int32_t steps,
                         std::uint8_t wrap, std::uint8_t* wrapped) {
    if (list == g_tab_list.load(std::memory_order_acquire)) {
        // The tabs' own steps (their sideways keys while active): CMFM moves
        // their cursor itself (tab_keys), with the stick and repeats.
        return static_cast<std::uint32_t>(cursor);
    }
    if (split_categories(list)) {
        // The mod list moves the way the key or stick points; the category
        // cursor keeps its row (moveCursor ignores an unchanged row). Its own
        // step would wrap on a short list and read the other way: with three
        // pages the parked row 2 is the last one, and Down came out as Up
        // (closed beta; the DLSS session's handoff-cmfm-20261014-dpad).
        step_highlight(down, steps);
        return static_cast<std::uint32_t>(cursor);
    }
    void* unit = g_view_unit.load(std::memory_order_acquire);
    const std::size_t visible = panel_visible();
    if (!down && g_tabs_shown && g_menu_key != nullptr && split_view() && unit != nullptr && page_tab_input(unit) &&
        list == panel_list(unit) && cursor >= 0 && static_cast<std::size_t>(cursor) < visible) {
        // Up from the first row that takes the cursor: to the page tabs (a mod
        // with one page wraps as before). The cursor stays; the page's update
        // moves the focus (tab_keys), not this list's own step.
        std::size_t first = 0;
        while (first < g_panel_count && !g_panel_selectable[first]) {
            ++first;
        }
        if (g_panel_top + static_cast<std::size_t>(cursor) <= first) {
            g_tab_keys_wanted = true;
            return static_cast<std::uint32_t>(cursor);
        }
    }
    if (!split_view() || unit == nullptr || list != panel_list(unit) || g_panel_count <= visible || cursor < 0 ||
        static_cast<std::size_t>(cursor) >= visible) {
        return next(list, cursor, steps, wrap, wrapped);
    }
    const std::size_t old_top = g_panel_top;
    const WindowStep step = window_step(g_panel_selectable, g_panel_count, visible, old_top,
                                        static_cast<std::size_t>(cursor), steps > 0 ? static_cast<std::uint32_t>(steps) : 1u,
                                        down, wrap != 0);
    if (step.wrapped && wrapped != nullptr) {
        *wrapped = 1;
    }
    if (step.top != old_top) {
        scroll_panel(unit, step.top, false);
        if (step.row == static_cast<std::size_t>(cursor)) {
            const std::int64_t moved = static_cast<std::int64_t>(old_top + cursor) - static_cast<std::int64_t>(step.top);
            std::size_t from = step.row == 0 ? 1 : step.row - 1;
            if (moved >= 0 && static_cast<std::size_t>(moved) < visible && static_cast<std::size_t>(moved) != step.row) {
                from = static_cast<std::size_t>(moved);
            }
            field<std::int32_t>(list, game::kListCursor) = static_cast<std::int32_t>(from);
        }
    }
    return static_cast<std::uint32_t>(step.row);
}

std::uint32_t step_down_detour(void* list, std::int32_t cursor, std::int32_t steps, std::uint8_t wrap,
                               std::uint8_t* wrapped) {
    return panel_step(true, g_step_down_next, list, cursor, steps, wrap, wrapped);
}

std::uint32_t step_up_detour(void* list, std::int32_t cursor, std::int32_t steps, std::uint8_t wrap,
                             std::uint8_t* wrapped) {
    return panel_step(false, g_step_up_next, list, cursor, steps, wrap, wrapped);
}

// ---- Split mode: the scrollbars ----

// A scroll bar instance of CMFM's layout by name, checked by its class.
void* find_scroll_bar(void* unit, const char* name) {
    void* instance = gui_get_instance(unit, name);
    if (instance == nullptr) {
        return nullptr;
    }
    using GetDtiFn = const void* (*)(void* instance);
    const auto get_dti = field<GetDtiFn const*>(instance, 0)[game::kInstanceGetDtiSlot];
    const void* dti = get_dti != nullptr ? get_dti(instance) : nullptr;
    if (dti == nullptr ||
        *reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(dti) + game::kDtiClassIdOffset) !=
            game::kScrollBarClassId) {
        return nullptr;
    }
    return instance;
}

// The track, the bar and their group; false (and an empty `out`) if the
// layout lacks them or the shared scrollbar layout did not load (no parts
// to draw).
bool find_scroll(void* unit, const char* name, ScrollBar& out) {
    char part[64];
    out = {};
    void* root = gui_get_instance(unit, name);
    std::snprintf(part, sizeof part, "%s_base", name);
    void* track = find_scroll_bar(unit, part);
    std::snprintf(part, sizeof part, "%s_bar", name);
    void* bar = find_scroll_bar(unit, part);
    if (root == nullptr || track == nullptr || bar == nullptr) {
        return false;
    }
    for (void* b : {track, bar}) {
        for (std::size_t k = 0; k < 3; ++k) {
            if (field<void*>(b, game::kScrollTextures + 8 * k) == nullptr) {
                return false;
            }
        }
        field<std::uint8_t>(b, game::kScrollWheel) = 0;  // CMFM's wheel moves the lists
    }
    field<std::uint8_t>(track, game::kScrollFullLength) = 1;
    out = {root, track, bar};
    return true;
}

// Shows the bar for `visible` of `total` rows from `top`, or hides it when
// everything fits (setPlay acts only on a change).
void show_scroll(ScrollBar& b, std::size_t total, std::size_t visible, std::size_t top) {
    if (b.root == nullptr) {
        return;
    }
    b.shown = visible > 0 && total > visible;
    gui_set_play(b.root, b.shown);
    if (b.shown) {
        field<float>(b.bar, game::kScrollTotal) = static_cast<float>(total);
        field<float>(b.bar, game::kScrollVisible) = static_cast<float>(visible);
        if (field<std::uint8_t>(b.bar, game::kScrollDragging) == 0) {  // a held thumb stays under the mouse
            field<float>(b.bar, game::kScrollRatio) = static_cast<float>(top) / static_cast<float>(total - visible);
        }
    }
}

bool scroll_dragging() {
    for (const ScrollBar* b : {&g_list_bar, &g_panel_bar, &g_tab_bar}) {
        if (b->shown && field<std::uint8_t>(b->bar, game::kScrollDragging) != 0) {
            return true;
        }
    }
    return false;
}

// The window's first row for the thumb's position.
std::size_t scroll_top(const ScrollBar& b, std::size_t total, std::size_t visible) {
    const float ratio = field<float>(b.bar, game::kScrollRatio);
    const float rows = static_cast<float>(total - visible);
    const float top = ratio <= 0.0f ? 0.0f : ratio >= 1.0f ? rows : ratio * rows + 0.5f;
    return static_cast<std::size_t>(top);
}

// The mod list's window dragged to `top`: the window moves, the highlight
// stays inside it and the panel shows it. A drag (a hold on the list's own
// thumb) from inside the panel gives the list the focus first.
void drag_mod_list(void* unit, std::size_t top) {
    if (field<std::int32_t>(unit, game::kOptFocus) == 1) {
        leave_panel(unit);
    }
    const std::size_t visible = list_visible();
    g_mod_top = top;
    if (g_highlight < top) {
        g_highlight = top;
    } else if (visible > 0 && g_highlight >= top + visible) {
        g_highlight = top + visible - 1;
    }
    keep_highlight_in_view();
    show_highlight(unit);
}

// The panel's window dragged to `top`: the cursor keeps its row (on the
// nearest selectable one if that is now a heading); with the panel's focus
// the game's cursor handler then refreshes the rows and gives the row's help
// line, otherwise only the rows are refreshed (the help line keeps the mod's).
void drag_panel(void* unit, std::size_t top) {
    scroll_panel(unit, top, false);
    char* tab = tab_object(unit, game::kSplitPanel);
    void* list = panel_list(unit);
    const std::size_t visible = panel_visible();
    std::size_t cursor = field<std::uint32_t>(list, game::kListCursor);
    cursor = cursor < visible ? cursor : visible - 1;
    for (std::size_t d = 1; d < visible && !g_panel_selectable[top + cursor]; ++d) {
        if (cursor + d < visible && g_panel_selectable[top + cursor + d]) {
            cursor += d;
        } else if (cursor >= d && g_panel_selectable[top + cursor - d]) {
            cursor -= d;
        }
    }
    field<std::int32_t>(list, game::kListCursor) = static_cast<std::int32_t>(cursor);
    void* const* vtable = field<void* const*>(tab, 0);
    const bool handler_ok = vtable != nullptr && reinterpret_cast<std::uintptr_t>(vtable[game::kTabCursorSlot]) ==
                                                     g_env.base + game::kRvaTabCursor;
    if (field<std::int32_t>(unit, game::kOptFocus) == 1 && handler_ok) {
        reinterpret_cast<TabCursorFn>(vtable[game::kTabCursorSlot])(tab, list, static_cast<std::int32_t>(cursor));
    } else if (g_tab_refresh != nullptr) {
        g_tab_refresh(tab);
        decorate_panel(unit);
    }
}

// Every frame while the page shows CMFM's menu: the bars' mouse (a drag on a
// thumb moves its list's window); otherwise the thumbs sit on whole rows.
void drag_scrollbars(void* unit) {
    void* list = g_mod_list.load(std::memory_order_acquire);
    if (g_scroll_mouse == nullptr || list == nullptr) {
        return;
    }
    const std::size_t entries = g_list.mod_count();
    const std::size_t list_rows = g_list_window.load(std::memory_order_relaxed);
    if (g_list_bar.shown) {
        if (g_scroll_mouse(g_list_bar.bar) != 0) {
            const std::size_t top = scroll_top(g_list_bar, entries, list_rows);
            if (top != g_mod_top) {
                drag_mod_list(unit, top);
            }
        } else {
            show_scroll(g_list_bar, entries, list_rows, g_mod_top);
        }
    }
    if (g_panel_bar.shown) {
        if (g_scroll_mouse(g_panel_bar.bar) != 0) {
            const std::size_t top = scroll_top(g_panel_bar, g_panel_count, panel_visible());
            if (top != g_panel_top) {
                drag_panel(unit, top);
            }
        } else {
            show_scroll(g_panel_bar, g_panel_count, panel_visible(), g_panel_top);
        }
    }
    void* tabs = g_tab_list.load(std::memory_order_acquire);
    if (g_tab_bar.shown && tabs != nullptr) {
        const std::size_t pages = g_list.page_count(g_list.shown_mod());
        const std::size_t count = field<std::uint32_t>(tabs, game::kListRowCount);
        if (g_scroll_mouse(g_tab_bar.bar) != 0) {
            scroll_tabs(scroll_top(g_tab_bar, pages, count));
        } else {
            show_scroll(g_tab_bar, pages, count, g_tab_top);
        }
    }
}

// The wheel in split mode (wheel up, positive, goes towards the top; no
// wrap): over the page tabs or their scrollbar, with either focus, for the
// tabs (their window moves, the page stays); else with the panel's focus for
// the panel (the list takes no input then); with the list's, for the list
// under the pointer (the panel's window or a mod row), else the list. A long
// panel scrolls its rows as a drag on its thumb does (a panel that fits has
// nothing to scroll); the mod list's window scrolls, the highlight staying
// inside it (the panel shows it).
void wheel_split(void* unit, int steps) {
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    void* list = g_mod_list.load(std::memory_order_acquire);
    void* tabs = g_tab_list.load(std::memory_order_acquire);
    if (g_tabs_shown && tabs != nullptr && g_mouse_position != nullptr && g_mouse_hit != nullptr) {
        alignas(16) float position[4] = {};
        g_mouse_position(unit, position, nullptr);
        if (tab_at(unit, position) >= 0 ||
            (g_tab_bar.shown && g_mouse_hit(unit, g_tab_bar.track, position, nullptr) != 0)) {
            const std::size_t pages = g_list.page_count(g_list.shown_mod());
            const std::size_t count = field<std::uint32_t>(tabs, game::kListRowCount);
            const auto last = static_cast<std::int64_t>(pages > count ? pages - count : 0);
            std::int64_t next = static_cast<std::int64_t>(g_tab_top) - steps;
            next = next < 0 ? 0 : next > last ? last : next;
            scroll_tabs(static_cast<std::size_t>(next));
            hover_tabs(unit, position);  // the tab now under the pointer
            return;
        }
    }
    bool panel = focus == 1;
    if (!panel && g_mouse_position != nullptr && g_mouse_hit != nullptr && list != nullptr) {
        alignas(16) float position[4] = {};
        g_mouse_position(unit, position, nullptr);
        void* window = field<void*>(tab_object(unit, game::kSplitPanel), game::kTabWindow);
        if (window != nullptr && g_mouse_hit(unit, window, position, nullptr) != 0) {
            panel = true;
        } else {
            const std::uint32_t rows = field<std::uint32_t>(list, game::kListRowCount);
            for (std::uint32_t r = 0; r < rows; ++r) {
                void* row = list_row(list, r);
                if (row != nullptr && g_mouse_hit(unit, row, position, nullptr) != 0) {
                    panel = false;
                    break;
                }
            }
        }
    }
    if (panel) {
        const std::size_t visible = panel_visible();
        if (g_panel_count > visible) {
            const std::int64_t last = static_cast<std::int64_t>(g_panel_count - visible);
            std::int64_t next = static_cast<std::int64_t>(g_panel_top) - steps;
            next = next < 0 ? 0 : next > last ? last : next;
            if (static_cast<std::size_t>(next) != g_panel_top) {
                drag_panel(unit, static_cast<std::size_t>(next));
            }
        }
        return;
    }
    const std::size_t rows = g_list.mod_count();
    const std::size_t visible = list_visible();
    if (rows <= visible) {
        return;
    }
    std::int64_t next = static_cast<std::int64_t>(g_mod_top) - steps;
    const auto last = static_cast<std::int64_t>(rows - visible);
    next = next < 0 ? 0 : next > last ? last : next;
    if (static_cast<std::size_t>(next) != g_mod_top) {
        drag_mod_list(unit, static_cast<std::size_t>(next));
    }
}

// Both bars as the mod list's and the panel's windows stand (after any
// change to either; the bar draws it on the next update).
void update_scrollbars() {
    void* unit = g_view_unit.load(std::memory_order_acquire);
    void* list = g_mod_list.load(std::memory_order_acquire);
    if (!split_view() || unit == nullptr || list == nullptr) {
        return;
    }
    if (unit != g_bars_unit) {
        g_bars_unit = unit;
        const bool list_ok = find_scroll(unit, game::kListScrollName, g_list_bar);
        const bool panel_ok = find_scroll(unit, game::kPanelScrollName, g_panel_bar);
        const bool tabs_ok = find_scroll(unit, game::kTabScrollName, g_tab_bar);
        if (!list_ok || !panel_ok || !tabs_ok) {
            logf(AL_LOG_WARNING, "Mods page: scrollbars missing or not loaded (mod list %s, panel %s, tabs %s); lists "
                 "scroll without them", list_ok ? "ok" : "none", panel_ok ? "ok" : "none", tabs_ok ? "ok" : "none");
        }
    }
    show_scroll(g_list_bar, g_list.mod_count(), g_list_window.load(std::memory_order_relaxed), g_mod_top);
    show_scroll(g_panel_bar, g_panel_count, panel_visible(), g_panel_top);
    void* tabs = g_tab_list.load(std::memory_order_acquire);
    show_scroll(g_tab_bar, g_tabs_shown ? g_list.page_count(g_list.shown_mod()) : 0,
                tabs != nullptr ? field<std::uint32_t>(tabs, game::kListRowCount) : 0, g_tab_top);
}

// The panels show the selected tab again, the game's way (initCategory), and
// the audio panel is shown with its help line. A tab longer than the panel
// starts at its top.
void fill_split_panel(void* unit) {
    const OptionsModel& model = menu::service_model();
    const std::size_t selected = g_selected.load(std::memory_order_relaxed);
    load_panel_rows(0);
    with_tables(model, [&] {
        prepare_tab(unit, game::kSplitPanel);
        g_init_category(tab_object(unit, game::kSplitPanel), unit, static_cast<std::int32_t>(game::kSplitPanel),
                        game::kTitleTabNames[game::kSplitPanel], model.tab_name(selected));
    });
    show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                         static_cast<std::int32_t>(game::kSplitPanel));
    decorate_panel(unit);
    show_panel_header(unit);
    request_split_help(unit);
}

// Rows came or went (a lock with CMFM_FLAG_HIDE_LOCKED changed after a
// change): the panel is filled again with the rows now shown, the cursor on
// the setting it was on (or the nearest row that shows, if that one hid), the
// window keeping it in view, and the panel's focus kept.
void rebuild_panel(void* unit) {
    void* list = panel_list(unit);
    if (list == nullptr) {
        return;
    }
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    const std::size_t old_visible = panel_visible();
    const std::size_t cursor = field<std::uint32_t>(list, game::kListCursor);
    const std::size_t old_tab_row = cursor < old_visible ? g_panel_shown[g_panel_top + cursor] : 0;
    const std::size_t old_top = g_panel_top;
    load_panel_rows(0);
    // The shown row at or after the old one's tab row (else the last shown),
    // then the nearest selectable one.
    std::size_t k = 0;
    while (k + 1 < g_panel_count && g_panel_shown[k] < old_tab_row) {
        ++k;
    }
    for (std::size_t d = 0; d < g_panel_count && !g_panel_selectable[k]; ++d) {
        if (k + d < g_panel_count && g_panel_selectable[k + d]) {
            k += d;
        } else if (k >= d && g_panel_selectable[k - d]) {
            k -= d;
        }
    }
    load_panel_rows(window_top_for(g_panel_selectable, g_panel_count, panel_visible(), old_top, k), false);
    const OptionsModel& model = menu::service_model();
    const std::size_t selected = g_selected.load(std::memory_order_relaxed);
    with_tables(model, [&] {
        prepare_tab(unit, game::kSplitPanel);
        g_init_category(tab_object(unit, game::kSplitPanel), unit, static_cast<std::int32_t>(game::kSplitPanel),
                        game::kTitleTabNames[game::kSplitPanel], model.tab_name(selected));
    });
    show_category_detour(static_cast<char*>(unit) + game::kOptCategories,
                         static_cast<std::int32_t>(game::kSplitPanel));
    const auto row = static_cast<std::uint32_t>(k - g_panel_top);
    if (focus == 1 && g_set_focus != nullptr) {
        g_set_focus(unit, 1);
    }
    gui_set_cursor(list, row);
    decorate_panel(unit, static_cast<std::int32_t>(row));
    field<std::int32_t>(list, game::kListStartRow) = static_cast<std::int32_t>(row);  // after the decoration's
    show_panel_header(unit);
}

// The mod list's help line: the highlighted mod's (the help of the page it
// shows, and how many pages it has), with one of CMFM's two ids in turn
// (mods_page_text answers both): the help bar redraws only when the id
// changes.
void request_split_help(void* unit) {
    const OptionsModel& model = menu::service_model();
    const std::vector<menu::Mod>& mods = menu::service_mods();
    if (g_highlight < g_list.mod_count() && g_highlight < mods.size()) {
        const menu::Mod& mod = mods[g_highlight];
        const std::size_t page = g_list.page_of(g_highlight);
        const char* help = model.tab_help(mod.tabs[page < mod.tabs.size() ? page : 0]);
        g_highlight_help = help != nullptr ? help : "";
        if (g_tab_keys) {
            g_highlight_help += " Left / Right: choose a page. Confirm: open it. Down: back to the settings.";
        } else if (mod.tabs.size() > 1) {
            g_highlight_help += " " + std::to_string(mod.tabs.size()) + " pages: the tabs above, or Q / E.";
        }
    } else {
        const char* help = model.tab_help(g_selected.load(std::memory_order_relaxed));
        g_highlight_help = help != nullptr ? help : "";
    }
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
void place_knob(void* slider, std::uint32_t index, std::uint32_t steps);

bool show_slider(char* row_object, const GameOptionRow& entry, std::uint32_t r, bool active) {
    const auto id = static_cast<std::uint32_t>(entry.setting_id);
    const std::uint32_t steps = menu::service_slider_steps(id);
    void* slider = field<void*>(row_object, game::kRowSlider);
    // Not while the game's showCategory runs (show_category_detour: its tab
    // show runs the row looks, then reads every slider row's value from save
    // data by id).
    if (steps == 0 || slider == nullptr || g_slider_from_setting == nullptr || g_showing_category) {
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
    // The game set the knob from the getter, rounding down (a step low for
    // some values with 100 steps, run 20261011-fineslider-1: 99 fell to 98,
    // then 97); it goes on its value before the game's sync reads it.
    if (field<std::uint8_t>(slider, game::kSliderDragging) == 0) {
        place_knob(slider, index, steps);
    }
    if (void* end = field<void*>(row_object, game::kRowSliderEnd)) {
        gui_set_hidden(end, true);
    }
    if (void* text = field<void*>(row_object, game::kRowSliderText)) {
        const char* label = menu::service_value_text(id, index);
        gui_format_text(text, label != nullptr ? label : "");
        gui_set_hidden(text, false);
    }
    return true;
}

// Rows of the split panel the game drives as sliders that are no slider any
// more (locked now, or all of them before the panel window moves): plain rows
// again, the slider stopped (the row look then keeps it stopped, and the tab
// refresh writes their value text).
void end_sliders(void* unit, bool all) {
    char* tab = tab_object(unit, game::kSplitPanel);
    void* list = field<void*>(tab, game::kTabList);
    if (list == nullptr) {
        return;
    }
    const std::uint32_t count = field<std::uint32_t>(list, game::kListRowCount);
    for (std::uint32_t r = 0; r < count && r < game::kTabRowObjects; ++r) {
        char* row_object = tab + (r + 1) * game::kRowObjectSize;
        const auto* entry = field<const GameOptionRow*>(row_object, game::kRowDef);
        if (field<std::uint8_t>(row_object, game::kRowIsSlider) == 0 || entry == nullptr ||
            !is_cmfm_row(entry->kind, static_cast<std::uint32_t>(entry->setting_id)) ||
            (!all && menu::service_slider_steps(static_cast<std::uint32_t>(entry->setting_id)) != 0)) {
            continue;
        }
        field<std::uint8_t>(row_object, game::kRowIsSlider) = 0;
        if (void* slider = field<void*>(row_object, game::kRowSlider)) {
            field<std::uint8_t>(slider, game::kSliderActive) = 0;
            gui_set_play(slider, false);
        }
        if (void* text = field<void*>(row_object, game::kRowSliderText)) {
            gui_set_hidden(text, true);
        }
    }
}

float slider_position(void* slider);

}  // namespace

bool mods_page_slider_index(std::uint32_t setting_id, std::uint32_t steps, std::uint32_t* index) {
    void* unit = g_view_unit.load(std::memory_order_acquire);
    if (unit == nullptr || steps == 0 || !split_view()) {
        return false;
    }
    char* tab = tab_object(unit, game::kSplitPanel);
    for (std::size_t i = 0; i < g_slider_count; ++i) {
        char* row_object = tab + (g_slider_rows[i].row + 1) * game::kRowObjectSize;
        const auto* entry = field<const GameOptionRow*>(row_object, game::kRowDef);
        void* slider = field<void*>(row_object, game::kRowSlider);
        if (entry == nullptr || slider == nullptr || field<std::uint8_t>(row_object, game::kRowIsSlider) == 0 ||
            static_cast<std::uint32_t>(entry->setting_id) != setting_id) {
            continue;
        }
        // The track in steps + 1 equal zones, one per value, the ends too
        // (rounding to the nearest step gave the end values half a zone: 100
        // only in the last half percent, run 20261011-fineslider-2). A knob
        // placed on value k (k / steps) lies inside zone k.
        const float zone = slider_position(slider) * static_cast<float>(steps + 1);
        const auto k = zone <= 0.0f ? 0u : static_cast<std::uint32_t>(zone);
        *index = k > steps ? steps : k;
        return true;
    }
    return false;
}

namespace {

// The knob exactly on value `index` of `steps`, its step counter with it (the
// keys move from the counter; a drag leaves it behind). The game's
// set-position rounds steps * position down, so the middle of the step goes
// in. Only when the knob is off its value or the counter is stale.
void place_knob(void* slider, std::uint32_t index, std::uint32_t steps) {
    const float exact = static_cast<float>(index) / static_cast<float>(steps);
    const float off = slider_position(slider) - exact;
    const float slack = 0.25f / static_cast<float>(steps);
    if (off > slack || off < -slack || field<std::int32_t>(slider, game::kSliderStep) != static_cast<std::int32_t>(index)) {
        g_slider_set_position(slider, (static_cast<float>(index) + 0.5f) / static_cast<float>(steps));
    }
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
        place_knob(slider, s.index, s.steps);  // after a drag: on the value it stands for
    }
}

// What the game's row filler does not know about CMFM's rows, after every
// fill of the split panel, focus change and restart state change: section
// headings the cursor skips (the row's "selectable" flag off, as the list's
// moves and hover test it), centred and without a value box, plain buttons
// (the same look, but selectable), and the restart mark (the row's star icon)
// on settings waiting for a restart.
void decorate_panel(void* unit, std::int32_t looks_cursor) {
    char* tab = tab_object(unit, game::kSplitPanel);
    void* list = field<void*>(tab, game::kTabList);
    if (list == nullptr) {
        return;
    }
    // A moved cursor reaches the row looks before the list stores it (run
    // 20261011-picklist-2: the pick list showed one row off).
    const std::uint32_t cursor =
        looks_cursor >= 0 ? static_cast<std::uint32_t>(looks_cursor) : field<std::uint32_t>(list, game::kListCursor);
    const GameOptionRow* rows = g_panel_rows;  // the window's rows
    const std::uint32_t count = field<std::uint32_t>(list, game::kListRowCount);
    std::int32_t first = -1;  // the first row the cursor may take
    std::uint32_t known = 0;  // rows with a table entry
    // The row the panel's cursor is on, while the panel has the focus.
    const std::uint32_t active_row = field<std::int32_t>(unit, game::kOptFocus) == 1 ? cursor : 0xFFFFFFFFu;
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
        if (!slider && ((g_set_focus != nullptr && menu::service_list_choices(id) != 0) || menu::service_is_button(id) ||
                        menu::service_is_key(id))) {
            field<std::uint8_t>(row_object, game::kRowOpensList) = 1;  // Confirm opens the pick list, presses, waits for a key
        }
        void* root = field<void*>(row, game::kInstRoot);
        if (void* star = root != nullptr ? gui_get_object(root, game::kRowStar) : nullptr) {
            gui_set_hidden(star, !menu::service_restart_pending(id));
        }
        const bool plain = menu::service_is_plain_button(id);
        if (void* value = root != nullptr ? gui_get_object(root, game::kRowValueText) : nullptr) {
            gui_set_hidden(value, section || slider || plain);  // a heading has no value box, a slider its own text
        }
        if (void* label = root != nullptr ? gui_get_object(root, game::kRowLabelText) : nullptr) {
            centre_label(label, section || plain);  // row objects are reused as the panel scrolls
        }
    }
    if (first >= 0) {
        if (looks_cursor < 0 && cursor < known &&
            menu::service_is_section(static_cast<std::uint32_t>(rows[cursor].setting_id))) {
            gui_set_cursor(list, static_cast<std::uint32_t>(first));
        }
        field<std::int32_t>(list, game::kListStartRow) = first;
    }
    // The row looks' rule for a cursor row that opens a list (sub_141E8E600;
    // on a fresh fill they ran before the pick rows were flagged): Confirm on,
    // arrows off, and its list shown while the panel or the list has the focus.
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
    // The arrows over the cursor row start from its value (the game's row
    // looks set it only for its own rows).
    if (g_arrow_value != nullptr && cursor < known) {
        char* cursor_object = tab + (cursor + 1) * game::kRowObjectSize;
        const auto id = static_cast<std::uint32_t>(rows[cursor].setting_id);
        void* arrows = field<void*>(tab, game::kTabNumInput);
        if (arrows != nullptr && is_cmfm_row(rows[cursor].kind, id) && !menu::service_is_section(id) &&
            field<std::uint8_t>(cursor_object, game::kRowIsSlider) == 0 &&
            field<std::uint8_t>(cursor_object, game::kRowOpensList) == 0) {
            std::uint8_t& guard = field<std::uint8_t>(tab, game::kTabArrowGuard);  // as the row looks do
            const std::uint8_t was = guard;
            guard = 1;
            g_arrow_value(arrows, static_cast<std::int32_t>(menu::service_get(id)));
            guard = was;
        }
    }
    update_scrollbars();
}

// A row label left in its box, or centred over the row (headings, and plain
// buttons like the game's "Advanced Graphics Settings"): its layout and box
// width, then the
// request to lay the text out again. A label that is neither is left alone.
void centre_label(void* label, bool centred) {
    std::uint8_t& layout = field<std::uint8_t>(label, game::kMessageLayout);
    float& width = field<float>(label, game::kMessageWidth);
    if ((layout != game::kLabelLeft && layout != game::kLabelCentred) ||
        (width != game::kLabelWidth && width != game::kCentredLabelWidth)) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            logf(AL_LOG_WARNING, "Mods page: a row label is not as expected (layout %u, width %.1f); not centred",
                 static_cast<unsigned>(layout), static_cast<double>(width));
        }
        return;
    }
    const std::uint8_t want_layout = centred ? game::kLabelCentred : game::kLabelLeft;
    const float want_width = centred ? game::kCentredLabelWidth : game::kLabelWidth;
    if (layout != want_layout || width != want_width) {
        layout = want_layout;
        width = want_width;
        field<std::uint32_t>(label, game::kMessageFlags) |= game::kMessageRelayout;
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

// The line above the panel: the highlighted mod's title, version and "Restart
// needed" while one of its changes waits (menu::Menu::tab_header).
void show_panel_header(void* unit) {
    set_instance_text(unit, game::kPanelHeaderName, game::kTitleText,
                      menu::service_tab_header(g_selected.load(std::memory_order_relaxed)));
}

// The panel shows the mod list's selection (ModList's shown mod and page).
// `keep_focus` (Q / E inside the panel): the panel keeps the focus, its
// cursor on the new page's first row.
void show_selected(void* unit, bool keep_focus) {
    const std::vector<menu::Mod>& mods = menu::service_mods();
    if (g_list.shown_mod() >= mods.size() || g_list.shown_page() >= mods[g_list.shown_mod()].tabs.size()) {
        return;
    }
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    g_selected.store(mods[g_list.shown_mod()].tabs[g_list.shown_page()], std::memory_order_relaxed);
    show_mod_list();
    show_tabs();
    fill_split_panel(unit);
    if (keep_focus && focus == 1 && g_set_focus != nullptr) {
        g_set_focus(unit, 1);
        if (void* list = panel_list(unit)) {
            const std::int32_t start = field<std::int32_t>(list, game::kListStartRow);
            if (start >= 0) {
                gui_set_cursor(list, static_cast<std::uint32_t>(start));
                decorate_panel(unit, start);
            }
        }
    }
}

// The panel takes the focus (the game's own category Confirm).
void enter_panel(void* unit) {
    void* owner = static_cast<char*>(unit) + game::kOptCategories;
    PanelFocusFn enter = g_list_confirm ? g_category_confirm_next
                                        : focus_function(owner, game::kEnterPanelSlot, game::kRvaEnterPanel,
                                                         "the category Confirm");
    if (enter != nullptr) {
        enter(owner, static_cast<std::int32_t>(game::kSplitPanel));
    }
}

// The list takes the focus back (the panel's own Back, tab slot 9).
void leave_panel(void* unit) {
    char* tab = tab_object(unit, game::kSplitPanel);
    if (PanelFocusFn leave = focus_function(tab, game::kLeavePanelSlot, game::kRvaLeavePanel, "a panel's Back")) {
        leave(tab, 0);
    }
}

// Q / E (pad LB / RB): the shown mod's previous or next page, from the mod
// list or inside the panel (which keeps the focus); not while a key binding
// waits, a question is open or the pick list has the focus.
void page_keys(void* unit) {
    const al_input_t* input = g_env.input;
    bool held[2] = {false, false};
    if (input != nullptr) {
        constexpr std::uint32_t kQ = 0x51, kE = 0x45;
        held[0] = input->key_down(g_env.self, kQ) != 0;
        held[1] = input->key_down(g_env.self, kE) != 0;
        for (std::uint32_t slot = 0; slot < 4; ++slot) {
            al_pad_state_t pad = {};
            pad.size = sizeof pad;
            if (input->pad_state(g_env.self, slot, &pad) == AL_OK && pad.connected != 0) {
                held[0] = held[0] || (pad.buttons & AL_PAD_LEFT_SHOULDER) != 0;
                held[1] = held[1] || (pad.buttons & AL_PAD_RIGHT_SHOULDER) != 0;
            }
        }
    }
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    const bool usable = g_key_wait.id < 0 && g_pending_button < 0 && (focus == 0 || focus == 1);
    for (int d = 0; d < 2; ++d) {
        const bool pressed = held[d] && !g_page_keys[d];
        g_page_keys[d] = held[d];
        if (pressed && usable && g_list.step_page(d == 0 ? -1 : 1).changed) {
            g_tab_hover = -1;  // the tabs' cursor goes to the shown page until the pointer moves
            g_highlight = g_list.shown_mod();
            keep_highlight_in_view();
            show_selected(unit, true);
            request_split_help(unit);
        }
    }
}

// The tab list reads keys (or stops); the GUI marks an active list for input
// from the next frame on.
void tab_list_input(bool on) {
    if (void* tabs = g_tab_list.load(std::memory_order_acquire)) {
        field<std::uint8_t>(tabs, game::kListInactive) = on ? 0 : 1;
    }
}

// The page tabs take the keys (Up from the panel's first row): the page's
// focus goes to the list side (the panel's own Back), the category list stops
// reading keys and the tab list starts (registered, so the GUI keeps the
// panels' side bright); the tabs' cursor starts on the shown page.
void enter_tab_keys(void* unit) {
    leave_panel(unit);
    if (void* categories = field<void*>(unit, game::kOptCategoryList)) {
        field<std::uint8_t>(categories, game::kListInactive) = 1;
    }
    tab_list_input(true);
    g_tab_keys = true;
    g_tab_mark = g_list.shown_page();
    g_tab_hover = -1;
    place_tab_cursor();
    request_split_help(unit);
}

// The tabs give the keys back: to the panel (Down, Confirm, the mouse) or to
// the mod list (Back). The category list reads keys again from the next
// frames on (a list reads only after the GUI marked it for input), so the key
// that left does not reach it too. The tabs' window and cursor go back to the
// shown page.
void leave_tab_keys(void* unit, bool to_panel) {
    g_tab_keys = false;
    tab_list_input(false);
    if (void* categories = field<void*>(unit, game::kOptCategoryList)) {
        field<std::uint8_t>(categories, game::kListInactive) = 0;
    }
    if (to_panel) {
        enter_panel(unit);
    }
    show_tabs(true);
    request_split_help(unit);
}

// The tabs' cursor to `page` (the window following), with the game's cursor
// sound: moveCursor makes a sound only for a real change, so a cursor whose
// row stays (the window moved under it) is first told it came from the next
// row.
void mark_tab(std::size_t page) {
    void* list = g_tab_list.load(std::memory_order_acquire);
    const std::size_t pages = g_list.page_count(g_list.shown_mod());
    if (list == nullptr || page >= pages) {
        return;
    }
    g_tab_mark = page;
    const std::size_t count = field<std::uint32_t>(list, game::kListRowCount);
    const std::size_t top = tab_window_top(g_tab_top, page, pages, count);
    if (top != g_tab_top) {
        scroll_tabs(top);  // the labels; place_tab_cursor puts the cursor on the mark
    }
    const auto row = static_cast<std::int32_t>(page - g_tab_top);
    if (g_list_move_next != nullptr && count > 1) {
        std::int32_t& cursor = field<std::int32_t>(list, game::kListCursor);
        if (cursor == row) {
            cursor = row + 1 < static_cast<std::int32_t>(count) ? row + 1 : row - 1;
        }
        g_list_move_next(list, row);
    }
    place_tab_cursor();
}

// Every frame while the page shows CMFM's menu: keyboard and pad on the page
// tabs, read through the game's menu input (the player's bindings; the stick
// too; Left / Right repeat while held). Left / Right move the tabs' cursor
// over all pages (wrapping), Confirm shows the marked page and goes into it,
// Down goes back into the panel on the shown page, Back to the mod list.
// Anything else that moves the focus, a mod without tabs, a key wait or a
// question ends it.
void tab_keys(void* unit) {
    if (g_menu_key == nullptr || g_menu_direction == nullptr) {
        return;
    }
    const std::int32_t focus = field<std::int32_t>(unit, game::kOptFocus);
    if (g_tab_keys_wanted) {
        g_tab_keys_wanted = false;
        if (!g_tab_keys && focus == 1 && g_tabs_shown && g_key_wait.id < 0 && g_pending_button < 0) {
            enter_tab_keys(unit);
        }
        return;
    }
    if (!g_tab_keys) {
        return;
    }
    if (focus != 0) {
        // The focus moved (a click): the game set the page's lists for it.
        g_tab_keys = false;
        tab_list_input(false);
        show_tabs(true);
        request_split_help(unit);
        return;
    }
    if (!g_tabs_shown || g_key_wait.id >= 0 || g_pending_button >= 0) {
        leave_tab_keys(unit, false);
        return;
    }
    const std::size_t pages = g_list.page_count(g_list.shown_mod());
    std::int32_t sides = game::kMenuSourcesLeftRight;
    std::int32_t repeat = game::kMenuModeRepeat;
    const std::uint32_t side = g_menu_direction(unit, &sides, &repeat, nullptr);
    if ((side == game::kMenuLeft || side == game::kMenuRight) && pages > 1) {
        mark_tab(side == game::kMenuLeft ? (g_tab_mark + pages - 1) % pages : (g_tab_mark + 1) % pages);
        return;
    }
    std::int32_t updown = game::kMenuSourcesUpDown;
    std::int32_t pressed = game::kMenuModePressed;
    void* tabs = g_tab_list.load(std::memory_order_acquire);
    if (g_menu_key(unit, game::kMenuKeyConfirm, tabs) != 0) {
        if (g_list.show_page(g_tab_mark).changed) {
            show_selected(unit, false);
        }
        leave_tab_keys(unit, true);
    } else if (g_menu_direction(unit, &updown, &pressed, nullptr) == game::kMenuDown) {
        leave_tab_keys(unit, true);
    } else if (g_menu_key(unit, game::kMenuKeyBack, nullptr) != 0) {
        leave_tab_keys(unit, false);
    }
}

// The category list's Confirm on CMFM's page (slot 8 of the categories
// object, CMFM's copy of its vtable): the mod list's highlighted row is
// activated (a mod's dropdown opens), then the game's Confirm gives the panel
// the focus.
std::uint64_t category_confirm_detour(void* categories, std::int32_t category) {
    void* unit = field<void*>(categories, game::kCategoriesUnit);
    if (split_view() && unit == g_view_unit.load(std::memory_order_acquire) &&
        g_mod_list.load(std::memory_order_acquire) != nullptr && g_key_wait.id < 0 &&
        !activate_row(unit, g_highlight, false)) {
        return 0;
    }
    return g_category_confirm_next(categories, category);
}

// CMFM's page gets CMFM's copy of the categories object's vtable (slot 8
// replaced); false (logged once) if the object does not hold the game's
// vtable with the Confirm expected: Confirm then only gives the panel the
// focus (dropdowns open with a click or Q / E).
bool own_categories(void* unit) {
    void* owner = static_cast<char*>(unit) + game::kOptCategories;
    void* const*& vtable = field<void* const*>(owner, 0);
    if (vtable == g_categories_vtable) {
        return true;
    }
    auto* const* game_vtable = reinterpret_cast<void* const*>(g_env.base + game::kRvaCategoriesVtable);
    if (vtable != game_vtable ||
        reinterpret_cast<std::uintptr_t>(game_vtable[game::kEnterPanelSlot]) != g_env.base + game::kRvaEnterPanel) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            logf(AL_LOG_WARNING, "Mods page: the category list's Confirm is not where expected; the mod list selects "
                 "as its cursor moves");
        }
        return false;
    }
    if (g_category_confirm_next == nullptr) {
        std::memcpy(g_categories_vtable, game_vtable, sizeof g_categories_vtable);
        g_category_confirm_next = reinterpret_cast<PanelFocusFn>(game_vtable[game::kEnterPanelSlot]);
        g_categories_vtable[game::kEnterPanelSlot] = reinterpret_cast<void*>(&category_confirm_detour);
    }
    vtable = g_categories_vtable;
    return true;
}

// Unload: CMFM's page gets the game's vtable back.
void release_categories() {
    void* page = page_alive();
    if (page == nullptr) {
        return;
    }
    void* const*& vtable = field<void* const*>(static_cast<char*>(page) + game::kOptCategories, 0);
    if (vtable == g_categories_vtable) {
        vtable = reinterpret_cast<void* const*>(g_env.base + game::kRvaCategoriesVtable);
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
    void* mod_list = nullptr;
    void* tab_list = nullptr;
    if (g_split.load(std::memory_order_relaxed)) {
        mod_list = gui_get_instance(unit, game::kModListName);
        if (mod_list == nullptr || field<void*>(mod_list, 0) != field<void*>(categories, 0) ||
            field<std::uint32_t>(mod_list, game::kListDuplicateNum) == 0) {
            logf(AL_LOG_WARNING, "Mods page: the layout has no mod list (%s)", game::kModListName);
            menu::service_abort();
            return false;
        }
        if (mod_list != g_list_window_of) {
            // A list CMFM has not filled yet: its DuplicateNum is still the layout's.
            g_list_window_of = mod_list;
            g_list_window.store(field<std::uint32_t>(mod_list, game::kListDuplicateNum), std::memory_order_relaxed);
        }
        // The page tabs, a list of the same class; read its window the same way.
        tab_list = gui_get_instance(unit, game::kTabListName);
        if (tab_list == nullptr || field<void*>(tab_list, 0) != field<void*>(categories, 0)) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                logf(AL_LOG_WARNING, "Mods page: the layout has no page tabs (%s); Q / E switch pages",
                     game::kTabListName);
            }
            tab_list = nullptr;
        } else if (tab_list != g_tab_window_of) {
            g_tab_window_of = tab_list;
            g_tab_window = field<std::uint32_t>(tab_list, game::kListDuplicateNum);
        }
        // Its cursor, hidden while the shown page is scrolled out of the
        // window; shown again (setPlay acts only on a change) when placed.
        g_tab_cursor = tab_list != nullptr ? gui_get_instance(unit, game::kTabCursorName) : nullptr;
        g_tab_cursor_shown = false;
        g_tab_hover = -1;
        // One entry per mod; the last selection again, if it still exists.
        const std::vector<menu::Mod>& mods = menu::service_mods();
        std::vector<std::size_t> pages;
        for (const menu::Mod& m : mods) {
            pages.push_back(m.tabs.size());
        }
        g_list.reset(std::move(pages));
        const std::size_t last = g_selected.load(std::memory_order_relaxed);
        bool kept = false;
        if (last < g_entries.load(std::memory_order_relaxed)) {
            const std::size_t m = menu::service_mod_of_tab(last);
            for (std::size_t p = 0; m < mods.size() && p < mods[m].tabs.size(); ++p) {
                kept = kept || (mods[m].tabs[p] == last && g_list.select(m, p));
            }
        }
        if (!kept) {
            g_selected.store(0, std::memory_order_relaxed);
            g_mod_top = 0;
        }
        g_highlight = g_list.shown_mod();
        g_page_keys[0] = g_page_keys[1] = true;  // a key held while the page opens does not count
        g_list_confirm = own_categories(unit);
    }
    if (!prepare_rebuild(unit)) {
        menu::service_abort();
        return false;
    }
    g_view.store(true, std::memory_order_release);  // the text hook answers the category labels
    if (g_split.load(std::memory_order_relaxed)) {
        load_panel_rows(0);  // every panel holds the highlighted tab's window
    }
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
    // Split mode: the category list is off screen and its cursor stays on the
    // panel's row (kSplitPanel), the row the game enters the panel from; every
    // tab object holds the highlighted tab. That row must exist also with fewer
    // pages: with the Overview and one mod's page (two rows) the game would not
    // put the cursor there, and the panel never took the focus (no click, no
    // key; a client mod's report, 2026-10-17).
    std::uint32_t rows = used;
    if (mod_list != nullptr && rows < game::kSplitPanel + 1) {
        rows = static_cast<std::uint32_t>(game::kSplitPanel + 1);
    }
    gui_set_row_count(categories, rows);
    for (std::uint32_t t = 0; t < used; ++t) {
        set_row_text(list_row(categories, t), "msg_1", nullptr, model->tab_name(t));
    }
    if (field<std::uint32_t>(categories, game::kListRowCount) != rows) {
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
        if (tab_list != nullptr) {
            field<std::uint8_t>(tab_list, game::kListInactive) = 1;  // the same for the page tabs
        }
        g_tab_list.store(tab_list, std::memory_order_release);
        options_view_sliders(g_slider_from_setting != nullptr);  // number rows as sliders
        g_pick_count = 0;
        g_pick_filled = -1;
        const std::size_t visible = g_list_window.load(std::memory_order_relaxed);
        keep_highlight_in_view();
        show_mod_list();
        show_tabs();
        logf(AL_LOG_INFO, "Mods page: mod list on (%zu mods, %zu pages, %zu rows visible, %zu tabs; %s)",
             menu::service_mods().size(), entries, visible, tab_list != nullptr ? g_tab_window : 0,
             g_list_confirm ? "the cursor shows, click or Confirm enters" : "the cursor shows, Confirm without the list");
    } else if (entries > game::kTitleViewTabs) {
        start_window(unit, categories);
    }
    set_header(unit, model->header());
    logf(AL_LOG_INFO, "Mods page: on (%zu categories, %zu rows)", entries, model->row_count());
    measure::page_shown(true);
    return true;
}

// The page opened from the Start menu: the menu closes, the game stays in
// menu mode (a child page open) and the page holds the hunter.
void leave_start_menu() {
    void* menu = game_ref_alive(g_return_menu);
    if (menu == nullptr) {
        return;
    }
    g_hold_player.store(true, std::memory_order_relaxed);
    field<std::uint8_t>(menu, game::kStartMenuChildOpen) = 1;
    gui_request_close(menu);
}

// CMFM's page, vtable slot 90: the open handler.
std::uint64_t page_open(void* unit) {
    // Kept while open: a scene change then closes it the game's way (message
    // 75, slot 72: the close handler runs) instead of killing it.
    page_keep(unit, true);
    if (g_leave_menu) {
        g_leave_menu = false;
        leave_start_menu();
    }
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
        show_tabs();
        g_restart_seen = menu::service_restart_revision();
        g_lock_seen = menu::service_lock_revision();
        g_decorated_focus = field<std::int32_t>(unit, game::kOptFocus);
        decorate_panel(unit);
        show_restart_line(unit);
        show_panel_header(unit);
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
    // Not kept once closing: a page opened from the Start menu is then killed
    // by the game when it has closed (kGuiStateKillOnClose) and leaves sMhGUI's
    // unit list; the title's page dies with the title scene.
    page_keep(unit, false);
    const std::uint64_t result = page_next(game::kSlotClose)(unit);
    g_hold_player.store(false, std::memory_order_relaxed);  // slot 81 stops running here too
    if (g_tab_keys) {
        // The category list reads keys again on the next opening, the tabs not.
        if (void* categories = field<void*>(unit, game::kOptCategoryList)) {
            field<std::uint8_t>(categories, game::kListInactive) = 0;
        }
        tab_list_input(false);
    }
    g_mod_list.store(nullptr, std::memory_order_release);
    g_tab_list.store(nullptr, std::memory_order_release);
    g_tabs_shown = false;
    g_tab_hover = -1;
    g_tab_cursor = nullptr;
    g_tab_keys = false;
    g_tab_keys_wanted = false;
    g_mod_hover = false;
    g_slider_count = 0;
    g_pick_count = 0;
    g_pick_filled = -1;
    g_pending_button = -1;
    if (g_key_wait.id >= 0) {
        end_key_wait(nullptr, false, 0, "the page closed");
    }
    g_row_help = -1;
    g_bars_unit = nullptr;  // looked up again on the next open
    for (ScrollBar* b : {&g_list_bar, &g_panel_bar, &g_tab_bar}) {
        if (b->bar != nullptr) {
            field<std::uint8_t>(b->bar, game::kScrollDragging) = 0;  // a drag does not survive a reopen
        }
        b->shown = false;
    }
    options_view_sliders(false);
    if (g_view.exchange(false, std::memory_order_acq_rel)) {
        menu::service_closed();
        logf(AL_LOG_INFO, "Mods page: closed");
        measure::page_shown(false);
        if (const std::string m = measure::report(); !m.empty()) {
            logf(AL_LOG_INFO, "%s", m.c_str());
        }
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
//
// The game's tab show (sub_141E8C6E0, only ever called by showCategory, when a
// panel that was hidden shows) places the knob of every row object marked
// as a slider from the save data by setting id (sUserData + 0x100 + id, any
// id from 32 reading 4 GB past it): crash 20261012-030509, CMFM's page
// opened in game. So CMFM's slider rows are plain rows while it runs (all 24
// row objects of the split panel), and sliders again after it.
std::uint64_t show_category_detour(void* categories, std::int32_t category) {
    void* unit = field<void*>(categories, game::kCategoriesUnit);
    const bool our_page = unit != nullptr && unit == page_alive();
    const bool ours = our_page && g_view.load(std::memory_order_acquire);
    const bool sliders = ours && split_view();
    if (our_page && g_split.load(std::memory_order_relaxed)) {  // also while the page is being filled
        end_sliders(unit, true);
        char* tab = tab_object(unit, game::kSplitPanel);
        for (std::size_t r = 0; r < game::kTabRowObjects; ++r) {
            char* row_object = tab + (r + 1) * game::kRowObjectSize;
            const auto* entry = field<const GameOptionRow*>(row_object, game::kRowDef);
            if (entry != nullptr && is_cmfm_row(entry->kind, static_cast<std::uint32_t>(entry->setting_id))) {
                field<std::uint8_t>(row_object, game::kRowIsSlider) = 0;
            }
        }
    }
    // The tab show sets the panel list's cursor and runs the row looks before
    // its knob loop (crash 20261012-034753: CMFM's row looks hook marked the
    // sliders again in between): no slider marks until it returns.
    const bool outer = !g_showing_category;
    g_showing_category = true;
    const std::uint64_t result = g_show_category_next(categories, category);
    if (outer) {
        g_showing_category = false;
    }
    if (ours) {
        void* vram = field<void*>(unit, game::kOptVramPanel);
        if (vram != nullptr) {
            gui_set_play(vram, false);
        }
    }
    if (sliders && outer) {
        decorate_panel(unit);
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
        decorate_panel(unit, static_cast<std::int32_t>(cursor));
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

// A button pressed: the mod's action, then every row of the panel read and
// written again the game's way (the mod may have changed any value or text).
void press_button(void* unit, std::uint32_t id) {
    menu::service_press(id);
    if (g_tab_refresh != nullptr) {
        g_tab_refresh(tab_object(unit, game::kSplitPanel));
    }
}

// Confirm on a button: pressed now, or after the game's Yes/No question,
// asked the way the title page's Default asks (its lambda carries the
// answer; the answer hook below takes it for CMFM's page).
void ask_button(void* unit, std::uint32_t id) {
    const char* question = menu::service_button_question(id);
    if (question == nullptr) {
        press_button(unit, id);
        return;
    }
    void* manager = g_gui_manager != nullptr ? *g_gui_manager : nullptr;
    const auto* lambda = reinterpret_cast<const std::uintptr_t*>(g_env.base + game::kRvaDefaultLambdaVtable);
    if (g_yes_no_dialog == nullptr || manager == nullptr || lambda[0] != g_env.base + game::kRvaDefaultLambdaCopy ||
        lambda[2] != g_env.base + game::kRvaDefaultLambdaCall) {
        logf(AL_LOG_WARNING, "Mods page: no Yes/No question available; the button is not pressed");
        return;
    }
    std::snprintf(g_question, sizeof g_question, "%s", question);  // the request keeps the pointer
    alignas(16) unsigned char request[game::kYesNoSize] = {};
    const auto* vector = reinterpret_cast<const float*>(g_env.base + game::kRvaDialogVector);
    field<const char*>(request, game::kYesNoText) = g_question;
    field<std::int32_t>(request, game::kYesNoX) = *reinterpret_cast<const std::int32_t*>(g_env.base + game::kRvaScreenWidth) / 2;
    field<std::int32_t>(request, game::kYesNoY) = *reinterpret_cast<const std::int32_t*>(g_env.base + game::kRvaScreenHeight) / 2;
    for (std::size_t i = 0; i < 3; ++i) {
        field<float>(request, game::kYesNoVector + 4 * i) = vector[i];
    }
    field<std::uint32_t>(request, game::kYesNoKind) = game::kYesNoKindYesNo;
    field<const void*>(request, game::kYesNoAnswer) = lambda;
    field<void*>(request, game::kYesNoAnswer + 8) = static_cast<char*>(unit) + game::kOptCategories;
    field<void*>(request, game::kYesNoAnswerImpl) = request + game::kYesNoAnswer;
    g_pending_button = static_cast<std::int32_t>(id);
    g_yes_no_dialog(manager, request);  // copies the request
}

// A panel row's value text written again the way the game's tab refresh
// writes it (the text filler without the label, no row looks).
void rewrite_value(void* unit, std::uint32_t id) {
    char* tab = tab_object(unit, game::kSplitPanel);
    for (std::size_t r = 0; r < panel_visible(); ++r) {
        if (static_cast<std::uint32_t>(g_panel_rows[r].setting_id) != id) {
            continue;
        }
        char* row_object = tab + (r + 1) * game::kRowObjectSize;
        void* row = field<void*>(row_object, game::kRowInstance);
        void* value = field<void*>(row_object, game::kRowValueObject);
        if (g_row_text != nullptr && row != nullptr && value != nullptr) {
            g_row_text(row, nullptr, value, &g_panel_rows[r]);
        }
    }
}

// The help bar shows a panel row's help line again (it changed in place).
void request_row_help(void* unit, std::uint32_t id) {
    void* manager = g_gui_manager != nullptr ? *g_gui_manager : nullptr;
    if (g_help_request == nullptr || manager == nullptr) {
        return;
    }
    g_row_help = static_cast<std::int32_t>(id);
    g_row_help_flip ^= 1;
    g_help_request(manager, unit, static_cast<std::int32_t>(game::kRowHelpIds[g_row_help_flip]),
                   static_cast<std::int32_t>(game::kHelpFile), 0.0f, 0);
}

const al_input_t* key_input(al_input_status_t& status) {
    const al_input_t* input = g_env.input;
    status = {};
    status.size = sizeof status;
    if (input == nullptr || input->size < offsetof(al_input_t, status) + sizeof input->status) {
        return nullptr;
    }
    input->status(g_env.self, &status);
    return input;
}

// Confirm on a KEY row: the row says "Press a key", its help line how; the
// game's input is captured (keyboard, mouse and pads, as Astera Loader can)
// so nothing the player presses reaches the menu meanwhile.
void begin_key_wait(void* unit, const GameOptionRow& entry) {
    const auto id = static_cast<std::uint32_t>(entry.setting_id);
    al_input_status_t status;
    const al_input_t* input = key_input(status);
    if (input == nullptr || status.state != AL_INPUT_RUNNING || (status.devices & AL_INPUT_KEYBOARD) == 0) {
        logf(AL_LOG_WARNING, "Mods page: key bindings need Astera Loader's keyboard capture (%s)",
             input == nullptr ? "no al.input" : status.reason != nullptr && status.reason[0] ? status.reason : "not running");
        return;
    }
    if (!menu::service_begin_key(id)) {
        return;  // locked (logged)
    }
    if (input->capture(g_env.self, status.devices & AL_INPUT_ALL) != AL_OK) {
        menu::service_cancel_key(id);
        logf(AL_LOG_WARNING, "Mods page: the input capture for a key binding failed (see the log above)");
        return;
    }
    KeyWait& w = g_key_wait;
    w = KeyWait{};
    w.id = static_cast<std::int32_t>(id);
    w.end = std::chrono::steady_clock::now() + std::chrono::seconds(kKeyWaitSeconds);
    w.seconds = kKeyWaitSeconds;
    for (std::uint32_t vk = 0; vk < 256; ++vk) {
        if (input->key_down(g_env.self, vk) == 0) {
            w.armed[vk / 64] |= 1ull << (vk % 64);
        }
    }
    for (std::uint32_t slot = 0; slot < 4; ++slot) {
        al_pad_state_t pad = {};
        pad.size = sizeof pad;
        const bool read = input->pad_state(g_env.self, slot, &pad) == AL_OK && pad.connected != 0;
        w.pad_armed[slot] = read ? ~static_cast<std::uint32_t>(pad.buttons) : 0xFFFFFFFFu;
    }
    rewrite_value(unit, id);
    request_row_help(unit, id);
    logf(AL_LOG_INFO, "Mods page: waiting for a key (setting %u)", id);
}

// Every frame of a wait: Esc, a left or right click or a pad's B cancels;
// Backspace sets no key; the first other key (pressed after the wait began)
// is the binding, with the Ctrl, Shift and Alt held with it. Ten seconds
// without one cancel too, as does losing the capture (the game lost focus).
void poll_key_wait(void* unit) {
    KeyWait& w = g_key_wait;
    al_input_status_t status;
    const al_input_t* input = key_input(status);
    if (input == nullptr || (status.captured & AL_INPUT_KEYBOARD) == 0) {
        end_key_wait(unit, false, 0, "the capture ended (the game lost the focus?)");
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= w.end) {
        end_key_wait(unit, false, 0, "no key in 10 seconds");
        return;
    }
    const auto left = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(w.end - now).count() / 1000 + 1);
    if (left != w.seconds) {
        w.seconds = left;
        menu::service_key_wait(static_cast<std::uint32_t>(w.id), left);
        rewrite_value(unit, static_cast<std::uint32_t>(w.id));
    }
    // Keys pressed since the wait began: held now, and seen up before.
    std::uint64_t fresh[4] = {};
    for (std::uint32_t vk = 1; vk < 256; ++vk) {
        const std::uint64_t bit = 1ull << (vk % 64);
        if (input->key_down(g_env.self, vk) == 0) {
            w.armed[vk / 64] |= bit;
        } else if ((w.armed[vk / 64] & bit) != 0) {
            fresh[vk / 64] |= bit;
        }
    }
    const auto pressed = [&](std::uint32_t vk) { return (fresh[vk / 64] & (1ull << (vk % 64))) != 0; };
    bool pad_back = false;
    for (std::uint32_t slot = 0; slot < 4; ++slot) {
        al_pad_state_t pad = {};
        pad.size = sizeof pad;
        if (input->pad_state(g_env.self, slot, &pad) != AL_OK || pad.connected == 0) {
            continue;
        }
        const std::uint32_t buttons = pad.buttons;
        pad_back = pad_back || (buttons & w.pad_armed[slot] & AL_PAD_B) != 0;
        w.pad_armed[slot] |= ~buttons;
    }
    constexpr std::uint32_t kEsc = 0x1B, kBackspace = 0x08, kLeftClick = 0x01, kRightClick = 0x02;
    if (pressed(kEsc) || pressed(kLeftClick) || pressed(kRightClick) || pad_back) {
        end_key_wait(unit, false, 0, "cancelled");
        return;
    }
    if (pressed(kBackspace)) {
        end_key_wait(unit, true, 0, "no key");
        return;
    }
    for (std::uint32_t vk = 1; vk < 256; ++vk) {
        if (!pressed(vk) || !menu::key_bindable(vk)) {
            continue;
        }
        constexpr std::uint32_t kCtrl = 0x11, kShift = 0x10, kAlt = 0x12;  // either side (al.input)
        std::int32_t key = static_cast<std::int32_t>(vk);
        key |= input->key_down(g_env.self, kCtrl) != 0 ? CMFM_KEY_CTRL : 0;
        key |= input->key_down(g_env.self, kShift) != 0 ? CMFM_KEY_SHIFT : 0;
        key |= input->key_down(g_env.self, kAlt) != 0 ? CMFM_KEY_ALT : 0;
        end_key_wait(unit, true, key, "a key");
        return;
    }
}

// The wait ends: the key set (or refused, the reason in the help line) or the
// wait cancelled; the capture released (keys still held stay hidden from the
// game until released, so the key just bound does nothing in the menu). With
// the page (`unit`), every row is written again (an overlap may have come or
// gone on other rows) and the row's own help line comes back.
void end_key_wait(void* unit, bool set, std::int32_t key, const char* how) {
    const KeyWait w = g_key_wait;
    g_key_wait.id = -1;
    if (w.id < 0) {
        return;
    }
    const auto id = static_cast<std::uint32_t>(w.id);
    if (g_env.input != nullptr) {
        g_env.input->release(g_env.self);
    }
    const bool ok = set && menu::service_set_key(id, key);
    if (!set) {
        menu::service_cancel_key(id);
    }
    logf(AL_LOG_INFO, "Mods page: key wait for setting %u ended: %s%s", id, how, set && !ok ? " (refused)" : "");
    if (unit == nullptr) {
        return;
    }
    if (g_tab_refresh != nullptr) {
        g_tab_refresh(tab_object(unit, game::kSplitPanel));
    }
    decorate_panel(unit);
    request_row_help(unit, id);
}

// Confirm on a panel row: buttons and the pick list for CMFM's long CHOICE
// rows (the game's own decide does something only for its language and
// dialog rows).
void row_decide_detour(void* row) {
    const auto* entry = field<const GameOptionRow*>(row, game::kRowDef);
    void* unit = field<void*>(row, game::kRowObjectUnit);
    if (entry != nullptr && unit != nullptr && split_view() && unit == g_view_unit.load(std::memory_order_acquire) &&
        is_cmfm_row(entry->kind, static_cast<std::uint32_t>(entry->setting_id))) {
        const auto id = static_cast<std::uint32_t>(entry->setting_id);
        if (menu::service_is_button(id)) {
            ask_button(unit, id);
            return;
        }
        if (menu::service_is_key(id)) {
            begin_key_wait(unit, *entry);
            return;
        }
        if (open_pick_list(unit, *entry)) {
            return;
        }
    }
    g_row_decide_next(row);
}

// The Default question's answer. On CMFM's page (which never asks it) it is
// a button's answer: Yes presses the button; the game's handler (a reset of
// every option) never runs for it.
void default_answer_detour(void* const* capture, std::uint32_t unknown, std::int32_t answer) {
    void* page = page_alive();
    if (capture != nullptr && page != nullptr && *capture == static_cast<char*>(page) + game::kOptCategories) {
        const std::int32_t id = g_pending_button;
        g_pending_button = -1;
        logf(AL_LOG_INFO, "Mods page: button question answered %s", answer == 0 ? "Yes" : "No");
        if (answer == 0 && id >= 0 && g_view.load(std::memory_order_acquire)) {
            press_button(page, static_cast<std::uint32_t>(id));
        }
        return;
    }
    g_default_answer_next(capture, unknown, answer);
}

// The enabled check for CMFM's rows: locked rows are greyed by the game.
std::uint8_t enabled_check_detour(const GameOptionRow* row, std::int32_t* reason, std::uint8_t flag) {
    if (row != nullptr && is_cmfm_row(row->kind, static_cast<std::uint32_t>(row->setting_id))) {
        return menu::service_locked(static_cast<std::uint32_t>(row->setting_id)) ? 0 : 1;
    }
    return g_enabled_check_next(row, reason, flag);
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

// The game's direction reader, from its call in a list's direction check.
bool find_menu_direction(const Env& env, std::string& why) {
    void* match = nullptr;
    if (!find_game_function(env, "menu direction", game::kMenuDirectionCallerPattern, &match, why)) {
        return false;
    }
    const auto* code = static_cast<const std::uint8_t*>(match);
    g_menu_direction = reinterpret_cast<MenuDirectionFn>(rip_target(code, game::kMenuDirectionCall + 5));
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
        {"CMFM:OptionRow::isEnabled", game::kEnabledCheckPattern, reinterpret_cast<void*>(&enabled_check_detour),
         reinterpret_cast<void**>(&g_enabled_check_next)},
        {"CMFM:uGUIOption::defaultAnswer", game::kDefaultAnswerPattern, reinterpret_cast<void*>(&default_answer_detour),
         reinterpret_cast<void**>(&g_default_answer_next)},
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
         "Mods page: hooks registered (Options show category; list cursor moves; row looks; row and pick decide; "
         "row enabled; Default answer)");
    const Spec steps[2] = {
        {"CMFM:cGUIInstButtonList::stepDown", game::kListStepDownPattern, reinterpret_cast<void*>(&step_down_detour),
         reinterpret_cast<void**>(&g_step_down_next)},
        {"CMFM:cGUIInstButtonList::stepUp", game::kListStepUpPattern, reinterpret_cast<void*>(&step_up_detour),
         reinterpret_cast<void**>(&g_step_up_next)},
    };
    std::string window_why;
    g_panel_window_on = find_game_function(env, "row text filler", game::kRowTextPattern,
                                           reinterpret_cast<void**>(&g_row_text), window_why);
    if (!g_panel_window_on) {
        logf(AL_LOG_WARNING, "Mods page: %s; long pages show all their rows", window_why.c_str());
    }
    for (int i = 0; i < 2 && g_panel_window_on; ++i) {
        al_hook_t h = {sizeof(al_hook_t)};
        h.name = steps[i].name;
        h.pattern = steps[i].pattern;
        h.detour = steps[i].detour;
        h.next = steps[i].next;
        g_step_hooks[i] = env.hooks->hook(env.self, &h);
        if (g_step_hooks[i] == 0) {
            if (i == 1) {
                env.hooks->set_enabled(env.self, g_step_hooks[0], 0);
            }
            g_panel_window_on = false;
            logf(AL_LOG_WARNING, "Mods page: the hook %s was refused; long pages show all their rows", steps[i].name);
        }
    }
    std::string arrow_why;
    if (!find_game_function(env, "arrow value", game::kArrowValuePattern, reinterpret_cast<void**>(&g_arrow_value),
                            arrow_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; arrows may start from another row's value", arrow_why.c_str());
    }
    std::string bar_why;
    if (!find_game_function(env, "scroll bar mouse", game::kScrollMousePattern,
                            reinterpret_cast<void**>(&g_scroll_mouse), bar_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; the scrollbars only show", bar_why.c_str());
    }
    std::string mouse_why;
    if (!find_mouse(env, mouse_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; the mod list will not follow the mouse", mouse_why.c_str());
    }
    std::string keys_why;
    if (!find_game_function(env, "menu key", game::kMenuKeyPattern, reinterpret_cast<void**>(&g_menu_key), keys_why) ||
        !find_menu_direction(env, keys_why)) {
        g_menu_key = nullptr;
        g_menu_direction = nullptr;
        logf(AL_LOG_WARNING, "Mods page: %s; keys and pad reach the page tabs only through Q / E", keys_why.c_str());
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
    std::string button_why;
    if (!find_game_function(env, "tab refresh", game::kTabRefreshPattern, reinterpret_cast<void**>(&g_tab_refresh),
                            button_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; rows update later after a button", button_why.c_str());
    }
    if (!find_game_function(env, "Yes/No dialog", game::kYesNoDialogPattern,
                            reinterpret_cast<void**>(&g_yes_no_dialog), button_why)) {
        logf(AL_LOG_WARNING, "Mods page: %s; buttons with a question are not pressed", button_why.c_str());
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
    if (g_layout_resource != nullptr) {
        return;  // loaded and held already (a page is made for every visit in game)
    }
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
    logf(AL_LOG_INFO, "Mods page: CMFM's page made (unit %p, page %u)", page, g_pages_created);
    return page;
}

bool mods_page_open_from_start_menu(void* menu) {
    if (!g_ready.load(std::memory_order_acquire) || menu == nullptr ||
        field<std::uintptr_t>(menu, 0) != g_env.base + game::kRvaStartMenuVtable) {
        return false;
    }
    // In game the page lives only while it is open: made now (it opens after
    // its setup, on a later frame) and killed by the game when it has closed,
    // so it never sits in sMhGUI's unit list while Mods is closed.
    const bool made = page_alive() == nullptr;
    void* page = mods_page_ensure();
    if (page == nullptr || !unit_alive(page)) {
        return false;
    }
    field<std::uint32_t>(page, game::kGuiState) |= game::kGuiStateKillOnClose;
    // What the Start menu's own Options action (onItemAction case 71) does,
    // with CMFM's page for the game's Options page: open the page, mark a page
    // of the menu's own as open (the game stays in menu mode, so the hunter
    // does not move; sMhGUI+83664) and close the menu, the last two when the
    // page opens (leave_start_menu). The page's close handler opens the menu
    // again.
    field<std::int32_t>(menu, game::kStartMenuDialog) = game::kStartMenuNoDialog;
    field<std::int32_t>(menu, game::kStartMenuAction) = -1;
    game_ref_set(g_return_menu, menu);
    g_leave_menu = true;
    gui_request_open(page, nullptr);
    ++g_opened_in_game;
    logf(AL_LOG_INFO, "Start menu: Mods opens CMFM's page%s", made ? " (made now, released when it closes)" : "");
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
            return g_highlight_help.c_str();
        }
        if ((id == game::kRowHelpIds[0] || id == game::kRowHelpIds[1]) && g_row_help >= 0) {
            return menu::service_row_help(static_cast<std::uint32_t>(g_row_help));
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
    g_leave_menu = false;
    release_categories();
    page_release();
    logf(AL_LOG_INFO, "Mods page: done; %u page(s) made, opened from the Start menu %u time(s)", g_pages_created,
         g_opened_in_game);
}

}  // namespace cmfm::native
