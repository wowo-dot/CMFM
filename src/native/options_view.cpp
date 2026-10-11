#include "native/options_view.hpp"

#include "menu/service.hpp"
#include "native/data_patch.hpp"
#include "native/game_421810.hpp"
#include "native/mods_page.hpp"
#include "native/options_model.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace cmfm::native {
namespace {

using GetFn = std::uint64_t (*)(std::int32_t kind, std::uint32_t id);
using SetFn = std::uint64_t (*)(std::int32_t kind, std::uint32_t id, std::int32_t value);
using CloseFn = std::uint64_t (*)(void* page);
using DefaultsFn = void (*)(void* closure, void* unknown, std::int32_t* answer);
using RefreshFn = void (*)(void* page);

GetFn g_get_next = nullptr;
SetFn g_set_next = nullptr;
CloseFn g_close_next = nullptr;
DefaultsFn g_defaults_next = nullptr;
RefreshFn g_refresh = nullptr;  // uGUIMenuOption::refreshRows, found by pattern
std::uint32_t g_get_hook = 0;
std::uint32_t g_set_hook = 0;
std::uint32_t g_close_hook = 0;
std::uint32_t g_defaults_hook = 0;
Env g_env = {};
bool g_ready = false;
std::atomic<bool> g_view{false};
QwordPatch g_tabs[kTabCount];

// Short lines from the game thread on user actions only (no allocation).
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

std::uintptr_t tab_slot(std::size_t tab) {
    return g_env.base + game::kRvaOptionTabTables + 8 * tab;
}

// Slider rows (CMFM's page): the game reads and writes them on the slider's
// 0-100 scale (sub_140EEB860 every frame: the knob's position * 100, rounded
// up, against the getter; the setter when they differ). The setter takes the
// value from the knob itself (mods_page_slider_index: equal zones per value;
// the game's rounding up is a step too far for some positions with 100
// steps). The last 0-100 value the game set is kept with the value it meant,
// so the getter answers the game's own number while the value stands and the
// game does not set it again every frame. -1: none.
std::int16_t g_slider_raw[kSettingIdLimit - kSettingIdFirst];
std::uint8_t g_slider_meant[kSettingIdLimit - kSettingIdFirst];

std::uint32_t slider_index(std::int32_t raw, std::uint32_t steps) {
    raw = raw < 0 ? 0 : raw > 100 ? 100 : raw;
    return (static_cast<std::uint32_t>(raw) * steps + 50) / 100;
}

std::int32_t slider_raw(std::uint32_t index, std::uint32_t steps) {
    return static_cast<std::int32_t>((index * 100 + steps / 2) / steps);
}

std::uint64_t get_detour(std::int32_t kind, std::uint32_t id) {
    if (is_cmfm_row(kind, id)) {
        const std::uint64_t index = menu::service_get(id);
        if (const std::uint32_t steps = menu::service_slider_steps(id)) {
            const std::int16_t raw = g_slider_raw[id - kSettingIdFirst];
            return raw >= 0 && g_slider_meant[id - kSettingIdFirst] == index
                       ? static_cast<std::uint64_t>(raw)
                       : static_cast<std::uint64_t>(slider_raw(static_cast<std::uint32_t>(index), steps));
        }
        return index;
    }
    return g_get_next(kind, id);
}

std::uint64_t set_detour(std::int32_t kind, std::uint32_t id, std::int32_t value) {
    if (is_cmfm_row(kind, id)) {
        // Never reaches the game's setter: it would write sUserData out of range.
        if (menu::service_locked(id)) {
            return 0;  // greyed: no change (a slider's knob snaps back)
        }
        if (const std::uint32_t steps = menu::service_slider_steps(id)) {
            std::uint32_t index = 0;
            if (!mods_page_slider_index(id, steps, &index)) {
                index = slider_index(value, steps);
            }
            g_slider_raw[id - kSettingIdFirst] = static_cast<std::int16_t>(value < 0 ? 0 : value > 100 ? 100 : value);
            g_slider_meant[id - kSettingIdFirst] = static_cast<std::uint8_t>(index);
            if (index != menu::service_get(id)) {
                menu::service_set(id, static_cast<std::int32_t>(index));
            }
            return 0;
        }
        menu::service_set(id, value);
        return 0;
    }
    return g_set_next(kind, id, value);
}

std::uint64_t close_detour(void* page) {
    const std::uint64_t result = g_close_next(page);
    if (g_view.load(std::memory_order_acquire)) {
        options_view_exit();
    }
    return result;
}

// "Set To Default" answered. In the CMFM view the game's handler must not
// run: in mode 0 it resets every game and system option.
void defaults_detour(void* closure, void* unknown, std::int32_t* answer) {
    if (!g_view.load(std::memory_order_acquire)) {
        g_defaults_next(closure, unknown, answer);
        return;
    }
    if (answer != nullptr && *answer == 0) {  // Yes
        menu::service_reset();
        void* page = *reinterpret_cast<void* const*>(static_cast<char*>(closure) + 8);
        if (g_refresh != nullptr && page != nullptr) {
            g_refresh(page);
        }
    }
}

// A game row with a CMFM id would be answered by CMFM; none exists in 421810
// (kind-1 ids 0-34), and this keeps it so.
bool game_tables_clear(const Env& env, std::string& why) {
    std::uintptr_t tables[game::kOptionTabTableCount + 2 + game::kTitleOptionTabTableCount] = {};
    std::size_t n = 0;
    for (std::size_t i = 0; i < game::kOptionTabTableCount; ++i) {
        tables[n++] = *reinterpret_cast<const std::uintptr_t*>(env.base + game::kRvaOptionTabTables + 8 * i);
    }
    tables[n++] = env.base + game::kRvaOptionRowsMode2;
    tables[n++] = env.base + game::kRvaOptionRowsMode3;
    for (std::size_t i = 0; i < game::kTitleOptionTabTableCount; ++i) {
        tables[n++] = *reinterpret_cast<const std::uintptr_t*>(env.base + game::kRvaTitleOptionTabTables + 8 * i);
    }
    for (std::size_t t = 0; t < n; ++t) {
        const auto* row = reinterpret_cast<const GameOptionRow*>(tables[t]);
        std::size_t i = 0;
        for (; i < game::kMaxGameTableRows && row[i].kind != kRowTableEnd; ++i) {
            if (is_cmfm_row(row[i].kind, static_cast<std::uint32_t>(row[i].setting_id))) {
                why = "a game Options row uses setting id " + std::to_string(row[i].setting_id);
                return false;
            }
        }
        if (i == game::kMaxGameTableRows) {
            why = "a game Options row table has no end";
            return false;
        }
    }
    return true;
}

bool register_hook(const char* name, const char* pattern, void* detour, void** next, std::uint32_t& handle) {
    al_hook_t h = {sizeof(al_hook_t)};
    h.name = name;
    h.pattern = pattern;
    h.detour = detour;
    h.next = next;
    handle = g_env.hooks->hook(g_env.self, &h);
    return handle != 0;
}

}  // namespace

bool options_view_register(const Env& env, std::string& why) {
    g_env = env;
    if (!register_hook("CMFM:OptionRow::get", game::kOptionGetPattern, reinterpret_cast<void*>(&get_detour),
                       reinterpret_cast<void**>(&g_get_next), g_get_hook) ||
        !register_hook("CMFM:OptionRow::set", game::kOptionSetPattern, reinterpret_cast<void*>(&set_detour),
                       reinterpret_cast<void**>(&g_set_next), g_set_hook) ||
        !register_hook("CMFM:uGUIMenuOption::close", game::kMenuOptionClosePattern,
                       reinterpret_cast<void*>(&close_detour), reinterpret_cast<void**>(&g_close_next),
                       g_close_hook) ||
        !register_hook("CMFM:uGUIMenuOption::setToDefault", game::kMenuOptionDefaultsPattern,
                       reinterpret_cast<void*>(&defaults_detour), reinterpret_cast<void**>(&g_defaults_next),
                       g_defaults_hook)) {
        for (const std::uint32_t h : {g_get_hook, g_set_hook, g_close_hook, g_defaults_hook}) {
            if (h != 0) {
                g_env.hooks->set_enabled(g_env.self, h, 0);
            }
        }
        why = "an options view hook was refused (see the lines above)";
        return false;
    }
    // The redraw after Set To Default; optional (rows otherwise update on
    // their next redraw).
    void* match = nullptr;
    if (g_env.scanner != nullptr &&
        g_env.scanner->find(g_env.self, game::kMenuOptionRefreshPattern, AL_SCAN_CODE, &match, 1) == 1) {
        g_refresh = reinterpret_cast<RefreshFn>(match);
    } else {
        logf(AL_LOG_WARNING, "options view: refreshRows not found; Set To Default redraws later");
    }
    logf(AL_LOG_INFO, "options view: hooks registered (option get, option set, Options close, Set To Default)");
    return true;
}

bool options_view_check(const Env& env, std::string& why) {
    for (const std::uint32_t h : {g_get_hook, g_set_hook, g_close_hook, g_defaults_hook}) {
        if (env.hooks->state(env.self, h) != AL_HOOK_ACTIVE) {
            why = "the options view hooks are not active";
            return false;
        }
    }
    for (std::size_t t = 0; t < kTabCount; ++t) {
        const std::uint64_t found = *reinterpret_cast<const std::uint64_t*>(tab_slot(t));
        const std::uint64_t expected = env.base + game::kRvaOptionRows[t];
        if (found != expected) {
            char b[128];
            std::snprintf(b, sizeof b, "Options tab %zu table is 0x%llx, expected 0x%llx", t,
                          static_cast<unsigned long long>(found), static_cast<unsigned long long>(expected));
            why = b;
            return false;
        }
    }
    if (!game_tables_clear(env, why)) {
        return false;
    }
    g_ready = true;
    logf(AL_LOG_INFO, "options view: ready");
    return true;
}

bool options_view_ready() { return g_ready; }

void options_view_sliders(bool on) {
    for (std::int16_t& raw : g_slider_raw) {
        raw = -1;
    }
    menu::service_set_sliders(on);
}

bool options_view_enter() {
    if (!g_ready) {
        return false;
    }
    if (g_view.load(std::memory_order_acquire)) {
        return true;
    }
    const OptionsModel* model = menu::service_prepare();
    if (model == nullptr) {
        return false;
    }
    std::string why;
    for (std::size_t t = 0; t < kTabCount; ++t) {
        const std::uint64_t expected = g_env.base + game::kRvaOptionRows[t];
        // The game's tab holds 24 rows; a longer page shows its first 24 here.
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(model->game_table(t));
        if (model->tab_rows(t) > kGameRowsPerTab) {
            logf(AL_LOG_INFO, "options view: tab %zu shows the first %zu of its %zu rows (the Mods page shows all)", t,
                 kGameRowsPerTab, model->tab_rows(t));
        }
        if (!g_tabs[t].apply("Options tab", tab_slot(t), expected, ours, why)) {
            for (std::size_t u = 0; u < t; ++u) {
                std::string ignored;
                g_tabs[u].restore(ignored);
            }
            menu::service_abort();
            logf(AL_LOG_WARNING, "options view: could not swap tab %zu: %s", t, why.c_str());
            return false;
        }
    }
    g_view.store(true, std::memory_order_release);
    logf(AL_LOG_INFO, "options view: on");
    return true;
}

void options_view_exit() {
    if (!g_view.load(std::memory_order_acquire)) {
        return;
    }
    for (std::size_t t = 0; t < kTabCount; ++t) {
        std::string why;
        if (!g_tabs[t].restore(why)) {
            logf(AL_LOG_WARNING, "options view: could not restore tab %zu: %s", t, why.c_str());
        }
    }
    g_view.store(false, std::memory_order_release);
    menu::service_closed();
    logf(AL_LOG_INFO, "options view: off (game tables back)");
}

const char* options_view_text(void* message_file_ptr, std::uint32_t id) {
    if (!menu::service_texts_on()) {
        return nullptr;
    }
    const OptionsModel& model = menu::service_model();
    if (id >= kLabelIdFirst && id < kTextIdLimit && message_file_ptr == message_file(game::kRvaMessages12)) {
        return model.label_text(id);
    }
    if (id >= kHelpIdFirst && id < kTextIdLimit && message_file_ptr == message_file(game::kRvaMessages18)) {
        return model.help_text(id);
    }
    if (id <= kTabCount && g_view.load(std::memory_order_relaxed) &&
        message_file_ptr == message_file(game::kRvaMessages12)) {
        return id == 0 ? model.header() : model.tab_name(id - 1);
    }
    return nullptr;
}

void options_view_restore(const Env&) {
    options_view_exit();
}

}  // namespace cmfm::native
