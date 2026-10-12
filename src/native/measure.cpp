#include "native/measure.hpp"

#if defined(CMFM_MEASURE)

#include <windows.h>

#include <cstdio>

namespace cmfm::native::measure {

Counter g_counters[static_cast<int>(Hook::Count)][2];
std::atomic<bool> g_page_shown{false};

namespace {

LARGE_INTEGER g_qpc_begin = {};
std::uint64_t g_tsc_begin = 0;
std::uint64_t g_shown_since = 0;  // QPC; 0 while closed
std::uint64_t g_shown_qpc = 0;    // QPC counts spent with the page shown

std::uint64_t qpc() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<std::uint64_t>(now.QuadPart);
}

}  // namespace

void begin() {
    QueryPerformanceCounter(&g_qpc_begin);
    g_tsc_begin = __rdtsc();
}

void page_shown(bool shown) {
    if (shown && g_shown_since == 0) {
        g_shown_since = qpc();
    } else if (!shown && g_shown_since != 0) {
        g_shown_qpc += qpc() - g_shown_since;
        g_shown_since = 0;
    }
    g_page_shown.store(shown, std::memory_order_relaxed);
}

std::string report() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    const std::uint64_t now = qpc();
    const double elapsed = static_cast<double>(now - static_cast<std::uint64_t>(g_qpc_begin.QuadPart)) /
                           static_cast<double>(frequency.QuadPart);
    if (elapsed <= 0) {
        return "measure: no time yet";
    }
    const double tsc_per_second = static_cast<double>(__rdtsc() - g_tsc_begin) / elapsed;
    const std::uint64_t shown_qpc = g_shown_qpc + (g_shown_since != 0 ? now - g_shown_since : 0);
    const double shown = static_cast<double>(shown_qpc) / static_cast<double>(frequency.QuadPart);
    const double seconds[2] = {elapsed - shown, shown};
    static const char* const kNames[] = {"text lookup", "title scene update"};
    std::string out = "measure (upper bounds; TSC " + std::to_string(static_cast<long long>(tsc_per_second / 1e6)) +
                      " MHz):";
    for (int state = 0; state < 2; ++state) {
        char head[96];
        std::snprintf(head, sizeof head, " page %s %.0f s:", state == 0 ? "closed" : "shown", seconds[state]);
        out += head;
        for (int h = 0; h < static_cast<int>(Hook::Count); ++h) {
            const Counter& c = g_counters[h][state];
            const double calls = static_cast<double>(c.calls.load(std::memory_order_relaxed));
            const double busy = static_cast<double>(c.ticks.load(std::memory_order_relaxed)) / tsc_per_second;
            char part[192];
            std::snprintf(part, sizeof part, " %s %.0f calls (%.0f/s), %.0f ns each, %.1f us/s;", kNames[h], calls,
                          seconds[state] > 0 ? calls / seconds[state] : 0.0, calls > 0 ? busy * 1e9 / calls : 0.0,
                          seconds[state] > 0 ? busy * 1e6 / seconds[state] : 0.0);
            out += part;
        }
    }
    return out;
}

}  // namespace cmfm::native::measure

#else

namespace cmfm::native::measure {

void begin() {}
void page_shown(bool) {}
std::string report() { return {}; }

}  // namespace cmfm::native::measure

#endif
