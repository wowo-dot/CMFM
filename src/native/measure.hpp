// A measurement build (CMake option CMFM_MEASURE, off by default): counts and
// times the CMFM hooks that run while the menu is closed (the text lookup,
// in play too; the title scene's update, every frame on the title screen)
// with the CPU's time stamp counter, split by whether CMFM's page is shown.
// The timing itself adds a little, so the numbers are an upper bound. Normal
// builds compile all of this to nothing.
#pragma once

#include <cstdint>
#include <string>

#if defined(CMFM_MEASURE)
#include <atomic>
#include <intrin.h>
#endif

namespace cmfm::native::measure {

enum class Hook { Text, TitleScene, Count };

#if defined(CMFM_MEASURE)
struct Counter {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> ticks{0};
};
extern Counter g_counters[static_cast<int>(Hook::Count)][2];  // [hook][page shown]
extern std::atomic<bool> g_page_shown;

inline std::uint64_t start() { return __rdtsc(); }
// Adds the time since `t0`; `call`: this ends one call (false: a second
// stretch of the same call, after the game's own function returned).
inline void stop(Hook hook, std::uint64_t t0, bool call = true) {
    const std::uint64_t ticks = __rdtsc() - t0;
    Counter& c = g_counters[static_cast<int>(hook)][g_page_shown.load(std::memory_order_relaxed) ? 1 : 0];
    if (call) {
        c.calls.fetch_add(1, std::memory_order_relaxed);
    }
    c.ticks.fetch_add(ticks, std::memory_order_relaxed);
}
#else
inline std::uint64_t start() { return 0; }
inline void stop(Hook, std::uint64_t, bool = true) {}
#endif

// At init: the clocks' starting points.
void begin();
// CMFM's page shown or closed (its own time is kept apart).
void page_shown(bool shown);
// The numbers so far as one log line; "" in a normal build.
std::string report();

}  // namespace cmfm::native::measure
