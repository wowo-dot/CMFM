#include "native/data_patch.hpp"

#include <windows.h>

#include <cstdio>

namespace cmfm::native {
namespace {

std::string hex(std::uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(v));
    return b;
}

// Compare-and-swap under a temporary PAGE_READWRITE; the old protection is
// restored either way. Returns the value found before the swap.
bool swap_value(std::uintptr_t address, std::uint64_t from, std::uint64_t to, std::uint64_t& found,
                std::string& why) {
    if ((address & 7) != 0) {
        why = "address " + hex(address) + " is not 8-byte aligned";
        return false;
    }
    void* p = reinterpret_cast<void*>(address);
    DWORD old = 0;
    if (!VirtualProtect(p, sizeof(std::uint64_t), PAGE_READWRITE, &old)) {
        why = "VirtualProtect failed, error " + std::to_string(GetLastError());
        return false;
    }
    found = static_cast<std::uint64_t>(InterlockedCompareExchange64(
        static_cast<volatile LONG64*>(p), static_cast<LONG64>(to), static_cast<LONG64>(from)));
    DWORD ignored = 0;
    VirtualProtect(p, sizeof(std::uint64_t), old, &ignored);
    if (found != from) {
        why = "expected " + hex(from) + " at " + hex(address) + ", found " + hex(found);
        return false;
    }
    return true;
}

}  // namespace

bool QwordPatch::apply(const char* name, std::uintptr_t address, std::uint64_t expected, std::uint64_t replacement,
                       std::string& why) {
    if (applied_) {
        why = "already applied";
        return false;
    }
    std::uint64_t found = 0;
    if (!swap_value(address, expected, replacement, found, why)) {
        return false;
    }
    name_ = name;
    address_ = address;
    expected_ = expected;
    replacement_ = replacement;
    applied_ = true;
    return true;
}

bool QwordPatch::retarget(std::uint64_t replacement, std::string& why) {
    if (!applied_) {
        why = "not applied";
        return false;
    }
    std::uint64_t found = 0;
    if (!swap_value(address_, replacement_, replacement, found, why)) {
        return false;
    }
    replacement_ = replacement;
    return true;
}

bool QwordPatch::restore(std::string& why) {
    if (!applied_) {
        return true;
    }
    std::uint64_t found = 0;
    if (!swap_value(address_, replacement_, expected_, found, why)) {
        return false;
    }
    applied_ = false;
    return true;
}

}  // namespace cmfm::native
