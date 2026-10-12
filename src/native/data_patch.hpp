// Swapping one pointer-sized value in the game's data (decisions.md item 9).
// It checks the expected old value, swaps atomically, and restores only if
// the value is still ours. Writable data (.data: the Options pages' row
// tables) is CMFM's own business. Read-only data (.rdata: the Start menu's
// tab lists) needs its protection changed: that is the prototype helper,
// to be replaced by Astera Loader's service (docs/al-requests.md, item 1).
#pragma once

#include <cstdint>
#include <string>

namespace cmfm::native {

class QwordPatch {
public:
    // Writes `replacement` at `address` if it holds `expected`; the address
    // must be in writable data. On failure nothing is written and `why` says
    // what was found.
    bool apply(const char* name, std::uintptr_t address, std::uint64_t expected, std::uint64_t replacement,
               std::string& why);
    // The same in read-only data, under a temporary PAGE_READWRITE (the
    // prototype helper; only the Start menu's tab lists use it).
    bool apply_read_only(const char* name, std::uintptr_t address, std::uint64_t expected, std::uint64_t replacement,
                         std::string& why);
    // Puts `expected` back if the value is still `replacement`.
    bool restore(std::string& why);
    bool applied() const { return applied_; }
    const char* name() const { return name_; }

private:
    bool apply_with(bool read_only, const char* name, std::uintptr_t address, std::uint64_t expected,
                    std::uint64_t replacement, std::string& why);

    const char* name_ = "";
    std::uintptr_t address_ = 0;
    std::uint64_t expected_ = 0;
    std::uint64_t replacement_ = 0;
    bool read_only_ = false;
    bool applied_ = false;
};

}  // namespace cmfm::native
