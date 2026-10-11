// Key values of KEY settings (cmfm.h, CMFM_TYPE_KEY): a Windows virtual-key
// code in bits 0-7 and CMFM_KEY_CTRL / _SHIFT / _ALT, as Astera Loader's
// hotkeys take them. Their names as the Mods page shows them, which keys a
// setting may hold, and when two of them overlap. Pure logic, tested in
// tests/test_menu.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cmfm::menu {

// A key a KEY setting may hold: not a modifier (Ctrl, Shift, Alt, either
// side), not a Windows key, not the left or right mouse button.
bool key_bindable(std::uint32_t vk);
// 0 (no key), or a bindable key with only CMFM_KEY_* bits above it.
bool key_valid(std::int32_t key);
// Astera Loader fires a hotkey while its key and at least its modifiers are
// held, so F5 fires with Ctrl+F5 too: the same key, one's modifiers part of
// the other's. No key overlaps nothing.
bool keys_overlap(std::int32_t a, std::int32_t b);

// The character a key types on the player's layout, for the keys whose
// character depends on it (";", "[", the key left of 1 ...); UTF-8 into
// `out`, false if none. Without one, a US layout's characters are used.
using KeyCharFn = bool (*)(std::uint32_t vk, char* out, std::size_t size);
void set_key_chars(KeyCharFn fn);

// "Ctrl+Shift+F5", "Mouse 4", "None"; "Key 0x92" for keys without a name.
std::string key_name(std::int32_t key);

}  // namespace cmfm::menu
