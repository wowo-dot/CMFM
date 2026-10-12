#include "native/title_menu.hpp"

#include "native/game_421810.hpp"
#include "native/gui.hpp"
#include "native/measure.hpp"
#include "native/mods_page.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace cmfm::native {
namespace {

using TitlePlayFn = std::uint64_t (*)(void* title);
using TitleCursorFn = std::uint64_t (*)(void* title, void* old_row, std::int32_t row);
using TitleDecidedFn = void (*)(void* title, std::int32_t row);
using SceneFlowsFn = std::uint64_t (*)(void* scene, void* a2, void* a3, void* a4);

TitlePlayFn g_play_next = nullptr;
TitleCursorFn g_cursor_next = nullptr;
TitleDecidedFn g_decided_next = nullptr;
SceneFlowsFn g_scene_flows_next = nullptr;

enum Hook { kPlay, kCursor, kDecided, kSceneFlows, kHookCount };
std::uint32_t g_hooks[kHookCount] = {};

Env g_env = {};
std::atomic<bool> g_ready{false};
// The title unit whose menu has the Mods row, and its Options row.
std::atomic<void*> g_entry_title{nullptr};
std::int32_t g_entry_options_row = -1;
// "Mods" was chosen: the title scene's next Options flow opens CMFM's page.
std::atomic<bool> g_pending{false};
// The running Options flow is CMFM's: the scene's page is CMFM's during its updates.
bool g_flow_ours = false;
std::uint32_t g_mods_selected = 0;

// Short lines on user actions only (no allocation).
void logf(al_log_level_t level, const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    g_env.api->log(g_env.self, level, line);
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
    gui_set_play(last, true);
    g_entry_options_row = options;
    g_entry_title.store(title, std::memory_order_release);
    logf(AL_LOG_INFO, "title: Mods row added below Options (row %d of %u)", options + 1, rows);
}

std::uint64_t play_detour(void* title) {
    if (!g_ready.load(std::memory_order_acquire)) {
        return g_play_next(title);
    }
    g_entry_title.store(nullptr, std::memory_order_release);
    g_pending.store(false, std::memory_order_release);
    void* list = field<void*>(title, game::kTitleList);
    if (list != nullptr && field<std::uint32_t>(list, game::kListDuplicateNum) == game::kTitleRows &&
        field<std::uint32_t>(list, game::kListRowCount) == game::kTitleRows) {
        gui_set_row_count(list, game::kTitleRows + 1);  // play() hides the rows after Quit
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

// ---- The title scene: CMFM's page and the Options flow ----

// Every frame of the title scene (its own update, where the scene makes its
// own pages): CMFM's page is made here once. When "Mods" started the Options
// flow (flow 9), the flow's page is CMFM's for the length of each update, so
// the game's own code opens it, waits for it to close and checks it, and the
// game's Options page is never touched.
std::uint64_t scene_flows_detour(void* scene, void* a2, void* a3, void* a4) {
    std::uint64_t t0 = measure::start();
    if (!g_ready.load(std::memory_order_acquire)) {
        measure::stop(measure::Hook::TitleScene, t0);
        return g_scene_flows_next(scene, a2, a3, a4);
    }
    void* page = mods_page_ensure();
    if (field<std::int32_t>(scene, game::kSceneFlow) != game::kSceneFlowOptions) {
        g_flow_ours = false;
    } else if (g_pending.exchange(false, std::memory_order_acq_rel)) {
        g_flow_ours = page != nullptr;
        logf(AL_LOG_INFO, g_flow_ours ? "title: the Options flow opens CMFM's page"
                                      : "title: CMFM's page is missing; the Options flow opens the Options page");
    }
    if (!g_flow_ours || page == nullptr) {
        measure::stop(measure::Hook::TitleScene, t0);
        return g_scene_flows_next(scene, a2, a3, a4);
    }
    // Only the pointer in the scene's tracked reference; its links stay the game's.
    void*& slot = field<void*>(scene, game::kSceneOptionsPage);
    void* game_page = slot;
    slot = page;
    measure::stop(measure::Hook::TitleScene, t0);
    const std::uint64_t result = g_scene_flows_next(scene, a2, a3, a4);
    t0 = measure::start();
    if (slot == page) {
        slot = game_page;
    }
    if (field<std::int32_t>(scene, game::kSceneFlow) != game::kSceneFlowOptions) {
        g_flow_ours = false;
    }
    measure::stop(measure::Hook::TitleScene, t0, false);
    return result;
}

}  // namespace

bool title_menu_register(const Env& env, std::string& why) {
    g_env = env;
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
        {"CMFM:TitleScene::updateFlows", game::kTitleSceneFlowsPattern, reinterpret_cast<void*>(&scene_flows_detour),
         reinterpret_cast<void**>(&g_scene_flows_next)},
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
    logf(AL_LOG_INFO, "title: hooks registered (menu play, cursor, selected; title scene flows)");
    return true;
}

bool title_menu_check(const Env& env, std::string& why) {
    for (const std::uint32_t h : g_hooks) {
        if (env.hooks->state(env.self, h) != AL_HOOK_ACTIVE) {
            why = "the title hooks are not active";
            return false;
        }
    }
    if (!mods_page_ready()) {
        why = "CMFM's page is not ready (see the Mods page lines)";
        return false;
    }
    g_ready.store(true, std::memory_order_release);
    logf(AL_LOG_INFO, "title: ready");
    return true;
}

void title_menu_restore(const Env&) {
    if (g_env.api == nullptr) {
        return;  // never registered
    }
    g_ready.store(false, std::memory_order_release);
    logf(AL_LOG_INFO, "title: done; Mods selected %u time(s)", g_mods_selected);
}

}  // namespace cmfm::native
