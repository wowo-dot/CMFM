#include "native/options_view.hpp"

#include "native/data_patch.hpp"
#include "native/game_421810.hpp"
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
OptionsModel g_model;
QwordPatch g_tabs[kTabCount];

// Prototype 2 content: one tab with two test rows.
const char* const kTabNames[kTabCount] = {"CMFM", "-", "-", "-", "-"};
const char* const kOnOff[] = {"Off", "On"};
const char* const kChoice[] = {"Alpha", "Beta", "Gamma"};
const OptionSpec kTestRows[] = {
    {"Test toggle", "Prototype row: switches between Off and On.", kOnOff, 2, 0, 0},
    {"Test choice", "Prototype row: picks one of three values.", kChoice, 3, 0, 0},
};
// Every tab needs a row (the page reads the row under the cursor unchecked).
const char* const kDash[] = {"-"};
const OptionSpec kFreeTab = {"Free tab", "No mod settings on this tab yet.", kDash, 1, 0, 0};

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

std::uint64_t get_detour(std::int32_t kind, std::uint32_t id) {
    if (kind == kCmfmRowKind) {
        std::uint32_t value = 0;
        g_model.get(id, value);
        return value;
    }
    return g_get_next(kind, id);
}

std::uint64_t set_detour(std::int32_t kind, std::uint32_t id, std::int32_t value) {
    if (kind == kCmfmRowKind) {
        // Never reaches the game's setter: an unknown kind would write the save data.
        if (value >= 0 && g_model.set(id, static_cast<std::uint32_t>(value))) {
            const OptionSpec* s = g_model.spec(id);
            logf(AL_LOG_INFO, "options view: %s = %s", s->label, s->values[value]);
        } else {
            logf(AL_LOG_WARNING, "options view: refused row %u value %d", id, value);
        }
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
        g_model.reset();
        void* page = *reinterpret_cast<void* const*>(static_cast<char*>(closure) + 8);
        if (g_refresh != nullptr && page != nullptr) {
            g_refresh(page);
        }
        logf(AL_LOG_INFO, "options view: Set To Default reset the CMFM rows only");
    }
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
    const char* fail = nullptr;
    if (!g_model.build("Mods", kTabNames, kTestRows, sizeof kTestRows / sizeof kTestRows[0], kFreeTab, fail)) {
        why = std::string("test rows: ") + fail;
        return false;
    }
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
    logf(AL_LOG_INFO,
         "options view: hooks registered (option get, option set, Options close, Set To Default); %u rows",
         static_cast<unsigned>(g_model.row_count()));
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
    g_ready = true;
    logf(AL_LOG_INFO, "options view: ready");
    return true;
}

bool options_view_ready() { return g_ready; }

bool options_view_enter() {
    if (!g_ready) {
        return false;
    }
    if (g_view.load(std::memory_order_acquire)) {
        return true;
    }
    std::string why;
    for (std::size_t t = 0; t < kTabCount; ++t) {
        const std::uint64_t expected = g_env.base + game::kRvaOptionRows[t];
        const std::uint64_t ours = reinterpret_cast<std::uint64_t>(g_model.table(t));
        if (!g_tabs[t].apply("Options tab", tab_slot(t), expected, ours, why)) {
            for (std::size_t u = 0; u < t; ++u) {
                std::string ignored;
                g_tabs[u].restore(ignored);
            }
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
    logf(AL_LOG_INFO, "options view: off (game tables back)");
}

const char* options_view_text(void* message_file_ptr, std::uint32_t id) {
    if (id >= kLabelIdFirst && id < kTextIdLimit && message_file_ptr == message_file(game::kRvaMessages12)) {
        return g_model.label_text(id);
    }
    if (id >= kHelpIdFirst && id < kTextIdLimit && message_file_ptr == message_file(game::kRvaMessages18)) {
        return g_model.help_text(id);
    }
    if (id <= kTabCount && g_view.load(std::memory_order_relaxed) &&
        message_file_ptr == message_file(game::kRvaMessages12)) {
        return id == 0 ? g_model.header() : g_model.tab_name(id - 1);
    }
    return nullptr;
}

void options_view_restore(const Env&) {
    options_view_exit();
}

}  // namespace cmfm::native
