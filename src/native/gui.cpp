#include "native/gui.hpp"

#include "native/game_421810.hpp"

namespace cmfm::native {
namespace {

using SetRowCountFn = void (*)(void* list, std::uint64_t count);
using SetCursorFn = std::uint64_t (*)(void* list, std::uint32_t row);
using GetObjectFn = void* (*)(void* root, const char* name);
using FormatTextFn = std::uint64_t (*)(void* object, const char* text);
using GetInstanceFn = void* (*)(void* unit, const char* name);
using SetPlayFn = void (*)(void* instance, bool play);

SetRowCountFn g_set_row_count = nullptr;
SetCursorFn g_set_cursor = nullptr;
GetObjectFn g_get_object = nullptr;
FormatTextFn g_format_text = nullptr;
GetInstanceFn g_get_instance = nullptr;
SetPlayFn g_set_play = nullptr;

}  // namespace

bool find_game_function(const Env& env, const char* name, const char* pattern, void** out, std::string& why) {
    void* match = nullptr;
    if (env.scanner == nullptr || env.scanner->find(env.self, pattern, AL_SCAN_CODE, &match, 1) != 1) {
        why = std::string("game function not found: ") + name;
        return false;
    }
    *out = match;
    return true;
}

bool gui_register(const Env& env, std::string& why) {
    return find_game_function(env, "setRowCount", game::kListSetRowCountPattern,
                              reinterpret_cast<void**>(&g_set_row_count), why) &&
           find_game_function(env, "setCursor", game::kListSetCursorPattern, reinterpret_cast<void**>(&g_set_cursor),
                              why) &&
           find_game_function(env, "getObjectFromName", game::kGetObjectFromNamePattern,
                              reinterpret_cast<void**>(&g_get_object), why) &&
           find_game_function(env, "formatText", game::kFormatTextPattern, reinterpret_cast<void**>(&g_format_text),
                              why) &&
           find_game_function(env, "getInstanceFromName", game::kGetInstanceFromNamePattern,
                              reinterpret_cast<void**>(&g_get_instance), why) &&
           find_game_function(env, "setPlay", game::kSetPlayPattern, reinterpret_cast<void**>(&g_set_play), why);
}

void gui_set_row_count(void* list, std::uint64_t count) {
    g_set_row_count(list, count);
}

void gui_set_cursor(void* list, std::uint32_t row) {
    g_set_cursor(list, row);
}

void* gui_get_instance(void* unit, const char* name) {
    return g_get_instance(unit, name);
}

void* gui_get_object(void* root, const char* name) {
    return g_get_object(root, name);
}

void gui_format_text(void* object, const char* text) {
    g_format_text(object, text);
}

void gui_set_play(void* instance, bool play) {
    g_set_play(instance, play);
}

void* list_row(void* list, std::uint32_t row) {
    if (list == nullptr || row >= field<std::uint32_t>(list, game::kListRowCount)) {
        return nullptr;
    }
    return field<void**>(list, game::kListRows)[row];
}

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

}  // namespace cmfm::native
