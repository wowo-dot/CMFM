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

bool writable(DWORD protect) {
    switch (protect & 0xFF) {
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY: return true;
        default: return false;
    }
}

// Compare-and-swap in writable data; no protection is changed. `found` is the
// value seen before the swap.
bool swap_value(std::uintptr_t address, std::uint64_t from, std::uint64_t to, std::uint64_t& found,
                std::string& why) {
    if ((address & 7) != 0) {
        why = "address " + hex(address) + " is not 8-byte aligned";
        return false;
    }
    void* p = reinterpret_cast<void*>(address);
    MEMORY_BASIC_INFORMATION info = {};
    if (VirtualQuery(p, &info, sizeof info) != sizeof info || info.State != MEM_COMMIT) {
        why = "address " + hex(address) + " is not in use";
        return false;
    }
    if (!writable(info.Protect)) {
        why = "address " + hex(address) + " is read-only (expected writable data)";
        return false;
    }
    found = static_cast<std::uint64_t>(InterlockedCompareExchange64(
        static_cast<volatile LONG64*>(p), static_cast<LONG64>(to), static_cast<LONG64>(from)));
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
