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

// Compare-and-swap. Writable data is written as it is; read-only data under a
// temporary PAGE_READWRITE, the old protection restored either way. `found`
// is the value seen before the swap.
bool swap_value(bool read_only, std::uintptr_t address, std::uint64_t from, std::uint64_t to, std::uint64_t& found,
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
    if (writable(info.Protect) == read_only) {
        why = "address " + hex(address) + (read_only ? " is writable (expected read-only data)"
                                                     : " is read-only (expected writable data)");
        return false;
    }
    DWORD old = 0;
    if (read_only && !VirtualProtect(p, sizeof(std::uint64_t), PAGE_READWRITE, &old)) {
        why = "VirtualProtect failed, error " + std::to_string(GetLastError());
        return false;
    }
    found = static_cast<std::uint64_t>(InterlockedCompareExchange64(
        static_cast<volatile LONG64*>(p), static_cast<LONG64>(to), static_cast<LONG64>(from)));
    if (read_only) {
        DWORD ignored = 0;
        VirtualProtect(p, sizeof(std::uint64_t), old, &ignored);
    }
    if (found != from) {
        why = "expected " + hex(from) + " at " + hex(address) + ", found " + hex(found);
        return false;
    }
    return true;
}

}  // namespace

bool QwordPatch::apply(const char* name, std::uintptr_t address, std::uint64_t expected, std::uint64_t replacement,
                       std::string& why) {
    return apply_with(false, name, address, expected, replacement, why);
}

bool QwordPatch::apply_read_only(const char* name, std::uintptr_t address, std::uint64_t expected,
                                 std::uint64_t replacement, std::string& why) {
    return apply_with(true, name, address, expected, replacement, why);
}

bool QwordPatch::apply_with(bool read_only, const char* name, std::uintptr_t address, std::uint64_t expected,
                            std::uint64_t replacement, std::string& why) {
    if (applied_) {
        why = "already applied";
        return false;
    }
    std::uint64_t found = 0;
    if (!swap_value(read_only, address, expected, replacement, found, why)) {
        return false;
    }
    name_ = name;
    address_ = address;
    expected_ = expected;
    replacement_ = replacement;
    read_only_ = read_only;
    applied_ = true;
    return true;
}

bool QwordPatch::restore(std::string& why) {
    if (!applied_) {
        return true;
    }
    std::uint64_t found = 0;
    if (!swap_value(read_only_, address_, replacement_, expected_, found, why)) {
        return false;
    }
    applied_ = false;
    return true;
}

}  // namespace cmfm::native
