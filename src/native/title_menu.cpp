#include "native/title_menu.hpp"

#include "native/data_patch.hpp"
#include "native/game_421810.hpp"
#include "native/options_model.hpp"
#include "native/options_view.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace cmfm::native {
namespace {

// Hooked.
using TitlePlayFn = std::uint64_t (*)(void* title);
using TitleCursorFn = std::uint64_t (*)(void* title, void* list, std::int32_t row);
using TitleDecidedFn = void (*)(void* title, std::int32_t row);
using OptionsOpenFn = std::uint64_t (*)(void* unit);
using OptionsCloseFn = std::uint64_t (*)(void* unit);
// Called.
using SetRowCountFn = void (*)(void* list, std::uint64_t count);
using SetCursorFn = std::uint64_t (*)(void* list, std::uint32_t row);
using GetObjectFn = void* (*)(void* root, const char* name);
using FormatTextFn = std::uint64_t (*)(void* object, const char* text);
using GetInstanceFn = void* (*)(void* unit, const char* name);
using SetPlayFn = void (*)(void* instance, bool play);
using BuildCategoriesFn = void (*)(void* categories, void* unit);

TitlePlayFn g_play_next = nullptr;
TitleCursorFn g_cursor_next = nullptr;
TitleDecidedFn g_decided_next = nullptr;
OptionsOpenFn g_open_next = nullptr;
OptionsCloseFn g_close_next = nullptr;
SetRowCountFn g_set_row_count = nullptr;
SetCursorFn g_set_cursor = nullptr;
GetObjectFn g_get_object = nullptr;
FormatTextFn g_format_text = nullptr;
GetInstanceFn g_get_instance = nullptr;
SetPlayFn g_set_play = nullptr;

enum Hook { kPlay, kCursor, kDecided, kOpen, kClose, kHookCount };
std::uint32_t g_hooks[kHookCount] = {};

Env g_env = {};
std::atomic<bool> g_ready{false};
// The title unit whose menu has the Mods row, and its Options row.
std::atomic<void*> g_entry_title{nullptr};
std::int32_t g_entry_options_row = -1;
// "Mods" was chosen: the next title Options open shows CMFM's rows.
std::atomic<bool> g_pending{false};
// The title Options view is on: CMFM's tables are swapped in.
std::atomic<bool> g_view{false};
// The uGUIOption unit whose lists hold CMFM's rows (rebuilt on its next plain open).
void* g_built_unit = nullptr;
std::uint32_t g_mods_selected = 0;
QwordPatch g_tabs[game::kTitleViewTabs];

template <class T>
T& field(void* base, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

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

void* list_row(void* list, std::uint32_t row) {
    if (list == nullptr || row >= field<std::uint32_t>(list, game::kListRowCount)) {
        return nullptr;
    }
    return field<void**>(list, game::kListRows)[row];
}

// Writes `text` into the named text objects of a list row.
bool set_row_text(void* row, const char* name, const char* name2, const char* text) {
    void* root = row != nullptr ? field<void*>(row, game::kInstRoot) : nullptr;
    if (root == nullptr) {
        return false;
    }
    bool any = false;
    for (const char* n : {name, name2}) {
        if (n == nullptr) {
            continue;
        }
        if (void* object = g_get_object(root, n)) {
            g_format_text(object, text);
            any = true;
        }
    }
    return any;
}

// ---- The title menu's Mods row ----

// The game's row for a row of the menu with the Mods row in it.
std::int32_t game_row(std::int32_t row) {
    const std::int32_t mods = g_entry_options_row + 1;
    if (row == mods) {
        return g_entry_options_row;  // Mods acts as Options
    }
    return row > mods ? row - 1 : row;
}

// After play() labelled rows 0..Quit and hid the rest: Mods goes below
// Options, Credits, Support and Quit move down one, the last row is shown.
void add_mods_row(void* title) {
    void* list = field<void*>(title, game::kTitleList);
    const std::uint32_t rows = list != nullptr ? field<std::uint32_t>(list, game::kListRowCount) : 0;
    const std::int32_t options = field<std::int32_t>(title, game::kTitleOptionsRow);
    const std::int32_t quit = field<std::int32_t>(title, game::kTitleQuitRow);
    if (rows != game::kTitleRows + 1 || options < 1 || quit != options + 3 ||
        static_cast<std::uint32_t>(quit) + 1 >= rows) {
        logf(AL_LOG_WARNING, "title: menu differs (rows %u, Options row %d, Quit row %d); no Mods row", rows, options,
             quit);
        return;
    }
    const char* texts[4] = {
        kModsLabel,
        game_text(game::kRvaMessages13, game::kTitleCreditsId),
        game_text(game::kRvaMessages13, game::kTitleSupportId),
        game_text(game::kRvaMessages23, game::kTitleQuitId),
    };
    for (std::int32_t k = 0; k < 4; ++k) {
        if (!set_row_text(list_row(list, static_cast<std::uint32_t>(options + 1 + k)), "msg_1", "msg_2",
                          texts[k] ? texts[k] : "")) {
            logf(AL_LOG_WARNING, "title: row %d has no label objects; no Mods row", options + 1 + k);
            return;  // the hidden last row keeps the menu as the game made it
        }
    }
    void* last = list_row(list, static_cast<std::uint32_t>(quit + 1));
    field<std::uint32_t>(last, game::kInstFlags) |= game::kInstFlagActive;  // play() cleared it
    g_set_play(last, true);
    g_entry_options_row = options;
    g_entry_title.store(title, std::memory_order_release);
    logf(AL_LOG_INFO, "title: Mods row added below Options (row %d of %u)", options + 1, rows);
}

std::uint64_t play_detour(void* title) {
    if (!g_ready.load(std::memory_order_acquire)) {
        return g_play_next(title);
    }
    g_entry_title.store(nullptr, std::memory_order_release);
    void* list = field<void*>(title, game::kTitleList);
    if (list != nullptr && field<std::uint32_t>(list, game::kListDuplicateNum) == game::kTitleRows &&
        field<std::uint32_t>(list, game::kListRowCount) == game::kTitleRows) {
        g_set_row_count(list, game::kTitleRows + 1);  // play() hides the rows after Quit
    }
    const std::uint64_t result = g_play_next(title);
    add_mods_row(title);
    return result;
}

std::uint64_t cursor_detour(void* title, void* list, std::int32_t row) {
    if (title == g_entry_title.load(std::memory_order_acquire)) {
        row = game_row(row);
    }
    return g_cursor_next(title, list, row);
}

void decided_detour(void* title, std::int32_t row) {
    if (title != g_entry_title.load(std::memory_order_acquire)) {
        g_pending.store(false, std::memory_order_release);
        g_decided_next(title, row);
        return;
    }
    if (row == g_entry_options_row + 1) {
        // Greyed out with Options (never on PC so far).
        void* options = list_row(field<void*>(title, game::kTitleList), static_cast<std::uint32_t>(g_entry_options_row));
        if (options == nullptr || (field<std::uint32_t>(options, game::kInstFlags) & game::kInstFlagEnabled) == 0) {
            return;
        }
        ++g_mods_selected;
        g_pending.store(true, std::memory_order_release);
        logf(AL_LOG_INFO, "title: Mods selected");
    } else {
        g_pending.store(false, std::memory_order_release);
    }
    g_decided_next(title, game_row(row));
}

// ---- The title Options page ----

// The page's lists must be rebuilt after a table swap. Before that: every
// cursor to row 0 (setRowCount keeps the cursor, and the page reads the row
// under it unchecked) and the row objects as the constructor leaves them
// (the game iterates all 24 and would use stale rows beyond the new count).
bool prepare_rebuild(void* unit) {
    void* categories = field<void*>(unit, game::kOptCategoryList);
    const auto cursor_to_top = [](void* list) {
        if (list == nullptr || field<std::uint32_t>(list, game::kListRowCount) == 0) {
            return;
        }
        if (field<std::uint32_t>(list, game::kListCursor) < field<std::uint32_t>(list, game::kListRowCount)) {
            g_set_cursor(list, 0);
        } else {
            field<std::uint32_t>(list, game::kListCursor) = 0;
        }
    };
    char* tabs = static_cast<char*>(unit) + game::kOptTabs;
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        if (field<std::int32_t>(tabs + t * game::kOptTabSize, game::kTabIndex) != static_cast<std::int32_t>(t)) {
            logf(AL_LOG_WARNING, "title options: tab object %zu is tab %d", t,
                 field<std::int32_t>(tabs + t * game::kOptTabSize, game::kTabIndex));
            return false;
        }
    }
    cursor_to_top(categories);
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        char* tab = tabs + t * game::kOptTabSize;
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
    return true;
}

BuildCategoriesFn build_categories(void* unit) {
    void* categories = static_cast<char*>(unit) + game::kOptCategories;
    void* const* vtable = field<void* const*>(categories, 0);
    void* fn = vtable != nullptr ? vtable[game::kBuildCategoriesSlot] : nullptr;
    if (reinterpret_cast<std::uintptr_t>(fn) != g_env.base + game::kRvaBuildCategories) {
        logf(AL_LOG_WARNING, "title options: the category builder is not where expected");
        return nullptr;
    }
    return reinterpret_cast<BuildCategoriesFn>(fn);
}

void set_header(void* unit, const char* text) {
    void* instance = g_get_instance(unit, "a_title_options");
    void* root = instance != nullptr ? field<void*>(instance, game::kInstRoot) : nullptr;
    void* object = root != nullptr ? g_get_object(root, "msg_title") : nullptr;
    if (object != nullptr) {
        g_format_text(object, text);
    }
}

void restore_tables() {
    for (QwordPatch& p : g_tabs) {
        std::string why;
        if (p.applied() && !p.restore(why)) {
            logf(AL_LOG_WARNING, "title options: could not restore a tab table: %s", why.c_str());
        }
    }
}

// Rebuilds the page from the game's tables, with the game's texts.
void rebuild_game(void* unit) {
    g_view.store(false, std::memory_order_release);
    restore_tables();
    BuildCategoriesFn build = build_categories(unit);
    if (build == nullptr || !prepare_rebuild(unit)) {
        return;
    }
    build(static_cast<char*>(unit) + game::kOptCategories, unit);
    set_header(unit, game_text(game::kRvaMessages12, 0));
    g_built_unit = nullptr;
    logf(AL_LOG_INFO, "title options: game rows back");
}

void enter_view(void* unit) {
    BuildCategoriesFn build = build_categories(unit);
    void* categories = field<void*>(unit, game::kOptCategoryList);
    if (build == nullptr || categories == nullptr) {
        return;
    }
    const OptionsModel& model = options_view_model();
    for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
        std::string why;
        const std::uint64_t expected = g_env.base + game::kRvaTitleOptionRows[t];
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(model.table(t));
        if (!g_tabs[t].applied() && !g_tabs[t].apply("title Options tab", title_tab_slot(t), expected, ours, why)) {
            logf(AL_LOG_WARNING, "title options: could not swap tab %zu: %s", t, why.c_str());
            restore_tables();
            return;
        }
    }
    if (!prepare_rebuild(unit)) {
        restore_tables();
        return;
    }
    g_view.store(true, std::memory_order_release);  // the text hook answers the category labels
    build(static_cast<char*>(unit) + game::kOptCategories, unit);
    g_built_unit = unit;
    // Five categories: Key Config and Default (a full reset) out of reach.
    if (field<std::uint32_t>(categories, game::kListRowCount) < game::kTitleViewTabs) {
        logf(AL_LOG_WARNING, "title options: only %u categories; game rows back",
             field<std::uint32_t>(categories, game::kListRowCount));
        rebuild_game(unit);
        return;
    }
    g_set_row_count(categories, game::kTitleViewTabs);
    for (std::uint32_t t = 0; t < game::kTitleViewTabs; ++t) {
        set_row_text(list_row(categories, t), "msg_1", nullptr, model.tab_name(t));
    }
    if (field<std::uint32_t>(categories, game::kListRowCount) != game::kTitleViewTabs) {
        logf(AL_LOG_WARNING, "title options: the category list did not shrink; game rows back");
        rebuild_game(unit);
        return;
    }
    set_header(unit, model.header());
    logf(AL_LOG_INFO, "title options: Mods page on (%zu rows)", model.row_count());
}

std::uint64_t open_detour(void* unit) {
    if (g_ready.load(std::memory_order_acquire)) {
        if (g_pending.exchange(false, std::memory_order_acq_rel)) {
            enter_view(unit);
        } else if (unit == g_built_unit || g_view.load(std::memory_order_acquire)) {
            rebuild_game(unit);
        }
    }
    return g_open_next(unit);
}

std::uint64_t close_detour(void* unit) {
    const std::uint64_t result = g_close_next(unit);
    if (g_view.load(std::memory_order_acquire)) {
        // The rows stay on screen while the page closes; the next plain open
        // rebuilds the game's lists.
        g_view.store(false, std::memory_order_release);
        restore_tables();
        logf(AL_LOG_INFO, "title options: Mods page closed (game tables back)");
    }
    return result;
}

bool find(const char* name, const char* pattern, void** out, std::string& why) {
    void* match = nullptr;
    if (g_env.scanner == nullptr ||
        g_env.scanner->find(g_env.self, pattern, AL_SCAN_CODE, &match, 1) != 1) {
        why = std::string("game function not found: ") + name;
        return false;
    }
    *out = match;
    return true;
}

}  // namespace

bool title_menu_register(const Env& env, std::string& why) {
    g_env = env;
    if (!find("setRowCount", game::kListSetRowCountPattern, reinterpret_cast<void**>(&g_set_row_count), why) ||
        !find("setCursor", game::kListSetCursorPattern, reinterpret_cast<void**>(&g_set_cursor), why) ||
        !find("getObjectFromName", game::kGetObjectFromNamePattern, reinterpret_cast<void**>(&g_get_object), why) ||
        !find("formatText", game::kFormatTextPattern, reinterpret_cast<void**>(&g_format_text), why) ||
        !find("getInstanceFromName", game::kGetInstanceFromNamePattern, reinterpret_cast<void**>(&g_get_instance),
              why) ||
        !find("setPlay", game::kSetPlayPattern, reinterpret_cast<void**>(&g_set_play), why)) {
        return false;
    }
    struct Spec {
        const char* name;
        const char* pattern;
        void* detour;
        void** next;
    };
    const Spec specs[kHookCount] = {
        {"CMFM:uGUITitle::play", game::kTitlePlayPattern, reinterpret_cast<void*>(&play_detour),
         reinterpret_cast<void**>(&g_play_next)},
        {"CMFM:uGUITitle::onCursorRow", game::kTitleCursorPattern, reinterpret_cast<void*>(&cursor_detour),
         reinterpret_cast<void**>(&g_cursor_next)},
        {"CMFM:uGUITitle::onRowDecided", game::kTitleDecidedPattern, reinterpret_cast<void*>(&decided_detour),
         reinterpret_cast<void**>(&g_decided_next)},
        {"CMFM:uGUIOption::open", game::kTitleOptionsOpenPattern, reinterpret_cast<void*>(&open_detour),
         reinterpret_cast<void**>(&g_open_next)},
        {"CMFM:uGUIOption::close", game::kTitleOptionsClosePattern, reinterpret_cast<void*>(&close_detour),
         reinterpret_cast<void**>(&g_close_next)},
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
    logf(AL_LOG_INFO, "title: hooks registered (menu play, cursor, selected; title Options open, close)");
    return true;
}

bool title_menu_check(const Env& env, std::string& why) {
    for (const std::uint32_t h : g_hooks) {
        if (env.hooks->state(env.self, h) != AL_HOOK_ACTIVE) {
            why = "the title hooks are not active";
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
    g_ready.store(true, std::memory_order_release);
    logf(AL_LOG_INFO, "title: ready");
    return true;
}

const char* title_menu_text(void* message_file_ptr, std::uint32_t id) {
    if (!g_view.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    if (message_file_ptr == message_file(game::kRvaMessages12)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryLabelIds[t]) {
                return options_view_model().tab_name(t);
            }
        }
    } else if (message_file_ptr == message_file(game::kRvaMessages18)) {
        for (std::size_t t = 0; t < game::kTitleViewTabs; ++t) {
            if (id == game::kTitleCategoryHelpIds[t]) {
                return options_view_model().tab_help(t);
            }
        }
    }
    return nullptr;
}

void title_menu_restore(const Env&) {
    g_ready.store(false, std::memory_order_release);
    g_view.store(false, std::memory_order_release);
    restore_tables();
    logf(AL_LOG_INFO, "title: tables restored; Mods selected %u time(s)", g_mods_selected);
}

}  // namespace cmfm::native
