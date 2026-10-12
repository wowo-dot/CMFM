#include "native/start_menu.hpp"

#include "native/game_421810.hpp"
#include "native/measure.hpp"
#include "native/options_view.hpp"
#include "native/system_items.hpp"
#include "native/mods_page.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>

namespace cmfm::native {
namespace {

using GetMessageFn = const char* (*)(void* self, std::uint32_t id);
using ItemActionFn = std::uint64_t (*)(void* menu, std::uint32_t id, void* arg);

constexpr char kModsHelp[] = "Settings for installed mods (CMFM).";
constexpr std::size_t kItemsCapacity = 24;

GetMessageFn g_get_message_next = nullptr;
ItemActionFn g_item_action_next = nullptr;
std::uint32_t g_get_message_hook = 0;
std::uint32_t g_item_action_hook = 0;
std::uintptr_t g_base = 0;
std::atomic<std::uint32_t> g_mods_selected{0};

alignas(16) StartMenuItem g_items[kItemsCapacity];
// al.patch handles of the two tab-list values (0: not changed).
std::uint32_t g_patch_tabs = 0;
std::uint32_t g_patch_tabs_in_quest = 0;

void log(const Env& env, al_log_level_t level, const std::string& line) {
    env.api->log(env.self, level, line.c_str());
}

std::string hex(std::uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(v));
    return b;
}

void* message_file(std::uintptr_t rva) {
    return *reinterpret_cast<void* const*>(g_base + rva);
}

// Detours run on the game's threads: no allocation, no logging in the text
// lookup (it runs for every text the game shows).
const char* cmfm_text(void* self, std::uint32_t id) {
    // Fast path: CMFM's ids are 9000 and up, plus the Options header and tab
    // names (0-5) while the CMFM view is on.
    if (id >= game::kModsLabelId || id <= 5) {
        if (id == game::kModsLabelId && self == message_file(game::kRvaMessages11)) {
            return kModsLabel;
        }
        if (id == game::kModsHelpId && self == message_file(game::kRvaMessages18)) {
            return kModsHelp;
        }
        if (const char* text = options_view_text(self, id)) {
            return text;
        }
    }
    // The category labels while CMFM's page shows CMFM's menu.
    return mods_page_text(self, id);
}

const char* get_message_detour(void* self, std::uint32_t id) {
    const std::uint64_t t0 = measure::start();
    const char* text = cmfm_text(self, id);
    measure::stop(measure::Hook::Text, t0);
    return text != nullptr ? text : g_get_message_next(self, id);
}

std::uint64_t item_action_detour(void* menu, std::uint32_t id, void* arg) {
    if (id == static_cast<std::uint32_t>(game::kItemMods)) {
        g_mods_selected.fetch_add(1, std::memory_order_relaxed);
        // CMFM's page when it is ready (the menu closes and opens again when
        // the page closes, as for Options).
        if (mods_page_open_from_start_menu(menu)) {
            return 0;
        }
        // Otherwise the Options page in mode 0, with CMFM's tables when the
        // view is ready; the plain Options page otherwise.
        options_view_enter();
        return g_item_action_next(menu, static_cast<std::uint32_t>(game::kItemOptions), arg);
    }
    if (id == static_cast<std::uint32_t>(game::kItemOptions)) {
        options_view_exit();  // the game's own Options always show the game's tables
    }
    return g_item_action_next(menu, id, arg);
}

// The System tab's entry in a tab list, and its item table pointer. The
// check and the swap both use these, so they cannot disagree.
std::uintptr_t system_entry(std::uintptr_t list_rva) {
    return g_base + list_rva + game::kSystemTabIndex * game::kTabListEntrySize;
}
std::uintptr_t system_items_slot(std::uintptr_t list_rva) {
    return system_entry(list_rva) + game::kTabListItemsOffset;
}

// One tab list's System table pointer, from `from` to `to`, through al.patch
// (it checks the old value, writes atomically and records CMFM as the
// owner). 0 if refused; Astera Loader's line in CMFM's log says why.
std::uint32_t change_items(const Env& env, const char* name, std::uintptr_t list_rva, std::uint64_t from,
                           std::uint64_t to) {
    al_patch_t change = {sizeof(al_patch_t)};
    change.kind = AL_PATCH_DATA;
    change.name = name;
    change.address = reinterpret_cast<void*>(system_items_slot(list_rva));
    change.length = sizeof(std::uint64_t);
    change.expected = &from;  // al.patch copies both during apply
    change.replacement = &to;
    return env.patches->apply(env.self, &change);
}

}  // namespace

bool start_menu_register(const Env& env, std::string& why) {
    g_base = env.base;
    al_hook_t h = {sizeof(al_hook_t)};
    h.name = "CMFM:rGUIMessage::getMessage";
    h.pattern = game::kGetMessagePattern;
    h.detour = reinterpret_cast<void*>(&get_message_detour);
    h.next = reinterpret_cast<void**>(&g_get_message_next);
    g_get_message_hook = env.hooks->hook(env.self, &h);
    if (g_get_message_hook == 0) {
        why = "the text lookup hook was refused (see the lines above)";
        return false;
    }
    h = {sizeof(al_hook_t)};
    h.name = "CMFM:uGUIStartMenu::onItemAction";
    h.pattern = game::kItemActionPattern;
    h.detour = reinterpret_cast<void*>(&item_action_detour);
    h.next = reinterpret_cast<void**>(&g_item_action_next);
    g_item_action_hook = env.hooks->hook(env.self, &h);
    if (g_item_action_hook == 0) {
        env.hooks->set_enabled(env.self, g_get_message_hook, 0);
        why = "the Start menu action hook was refused (see the lines above)";
        return false;
    }
    log(env, AL_LOG_INFO, "native: hooks registered (text lookup, Start menu action)");
    return true;
}

bool start_menu_hooks_active(const Env& env, std::string& why) {
    if (env.hooks->state(env.self, g_get_message_hook) != AL_HOOK_ACTIVE ||
        env.hooks->state(env.self, g_item_action_hook) != AL_HOOK_ACTIVE) {
        why = "the text and Start menu hooks are not active";
        return false;
    }
    return true;
}

bool start_menu_add_entry(const Env& env, std::string& why) {

    // The tab lists must name the System tab and point at the expected table.
    const std::uintptr_t system_items = env.base + game::kRvaSystemItems;
    for (const std::uintptr_t rva : {game::kRvaTabList, game::kRvaTabListInQuest}) {
        const std::uint32_t label = *reinterpret_cast<const std::uint32_t*>(system_entry(rva));
        const std::uint64_t items = *reinterpret_cast<const std::uint64_t*>(system_items_slot(rva));
        if (label != game::kSystemTabLabel || items != system_items) {
            why = "tab list at RVA " + hex(rva) + " differs: label " + std::to_string(label) + ", items " +
                  hex(items) + " (expected " + std::to_string(game::kSystemTabLabel) + ", " + hex(system_items) + ")";
            return false;
        }
    }

    const StartMenuItem mods = {game::kItemMods, static_cast<std::int32_t>(game::kModsLabelId),
                                static_cast<std::int32_t>(game::kModsHelpId), 0};
    const ItemTableResult built =
        insert_item_after(reinterpret_cast<const StartMenuItem*>(system_items), game::kSystemItemsMaxRead,
                          game::kItemOptions, mods, g_items, kItemsCapacity);
    if (built.error != ItemTableError::none) {
        why = std::string("System tab table: ") + to_string(built.error);
        return false;
    }
    std::string ids;
    for (std::size_t i = 0; i < built.count; ++i) {
        ids += (i ? " " : "") + std::to_string(g_items[i].id);
    }

    // The tab lists are constant tables in the game's read-only data:
    // changed through Astera Loader's al.patch (docs/al-requests.md, item 1).
    if (env.patches == nullptr) {
        why = "Astera Loader offers no al.patch (API 1.8 or newer needed)";
        return false;
    }
    const std::uint64_t ours = reinterpret_cast<std::uint64_t>(&g_items[0]);
    g_patch_tabs = change_items(env, "Start menu: System tab items", game::kRvaTabList, system_items, ours);
    if (g_patch_tabs == 0) {
        why = "Astera Loader refused to change the Start menu's tab list (its reason is in cmfm.log)";
        return false;
    }
    g_patch_tabs_in_quest =
        change_items(env, "Start menu in a quest: System tab items", game::kRvaTabListInQuest, system_items, ours);
    if (g_patch_tabs_in_quest == 0) {
        env.patches->set_enabled(env.self, g_patch_tabs, 0);
        g_patch_tabs = 0;
        why = "Astera Loader refused to change the quest Start menu's tab list (its reason is in cmfm.log)";
        return false;
    }
    log(env, AL_LOG_INFO, "native: System tab now " + ids + " (Mods = " + std::to_string(game::kItemMods) +
                              ", label " + std::to_string(game::kModsLabelId) + ", help " +
                              std::to_string(game::kModsHelpId) + ")");
    return true;
}

const char* game_text(std::uintptr_t file_rva, std::uint32_t id) {
    void* file = g_base != 0 ? message_file(file_rva) : nullptr;
    return file != nullptr && g_get_message_next != nullptr ? g_get_message_next(file, id) : nullptr;
}

void start_menu_restore(const Env& env) {
    for (std::uint32_t* handle : {&g_patch_tabs_in_quest, &g_patch_tabs}) {
        if (*handle != 0 && env.patches->set_enabled(env.self, *handle, 0) != AL_OK) {
            log(env, AL_LOG_WARNING, "native: a Start menu tab list was not put back (Astera Loader's reason above)");
        }
        *handle = 0;
    }
    log(env, AL_LOG_INFO, "native: System tab restored; Mods selected " +
                              std::to_string(g_mods_selected.load(std::memory_order_relaxed)) + " time(s)");
}

}  // namespace cmfm::native
