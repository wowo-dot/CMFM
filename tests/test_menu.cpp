// Unit tests for cmfm::menu (the cmfm.menu core) with a fake plugin.
#include "menu/menu.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

using namespace cmfm;
using namespace cmfm::menu;
using native::OptionsModel;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

bool same(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }

// ---- A fake plugin: values in memory, every call counted. ----
struct Fake {
    cmfm_value_t values[8] = {};
    int sets = 0;
    int opened = 0;
    int closed = 0;
    int texts = 0;
    int presses = 0;
    bool fail_get = false;
};

enum : std::uint32_t { kToggle = 10, kChoice = 11, kPercent = 12, kScale = 13, kStatus = 14, kRestart = 15 };

std::uint32_t slot(std::uint32_t id) { return id - kToggle; }

cmfm_result_t CMFM_CALL fake_get(void* user, std::uint32_t id, cmfm_value_t* v) {
    auto* f = static_cast<Fake*>(user);
    if (f->fail_get) {
        return CMFM_ERROR;
    }
    *v = f->values[slot(id)];
    return CMFM_OK;
}

cmfm_result_t CMFM_CALL fake_set(void* user, std::uint32_t id, const cmfm_value_t* v) {
    auto* f = static_cast<Fake*>(user);
    f->values[slot(id)] = *v;
    ++f->sets;
    return CMFM_OK;
}

// "Gamma" (index 2) only while the toggle is on.
cmfm_result_t CMFM_CALL fake_validate(void* user, std::uint32_t id, const cmfm_value_t* v, char* why,
                                      std::uint32_t why_size) {
    auto* f = static_cast<Fake*>(user);
    if (id == kChoice && v->i == 2 && f->values[slot(kToggle)].i == 0) {
        std::snprintf(why, why_size, "Gamma needs the toggle on.");
        return CMFM_ERROR;
    }
    return CMFM_OK;
}

void CMFM_CALL fake_text(void* user, std::uint32_t, char* text, std::uint32_t size) {
    auto* f = static_cast<Fake*>(user);
    ++f->texts;
    std::snprintf(text, size, "Opened %d times", f->opened);
}

void CMFM_CALL fake_event(void* user, cmfm_event_t event) {
    auto* f = static_cast<Fake*>(user);
    (event == CMFM_EVENT_OPENED ? f->opened : f->closed)++;
}

const char* const kAbc[] = {"Alpha", "Beta", "Gamma"};

cmfm_setting_t setting(std::uint32_t id, cmfm_type_t type, const char* label) {
    cmfm_setting_t s = {};
    s.size = sizeof s;
    s.id = id;
    s.type = type;
    s.label = label;
    s.help = "Help.";
    return s;
}

struct Demo {
    cmfm_setting_t settings[6];
    cmfm_page_t page;
};

Demo demo_page(Fake* f, const char* title = "Demo") {
    Demo d = {};
    d.settings[0] = setting(kToggle, CMFM_TYPE_BOOL, "Toggle");
    d.settings[1] = setting(kChoice, CMFM_TYPE_CHOICE, "Choice");
    d.settings[1].choices = kAbc;
    d.settings[1].choice_count = 3;
    d.settings[2] = setting(kPercent, CMFM_TYPE_INT, "Percent");
    d.settings[2].min = 0;
    d.settings[2].max = 100;
    d.settings[2].step = 10;
    d.settings[2].unit = "%";
    d.settings[2].default_value.i = 50;
    d.settings[3] = setting(kScale, CMFM_TYPE_FLOAT, "Scale");
    d.settings[3].min = 0.5;
    d.settings[3].max = 2.0;
    d.settings[3].step = 0.25;
    d.settings[3].decimals = 2;
    d.settings[3].unit = "x";
    d.settings[3].default_value.f = 1.0;
    d.settings[4] = setting(kStatus, CMFM_TYPE_TEXT, "Status");
    d.settings[5] = setting(kRestart, CMFM_TYPE_BOOL, "Restart thing");
    d.settings[5].flags = CMFM_FLAG_RESTART;
    d.settings[5].default_value.i = 1;
    d.page.size = sizeof d.page;
    d.page.plugin = "fake";
    d.page.title = title;
    d.page.help = "A fake page.";
    d.page.settings = d.settings;
    d.page.setting_count = 6;
    d.page.user = f;
    d.page.get = fake_get;
    d.page.set = fake_set;
    d.page.validate = fake_validate;
    d.page.get_text = fake_text;
    d.page.on_event = fake_event;
    return d;
}

// Adds a page as the service does: copy or refusal.
bool add(Menu& m, const cmfm_page_t& page) {
    Page copy;
    std::string why;
    if (!copy_page(&page, copy, why)) {
        m.add_refused(page.plugin ? page.plugin : "", why);
        return false;
    }
    m.add(std::move(copy));
    return true;
}

bool refused(const cmfm_page_t& page, const char* reason_part) {
    Page copy;
    std::string why;
    return !copy_page(&page, copy, why) && why.find(reason_part) != std::string::npos;
}

// A Demo is copied around: point its page at its own settings first.
cmfm_page_t& page_of(Demo& d) {
    d.page.settings = d.settings;
    return d.page;
}

// The model row with this label on a tab, or nullptr.
const native::GameOptionRow* find(const OptionsModel& m, std::size_t tab, const char* label) {
    for (const native::GameOptionRow* r = m.table(tab); r->kind != native::kRowTableEnd; ++r) {
        if (same(m.label_text(static_cast<std::uint32_t>(r->label_id)), label)) {
            return r;
        }
    }
    return nullptr;
}

const char* value_text(const OptionsModel& m, const native::GameOptionRow* r, std::uint32_t v) {
    return m.label_text(static_cast<std::uint32_t>(r->label_id) + 1 + v);
}

const char* help_text(const OptionsModel& m, const native::GameOptionRow* r) {
    return m.help_text(static_cast<std::uint32_t>(r->help_id));
}

std::uint32_t id(const native::GameOptionRow* r) { return static_cast<std::uint32_t>(r->setting_id); }

// ---- Tests ----

void validates_pages() {
    Fake f;
    Demo d = demo_page(&f);
    Page copy;
    std::string why;
    check(copy_page(&page_of(d), copy, why) && copy.settings.size() == 6, "the demo page is valid");
    check(copy.settings[2].value_count == 11 && copy.settings[3].value_count == 7, "range value counts");

    Demo bad = demo_page(&f);
    bad.page.title = "A title much longer than the tab has room for";
    check(refused(page_of(bad), "title"), "long title");
    bad = demo_page(&f);
    bad.page.setting_count = 0;
    check(refused(page_of(bad), "settings"), "no settings");
    bad = demo_page(&f);
    bad.settings[1].id = kToggle;
    check(refused(page_of(bad), "id used twice"), "duplicate id");
    bad = demo_page(&f);
    bad.settings[1].default_value.i = 3;
    check(refused(page_of(bad), "default"), "choice default out of range");
    bad = demo_page(&f);
    bad.settings[2].max = 1010;
    check(refused(page_of(bad), "101 values"), "102 values refused");
    bad = demo_page(&f);
    bad.settings[2].max = 1000;
    check(!refused(page_of(bad), ""), "101 values accepted");
    bad = demo_page(&f);
    bad.settings[2].max = 100;
    bad.settings[2].step = 1;
    check(!refused(page_of(bad), ""), "0 to 100 in steps of 1 accepted");
    bad = demo_page(&f);
    bad.settings[2].step = 2.5;
    check(refused(page_of(bad), "whole"), "INT step must be whole");
    bad = demo_page(&f);
    bad.settings[3].decimals = 4;
    check(refused(page_of(bad), "decimals"), "decimals above 3");
    bad = demo_page(&f);
    bad.settings[3].default_value.f = 3.0;
    check(refused(page_of(bad), "default"), "float default out of range");
    bad = demo_page(&f);
    bad.settings[0].flags = 0x8;
    check(refused(page_of(bad), "flags"), "unknown flag");
    bad = demo_page(&f);
    bad.settings[0].type = 9;
    check(refused(page_of(bad), "type"), "unknown type");
    bad = demo_page(&f);
    bad.page.set = nullptr;
    check(refused(page_of(bad), "get and set"), "set callback needed");
    bad = demo_page(&f);
    bad.page.get_text = nullptr;
    check(refused(page_of(bad), "get_text"), "get_text needed for TEXT");
    bad = demo_page(&f);
    bad.page.size = 8;
    check(refused(page_of(bad), "smaller"), "page smaller than 1.0");
    Page none;
    check(!copy_page(nullptr, none, why), "no page");
}

void reads_later_setting_sizes() {
    // A plugin built for a later minor version passes bigger settings.
    struct Later {
        cmfm_setting_t s;
        std::uint64_t added_in_1_1;
    };
    Fake f;
    Later later[2] = {};
    later[0].s = setting(1, CMFM_TYPE_BOOL, "One");
    later[1].s = setting(2, CMFM_TYPE_BOOL, "Two");
    later[0].s.size = later[1].s.size = sizeof(Later);
    cmfm_page_t page = {};
    page.size = sizeof page;
    page.plugin = "later";
    page.title = "Later";
    page.settings = &later[0].s;
    page.setting_count = 2;
    page.user = &f;
    page.get = fake_get;
    page.set = fake_set;
    Page copy;
    std::string why;
    check(copy_page(&page, copy, why) && copy.settings.size() == 2 && copy.settings[1].label == "Two",
          "steps through the array by settings[0].size");
}

void lays_out_the_status_page() {
    static OptionsModel model;
    Menu m("0.1.0-dev");
    std::string why;
    check(m.build(model, why) && m.used_tabs() == 1, "no pages: only the status tab");
    check(same(model.tab_name(0), "Overview") && same(model.tab_name(1), "-"), "tab names");
    const native::GameOptionRow* r = find(model, 0, "CMFM");
    check(r != nullptr && same(value_text(model, r, 0), "0.1.0-dev"), "CMFM's version row");
    check(find(model, 0, "No mod pages yet") != nullptr, "says there are no pages");
    check(find(model, 1, "Empty") != nullptr, "empty tabs get a row");
    check(m.get(id(r)) == 0 && !m.set(id(r), 0), "status rows have no value");
}

void shows_a_page() {
    static OptionsModel model;
    Fake f;
    Menu m("0.1.0-dev");
    Demo d = demo_page(&f);
    check(add(m, page_of(d)) && m.dirty(), "page added");
    std::string why;
    check(m.build(model, why) && !m.dirty() && m.used_tabs() == 2, "built with one page");
    check(same(model.tab_name(1), "Demo") && same(model.tab_help(1), "A fake page."), "the page's tab");
    const native::GameOptionRow* status = find(model, 0, "Demo");
    check(status && same(value_text(model, status, 0), "Active") && std::strstr(help_text(model, status), "fake") &&
              std::strstr(help_text(model, status), "6 setting"),
          "Overview row for the page");

    const native::GameOptionRow* toggle = find(model, 1, "Toggle");
    const native::GameOptionRow* percent = find(model, 1, "Percent");
    const native::GameOptionRow* scale = find(model, 1, "Scale");
    const native::GameOptionRow* restart = find(model, 1, "Restart thing");
    check(toggle && toggle->value_count == 2 && same(value_text(model, toggle, 1), "On"), "bool values");
    check(percent && percent->value_count == 11 && same(m.value_text(id(percent), 0), "0%") &&
              same(m.value_text(id(percent), 10), "100%"),
          "int values with unit");
    check(scale && scale->value_count == 7 && same(m.value_text(id(scale), 0), "0.50x") &&
              same(m.value_text(id(scale), 6), "2.00x"),
          "float values with decimals");
    check(m.value_text(id(toggle), 0) == nullptr, "no number text for other rows");
    // A number row's values share one text: the current value.
    const std::uint32_t now = m.get(id(percent));
    const std::string now_text = m.value_text(id(percent), now);
    check(percent->values != nullptr && same(value_text(model, percent, 0), now_text.c_str()),
          "a number row's one text is its current value");
    check(m.set(id(percent), 3) && same(value_text(model, percent, 0), "30%"), "and follows a change");
    check(percent->values[0].text == percent->label_id + 1 && percent->values[100].text == percent->label_id + 1 &&
              percent->values[50].help_id == percent->help_id && toggle->values == nullptr,
          "every slider position points at that text and the row's help");
    check(restart && std::strstr(help_text(model, restart), "restarting the game"), "restart note in the help");
}

void reads_and_writes_through_callbacks() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo d = demo_page(&f);
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* toggle = find(model, 1, "Toggle");
    const native::GameOptionRow* choice = find(model, 1, "Choice");
    const native::GameOptionRow* percent = find(model, 1, "Percent");
    const native::GameOptionRow* scale = find(model, 1, "Scale");

    f.values[slot(kPercent)].i = 37;  // between steps: shown as the nearest
    f.values[slot(kScale)].f = 1.3;
    check(m.get(id(percent)) == 4 && m.get(id(scale)) == 3, "values shown as the nearest step");
    check(m.set(id(percent), 3) && f.values[slot(kPercent)].i == 30, "index 3 -> 30");
    check(m.set(id(scale), 6) && f.values[slot(kScale)].f == 2.0, "index 6 -> 2.0");
    check(!m.set(id(percent), 11) && f.values[slot(kPercent)].i == 30, "out-of-range index ignored");

    // validate refuses Gamma while the toggle is off: value kept, reason shown.
    const int sets = f.sets;
    check(!m.set(id(choice), 2) && f.values[slot(kChoice)].i == 0 && f.sets == sets, "refused value not set");
    check(same(help_text(model, choice), "Not changed: Gamma needs the toggle on."), "reason in the help line");
    check(m.set(id(toggle), 1) && m.set(id(choice), 2) && f.values[slot(kChoice)].i == 2, "accepted once toggled");
    check(same(help_text(model, choice), "Help."), "help line back after a good value");

    f.fail_get = true;
    check(m.get(id(choice)) == 0, "a failed read shows the first value");
    f.fail_get = false;

    m.reset_defaults();
    check(f.values[slot(kToggle)].i == 0 && f.values[slot(kPercent)].i == 50 && f.values[slot(kScale)].f == 1.0 &&
              f.values[slot(kRestart)].i == 1,
          "Set To Default writes every default");
}

void refreshes_texts_and_sends_events() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo d = demo_page(&f);
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* status = find(model, 1, "Status");
    m.send(CMFM_EVENT_OPENED);
    m.refresh();
    check(f.opened == 1 && f.texts == 1 && same(value_text(model, status, 0), "Opened 1 times"), "text at opening");
    check(m.get(id(status)) == 0 && !m.set(id(status), 0), "TEXT rows have no value to set");
    m.send(CMFM_EVENT_CLOSED);
    check(f.closed == 1, "closed event");
}

void lists_every_page_on_the_status_tab() {
    static OptionsModel model;
    Fake f[5];
    Menu m;
    Demo d[5] = {demo_page(&f[0], "One"), demo_page(&f[1], "Two"), demo_page(&f[2], "Three"),
                 demo_page(&f[3], "Four"), demo_page(&f[4], "Five")};
    for (Demo& x : d) {
        add(m, page_of(x));
    }
    Demo bad = demo_page(&f[0]);
    bad.page.plugin = "broken";
    bad.page.set = nullptr;
    add(m, page_of(bad));
    std::string why;
    check(m.build(model, why) && m.used_tabs() == 6 && model.tab_count() == 6, "five pages: six tabs");
    check(same(model.tab_name(4), "Four") && same(model.tab_name(5), "Five"), "in the order added");
    std::size_t shown = 0;
    std::size_t refusals = 0;
    for (const native::GameOptionRow* r = model.table(0); r->kind != native::kRowTableEnd; ++r) {
        const char* v = value_text(model, r, 0);
        shown += same(v, "Active");
        refusals += same(v, "Refused");
    }
    check(shown == 5 && refusals == 1, "status: 5 shown, 1 refused");
    const native::GameOptionRow* broken = find(model, 0, "broken");
    check(broken && std::strstr(help_text(model, broken), "get and set"), "the refusal's reason");
    m.send(CMFM_EVENT_OPENED);
    check(f[3].opened == 1 && f[4].opened == 1, "events go to every shown page");
}

void keeps_five_tabs_for_the_in_game_page() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo d = demo_page(&f);
    add(m, page_of(d));
    std::string why;
    check(m.build(model, why) && model.tab_count() == 5 && m.used_tabs() == 2, "one page: still five tabs");
    check(same(model.tab_name(2), "-") && find(model, 4, "Empty") != nullptr, "unused tabs");
}

void stops_at_24_pages() {
    static OptionsModel model;
    static Fake f;
    static cmfm_setting_t one[1];
    one[0] = setting(kToggle, CMFM_TYPE_BOOL, "One");
    Menu m;
    for (int p = 0; p < 26; ++p) {
        cmfm_page_t page = {};
        page.size = sizeof page;
        page.plugin = "many";
        page.title = "Many";
        page.settings = one;
        page.setting_count = 1;
        page.user = &f;
        page.get = fake_get;
        page.set = fake_set;
        add(m, page);
    }
    std::string why;
    check(m.build(model, why) && m.used_tabs() == 25 && model.tab_count() == 25, "24 pages shown");
    // 26 entries: the status list holds 22 and a "More" row for the other 4.
    const native::GameOptionRow* more = find(model, 0, "More");
    check(more != nullptr && same(value_text(model, more, 0), "4"), "the status list says 4 more");
}

void drops_pages_that_do_not_fit() {
    // Four pages of 24 choices of 32 names each overflow the text ids
    // (number rows no longer would: their values share one text).
    static OptionsModel model;
    static Fake f;
    static const char* const names[CMFM_MAX_CHOICES] = {
        "c0",  "c1",  "c2",  "c3",  "c4",  "c5",  "c6",  "c7",  "c8",  "c9",  "c10", "c11", "c12", "c13", "c14", "c15",
        "c16", "c17", "c18", "c19", "c20", "c21", "c22", "c23", "c24", "c25", "c26", "c27", "c28", "c29", "c30", "c31"};
    static cmfm_setting_t big[CMFM_MAX_SETTINGS];
    for (std::uint32_t i = 0; i < CMFM_MAX_SETTINGS; ++i) {
        big[i] = setting(i, CMFM_TYPE_CHOICE, "Big");
        big[i].choices = names;
        big[i].choice_count = CMFM_MAX_CHOICES;
    }
    Menu m;
    for (int p = 0; p < 4; ++p) {
        cmfm_page_t page = {};
        page.size = sizeof page;
        page.plugin = "big";
        page.title = "Big";
        page.settings = big;
        page.setting_count = CMFM_MAX_SETTINGS;
        page.user = &f;
        page.get = fake_get;
        page.set = fake_set;
        add(m, page);
    }
    std::string why;
    check(m.build(model, why) && m.used_tabs() == 2, "only the first big page fits");
    check(model.tab_count() == 5, "five tabs for the in-game page");
    std::size_t not_shown = 0;
    for (const native::GameOptionRow* r = model.table(0); r->kind != native::kRowTableEnd; ++r) {
        not_shown += same(value_text(model, r, 0), "Not shown");
    }
    check(not_shown == 3, "the others are listed as not shown");
}

void shows_sections() {
    static OptionsModel model;
    Fake f;
    cmfm_setting_t s[4] = {setting(1, CMFM_TYPE_SECTION, "Upscaling"), setting(kToggle, CMFM_TYPE_BOOL, "Toggle"),
                           setting(2, CMFM_TYPE_SECTION, "Frame generation"),
                           setting(kRestart, CMFM_TYPE_BOOL, "Restart thing")};
    cmfm_page_t page = {};
    page.size = sizeof page;
    page.plugin = "sections";
    page.title = "Sections";
    page.settings = s;
    page.setting_count = 4;
    page.user = &f;
    page.get = fake_get;
    page.set = fake_set;
    check(!refused(page, ""), "a page with sections is valid");
    s[0].flags = CMFM_FLAG_RESTART;
    check(refused(page, "no flags"), "a section takes no flags");
    s[0].flags = 0;
    cmfm_setting_t only[1] = {setting(1, CMFM_TYPE_SECTION, "Alone")};
    cmfm_page_t empty = page;
    empty.settings = only;
    empty.setting_count = 1;
    check(refused(empty, "besides its sections"), "a page of sections only is refused");

    Menu m;
    add(m, page);
    std::string why;
    check(m.build(model, why), "built with sections");
    const native::GameOptionRow* heading = find(model, 1, "<STYL MOJI_YELLOW_DEFAULT>Upscaling</STYL>");
    const native::GameOptionRow* toggle = find(model, 1, "Toggle");
    check(heading && heading->value_count == 1 && same(value_text(model, heading, 0), ""), "a heading row, no value");
    check(heading && m.is_section(id(heading)) && toggle && !m.is_section(id(toggle)), "sections are told apart");
    check(heading && m.get(id(heading)) == 0 && !m.set(id(heading), 0), "a heading has no value");
    const native::GameOptionRow* status = find(model, 0, "Sections");
    check(status && std::strstr(help_text(model, status), "2 setting"), "the Overview counts settings, not sections");
    const int sets = f.sets;
    m.reset_defaults();
    check(f.sets == sets + 2, "Set To Default skips the headings");
}

void tracks_restart_changes() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo d = demo_page(&f);
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* restart = find(model, 1, "Restart thing");
    const native::GameOptionRow* toggle = find(model, 1, "Toggle");
    const native::GameOptionRow* status = find(model, 0, "Demo");
    const std::uint32_t r0 = m.restart_revision();
    check(m.restart_count() == 0 && !m.tab_restart_pending(1), "nothing pending at first");

    check(m.get(id(restart)) == 0, "the game started with Off");
    check(m.set(id(toggle), 1) && m.restart_count() == 0 && m.restart_revision() == r0, "a live setting never waits");
    check(m.set(id(restart), 1) && m.restart_pending(id(restart)) && m.tab_restart_pending(1) &&
              !m.tab_restart_pending(0) && m.restart_count() == 1 && m.restart_revision() != r0,
          "a changed restart setting waits");
    check(same(value_text(model, status, 0), "Restart needed"), "the Overview says so");
    const std::uint32_t r1 = m.restart_revision();
    check(m.set(id(restart), 1) && m.restart_revision() == r1, "the same value again changes nothing");
    check(m.set(id(restart), 0) && !m.restart_pending(id(restart)) && m.restart_count() == 0 &&
              m.restart_revision() != r1 && same(value_text(model, status, 0), "Active"),
          "back to the start value: nothing waits");

    m.reset_defaults();  // the restart setting's default is On
    check(m.restart_pending(id(restart)) && m.restart_count() == 1, "Set To Default can make one wait");
    m.build(model, why);  // a rebuild keeps the session's state
    check(m.restart_count() == 1 && same(value_text(model, find(model, 0, "Demo"), 0), "Restart needed"),
          "kept across a rebuild");
}

void picks_slider_rows() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo d = demo_page(&f);
    d.settings[3].max = 1.0;  // Scale 0.5..1.0 in 0.25 steps: 3 values
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    check(m.slider_steps(id(find(model, 1, "Percent"))) == 10, "percent: 11 values, a slider of 10 steps");
    check(m.slider_steps(id(find(model, 1, "Scale"))) == 2, "3 values: still a slider");
    check(m.slider_steps(id(find(model, 1, "Toggle"))) == 0 && m.slider_steps(id(find(model, 1, "Choice"))) == 0 &&
              m.slider_steps(id(find(model, 1, "Status"))) == 0,
          "toggles, choices and text lines are no sliders");
    const std::uint32_t v0 = m.value_revision();
    check(m.set(id(find(model, 1, "Percent")), 3) && m.value_revision() != v0, "a set changes the value revision");
}

void picks_list_rows() {
    static OptionsModel model;
    static const char* const kSix[] = {"A", "B", "C", "D", "E", "F"};
    static const char* const kMany[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};
    Fake f;
    cmfm_setting_t s[5] = {setting(kToggle, CMFM_TYPE_CHOICE, "Three"), setting(kChoice, CMFM_TYPE_CHOICE, "Six"),
                           setting(kPercent, CMFM_TYPE_CHOICE, "Forced list"),
                           setting(kScale, CMFM_TYPE_CHOICE, "Forced arrows"),
                           setting(kStatus, CMFM_TYPE_CHOICE, "Fifteen")};
    s[0].choices = kAbc;
    s[0].choice_count = 3;
    s[1].choices = kSix;
    s[1].choice_count = 6;
    s[2].choices = kAbc;
    s[2].choice_count = 3;
    s[2].flags = CMFM_FLAG_LIST;
    s[3].choices = kSix;
    s[3].choice_count = 6;
    s[3].flags = CMFM_FLAG_ARROWS;
    s[4].choices = kMany;
    s[4].choice_count = 15;
    s[4].flags = CMFM_FLAG_LIST;
    cmfm_page_t page = {};
    page.size = sizeof page;
    page.plugin = "lists";
    page.title = "Lists";
    page.settings = s;
    page.setting_count = 5;
    page.user = &f;
    page.get = fake_get;
    page.set = fake_set;
    Menu m;
    check(add(m, page), "a page with list flags is valid");
    std::string why;
    m.build(model, why);
    check(m.list_choices(id(find(model, 1, "Three"))) == 0, "3 choices: arrows");
    check(m.list_choices(id(find(model, 1, "Six"))) == 6, "6 choices: the pick list");
    check(m.list_choices(id(find(model, 1, "Forced list"))) == 3, "LIST forces the pick list");
    check(m.list_choices(id(find(model, 1, "Forced arrows"))) == 0, "ARROWS forces the arrows");
    check(m.list_choices(id(find(model, 1, "Fifteen"))) == 0, "more than 14: arrows even with LIST");

    cmfm_setting_t bad = setting(1, CMFM_TYPE_BOOL, "Bool");
    bad.flags = CMFM_FLAG_LIST;
    cmfm_page_t one = page;
    one.settings = &bad;
    one.setting_count = 1;
    check(refused(one, "for CHOICE"), "LIST on a BOOL is refused");
    bad = s[1];
    bad.flags = CMFM_FLAG_LIST | CMFM_FLAG_ARROWS;
    check(refused(one, "together"), "LIST and ARROWS together are refused");
}

cmfm_result_t CMFM_CALL fake_press(void* user, std::uint32_t) {
    auto* f = static_cast<Fake*>(user);
    ++f->presses;
    f->values[slot(kPercent)].i = 50;  // the button resets Percent
    return CMFM_OK;
}

// Scale is locked while the toggle is off.
std::uint32_t CMFM_CALL fake_locked(void* user, std::uint32_t id, char* why, std::uint32_t why_size) {
    auto* f = static_cast<Fake*>(user);
    if (id == kScale && f->values[slot(kToggle)].i == 0) {
        std::snprintf(why, why_size, "needs the toggle on.");
        return 1;
    }
    return 0;
}

void presses_buttons() {
    static OptionsModel model;
    Fake f;
    Demo d = demo_page(&f);
    d.settings[4] = setting(kStatus, CMFM_TYPE_BUTTON, "Reset");  // replaces the TEXT row
    d.settings[4].button = "Reset";
    d.settings[4].confirm = "Reset the fake page?";
    d.page.press = fake_press;
    Menu m;
    check(add(m, page_of(d)), "a page with a button is valid");
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* button = find(model, 1, "Reset");
    check(button && button->value_count == 1 && same(value_text(model, button, 0), "Reset"), "the button's text");
    check(button && m.is_button(id(button)) && same(m.button_question(id(button)), "Reset the fake page?"),
          "a button with its question");
    check(button && m.get(id(button)) == 0 && !m.set(id(button), 0), "a button has no value");
    f.values[slot(kPercent)].i = 90;
    const std::uint32_t v0 = m.value_revision();
    check(button && m.press(id(button)) && f.presses == 1 && f.values[slot(kPercent)].i == 50 &&
              m.value_revision() != v0,
          "press runs the callback; values may change");
    check(!m.press(id(find(model, 1, "Toggle"))), "a toggle cannot be pressed");

    Demo plain = demo_page(&f);
    plain.settings[4] = setting(kStatus, CMFM_TYPE_BUTTON, "Run it");
    plain.page.press = fake_press;
    Menu m2;
    add(m2, page_of(plain));
    m2.build(model, why);
    const native::GameOptionRow* run = find(model, 1, "Run it");
    check(run && same(value_text(model, run, 0), "Run") && m2.button_question(id(run)) == nullptr,
          "default text, no question");

    Demo bad = demo_page(&f);
    bad.settings[4] = setting(kStatus, CMFM_TYPE_BUTTON, "Reset");
    check(refused(page_of(bad), "press"), "a button needs press");
    bad.page.press = fake_press;
    bad.page.size = offsetof(cmfm_page_t, press);  // a 1.0 page cannot have one
    check(refused(page_of(bad), "press"), "a 1.0-sized page cannot carry press");
}

void locks_rows() {
    static OptionsModel model;
    Fake f;
    Demo d = demo_page(&f);
    d.page.locked = fake_locked;
    Menu m;
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* scale = find(model, 1, "Scale");
    const native::GameOptionRow* toggle = find(model, 1, "Toggle");
    const std::uint32_t l0 = m.lock_revision();
    m.refresh();  // as when a page opens
    check(m.is_locked(id(scale)) && !m.is_locked(id(toggle)) && m.lock_revision() != l0, "locked at opening");
    check(same(help_text(model, scale), "Locked: needs the toggle on."), "the reason in the help line");
    check(!m.set(id(scale), 6) && f.values[slot(kScale)].f == 0.0, "a locked row refuses a change");
    const std::uint32_t l1 = m.lock_revision();
    check(m.slider_steps(id(scale)) == 0, "a locked number row is no slider");
    check(m.set(id(toggle), 1) && !m.is_locked(id(scale)) && m.lock_revision() != l1, "unlocked by another change");
    check(m.slider_steps(id(scale)) == 6, "a slider again once unlocked");
    check(same(help_text(model, scale), "Help."), "the help line back");
    check(m.set(id(scale), 6) && f.values[slot(kScale)].f == 2.0, "changes again");
}

void accepts_1_0_sizes() {
    Fake f;
    Demo d = demo_page(&f);
    d.page.size = offsetof(cmfm_page_t, press);
    for (cmfm_setting_t& s : d.settings) {
        s.size = offsetof(cmfm_setting_t, button);
    }
    // Settings smaller than the 1.1 struct: step through them by that size.
    struct Small {
        unsigned char bytes[offsetof(cmfm_setting_t, button)];
    };
    Small small[6];
    for (int i = 0; i < 6; ++i) {
        std::memcpy(small[i].bytes, &d.settings[i], sizeof small[i].bytes);
    }
    cmfm_page_t page = d.page;
    page.settings = reinterpret_cast<const cmfm_setting_t*>(small);
    Page copy;
    std::string why;
    check(copy_page(&page, copy, why) && copy.settings.size() == 6 && copy.settings[5].label == "Restart thing" &&
              copy.press == nullptr && copy.locked == nullptr,
          "pages and settings of cmfm.menu 1.0's size are read");
}

void reads_the_start_value_before_a_first_change() {
    static OptionsModel model;
    Fake f;
    f.values[slot(kRestart)].i = 1;
    Menu m;
    Demo d = demo_page(&f);
    add(m, page_of(d));
    std::string why;
    m.build(model, why);
    const native::GameOptionRow* restart = find(model, 1, "Restart thing");
    check(m.set(id(restart), 1) && !m.restart_pending(id(restart)), "set to the value it had: not pending");
    check(m.set(id(restart), 0) && m.restart_pending(id(restart)), "a real change waits");
}

void shows_page_headers() {
    static OptionsModel model;
    Fake f;
    Menu m;
    Demo a = demo_page(&f, "DLSS");
    a.page.version = "0.15.0";
    Demo b = demo_page(&f, "Other");
    b.page.version = "beta 2";
    Demo c = demo_page(&f, "Plain");
    add(m, page_of(a));
    add(m, page_of(b));
    add(m, page_of(c));
    std::string why;
    m.build(model, why);
    check(same(m.tab_header(0), "Overview"), "the Overview's header");
    check(same(m.tab_header(1), "DLSS \xC2\xB7 v0.15.0"), "title and version (v before a digit)");
    check(same(m.tab_header(2), "Other \xC2\xB7 beta 2"), "a version without a leading digit as written");
    check(same(m.tab_header(3), "Plain"), "no version: the title only");
    check(same(m.tab_header(4), ""), "an unused tab has none");
    const native::GameOptionRow* restart = find(model, 1, "Restart thing");
    m.get(id(restart));
    check(m.set(id(restart), 1) &&
              same(m.tab_header(1), "DLSS \xC2\xB7 v0.15.0 \xC2\xB7 <STYL MOJI_YELLOW_DEFAULT>Restart needed</STYL>"),
          "a waiting change shows in the header");
    check(same(m.tab_header(2), "Other \xC2\xB7 beta 2"), "only on its own page");

    Demo d = demo_page(&f);
    d.page.version = "0123456789012345678901234";  // 25 bytes
    check(refused(page_of(d), "version"), "a version longer than 24 bytes is refused");
    Demo e = demo_page(&f);
    e.page.version = "1.0";
    e.page.size = offsetof(cmfm_page_t, version);  // built before the field existed
    Page copy;
    check(copy_page(&page_of(e), copy, why) && copy.version.empty(), "a page without the field has no version");
}

}  // namespace

int main() {
    validates_pages();
    reads_later_setting_sizes();
    lays_out_the_status_page();
    shows_a_page();
    reads_and_writes_through_callbacks();
    refreshes_texts_and_sends_events();
    lists_every_page_on_the_status_tab();
    keeps_five_tabs_for_the_in_game_page();
    stops_at_24_pages();
    drops_pages_that_do_not_fit();
    shows_sections();
    tracks_restart_changes();
    reads_the_start_value_before_a_first_change();
    picks_slider_rows();
    picks_list_rows();
    presses_buttons();
    locks_rows();
    accepts_1_0_sizes();
    shows_page_headers();
    if (g_failures == 0) {
        std::printf("test_menu: all passed\n");
    }
    return g_failures == 0 ? 0 : 1;
}
