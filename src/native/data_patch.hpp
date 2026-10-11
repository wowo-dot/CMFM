// Prototype helper for writing one pointer-sized value in the game's
// read-only data (decisions.md item 9). It checks the expected old value,
// swaps atomically, and restores only if the value is still ours. Once the
// native menu is confirmed in game, this becomes an Astera Loader request.
#pragma once

#include <cstdint>
#include <string>

namespace cmfm::native {

class QwordPatch {
public:
    // Writes `replacement` at `address` if it holds `expected`. On failure
    // nothing is written and `why` says what was found.
    bool apply(const char* name, std::uintptr_t address, std::uint64_t expected, std::uint64_t replacement,
               std::string& why);
    // Puts `expected` back if the value is still `replacement`.
    bool restore(std::string& why);
    // While applied: writes another replacement if the value is still the
    // current one (restore still puts `expected` back).
    bool retarget(std::uint64_t replacement, std::string& why);
    bool applied() const { return applied_; }
    const char* name() const { return name_; }

private:
    const char* name_ = "";
    std::uintptr_t address_ = 0;
    std::uint64_t expected_ = 0;
    std::uint64_t replacement_ = 0;
    bool applied_ = false;
};

}  // namespace cmfm::native
