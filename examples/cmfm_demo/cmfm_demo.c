/* cmfm_demo: an Astera Loader plugin that adds a page to CMFM's Mods menu.
 * The example for plugin authors (plain C), and CMFM's test client.
 *
 * It shows every setting type of cmfm.menu 1.2 (rows that hide, live lines and
 * key bindings since 1.2; its hotkey is a real Astera Loader hotkey) on three
 * pages of one mod, "Demo" (its pages are tabs above the panel), keeps
 * its values in its own file (AsteraLoader\data\cmfm_demo\cmfm_demo.ini, the
 * source of truth), refuses one combination in `validate`, and saves when
 * the Mods page closes.
 * With `extra_pages = N` in the ini it adds N small pages more (CMFM's tests
 * of a long list of mods, up to 1023); the first of them is a long page of
 * 1024 rows (the most a page holds), the
 * second holds a key binding that overlaps the demo's hotkey (F5 against
 * Ctrl+F5), so CMFM marks both.
 * With `tab_pages = N` it adds a mod "Many tabs" of N pages (up to 64; CMFM's
 * tests of the page tabs: more than fit, short and 24-byte titles).
 *
 * Install: <game>\AsteraLoader\plugins\cmfm_demo\cmfm_demo.dll, next to CMFM.
 */
#include "al/al_plugin.h"
#include "cmfm/cmfm.h"
#include "cmfm/cmfm_check.h" /* add_page's rules, to check pages first */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

enum {
    ID_TOGGLE = 1, ID_CHOICE, ID_PERCENT, ID_SCALE, ID_RESTART, ID_STATUS, ID_BASICS, ID_NUMBERS, ID_OTHER,
    ID_QUALITY, ID_PRESET, ID_UPSCALING, ID_UPSCALER, ID_DLSS_PRESET, ID_FSR_SHARPNESS, ID_XESS_QUALITY, ID_LIVE,
    ID_CLOCK, ID_KEYS, ID_HOTKEY, ID_PRESSES
};

#define VK_F5_KEY 0x74 /* winuser.h's VK_F5 */
#define DEMO_SETTINGS 21

#define DEMO_VERSION "0.1.0" /* the plugin's version: Astera Loader's info and the page's header */

static const al_api_t* g_api;
static const al_input_t* g_input; /* Astera Loader's input service, for the hotkey; may be NULL */
static uint32_t g_hotkey;         /* its handle, 0: none */
static volatile long g_presses;   /* counted on the game's frame thread */
static time_t g_start; /* the live clock counts from the plugin's start */
static al_plugin_t* g_self;
static wchar_t g_ini[1024];

/* The values: the plugin's own state, loaded from and saved to its ini. */
static struct {
    int toggle;
    int choice;
    int percent;
    double scale;
    int restart;
    int quality;
    int upscaler; /* 0 DLSS, 1 FSR, 2 XeSS */
    int dlss_preset;
    int fsr_sharpness;
    int xess_quality;
    int hotkey; /* a key value: CMFM_KEY_CTRL | VK_F5 is Ctrl+F5 */
} g_values = {0, 0, 50, 1.0, 0, 3, 0, 1, 50, 1, CMFM_KEY_CTRL | VK_F5_KEY};
static int g_dirty;
static int g_opened;
#define MAX_EXTRA 1023 /* CMFM shows 1024 pages since cmfm.menu 1.2 */
static int g_extra_pages; /* from the ini: test pages to add */
static int g_extra[MAX_EXTRA];
#define MAX_TAB_PAGES 64
static int g_tab_pages; /* from the ini: pages of the mod "Many tabs" */
static int g_tab_values[MAX_TAB_PAGES];

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
        else if (sscanf(line, " quality = %d", &i) == 1 && i >= 0 && i <= 6) g_values.quality = i;
        else if (sscanf(line, " upscaler = %d", &i) == 1 && i >= 0 && i <= 2) g_values.upscaler = i;
        else if (sscanf(line, " dlss_preset = %d", &i) == 1 && i >= 0 && i <= 2) g_values.dlss_preset = i;
        else if (sscanf(line, " fsr_sharpness = %d", &i) == 1 && i >= 0 && i <= 100) g_values.fsr_sharpness = i;
        else if (sscanf(line, " xess_quality = %d", &i) == 1 && i >= 0 && i <= 3) g_values.xess_quality = i;
        else if (sscanf(line, " hotkey = %d", &i) == 1 && i >= 0 && i <= 0x7FF) g_values.hotkey = i;
        else if (sscanf(line, " extra_pages = %d", &i) == 1 && i >= 0 && i <= MAX_EXTRA) g_extra_pages = i;
        else if (sscanf(line, " tab_pages = %d", &i) == 1 && i >= 0 && i <= MAX_TAB_PAGES) g_tab_pages = i;
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
    fprintf(f, "toggle = %d\nchoice = %d\npercent = %d\nscale = %.2f\nrestart = %d\nquality = %d\n",
            g_values.toggle, g_values.choice, g_values.percent, g_values.scale, g_values.restart, g_values.quality);
    fprintf(f, "upscaler = %d\ndlss_preset = %d\nfsr_sharpness = %d\nxess_quality = %d\n", g_values.upscaler,
            g_values.dlss_preset, g_values.fsr_sharpness, g_values.xess_quality);
    fprintf(f, "# a virtual-key code plus 256 Ctrl, 512 Shift, 1024 Alt; 0 for none\nhotkey = %d\n", g_values.hotkey);
    fprintf(f, "# test pages for a long list of mods (0 to %d)\nextra_pages = %d\n", MAX_EXTRA, g_extra_pages);
    fprintf(f, "# pages of the test mod \"Many tabs\" (0 to %d)\ntab_pages = %d\n", MAX_TAB_PAGES, g_tab_pages);
    fclose(f);
    g_dirty = 0;
    logline(AL_LOG_INFO, "cmfm_demo.ini saved");
}

/* ---- The hotkey (Astera Loader's al.input) ---- */

static void AL_CALL on_hotkey(al_plugin_t* self, uint32_t handle, void* user) {
    (void)self;
    (void)handle;
    (void)user;
    ++g_presses; /* the game's frame thread; the menu reads it */
}

/* A KEY setting's value is what al.input's hotkeys take: the virtual-key code
 * and the AL_MOD_* modifiers. A new binding replaces the old hotkey. */
static void bind_hotkey(int key) {
    al_hotkey_t hotkey;
    if (g_input == NULL) {
        return;
    }
    if (g_hotkey != 0) {
        g_input->remove_hotkey(g_self, g_hotkey);
        g_hotkey = 0;
    }
    if (key == 0) {
        return;
    }
    memset(&hotkey, 0, sizeof hotkey);
    hotkey.size = sizeof hotkey;
    hotkey.key = CMFM_KEY_VK(key);
    hotkey.modifiers = CMFM_KEY_MODS(key);
    hotkey.name = "Demo hotkey";
    g_hotkey = g_input->add_hotkey(g_self, &hotkey, on_hotkey, NULL);
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
        case ID_QUALITY: value->i = g_values.quality; return CMFM_OK;
        case ID_UPSCALER: value->i = g_values.upscaler; return CMFM_OK;
        case ID_DLSS_PRESET: value->i = g_values.dlss_preset; return CMFM_OK;
        case ID_FSR_SHARPNESS: value->i = g_values.fsr_sharpness; return CMFM_OK;
        case ID_XESS_QUALITY: value->i = g_values.xess_quality; return CMFM_OK;
        case ID_HOTKEY: value->i = g_values.hotkey; return CMFM_OK;
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
    if (id == ID_HOTKEY && CMFM_KEY_VK(value->i) == 0x0D) { /* VK_RETURN */
        snprintf(why, why_size, "Enter is the game's Confirm; pick another key.");
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
        case ID_QUALITY: g_values.quality = value->i; break;
        case ID_UPSCALER: g_values.upscaler = value->i; break;
        case ID_DLSS_PRESET: g_values.dlss_preset = value->i; break;
        case ID_FSR_SHARPNESS: g_values.fsr_sharpness = value->i; break;
        case ID_XESS_QUALITY: g_values.xess_quality = value->i; break;
        case ID_HOTKEY:
            g_values.hotkey = value->i;
            bind_hotkey(value->i); /* live: the new key works at once */
            break;
        default: return CMFM_ERROR;
    }
    g_dirty = 1;
    snprintf(line, sizeof line, "set %u: i=%d f=%.2f", id, value->i, value->f);
    logline(AL_LOG_INFO, line);
    return CMFM_OK;
}

/* Since cmfm.menu 1.1: the button (after the player said Yes to its question).
 * Resetting to defaults needs no button of the plugin's own since 1.2: CMFM
 * ends the page with a "Reset to defaults" row that uses the defaults. */
static cmfm_result_t CMFM_CALL demo_press(void* user, uint32_t id) {
    (void)user;
    if (id != ID_PRESET) {
        return CMFM_ERROR;
    }
    g_values.percent = 75;
    g_values.scale = 1.5;
    g_values.quality = 4;
    g_dirty = 1;
    logline(AL_LOG_INFO, "High preset applied (button)");
    return CMFM_OK;
}

/* Since cmfm.menu 1.3: the button's question, worded when it is pressed (an
 * older CMFM ignores this callback and shows the setting's `confirm`). */
static cmfm_result_t CMFM_CALL demo_question(void* user, uint32_t id, char* text, uint32_t text_size) {
    (void)user;
    if (id != ID_PRESET) {
        return CMFM_ERROR;
    }
    snprintf(text, text_size, "Apply the demo's High preset? (Demo percent is %d%% now.)", g_values.percent);
    return CMFM_OK;
}

/* Since cmfm.menu 1.1: Demo scale can be changed only while Demo toggle is on
 * (greyed). Since 1.2: each upscaler's setting only with that upscaler (its
 * rows have CMFM_FLAG_HIDE_LOCKED, so they hide instead of greying). */
static uint32_t CMFM_CALL demo_locked(void* user, uint32_t id, char* why, uint32_t why_size) {
    static const char* const kOnlyWith[] = {"Only with DLSS.", "Only with FSR.", "Only with XeSS."};
    int needs = -1;
    (void)user;
    if (id == ID_SCALE && !g_values.toggle) {
        snprintf(why, why_size, "Turn on Demo toggle first.");
        return 1;
    }
    if (id == ID_DLSS_PRESET) needs = 0;
    if (id == ID_FSR_SHARPNESS) needs = 1;
    if (id == ID_XESS_QUALITY) needs = 2;
    if (needs >= 0 && g_values.upscaler != needs) {
        snprintf(why, why_size, "%s", kOnlyWith[needs]);
        return 1;
    }
    return 0;
}

static void CMFM_CALL demo_text(void* user, uint32_t id, char* text, uint32_t text_size) {
    (void)user;
    if (id == ID_STATUS) {
        snprintf(text, text_size, "Opened %d time%s", g_opened, g_opened == 1 ? "" : "s");
    } else if (id == ID_CLOCK) { /* live: asked four times a second while it shows */
        const long seconds = (long)difftime(time(NULL), g_start);
        snprintf(text, text_size, "%02ld:%02ld:%02ld since start", seconds / 3600, seconds / 60 % 60, seconds % 60);
    } else if (id == ID_PRESSES) { /* live too */
        if (g_input == NULL) {
            snprintf(text, text_size, "No hotkeys (no al.input)");
        } else {
            snprintf(text, text_size, "Pressed %ld time%s", g_presses, g_presses == 1 ? "" : "s");
        }
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
/* The first one is a long page instead: CMFM_MAX_SETTINGS rows (1024 since
 * cmfm.menu 1.2), sections of 15 toggles (CMFM's tests of a page taller than
 * the panel and of the longest page). */

static int g_long[CMFM_MAX_SETTINGS];

static cmfm_result_t CMFM_CALL extra_get(void* user, uint32_t id, cmfm_value_t* value) {
    if ((intptr_t)user == 1) {
        value->i = g_long[id - 1];
        return CMFM_OK;
    }
    value->i = g_extra[(intptr_t)user - 1];
    return CMFM_OK;
}

static cmfm_result_t CMFM_CALL extra_set(void* user, uint32_t id, const cmfm_value_t* value) {
    if ((intptr_t)user == 1) {
        g_long[id - 1] = value->i;
        return CMFM_OK;
    }
    g_extra[(intptr_t)user - 1] = value->i;
    return CMFM_OK;
}

static void add_extra_pages(const cmfm_menu_t* menu) {
    static cmfm_setting_t s[CMFM_MAX_SETTINGS]; /* static: too big for the stack */
    static char labels[CMFM_MAX_SETTINGS][40];  /* CMFM copies them in add_page */
    cmfm_page_t page;
    char title[24];
    char why[200];
    for (int n = 1; n <= g_extra_pages; ++n) {
        const uint32_t count = n == 1 ? CMFM_MAX_SETTINGS : 1;
        memset(s, 0, sizeof s);
        for (uint32_t i = 0; i < count; ++i) {
            s[i].size = sizeof s[i];
            s[i].id = i + 1;
            s[i].type = CMFM_TYPE_BOOL;
            s[i].label = "Extra toggle";
            s[i].help = "A test page for CMFM's list of mods.";
            if (n == 1 && i % 16 == 0) {
                snprintf(labels[i], sizeof labels[i], "Group %u", i / 16 + 1);
                s[i].type = CMFM_TYPE_SECTION;
                s[i].label = labels[i];
                s[i].help = NULL;
            } else if (n == 1) {
                snprintf(labels[i], sizeof labels[i], "Long toggle %u%s", i - i / 16,
                         i == count - 1 ? " (the last row)" : "");
                s[i].label = labels[i];
                s[i].help = "A test page of 1024 rows, the most a page holds (since 1.2).";
            }
        }
        if (n == 2) { /* a key binding as another mod's: F5 overlaps the demo's Ctrl+F5 */
            s[0].type = CMFM_TYPE_KEY;
            s[0].label = "Extra hotkey";
            s[0].help = "A key binding of a pretend second mod (not a real hotkey).";
            s[0].default_value.i = VK_F5_KEY;
            g_extra[1] = VK_F5_KEY;
        }
        memset(&page, 0, sizeof page);
        if (n == 1) {
            snprintf(title, sizeof title, "Long page");
        } else {
            snprintf(title, sizeof title, "Extra %d", n);
        }
        page.size = sizeof page;
        page.plugin = "cmfm_demo";
        page.title = title;
        page.mod = title; /* since 1.2: a mod each, not pages of "Demo" */
        page.settings = s;
        page.setting_count = count;
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

/* The mod "Many tabs": one toggle per page, the page's number in `user`. */
static cmfm_result_t CMFM_CALL tab_get(void* user, uint32_t id, cmfm_value_t* value) {
    (void)id;
    value->i = g_tab_values[(intptr_t)user];
    return CMFM_OK;
}

static cmfm_result_t CMFM_CALL tab_set(void* user, uint32_t id, const cmfm_value_t* value) {
    (void)id;
    g_tab_values[(intptr_t)user] = value->i != 0;
    return CMFM_OK;
}

static void add_tab_pages(const cmfm_menu_t* menu) {
    cmfm_setting_t s;
    cmfm_page_t page;
    char title[CMFM_MAX_TITLE + 1];
    char label[CMFM_MAX_LABEL + 1];
    char why[200];
    for (int n = 1; n <= g_tab_pages; ++n) {
        /* Short and long titles in turn; "Page 12 with a long name" is 24
         * bytes, the most a title holds. */
        if (n % 3 == 1) {
            snprintf(title, sizeof title, "Tab %d", n);
        } else if (n % 3 == 2) {
            snprintf(title, sizeof title, "Settings %d", n);
        } else {
            snprintf(title, sizeof title, "Page %d with a long name", n);
        }
        snprintf(label, sizeof label, "Toggle on page %d of %d", n, g_tab_pages);
        memset(&s, 0, sizeof s);
        s.size = sizeof s;
        s.id = 1;
        s.type = CMFM_TYPE_BOOL;
        s.label = label;
        s.help = "A test page for CMFM's page tabs.";
        memset(&page, 0, sizeof page);
        page.size = sizeof page;
        page.plugin = "cmfm_demo";
        page.mod = "Many tabs";
        page.title = title;
        page.settings = &s;
        page.setting_count = 1;
        page.user = (void*)(intptr_t)(n - 1);
        page.get = tab_get;
        page.set = tab_set;
        if (menu->add_page(&page, why, sizeof why) == 0) {
            char line[256];
            snprintf(line, sizeof line, "CMFM refused tab page %d: %s", n, why);
            logline(AL_LOG_WARNING, line);
            return;
        }
    }
}

/* ---- The page ---- */

static const char* const kChoices[] = {"Alpha", "Beta", "Gamma"};
/* Seven choices: more than five, so the Mods page opens its pick list. */
static const char* const kQuality[] = {"Off", "Very low", "Low", "Medium", "High", "Very high", "Ultra"};
/* The rows that hide: one upscaler choice, each upscaler's own setting. */
static const char* const kUpscalers[] = {"DLSS", "FSR", "XeSS"};
static const char* const kDlssPresets[] = {"Quality", "Balanced", "Performance"};
static const char* const kXessQualities[] = {"Ultra quality", "Quality", "Balanced", "Performance"};

static void add_page(void) {
    const cmfm_menu_t* menu =
        (const cmfm_menu_t*)g_api->get_interface(g_self, CMFM_MENU_INTERFACE, CMFM_MAKE_VERSION(1, 2));
    cmfm_setting_t s[DEMO_SETTINGS];
    cmfm_page_t page;
    char why[200];
    if (menu == NULL) {
        logline(AL_LOG_INFO, "CMFM is not installed or older than 1.2; running without the menu");
        return;
    }
    memset(s, 0, sizeof s);
    for (int i = 0; i < DEMO_SETTINGS; ++i) {
        s[i].size = sizeof s[i];
    }
    s[0].id = ID_BASICS;
    s[0].type = CMFM_TYPE_SECTION;
    s[0].label = "Basics";

    s[1].id = ID_TOGGLE;
    s[1].type = CMFM_TYPE_BOOL;
    s[1].label = "Demo toggle";
    s[1].help = "Turns the demo's pretend feature on or off.";

    s[2].id = ID_CHOICE;
    s[2].type = CMFM_TYPE_CHOICE;
    s[2].label = "Demo choice";
    s[2].help = "Picks one of three values. Gamma needs Demo toggle on.";
    s[2].choices = kChoices;
    s[2].choice_count = 3;

    s[3].id = ID_NUMBERS;
    s[3].type = CMFM_TYPE_SECTION;
    s[3].label = "Numbers";

    s[4].id = ID_PERCENT;
    s[4].type = CMFM_TYPE_INT;
    s[4].label = "Demo percent";
    s[4].help = "A number from 0 to 100 in steps of 1: a fine slider (since 1.1).";
    s[4].min = 0;
    s[4].max = 100;
    s[4].step = 1;
    s[4].unit = "%";
    s[4].default_value.i = 50;

    s[5].id = ID_SCALE;
    s[5].type = CMFM_TYPE_FLOAT;
    s[5].label = "Demo scale";
    s[5].help = "A number from 0.50 to 2.00 in steps of 0.25. Needs Demo toggle on.";
    s[5].min = 0.5;
    s[5].max = 2.0;
    s[5].step = 0.25;
    s[5].decimals = 2;
    s[5].unit = "x";
    s[5].default_value.f = 1.0;

    s[6].id = ID_OTHER;
    s[6].type = CMFM_TYPE_SECTION;
    s[6].label = "Other";

    s[7].id = ID_RESTART;
    s[7].type = CMFM_TYPE_BOOL;
    s[7].flags = CMFM_FLAG_RESTART;
    s[7].label = "Demo restart";
    s[7].help = "Pretends to need a restart.";

    s[8].id = ID_STATUS;
    s[8].type = CMFM_TYPE_TEXT;
    s[8].label = "Demo status";
    s[8].help = "A read-only line the plugin writes.";

    s[9].id = ID_QUALITY;
    s[9].type = CMFM_TYPE_CHOICE;
    s[9].label = "Demo quality";
    s[9].help = "Seven values: Confirm opens the list to pick from.";
    s[9].choices = kQuality;
    s[9].choice_count = 7;
    s[9].default_value.i = 3;

    s[10].id = ID_PRESET;
    s[10].type = CMFM_TYPE_BUTTON;
    s[10].label = "Apply preset";
    s[10].help = "Sets the High preset: Demo percent 75%, Demo scale 1.50x, Demo quality High.";
    s[10].button = "High";
    s[10].confirm = "Apply the demo's High preset?";

    s[11].id = ID_UPSCALING;
    s[11].type = CMFM_TYPE_SECTION;
    s[11].label = "Upscaling";

    s[12].id = ID_UPSCALER;
    s[12].type = CMFM_TYPE_CHOICE;
    s[12].label = "Demo upscaler";
    s[12].help = "Pick one: only its own setting shows below (since 1.2).";
    s[12].choices = kUpscalers;
    s[12].choice_count = 3;

    s[13].id = ID_DLSS_PRESET;
    s[13].type = CMFM_TYPE_CHOICE;
    s[13].flags = CMFM_FLAG_HIDE_LOCKED;
    s[13].label = "DLSS preset";
    s[13].help = "Shows only while Demo upscaler is DLSS.";
    s[13].choices = kDlssPresets;
    s[13].choice_count = 3;
    s[13].default_value.i = 1;

    s[14].id = ID_FSR_SHARPNESS;
    s[14].type = CMFM_TYPE_INT;
    s[14].flags = CMFM_FLAG_HIDE_LOCKED;
    s[14].label = "FSR sharpness";
    s[14].help = "Shows only while Demo upscaler is FSR.";
    s[14].min = 0;
    s[14].max = 100;
    s[14].step = 1;
    s[14].unit = "%";
    s[14].default_value.i = 50;

    s[15].id = ID_XESS_QUALITY;
    s[15].type = CMFM_TYPE_CHOICE;
    s[15].flags = CMFM_FLAG_HIDE_LOCKED;
    s[15].label = "XeSS quality";
    s[15].help = "Shows only while Demo upscaler is XeSS.";
    s[15].choices = kXessQualities;
    s[15].choice_count = 4;
    s[15].default_value.i = 1;

    s[16].id = ID_LIVE;
    s[16].type = CMFM_TYPE_SECTION;
    s[16].label = "Live";

    s[17].id = ID_CLOCK;
    s[17].type = CMFM_TYPE_TEXT;
    s[17].flags = CMFM_FLAG_LIVE;
    s[17].label = "Demo clock";
    s[17].help = "A line read again four times a second while it shows (since 1.2).";

    s[18].id = ID_KEYS;
    s[18].type = CMFM_TYPE_SECTION;
    s[18].label = "Keys";

    s[19].id = ID_HOTKEY;
    s[19].type = CMFM_TYPE_KEY;
    s[19].label = "Demo hotkey";
    s[19].help = "A real hotkey: Confirm, then press the new key (since 1.2). The line below counts presses.";
    s[19].default_value.i = CMFM_KEY_CTRL | VK_F5_KEY;

    s[20].id = ID_PRESSES;
    s[20].type = CMFM_TYPE_TEXT;
    s[20].flags = CMFM_FLAG_LIVE;
    s[20].label = "Hotkey presses";
    s[20].help = "Counts presses of Demo hotkey.";

    /* Three pages of one mod (since 1.2, `mod`): the mod list shows "Demo"
     * once, its pages as tabs above the panel, the first the default. The callbacks
     * serve every page (the ids are unique over the plugin). */
    {
        static const struct {
            const char* title;
            const char* help;
            int first;
            int count;
        } kPages[] = {
            {"General", "CMFM's demo plugin: one setting of each type.", 0, 11},
            {"Upscaling", "Rows that hide: only the chosen upscaler's setting shows (since 1.2).", 11, 5},
            {"Live and keys", "A live line and a real hotkey (since 1.2).", 16, 5},
        };
        for (size_t p = 0; p < sizeof kPages / sizeof kPages[0]; ++p) {
            memset(&page, 0, sizeof page);
            page.size = sizeof page;
            page.plugin = "cmfm_demo";
            page.mod = "Demo";
            page.title = kPages[p].title;
            page.version = DEMO_VERSION; /* since 1.1: shown in the page's header */
            page.help = kPages[p].help;
            page.settings = &s[kPages[p].first];
            page.setting_count = (uint32_t)kPages[p].count;
            page.get = demo_get;
            page.set = demo_set;
            page.validate = demo_validate;
            page.get_text = demo_text;
            page.on_event = demo_event;
            page.press = demo_press;
            page.locked = demo_locked;
            page.question = demo_question; /* since 1.3 */
            /* cmfm_check.h: the reason CMFM would give, before asking it. */
            if (cmfm_check_page(&page, why, sizeof why) != CMFM_OK) {
                char line[256];
                snprintf(line, sizeof line, "the page %s does not pass cmfm_check_page: %s", kPages[p].title, why);
                logline(AL_LOG_WARNING, line);
                return;
            }
            /* CMFM copies the page: the local arrays may go away after this. */
            if (menu->add_page(&page, why, sizeof why) == 0) {
                char line[256];
                snprintf(line, sizeof line, "CMFM refused the page %s: %s", kPages[p].title, why);
                logline(AL_LOG_WARNING, line);
                return;
            }
        }
    }
    logline(AL_LOG_INFO, "pages added to CMFM");
    add_extra_pages(menu);
    add_tab_pages(menu);
}

/* ---- Astera Loader ---- */

static const al_plugin_info_t kInfo = {
    sizeof(al_plugin_info_t),
    AL_API_VERSION,
    "cmfm_demo",
    "CMFM demo",
    DEMO_VERSION,
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
    g_start = time(NULL);
    api->set_status(init->self, AL_STATUS_ACTIVE, NULL);
    if (api->size < offsetof(al_api_t, get_interface) + sizeof(api->get_interface)) {
        api->set_status(init->self, AL_STATUS_DEGRADED, "this Astera Loader has no services");
        return AL_OK;
    }
    if (init->size >= offsetof(al_init_t, data_folder) + sizeof(init->data_folder) && init->data_folder != NULL) {
        swprintf(g_ini, sizeof g_ini / sizeof g_ini[0], L"%ls\\cmfm_demo.ini", init->data_folder);
        load();
    }
    /* Asked in al_plugin_init, so Astera Loader installs its input layer. */
    g_input = (const al_input_t*)api->get_interface(init->self, AL_INPUT_INTERFACE, AL_INPUT_VERSION);
    bind_hotkey(g_values.hotkey);
    add_page();
    return AL_OK;
}
