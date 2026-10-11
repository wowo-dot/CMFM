#include "menu/service.hpp"

#include "cmfm/cmfm.h"
#include "menu/menu.hpp"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace cmfm::menu {
namespace {

const al_api_t* g_api = nullptr;
al_plugin_t* g_self = nullptr;

// add_page calls not yet taken in by the menu thread, under g_lock.
struct Pending {
    bool ok = false;
    Page page;
    std::string plugin;
    std::string why;
};
SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<Pending> g_pending;
std::uint32_t g_next_handle = 0;

class Lock {
public:
    Lock() { AcquireSRWLockExclusive(&g_lock); }
    ~Lock() { ReleaseSRWLockExclusive(&g_lock); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

// The game's menu thread only.
std::optional<Menu> g_menu;
native::OptionsModel g_model;
bool g_model_ok = false;
std::atomic<bool> g_texts{false};
bool g_sliders = false;

void log_line(int level, const char* line) {
    g_api->log(g_self, level != 0 ? AL_LOG_WARNING : AL_LOG_INFO, line);
}

void logf(int level, const char* format, ...) {
    char line[400];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    log_line(level, line);
}

std::uint32_t CMFM_CALL add_page(const cmfm_page_t* page, char* why, std::uint32_t why_size) {
    std::uint32_t handle = 0;
    std::string reason;
    std::string plugin;
    try {
        Pending p;
        p.ok = copy_page(page, p.page, reason);
        if (page != nullptr && page->size >= sizeof(cmfm_page_t) && page->plugin != nullptr) {
            plugin.assign(page->plugin, strnlen(page->plugin, kMaxPlugin));
        }
        p.plugin = plugin;
        p.why = reason;
        const bool ok = p.ok;
        const std::string title = ok ? p.page.title : std::string();
        const std::size_t settings = ok ? p.page.settings.size() : 0;
        {
            Lock lock;
            g_pending.push_back(std::move(p));
            handle = ok ? ++g_next_handle : 0;
        }
        if (ok) {
            logf(0, "cmfm.menu: page \"%s\" from %s added (%zu settings, handle %u)", title.c_str(),
                 plugin.c_str(), settings, handle);
        } else {
            logf(1, "cmfm.menu: page from %s refused: %s", plugin.empty() ? "(unnamed)" : plugin.c_str(),
                 reason.c_str());
        }
    } catch (...) {
        handle = 0;
        reason = "CMFM ran out of memory";
        log_line(1, "cmfm.menu: add_page failed: out of memory");
    }
    if (why != nullptr && why_size > 0) {
        std::snprintf(why, why_size, "%s", handle != 0 ? "" : reason.c_str());
    }
    return handle;
}

const cmfm_menu_t kTable = {sizeof(cmfm_menu_t), CMFM_MENU_VERSION, &add_page};

}  // namespace

bool service_publish(const al_api_t* api, al_plugin_t* self, const char* version, std::string& why) {
    g_api = api;
    g_self = self;
    g_menu.emplace(version, &log_line);
    if (api->size < offsetof(al_api_t, publish_interface) + sizeof(api->publish_interface)) {
        why = "this Astera Loader cannot publish interfaces";
        return false;
    }
    if (api->publish_interface(self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION, &kTable) != AL_OK) {
        why = "Astera Loader refused to publish cmfm.menu";
        return false;
    }
    logf(0, "cmfm.menu %u.%u published", CMFM_VERSION_MAJOR(CMFM_MENU_VERSION), CMFM_VERSION_MINOR(CMFM_MENU_VERSION));
    return true;
}

const native::OptionsModel* service_prepare() {
    if (!g_menu) {
        return nullptr;
    }
    if (g_texts.load(std::memory_order_acquire)) {
        log_line(1, "cmfm.menu: a Mods page opened while another was open; layout kept");
        return g_model_ok ? &g_model : nullptr;
    }
    std::vector<Pending> taken;
    {
        Lock lock;
        taken.swap(g_pending);
    }
    try {
        for (Pending& p : taken) {
            if (p.ok) {
                g_menu->add(std::move(p.page));
            } else {
                g_menu->add_refused(std::move(p.plugin), std::move(p.why));
            }
        }
        if (g_menu->dirty() || !g_model_ok) {
            std::string why;
            g_model_ok = g_menu->build(g_model, why);
            if (!g_model_ok) {
                logf(1, "cmfm.menu: no layout: %s", why.c_str());
            }
        }
    } catch (...) {
        g_model_ok = false;
        log_line(1, "cmfm.menu: out of memory while laying out the pages");
    }
    if (!g_model_ok) {
        return nullptr;
    }
    // Opened first: a plugin may update what its TEXT rows say.
    g_menu->send(CMFM_EVENT_OPENED);
    g_menu->refresh();
    g_texts.store(true, std::memory_order_release);
    return &g_model;
}

void service_closed() {
    if (!g_texts.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (g_menu && g_model_ok) {
        g_menu->send(CMFM_EVENT_CLOSED);
    }
}

void service_abort() {
    service_closed();
}

bool service_texts_on() {
    return g_texts.load(std::memory_order_acquire);
}

const native::OptionsModel& service_model() {
    return g_model;
}

std::size_t service_used_tabs() {
    return g_menu && g_model_ok ? g_menu->used_tabs() : 1;
}

std::uint64_t service_get(std::uint32_t setting_id) {
    return g_menu && g_model_ok ? g_menu->get(setting_id) : 0;
}

void service_set(std::uint32_t setting_id, std::int32_t value) {
    if (g_menu && g_model_ok) {
        g_menu->set(setting_id, value);
    }
}

void service_reset() {
    if (g_menu && g_model_ok) {
        g_menu->reset_defaults();
    }
}

bool service_is_section(std::uint32_t setting_id) {
    return g_menu && g_model_ok && g_menu->is_section(setting_id);
}

bool service_restart_pending(std::uint32_t setting_id) {
    return g_menu && g_model_ok && g_menu->restart_pending(setting_id);
}

bool service_tab_restart_pending(std::size_t tab) {
    return g_menu && g_model_ok && g_menu->tab_restart_pending(tab);
}

std::size_t service_restart_count() {
    return g_menu && g_model_ok ? g_menu->restart_count() : 0;
}

std::uint32_t service_restart_revision() {
    return g_menu ? g_menu->restart_revision() : 0;
}

void service_set_sliders(bool on) {
    g_sliders = on;
}

std::uint32_t service_slider_steps(std::uint32_t setting_id) {
    return g_sliders && g_menu && g_model_ok ? g_menu->slider_steps(setting_id) : 0;
}

std::uint32_t service_value_revision() {
    return g_menu ? g_menu->value_revision() : 0;
}

std::uint32_t service_list_choices(std::uint32_t setting_id) {
    return g_menu && g_model_ok ? g_menu->list_choices(setting_id) : 0;
}

bool service_is_button(std::uint32_t setting_id) {
    return g_menu && g_model_ok && g_menu->is_button(setting_id);
}

const char* service_button_question(std::uint32_t setting_id) {
    return g_menu && g_model_ok ? g_menu->button_question(setting_id) : nullptr;
}

bool service_press(std::uint32_t setting_id) {
    return g_menu && g_model_ok && g_menu->press(setting_id);
}

bool service_locked(std::uint32_t setting_id) {
    return g_menu && g_model_ok && g_menu->is_locked(setting_id);
}

std::uint32_t service_lock_revision() {
    return g_menu ? g_menu->lock_revision() : 0;
}

}  // namespace cmfm::menu
