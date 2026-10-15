#include "native/page_class.hpp"

#include "native/game_421810.hpp"
#include "native/gui.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace cmfm::native {
namespace {

// MtDTI as MtDti::ctor fills it.
struct Dti {
    void* const* vtable;
    const char* name;
    void* next;       // next sibling under the parent
    void* child;      // first child
    const void* parent;
    void* hash_next;  // chain in the game's class table
    std::uint32_t flags;  // object size / 4 in the low 23 bits, allocator bits above
    std::uint32_t id;
};
static_assert(sizeof(Dti) == 0x38);
static_assert(offsetof(Dti, parent) == 0x20 && offsetof(Dti, flags) == 0x30 && offsetof(Dti, id) == 0x34);

static_assert(sizeof(GameRef) == 24);

using DtiNewFn = void* (*)(const void* dti);
using DtiCtorFn = void* (*)(const void* dti, void* memory);
using DtiCtorArrayFn = void* (*)(const void* dti, void* memory, std::uint32_t count);
using AddBottomFn = std::uint8_t (*)(void* units, std::int32_t line, void* unit, std::uint64_t group_bits,
                                     std::uint8_t unused);
using RefLockFn = void* (*)();
using SetRefFn = void (*)(void* lock, void* unit, GameRef* slot);
using ClearRefFn = void (*)(void* lock, GameRef* slot);
using RequestOpenFn = void (*)(void* unit, void* opener);
using RequestCloseFn = std::uint32_t (*)(void* unit, std::uint8_t now);

// The game's class ids: crc32 without the final inversion, 31 bits.
constexpr std::uint32_t class_id(const char* name) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (; *name != '\0'; ++name) {
        c ^= static_cast<std::uint8_t>(*name);
        for (int k = 0; k < 8; ++k) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return c & 0x7FFFFFFFu;
}
static_assert(class_id(game::kOptionPageClassName) == game::kOptionPageClassId);

// Not in the game's class table (24,051 ids at 421810; no collision).
constexpr char kClassName[] = "uGUICmfmPage";
constexpr std::uint32_t kClassId = class_id(kClassName);
constexpr std::size_t kOptionPageSize = 0x7810;  // what uGUIOption's create allocates

Env g_env = {};
AddBottomFn g_add_bottom = nullptr;
RefLockFn g_ref_lock = nullptr;
SetRefFn g_set_ref = nullptr;
ClearRefFn g_clear_ref = nullptr;
RequestOpenFn g_request_open = nullptr;
RequestCloseFn g_request_close = nullptr;
void* g_found_update = nullptr;
void* g_found_open = nullptr;
void* g_found_close = nullptr;

const Dti* g_option_dti = nullptr;
Dti g_dti = {};
void* g_dti_vtable[game::kDtiVtableSlots] = {};
void* g_vtable[game::kOptionPageVtableSlots] = {};
UnitFn g_next_update = nullptr;
UnitFn g_next_open = nullptr;
UnitFn g_next_close = nullptr;
const char* g_layout = game::kOptionPageLayout;
GameRef g_ref = {};
bool g_ready = false;

void logf(al_log_level_t level, const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
    g_env.api->log(g_env.self, level, line);
}

bool find(const char* name, const char* pattern, void** out, std::string& why) {
    return find_game_function(g_env, name, pattern, out, why);
}

// The target of `call rel32` at `at`, or 0.
std::uintptr_t call_target(const std::uint8_t* at) {
    if (at[0] != 0xE8) {
        return 0;
    }
    std::int32_t rel = 0;
    std::memcpy(&rel, at + 1, sizeof rel);
    return reinterpret_cast<std::uintptr_t>(at) + 5 + rel;
}

// The address a `lea rax, [rip+rel32]; ret` function returns, or 0.
std::uintptr_t lea_return(const std::uint8_t* at) {
    if (at == nullptr || at[0] != 0x48 || at[1] != 0x8D || at[2] != 0x05 || at[7] != 0xC3) {
        return 0;
    }
    std::int32_t rel = 0;
    std::memcpy(&rel, at + 3, sizeof rel);
    return reinterpret_cast<std::uintptr_t>(at) + 7 + rel;
}

void set_class(void* object) {
    if (object != nullptr) {
        *static_cast<void* const**>(object) = g_vtable;
    }
}

// CMFM's class description: objects are made by uGUIOption's own code, then
// get CMFM's vtable.
void* dti_new(const void*) {
    void* object = reinterpret_cast<DtiNewFn>(g_option_dti->vtable[1])(g_option_dti);
    set_class(object);
    return object;
}

void* dti_ctor(const void*, void* memory) {
    void* object = reinterpret_cast<DtiCtorFn>(g_option_dti->vtable[2])(g_option_dti, memory);
    set_class(object);
    return object;
}

void* dti_ctor_array(const void*, void* memory, std::uint32_t count) {
    void* result = reinterpret_cast<DtiCtorArrayFn>(g_option_dti->vtable[3])(g_option_dti, memory, count);
    for (std::uint32_t i = 0; memory != nullptr && i < count; ++i) {
        set_class(static_cast<char*>(memory) + i * kOptionPageSize);
    }
    return result;
}

const void* get_dti(void*) {
    return &g_dti;
}

const char* get_gui_path(void*) {
    return g_layout;
}

}  // namespace

bool page_class_register(const Env& env, std::string& why) {
    g_env = env;
    void* add_bottom = nullptr;
    void* set_ref = nullptr;
    void* clear_ref = nullptr;
    void* request_open = nullptr;
    void* request_close = nullptr;
    if (!find("sMhUnit::addBottom", game::kAddBottomPattern, &add_bottom, why) ||
        !find("setRef", game::kSetRefPattern, &set_ref, why) ||
        !find("clearRef", game::kClearRefPattern, &clear_ref, why) ||
        !find("uMhGUI::requestOpen", game::kRequestOpenPattern, &request_open, why) ||
        !find("uMhGUI::requestClose", game::kRequestClosePattern, &request_close, why) ||
        !find("uGUIOption::update", game::kTitleOptionsUpdatePattern, &g_found_update, why) ||
        !find("uGUIOption::open", game::kTitleOptionsOpenPattern, &g_found_open, why) ||
        !find("uGUIOption::close", game::kTitleOptionsClosePattern, &g_found_close, why)) {
        return false;
    }
    const auto* open_code = static_cast<const std::uint8_t*>(request_open);
    const std::uintptr_t lock = call_target(open_code + game::kRequestOpenLockCall);
    if (lock == 0 || call_target(open_code + game::kRequestOpenSetRefCall) != reinterpret_cast<std::uintptr_t>(set_ref)) {
        why = "uMhGUI::requestOpen does not call setRef as expected";
        return false;
    }
    g_add_bottom = reinterpret_cast<AddBottomFn>(add_bottom);
    g_ref_lock = reinterpret_cast<RefLockFn>(lock);
    g_set_ref = reinterpret_cast<SetRefFn>(set_ref);
    g_clear_ref = reinterpret_cast<ClearRefFn>(clear_ref);
    g_request_open = reinterpret_cast<RequestOpenFn>(request_open);
    g_request_close = reinterpret_cast<RequestCloseFn>(request_close);
    return true;
}

bool page_class_check(const Env& env, const PageHandlers& handlers, std::string& why) {
    auto* const* vtable = reinterpret_cast<void* const*>(env.base + game::kRvaOptionPageVtable);
    const auto* dti = reinterpret_cast<const Dti*>(env.base + game::kRvaOptionPageDti);
    if (lea_return(static_cast<const std::uint8_t*>(vtable[game::kSlotGetDti])) !=
        reinterpret_cast<std::uintptr_t>(dti)) {
        why = "uGUIOption's vtable does not return its class description";
        return false;
    }
    const auto* path =
        reinterpret_cast<const char*>(lea_return(static_cast<const std::uint8_t*>(vtable[game::kSlotGetGuiPath])));
    if (path == nullptr || std::strcmp(path, game::kOptionPageLayout) != 0) {
        why = "uGUIOption's vtable does not name its layout";
        return false;
    }
    if (vtable[game::kSlotUpdate] != g_found_update || vtable[game::kSlotOpen] != g_found_open ||
        vtable[game::kSlotClose] != g_found_close) {
        why = "uGUIOption's vtable does not hold its update, open and close functions";
        return false;
    }
    if (dti->vtable == nullptr || dti->name == nullptr || std::strcmp(dti->name, game::kOptionPageClassName) != 0 ||
        dti->id != game::kOptionPageClassId || ((dti->flags & 0x7FFFFFu) << 2) != kOptionPageSize) {
        why = "uGUIOption's class description differs";
        return false;
    }
    for (std::size_t s = 0; s < game::kDtiVtableSlots; ++s) {
        if (dti->vtable[s] == nullptr) {
            why = "uGUIOption's class description has an empty slot";
            return false;
        }
    }
    g_option_dti = dti;
    std::memcpy(g_vtable, vtable, sizeof g_vtable);
    g_next_update = reinterpret_cast<UnitFn>(vtable[game::kSlotUpdate]);
    g_next_open = reinterpret_cast<UnitFn>(vtable[game::kSlotOpen]);
    g_next_close = reinterpret_cast<UnitFn>(vtable[game::kSlotClose]);
    g_vtable[game::kSlotGetDti] = reinterpret_cast<void*>(&get_dti);
    g_vtable[game::kSlotGetGuiPath] = reinterpret_cast<void*>(&get_gui_path);
    g_vtable[game::kSlotUpdate] = reinterpret_cast<void*>(handlers.update);
    g_vtable[game::kSlotOpen] = reinterpret_cast<void*>(handlers.open);
    g_vtable[game::kSlotClose] = reinterpret_cast<void*>(handlers.close);
    std::memcpy(g_dti_vtable, dti->vtable, sizeof g_dti_vtable);
    g_dti_vtable[1] = reinterpret_cast<void*>(&dti_new);
    g_dti_vtable[2] = reinterpret_cast<void*>(&dti_ctor);
    g_dti_vtable[3] = reinterpret_cast<void*>(&dti_ctor_array);
    g_dti = {g_dti_vtable, kClassName, nullptr, nullptr, dti, nullptr, dti->flags, kClassId};
    g_ready = true;
    logf(AL_LOG_INFO, "page class %s ready (id 0x%08x, child of %s, %zu vtable slots)", kClassName, kClassId,
         dti->name, game::kOptionPageVtableSlots);
    return true;
}

bool unit_alive(const void* unit) {
    return unit != nullptr && (static_cast<const std::uint8_t*>(unit)[game::kUnitState] & 0xE) != 0;
}

bool unit_running(const void* unit) {
    return unit != nullptr && (static_cast<const std::uint8_t*>(unit)[game::kUnitState] & 0x8) != 0;
}

void game_ref_set(GameRef& ref, void* unit) {
    if (unit != nullptr) {
        g_set_ref(g_ref_lock(), unit, &ref);
    } else if (ref.unit != nullptr) {
        g_clear_ref(g_ref_lock(), &ref);
    }
}

void* game_ref_alive(const GameRef& ref) {
    void* unit = *static_cast<void* const volatile*>(&ref.unit);
    return unit_alive(unit) ? unit : nullptr;
}

void* page_alive() {
    return game_ref_alive(g_ref);
}

void* page_create(std::string& why) {
    if (!g_ready) {
        why = "the page class is not ready";
        return nullptr;
    }
    void* units = *reinterpret_cast<void* const*>(g_env.base + game::kRvaUnitManager);
    if (units == nullptr ||
        *static_cast<const std::uintptr_t*>(units) != g_env.base + game::kRvaUnitManagerVtable) {
        why = "the unit manager is not where expected";
        return nullptr;
    }
    void* unit = dti_new(&g_dti);
    if (unit == nullptr) {
        why = "the game could not allocate the page";
        return nullptr;
    }
    g_add_bottom(units, game::kGuiMoveLine, unit, game::kGuiGroupBits, 0);
    // Not kept: it dies with its scene (page_class.hpp).
    g_set_ref(g_ref_lock(), unit, &g_ref);
    if (g_ref.unit != unit) {
        // setRef does nothing while the game has references switched off; the
        // page then lives on unfollowed and dies with the scene.
        why = "the game did not track the page";
        return nullptr;
    }
    return unit;
}

void page_keep(void* unit, bool keep) {
    if (unit == nullptr) {
        return;
    }
    std::uint32_t& flags = *reinterpret_cast<std::uint32_t*>(static_cast<char*>(unit) + game::kUnitFlags);
    flags = keep ? (flags | game::kUnitFlagKeep) : (flags & ~game::kUnitFlagKeep);
}

UnitFn page_next(std::size_t slot) {
    switch (slot) {
    case game::kSlotUpdate:
        return g_next_update;
    case game::kSlotOpen:
        return g_next_open;
    case game::kSlotClose:
        return g_next_close;
    default:
        return nullptr;
    }
}

void page_set_layout(const char* path) {
    g_layout = path;
}

const char* page_layout() {
    return g_layout;
}

void gui_request_open(void* unit, void* opener) {
    g_request_open(unit, opener);
}

void gui_request_close(void* unit) {
    g_request_close(unit, 0);
}

void page_release() {
    if (g_ready && g_ref.unit != nullptr) {
        g_clear_ref(g_ref_lock(), &g_ref);
    }
}

}  // namespace cmfm::native
