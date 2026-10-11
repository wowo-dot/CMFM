#include "menu/keys.hpp"

#include "cmfm/cmfm.h"

#include <cstdio>
#include <cstring>

namespace cmfm::menu {

namespace {

constexpr std::uint32_t kKeyBits = 0xFFu | CMFM_KEY_CTRL | CMFM_KEY_SHIFT | CMFM_KEY_ALT;

KeyCharFn g_key_chars = nullptr;

struct Named {
    std::uint32_t vk;
    const char* name;
};

// Windows virtual-key codes (winuser.h) with fixed names.
constexpr Named kNames[] = {
    {0x03, "Break"},       {0x04, "Mouse 3"},      {0x05, "Mouse 4"},     {0x06, "Mouse 5"},
    {0x08, "Backspace"},   {0x09, "Tab"},          {0x0C, "Clear"},       {0x0D, "Enter"},
    {0x13, "Pause"},       {0x14, "Caps Lock"},    {0x1B, "Esc"},         {0x20, "Space"},
    {0x21, "Page Up"},     {0x22, "Page Down"},    {0x23, "End"},         {0x24, "Home"},
    {0x25, "Left"},        {0x26, "Up"},           {0x27, "Right"},       {0x28, "Down"},
    {0x2C, "Print Screen"}, {0x2D, "Insert"},      {0x2E, "Delete"},      {0x5D, "Menu"},
    {0x6A, "Num *"},       {0x6B, "Num +"},        {0x6C, "Num Separator"},   {0x6D, "Num -"},
    {0x6E, "Num ."},       {0x6F, "Num /"},        {0x90, "Num Lock"},    {0x91, "Scroll Lock"},
    {0xAD, "Mute"},        {0xAE, "Volume Down"},  {0xAF, "Volume Up"},   {0xB0, "Next Track"},
    {0xB1, "Previous Track"}, {0xB2, "Stop"},      {0xB3, "Play/Pause"},
};

// The keys whose character depends on the layout, as a US layout types them.
constexpr Named kUsChars[] = {
    {0xBA, ";"}, {0xBB, "="}, {0xBC, ","}, {0xBD, "-"}, {0xBE, "."}, {0xBF, "/"},
    {0xC0, "`"}, {0xDB, "["}, {0xDC, "\\"}, {0xDD, "]"}, {0xDE, "'"}, {0xE2, "\\"},
};

std::string vk_name(std::uint32_t vk) {
    char b[32];
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= 0x70 && vk <= 0x87) {  // VK_F1..VK_F24
        std::snprintf(b, sizeof b, "F%u", vk - 0x70 + 1);
        return b;
    }
    if (vk >= 0x60 && vk <= 0x69) {  // VK_NUMPAD0..9
        std::snprintf(b, sizeof b, "Num %u", vk - 0x60);
        return b;
    }
    for (const Named& n : kNames) {
        if (n.vk == vk) {
            return n.name;
        }
    }
    for (const Named& n : kUsChars) {
        if (n.vk != vk) {
            continue;
        }
        // The game's text format reads '<' as a tag; such a key keeps its code.
        if (g_key_chars != nullptr && g_key_chars(vk, b, sizeof b) && b[0] != '\0' && std::strchr(b, '<') == nullptr &&
            std::strchr(b, '>') == nullptr) {
            return b;
        }
        return n.name;
    }
    std::snprintf(b, sizeof b, "Key 0x%02X", vk);
    return b;
}

}  // namespace

bool key_bindable(std::uint32_t vk) {
    switch (vk) {
        case 0x00:
        case 0x01:  // left mouse button
        case 0x02:  // right mouse button
        case 0x10:  // Shift, Ctrl, Alt
        case 0x11:
        case 0x12:
        case 0x5B:  // Windows keys
        case 0x5C:
            return false;
        default:
            return vk <= 0xFE && !(vk >= 0xA0 && vk <= 0xA5);  // left and right Shift, Ctrl, Alt
    }
}

bool key_valid(std::int32_t key) {
    const auto k = static_cast<std::uint32_t>(key);
    return key == 0 || ((k & ~kKeyBits) == 0 && key_bindable(CMFM_KEY_VK(k)));
}

bool keys_overlap(std::int32_t a, std::int32_t b) {
    const auto ka = static_cast<std::uint32_t>(a);
    const auto kb = static_cast<std::uint32_t>(b);
    if (a == 0 || b == 0 || CMFM_KEY_VK(ka) != CMFM_KEY_VK(kb)) {
        return false;
    }
    const std::uint32_t ma = CMFM_KEY_MODS(ka);
    const std::uint32_t mb = CMFM_KEY_MODS(kb);
    return (ma & mb) == ma || (ma & mb) == mb;
}

void set_key_chars(KeyCharFn fn) { g_key_chars = fn; }

std::string key_name(std::int32_t key) {
    if (key == 0) {
        return "None";
    }
    const auto k = static_cast<std::uint32_t>(key);
    std::string name;
    if ((k & CMFM_KEY_CTRL) != 0) {
        name += "Ctrl+";
    }
    if ((k & CMFM_KEY_SHIFT) != 0) {
        name += "Shift+";
    }
    if ((k & CMFM_KEY_ALT) != 0) {
        name += "Alt+";
    }
    return name + vk_name(CMFM_KEY_VK(k));
}

}  // namespace cmfm::menu
