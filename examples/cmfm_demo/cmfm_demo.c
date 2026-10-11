/* cmfm_demo: an Astera Loader plugin that adds a page to CMFM's Mods menu.
 * The example for plugin authors (plain C), and CMFM's test client.
 *
 * It shows every setting type of cmfm.menu 1.0, keeps its values in its
 * own file (AsteraLoader\data\cmfm_demo\cmfm_demo.ini, the source of truth),
 * refuses one combination in `validate`, and saves when the Mods page closes.
 * With `extra_pages = N` in the ini it adds N small pages more (CMFM's tests
 * of a long list of mods).
 *
 * Install: <game>\AsteraLoader\plugins\cmfm_demo\cmfm_demo.dll, next to CMFM.
 */
#include "al/al_plugin.h"
#include "cmfm/cmfm.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

enum { ID_TOGGLE = 1, ID_CHOICE, ID_PERCENT, ID_SCALE, ID_RESTART, ID_STATUS };

static const al_api_t* g_api;
static al_plugin_t* g_self;
static wchar_t g_ini[1024];

/* The values: the plugin's own state, loaded from and saved to its ini. */
static struct {
    int toggle;
    int choice;
    int percent;
    double scale;
    int restart;
} g_values = {0, 0, 50, 1.0, 0};
static int g_dirty;
static int g_opened;
#define MAX_EXTRA 23
static int g_extra_pages; /* from the ini: test pages to add */
static int g_extra[MAX_EXTRA];

static void logline(al_log_level_t level, const char* line) {
    g_api->log(g_self, level, line);
}

/* ---- The ini file: key = value lines ---- */

static void load(void) {
    FILE* f = _wfopen(g_ini, L"r");
    char line[128];
    if (f == NULL) {
        logline(AL_LOG_INFO, "no cmfm_demo.ini yet; using defaults");
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        int i;
        double d;
        if (sscanf(line, " toggle = %d", &i) == 1) g_values.toggle = i != 0;
        else if (sscanf(line, " choice = %d", &i) == 1 && i >= 0 && i <= 2) g_values.choice = i;
        else if (sscanf(line, " percent = %d", &i) == 1 && i >= 0 && i <= 100) g_values.percent = i;
        else if (sscanf(line, " scale = %lf", &d) == 1 && d >= 0.5 && d <= 2.0) g_values.scale = d;
        else if (sscanf(line, " restart = %d", &i) == 1) g_values.restart = i != 0;
        else if (sscanf(line, " extra_pages = %d", &i) == 1 && i >= 0 && i <= MAX_EXTRA) g_extra_pages = i;
    }
    fclose(f);
    logline(AL_LOG_INFO, "cmfm_demo.ini loaded");
}

static void save(void) {
    FILE* f = g_ini[0] != 0 ? _wfopen(g_ini, L"w") : NULL;
    if (f == NULL) {
        logline(AL_LOG_WARNING, "could not write cmfm_demo.ini");
        return;
    }
    fprintf(f, "# cmfm_demo settings (written by the plugin; editable by hand)\n");
    fprintf(f, "toggle = %d\nchoice = %d\npercent = %d\nscale = %.2f\nrestart = %d\n", g_values.toggle,
            g_values.choice, g_values.percent, g_values.scale, g_values.restart);
    fprintf(f, "# test pages for a long list of mods (0 to %d)\nextra_pages = %d\n", MAX_EXTRA, g_extra_pages);
    fclose(f);
    g_dirty = 0;
    logline(AL_LOG_INFO, "cmfm_demo.ini saved");
}

/* ---- cmfm.menu callbacks (the game's menu thread) ---- */

static cmfm_result_t CMFM_CALL demo_get(void* user, uint32_t id, cmfm_value_t* value) {
    (void)user;
    switch (id) {
        case ID_TOGGLE: value->i = g_values.toggle; return CMFM_OK;
        case ID_CHOICE: value->i = g_values.choice; return CMFM_OK;
        case ID_PERCENT: value->i = g_values.percent; return CMFM_OK;
        case ID_SCALE: value->f = g_values.scale; return CMFM_OK;
        case ID_RESTART: value->i = g_values.restart; return CMFM_OK;
        default: return CMFM_ERROR;
    }
}

/* Gamma only while the toggle is on: CMFM shows the reason, the value stays. */
static cmfm_result_t CMFM_CALL demo_validate(void* user, uint32_t id, const cmfm_value_t* value, char* why,
                                             uint32_t why_size) {
    (void)user;
    if (id == ID_CHOICE && value->i == 2 && !g_values.toggle) {
        snprintf(why, why_size, "Gamma needs Demo toggle on.");
        return CMFM_ERROR;
    }
    return CMFM_OK;
}

static cmfm_result_t CMFM_CALL demo_set(void* user, uint32_t id, const cmfm_value_t* value) {
    char line[96];
    (void)user;
    switch (id) {
        case ID_TOGGLE: g_values.toggle = value->i; break;
        case ID_CHOICE: g_values.choice = value->i; break;
        case ID_PERCENT: g_values.percent = value->i; break;
        case ID_SCALE: g_values.scale = value->f; break;
        case ID_RESTART: g_values.restart = value->i; break;
        default: return CMFM_ERROR;
    }
    g_dirty = 1;
    snprintf(line, sizeof line, "set %u: i=%d f=%.2f", id, value->i, value->f);
    logline(AL_LOG_INFO, line);
    return CMFM_OK;
}

static void CMFM_CALL demo_text(void* user, uint32_t id, char* text, uint32_t text_size) {
    (void)user;
    if (id == ID_STATUS) {
        snprintf(text, text_size, "Opened %d time%s", g_opened, g_opened == 1 ? "" : "s");
    }
}

static void CMFM_CALL demo_event(void* user, cmfm_event_t event) {
    (void)user;
    if (event == CMFM_EVENT_OPENED) {
        ++g_opened;
    } else if (event == CMFM_EVENT_CLOSED && g_dirty) {
        save();
    }
}

/* ---- The extra test pages: one toggle each; `user` is the page number ---- */

static cmfm_result_t CMFM_CALL extra_get(void* user, uint32_t id, cmfm_value_t* value) {
    (void)id;
    value->i = g_extra[(intptr_t)user - 1];
    return CMFM_OK;
}

static cmfm_result_t CMFM_CALL extra_set(void* user, uint32_t id, const cmfm_value_t* value) {
    (void)id;
    g_extra[(intptr_t)user - 1] = value->i;
    return CMFM_OK;
}

static void add_extra_pages(const cmfm_menu_t* menu) {
    cmfm_setting_t s;
    cmfm_page_t page;
    char title[24];
    char why[200];
    for (int n = 1; n <= g_extra_pages; ++n) {
        memset(&s, 0, sizeof s);
        s.size = sizeof s;
        s.id = 1;
        s.type = CMFM_TYPE_BOOL;
        s.label = "Extra toggle";
        s.help = "A test page for CMFM's list of mods.";
        memset(&page, 0, sizeof page);
        snprintf(title, sizeof title, "Extra %d", n);
        page.size = sizeof page;
        page.plugin = "cmfm_demo";
        page.title = title;
        page.settings = &s;
        page.setting_count = 1;
        page.user = (void*)(intptr_t)n;
        page.get = extra_get;
        page.set = extra_set;
        if (menu->add_page(&page, why, sizeof why) == 0) {
            char line[256];
            snprintf(line, sizeof line, "CMFM refused extra page %d: %s", n, why);
            logline(AL_LOG_WARNING, line);
            return;
        }
    }
}

/* ---- The page ---- */

static const char* const kChoices[] = {"Alpha", "Beta", "Gamma"};

static void add_page(void) {
    const cmfm_menu_t* menu =
        (const cmfm_menu_t*)g_api->get_interface(g_self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION);
    cmfm_setting_t s[6];
    cmfm_page_t page;
    char why[200];
    if (menu == NULL) {
        logline(AL_LOG_INFO, "CMFM is not installed; running without the menu");
        return;
    }
    memset(s, 0, sizeof s);
    for (int i = 0; i < 6; ++i) {
        s[i].size = sizeof s[i];
    }
    s[0].id = ID_TOGGLE;
    s[0].type = CMFM_TYPE_BOOL;
    s[0].label = "Demo toggle";
    s[0].help = "Turns the demo's pretend feature on or off.";

    s[1].id = ID_CHOICE;
    s[1].type = CMFM_TYPE_CHOICE;
    s[1].label = "Demo choice";
    s[1].help = "Picks one of three values. Gamma needs Demo toggle on.";
    s[1].choices = kChoices;
    s[1].choice_count = 3;

    s[2].id = ID_PERCENT;
    s[2].type = CMFM_TYPE_INT;
    s[2].label = "Demo percent";
    s[2].help = "A number from 0 to 100 in steps of 10.";
    s[2].min = 0;
    s[2].max = 100;
    s[2].step = 10;
    s[2].unit = "%";
    s[2].default_value.i = 50;

    s[3].id = ID_SCALE;
    s[3].type = CMFM_TYPE_FLOAT;
    s[3].label = "Demo scale";
    s[3].help = "A number from 0.50 to 2.00 in steps of 0.25.";
    s[3].min = 0.5;
    s[3].max = 2.0;
    s[3].step = 0.25;
    s[3].decimals = 2;
    s[3].unit = "x";
    s[3].default_value.f = 1.0;

    s[4].id = ID_RESTART;
    s[4].type = CMFM_TYPE_BOOL;
    s[4].flags = CMFM_FLAG_RESTART;
    s[4].label = "Demo restart";
    s[4].help = "Pretends to need a restart.";

    s[5].id = ID_STATUS;
    s[5].type = CMFM_TYPE_TEXT;
    s[5].label = "Demo status";
    s[5].help = "A read-only line the plugin writes.";

    memset(&page, 0, sizeof page);
    page.size = sizeof page;
    page.plugin = "cmfm_demo";
    page.title = "Demo";
    page.help = "CMFM's demo plugin: one setting of each type.";
    page.settings = s;
    page.setting_count = 6;
    page.get = demo_get;
    page.set = demo_set;
    page.validate = demo_validate;
    page.get_text = demo_text;
    page.on_event = demo_event;
    /* CMFM copies the page: the local arrays may go away after this. */
    if (menu->add_page(&page, why, sizeof why) == 0) {
        char line[256];
        snprintf(line, sizeof line, "CMFM refused the page: %s", why);
        logline(AL_LOG_WARNING, line);
        return;
    }
    logline(AL_LOG_INFO, "page added to CMFM");
    add_extra_pages(menu);
}

/* ---- Astera Loader ---- */

static const al_plugin_info_t kInfo = {
    sizeof(al_plugin_info_t),
    AL_API_VERSION,
    "cmfm_demo",
    "CMFM demo",
    "0.1.0",
    0,    /* no phases */
    0,    /* order */
    NULL, /* load_after: CMFM starts earlier (order -100) */
    NULL, /* load_before */
    NULL, /* any exe */
    NULL, /* on_phase */
    NULL, /* dependencies: works without CMFM */
};

AL_PLUGIN_EXPORT const al_plugin_info_t* AL_CALL al_plugin_info(void) {
    return &kInfo;
}

AL_PLUGIN_EXPORT al_result_t AL_CALL al_plugin_init(const al_api_t* api, const al_init_t* init) {
    g_api = api;
    g_self = init->self;
    api->set_status(init->self, AL_STATUS_ACTIVE, NULL);
    if (api->size < offsetof(al_api_t, get_interface) + sizeof(api->get_interface)) {
        api->set_status(init->self, AL_STATUS_DEGRADED, "this Astera Loader has no services");
        return AL_OK;
    }
    if (init->size >= offsetof(al_init_t, data_folder) + sizeof(init->data_folder) && init->data_folder != NULL) {
        swprintf(g_ini, sizeof g_ini / sizeof g_ini[0], L"%ls\\cmfm_demo.ini", init->data_folder);
        load();
    }
    add_page();
    return AL_OK;
}
