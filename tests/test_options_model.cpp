// Unit tests for cmfm::native::OptionsModel (no game, no framework).
#include "native/options_model.hpp"

#include <cstdio>
#include <cstring>

using namespace cmfm::native;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

bool same(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

const char* const kOnOff[] = {"Off", "On"};
const char* const kAbc[] = {"Alpha", "Beta", "Gamma"};
const char* const kTabs[] = {"CMFM", "", "", "", ""};
const OptionSpec kSpecs[] = {
    {"Test toggle", "A test switch.", kOnOff, 2, 1, 0},
    {"Test choice", "A test choice.", kAbc, 3, 0, 0},
    {"Other tab", "On tab 2.", kOnOff, 2, 0, 2},
};
const char* const kDash[] = {"-"};
const OptionSpec kFree = {"Free tab", "Nothing here.", kDash, 1, 0, 0};

void builds_rows_in_game_format() {
    static OptionsModel m;
    const char* why = nullptr;
    check(m.build("Mods", kTabs, kSpecs, 3, kFree, why), "build succeeds");
    const GameOptionRow* t0 = m.table(0);
    check(t0[0].kind == kCmfmRowKind && t0[0].setting_id == 0 && t0[0].value_count == 2, "row 0");
    check(t0[0].label_id == 9100 && t0[0].flag == 1 && t0[0].help_cat == 18 && t0[0].help_id == 9002,
          "row 0 text ids");
    check(t0[1].label_id == 9103 && t0[1].help_id == 9005 && t0[1].setting_id == 1, "row 1 follows row 0's values");
    check(t0[2].kind == kRowTableEnd, "tab 0 ends after 2 rows");
    check(m.table(2)[0].setting_id == 2 && m.table(2)[1].kind == kRowTableEnd, "tab 2 has its row");
    // No tab may be empty: the game reads the row under the cursor unchecked.
    for (std::size_t t = 0; t < kTabCount; ++t) {
        check(m.table(t)[0].kind == kCmfmRowKind, "every tab has a first row");
    }
    check(m.table(1)[0].value_count == 1 && m.table(1)[1].kind == kRowTableEnd, "tab 1 gets the placeholder");
    check(m.row_count() == 6, "3 rows + 3 placeholders (tabs 1, 3, 4)");
}

void resolves_texts() {
    static OptionsModel m;
    const char* why = nullptr;
    m.build("Mods", kTabs, kSpecs, 3, kFree, why);
    check(same(m.label_text(9100), "Test toggle"), "label");
    check(same(m.label_text(9101), "Off") && same(m.label_text(9102), "On"), "values follow the label");
    check(same(m.label_text(9103), "Test choice") && same(m.label_text(9106), "Gamma"), "second row");
    check(same(m.label_text(9107), "Other tab") && same(m.label_text(9109), "On"), "third row on another tab");
    check(same(m.label_text(9110), "Free tab") && same(m.label_text(9111), "-"), "placeholder texts");
    check(m.label_text(9099) == nullptr && m.label_text(9116) == nullptr, "outside our ids");
    check(same(m.help_text(9002), "A test switch.") && same(m.help_text(9004), "A test switch."), "help and value help");
    check(m.help_text(9001) == nullptr, "9001 is not the model's");
    check(same(m.header(), "Mods") && same(m.tab_name(0), "CMFM") && same(m.tab_name(1), ""), "header and tabs");
}

void gets_and_sets() {
    static OptionsModel m;
    const char* why = nullptr;
    m.build("Mods", kTabs, kSpecs, 3, kFree, why);
    std::uint32_t v = 99;
    check(m.get(0, v) && v == 1, "initial value");
    check(m.set(1, 2) && m.get(1, v) && v == 2, "set within range");
    check(!m.set(1, 3) && m.get(1, v) && v == 2, "out of range refused, value kept");
    check(!m.set(3, 1) && m.set(3, 0), "placeholder accepts only its one value");
    check(!m.set(7, 0) && !m.get(7, v), "unknown row refused");
    m.set(0, 0);
    m.reset();
    check(m.get(0, v) && v == 1 && m.get(1, v) && v == 0, "reset restores the initial values");
}

void rejects_bad_specs() {
    static OptionsModel m;
    const char* why = nullptr;
    const OptionSpec bad_tab[] = {{"x", "", kOnOff, 2, 0, 5}};
    check(!m.build("Mods", kTabs, bad_tab, 1, kFree, why), "tab out of range");
    const OptionSpec bad_initial[] = {{"x", "", kOnOff, 2, 2, 0}};
    check(!m.build("Mods", kTabs, bad_initial, 1, kFree, why), "initial out of range");
    const OptionSpec no_values[] = {{"x", "", kOnOff, 0, 0, 0}};
    check(!m.build("Mods", kTabs, no_values, 1, kFree, why), "no values");
    const OptionSpec bad_free = {"x", "", kOnOff, 0, 0, 0};
    check(!m.build("Mods", kTabs, kSpecs, 3, bad_free, why), "invalid placeholder");
    check(m.build("Mods", kTabs, nullptr, 0, kFree, why) && m.row_count() == 5, "no rows: 5 placeholders");
}

}  // namespace

int main() {
    builds_rows_in_game_format();
    resolves_texts();
    gets_and_sets();
    rejects_bad_specs();
    if (g_failures == 0) {
        std::printf("test_options_model: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
