// CMFM: the in-game settings menu for MHW mods, an Astera Loader plugin.
// Native menu prototype 1 (decisions.md items 7-9): a "Mods" entry in the
// Start menu's System tab. Exe 421810 only; on any other exe, or when a
// check fails, CMFM changes nothing and reports DEGRADED with the reason.
#include "al/al_plugin.h"
#include "native/game_421810.hpp"
#include "native/options_view.hpp"
#include "native/start_menu.hpp"

#include <windows.h>

#include <cstring>
#include <string>

#ifndef CMFM_COMMIT
#define CMFM_COMMIT "unknown"
#endif

namespace {

void AL_CALL on_phase(al_plugin_t* self, al_phase_t phase, const void* data);

const al_plugin_info_t kInfo = {
    sizeof(al_plugin_info_t),
    AL_API_VERSION,
    "cmfm",       // name: also the folder and DLL name
    "CMFM",       // display name
    "0.1.0-dev",  // this plugin's version
    AL_PHASE_BIT(AL_PHASE_WIN_MAIN) | AL_PHASE_BIT(AL_PHASE_SHUTDOWN),
    0,            // order
    nullptr,      // load_after
    nullptr,      // load_before
    nullptr,      // exe_sha256: loads on any exe; the native menu checks for 421810
    &on_phase,
    nullptr,      // dependencies
};

const al_api_t* g_api = nullptr;
cmfm::native::Env g_env = {};
bool g_native = false;  // hooks registered; activate at WIN_MAIN
bool g_view = false;    // options view hooks registered; check at WIN_MAIN

void log(al_log_level_t level, const std::string& line) {
    g_api->log(g_env.self, level, line.c_str());
}

void degrade(const std::string& why) {
    log(AL_LOG_WARNING, "native menu off: " + why);
    g_api->set_status(g_env.self, AL_STATUS_DEGRADED, ("native menu off: " + why).c_str());
}

void AL_CALL on_phase(al_plugin_t*, al_phase_t phase, const void*) {
    if (phase == AL_PHASE_WIN_MAIN && g_native) {
        std::string why;
        if (!cmfm::native::start_menu_activate(g_env, why)) {
            g_native = false;
            degrade(why);
            return;
        }
        if (g_view && !cmfm::native::options_view_check(g_env, why)) {
            g_view = false;
            log(AL_LOG_WARNING, "Mods page off, Mods opens the plain Options page: " + why);
            g_api->set_status(g_env.self, AL_STATUS_DEGRADED, ("Mods page off: " + why).c_str());
            return;
        }
        g_api->set_status(g_env.self, AL_STATUS_ACTIVE, nullptr);
    } else if (phase == AL_PHASE_SHUTDOWN && g_native) {
        cmfm::native::options_view_restore(g_env);
        cmfm::native::start_menu_restore(g_env);
    }
}

}  // namespace

extern "C" AL_PLUGIN_EXPORT const al_plugin_info_t* AL_CALL al_plugin_info(void) {
    return &kInfo;
}

extern "C" AL_PLUGIN_EXPORT al_result_t AL_CALL al_plugin_init(const al_api_t* api, const al_init_t* init) {
    g_api = api;
    g_env.api = api;
    g_env.self = init->self;
    log(AL_LOG_INFO, std::string(kInfo.name) + " " + kInfo.version + " (" CMFM_COMMIT ") on Astera Loader API " +
                         std::to_string(AL_VERSION_MAJOR(api->version)) + "." +
                         std::to_string(AL_VERSION_MINOR(api->version)));
    api->set_status(init->self, AL_STATUS_ACTIVE, nullptr);

    if (api->size < offsetof(al_api_t, get_interface) + sizeof(api->get_interface)) {
        degrade("this Astera Loader has no services (API 1.1 or newer needed)");
        return AL_OK;
    }
    const auto* game = static_cast<const al_game_t*>(api->get_interface(init->self, AL_GAME_INTERFACE, AL_GAME_VERSION));
    if (game == nullptr || game->exe_sha256 == nullptr) {
        degrade("al.game is unavailable, so the exe cannot be checked");
        return AL_OK;
    }
    if (std::strcmp(game->exe_sha256, cmfm::native::game::kTestedExeSha256) != 0) {
        degrade(std::string("exe ") + game->exe_sha256 + " is not the supported 421810 build");
        return AL_OK;
    }
    g_env.hooks = static_cast<const al_hooks_t*>(api->get_interface(init->self, AL_HOOKS_INTERFACE, AL_HOOKS_VERSION));
    if (g_env.hooks == nullptr) {
        degrade("al.hooks is unavailable (Astera Loader API 1.4 or newer needed)");
        return AL_OK;
    }
    g_env.base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    g_env.scanner =
        static_cast<const al_scanner_t*>(api->get_interface(init->self, AL_SCANNER_INTERFACE, AL_SCANNER_VERSION));
    log(AL_LOG_INFO, "exe 421810 confirmed (" + std::string(game->exe_sha256).substr(0, 12) + "...)");

    std::string why;
    if (!cmfm::native::start_menu_register(g_env, why)) {
        degrade(why);
        return AL_OK;
    }
    g_native = true;
    g_view = cmfm::native::options_view_register(g_env, why);
    if (!g_view) {
        log(AL_LOG_WARNING, "Mods page off, Mods opens the plain Options page: " + why);
    }
    return AL_OK;
}
