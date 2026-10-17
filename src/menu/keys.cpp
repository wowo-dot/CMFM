#include "menu/keys.hpp"

#include "cmfm/cmfm.h"
#include "cmfm/cmfm_check.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace cmfm::menu {

namespace {


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

bool same_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

// The virtual-key code of a key's name (no modifiers), or 0.
std::uint32_t vk_of_name(std::string_view n) {
    if (n.size() == 1) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(n[0])));
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
            return static_cast<std::uint32_t>(c);
        }
    }
    for (std::uint32_t vk = 1; vk < 0xFF; ++vk) {
        if (same_ci(vk_name(vk), n)) {
            return vk;
        }
    }
    return 0;
}

// Scan code -> virtual-key code (DirectInput's DIK_ constants, set 1 and the
// extended keys).
struct DikVk {
    std::uint32_t dik;
    std::uint32_t vk;
};
constexpr DikVk kDik[] = {
    {0x01, 0x1B}, {0x02, '1'},  {0x03, '2'},  {0x04, '3'},  {0x05, '4'},  {0x06, '5'},  {0x07, '6'},  {0x08, '7'},
    {0x09, '8'},  {0x0A, '9'},  {0x0B, '0'},  {0x0C, 0xBD}, {0x0D, 0xBB}, {0x0E, 0x08}, {0x0F, 0x09}, {0x10, 'Q'},
    {0x11, 'W'},  {0x12, 'E'},  {0x13, 'R'},  {0x14, 'T'},  {0x15, 'Y'},  {0x16, 'U'},  {0x17, 'I'},  {0x18, 'O'},
    {0x19, 'P'},  {0x1A, 0xDB}, {0x1B, 0xDD}, {0x1C, 0x0D}, {0x1D, 0xA2}, {0x1E, 'A'},  {0x1F, 'S'},  {0x20, 'D'},
    {0x21, 'F'},  {0x22, 'G'},  {0x23, 'H'},  {0x24, 'J'},  {0x25, 'K'},  {0x26, 'L'},  {0x27, 0xBA}, {0x28, 0xDE},
    {0x29, 0xC0}, {0x2A, 0xA0}, {0x2B, 0xDC}, {0x2C, 'Z'},  {0x2D, 'X'},  {0x2E, 'C'},  {0x2F, 'V'},  {0x30, 'B'},
    {0x31, 'N'},  {0x32, 'M'},  {0x33, 0xBC}, {0x34, 0xBE}, {0x35, 0xBF}, {0x36, 0xA1}, {0x37, 0x6A}, {0x38, 0xA4},
    {0x39, 0x20}, {0x3A, 0x14}, {0x3B, 0x70}, {0x3C, 0x71}, {0x3D, 0x72}, {0x3E, 0x73}, {0x3F, 0x74}, {0x40, 0x75},
    {0x41, 0x76}, {0x42, 0x77}, {0x43, 0x78}, {0x44, 0x79}, {0x45, 0x90}, {0x46, 0x91}, {0x47, 0x67}, {0x48, 0x68},
    {0x49, 0x69}, {0x4A, 0x6D}, {0x4B, 0x64}, {0x4C, 0x65}, {0x4D, 0x66}, {0x4E, 0x6B}, {0x4F, 0x61}, {0x50, 0x62},
    {0x51, 0x63}, {0x52, 0x60}, {0x53, 0x6E}, {0x56, 0xE2}, {0x57, 0x7A}, {0x58, 0x7B}, {0x64, 0x7C}, {0x65, 0x7D},
    {0x66, 0x7E}, {0x9D, 0xA3}, {0xB5, 0x6F}, {0xB7, 0x2C}, {0xB8, 0xA5}, {0xC5, 0x13}, {0xC7, 0x24}, {0xC8, 0x26},
    {0xC9, 0x21}, {0xCB, 0x25}, {0xCD, 0x27}, {0xCF, 0x23}, {0xD0, 0x28}, {0xD1, 0x22}, {0xD2, 0x2D}, {0xD3, 0x2E},
    {0xDD, 0x5D},
};

}  // namespace

bool parse_key_name(std::string_view name, std::int32_t& key) {
    std::uint32_t mods = 0;
    std::string_view rest = name;
    for (;;) {
        const std::size_t plus = rest.find('+');
        if (plus == std::string_view::npos || plus + 1 >= rest.size()) {
            break;  // the last part ("+" alone is no key name here)
        }
        const std::string_view part = rest.substr(0, plus);
        if (same_ci(part, "Ctrl") || same_ci(part, "Control")) {
            mods |= CMFM_KEY_CTRL;
        } else if (same_ci(part, "Shift")) {
            mods |= CMFM_KEY_SHIFT;
        } else if (same_ci(part, "Alt")) {
            mods |= CMFM_KEY_ALT;
        } else {
            return false;
        }
        rest = rest.substr(plus + 1);
    }
    if (same_ci(rest, "None") && mods == 0) {
        key = 0;
        return true;
    }
    const std::uint32_t vk = vk_of_name(rest);
    if (vk == 0 || !key_bindable(vk)) {
        return false;
    }
    key = static_cast<std::int32_t>(vk | mods);
    return true;
}

std::uint32_t vk_from_dik(std::uint32_t dik) {
    for (const DikVk& d : kDik) {
        if (d.dik == dik) {
            return d.vk;
        }
    }
    return dik == 0x9C ? 0x0D : 0;  // the keypad's Enter reads as Enter
}

std::uint32_t dik_from_vk(std::uint32_t vk) {
    switch (vk) {
        case 0x10: vk = 0xA0; break;  // Shift, Ctrl, Alt: the left key's code
        case 0x11: vk = 0xA2; break;
        case 0x12: vk = 0xA4; break;
        default: break;
    }
    for (const DikVk& d : kDik) {
        if (d.vk == vk) {
            return d.dik;
        }
    }
    return 0;
}

// The rules are cmfm_check.h's (plugins check their defaults with them).
bool key_bindable(std::uint32_t vk) {
    return cmfm_check_key_code(vk) != 0;
}

bool key_valid(std::int32_t key) {
    return cmfm_check_key(key) != 0;
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
