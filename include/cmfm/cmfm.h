/* CMFM menu interface "cmfm.menu", version 1.1. C ABI, 64-bit Windows.
 *
 * CMFM (Configuration Menu For Mods) is an Astera Loader plugin that shows
 * other plugins' settings inside Monster Hunter: World's own menus: "Mods"
 * right below Options, in the title menu and in the Start menu's System tab.
 * A plugin describes one page of settings; CMFM draws it and reads and
 * writes every value through the plugin's callbacks. The plugin's own
 * config file stays the source of truth: CMFM stores none of its values.
 *
 * Getting the table (CMFM publishes it from its al_plugin_init):
 *
 *   const cmfm_menu_t* menu = (const cmfm_menu_t*)api->get_interface(
 *       self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION);
 *
 * CMFM starts early (order -100), so asking in al_plugin_init works unless
 * load_after/load_before say otherwise; asking in PRE_MAIN or later always
 * works. NULL means CMFM is not installed or too old: run without the menu.
 *
 * Versions: major << 16 | minor, as Astera Loader's. Within a major version
 * changes are additive only: new fields at the end of structs, so `size`
 * grows. Structs a plugin passes in start with `size`; CMFM reads a field
 * only when `size` covers it. Check the table's `size` before calling a
 * function added after the version you built for. A plugin that uses a
 * feature marked "since 1.1" asks for CMFM_MAKE_VERSION(1, 1) (or checks the
 * table's `version`); a CMFM 1.0 refuses pages that use one.
 *
 * Threads:
 *   - add_page may be called from any thread at any time. A page added while
 *     a Mods page is open appears the next time one opens.
 *   - Callbacks run on the game's menu thread while a Mods page is opening,
 *     open or closing. They must return quickly, must not block, must not
 *     throw and must not call add_page. `get` runs every time a row is drawn.
 *
 * Memory: strings are UTF-8 and NUL-terminated. CMFM copies the page, its
 * settings and their strings during add_page; free them afterwards if you
 * like. `user` and the callbacks must stay valid until the process exits
 * (Astera Loader never unloads a plugin).
 *
 * What a page looks like: the Mods page lists "Overview" (CMFM's version and
 * every plugin's state) and then one entry per plugin page, by its title, in
 * the order pages were added (24 at most; further pages are listed as not
 * shown on the Overview). The highlighted entry's settings show next to the
 * list, one row each: the label on the left, the value on the right, the
 * help line at the bottom. SECTION rows group the rows below them. Above the
 * rows, the page's header: its title, its version (since 1.1) and "Restart
 * needed" while one of its changes waits for a game restart. A page with
 * more rows than fit (14) scrolls; keep the important settings near the top.
 */
#ifndef CMFM_H
#define CMFM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CMFM_MAKE_VERSION(major, minor) ((uint32_t)(((uint32_t)(major) << 16) | (uint32_t)(minor)))
#define CMFM_VERSION_MAJOR(version) ((uint32_t)(version) >> 16)
#define CMFM_VERSION_MINOR(version) ((uint32_t)(version) & 0xffffu)

#define CMFM_MENU_INTERFACE "cmfm.menu"
#define CMFM_MENU_VERSION CMFM_MAKE_VERSION(1, 1)

/* x64 Windows has one calling convention; this names it for readers. */
#define CMFM_CALL __cdecl

typedef uint32_t cmfm_result_t;
#define CMFM_OK 0u
#define CMFM_ERROR 1u /* any value other than CMFM_OK is a failure */

/* Setting types. */
typedef uint32_t cmfm_type_t;
#define CMFM_TYPE_BOOL 1u   /* Off / On: value.i is 0 or 1 */
/* On the Mods page a CHOICE of up to 5 values cycles with the arrows; one of
 * 6 to 14 values opens the game's pick list on Confirm (CMFM_FLAG_LIST and
 * CMFM_FLAG_ARROWS force either; more than 14 always uses the arrows). */
#define CMFM_TYPE_CHOICE 2u /* one of `choices`: value.i is its index */
/* INT and FLOAT with 3 or more values show as the game's slider on the Mods
 * page (the value next to the bar), with arrows elsewhere. */
#define CMFM_TYPE_INT 3u    /* value.i from min to max in steps of `step` */
#define CMFM_TYPE_FLOAT 4u  /* value.f from min to max in steps of `step` */
#define CMFM_TYPE_TEXT 5u   /* a read-only line: the label and a text from get_text */
/* Since 1.1: a heading over the settings that follow it, up to the next
 * SECTION. Only `id` and `label` are used (flags 0); the cursor skips it. A
 * page needs at least one setting besides its sections. */
#define CMFM_TYPE_SECTION 6u
/* Since 1.1: a row that runs an action. Confirm calls the page's `press`
 * (after the game's Yes/No dialog when `confirm` is set); its value box shows
 * `button`. Needs `press`; flags 0. */
#define CMFM_TYPE_BUTTON 7u

/* Setting flags. */
typedef uint32_t cmfm_flags_t;
/* Takes effect after the game restarts. CMFM says so in the help line; once
 * the player changes the value away from the one the game started with, CMFM
 * marks the row, the page's title and the Overview until the game restarts
 * (or the value goes back). */
#define CMFM_FLAG_RESTART 0x1u
/* Since 1.1, CHOICE only, not both: the pick list even for 5 values or fewer
 * (up to 14), or the arrows even for more than 5. */
#define CMFM_FLAG_LIST 0x2u
#define CMFM_FLAG_ARROWS 0x4u

/* Limits of CMFM 1.0's native menu. Pages beyond them are refused with the
 * reason; later versions may raise them without an interface change. */
#define CMFM_MAX_SETTINGS 24u /* per page */
#define CMFM_MAX_CHOICES 32u  /* CHOICE names, and INT/FLOAT steps from min to max */
#define CMFM_MAX_TEXT 128u    /* bytes CMFM offers get_text, including the NUL */

typedef struct cmfm_value_t {
    int32_t i;         /* BOOL, CHOICE, INT */
    uint32_t reserved; /* 0 */
    double f;          /* FLOAT */
} cmfm_value_t;

typedef struct cmfm_setting_t {
    uint32_t size;      /* sizeof(cmfm_setting_t) */
    uint32_t id;        /* the plugin's id for this setting, unique on its page */
    cmfm_type_t type;   /* CMFM_TYPE_... */
    cmfm_flags_t flags; /* CMFM_FLAG_... */
    const char* label;  /* the row's name, e.g. "Frame generation" */
    const char* help;   /* the help line while the row is highlighted; may be NULL */
    /* CHOICE: the value names, e.g. {"Off", "2x", "3x", "4x"}. */
    const char* const* choices;
    uint32_t choice_count; /* 1 to CMFM_MAX_CHOICES */
    /* INT and FLOAT: the range, and how a value is shown. */
    uint32_t decimals; /* FLOAT: digits after the point, 0 to 3 */
    double min;
    double max;        /* (max - min) / step + 1 values, at most CMFM_MAX_CHOICES */
    double step;       /* > 0; whole numbers for INT */
    const char* unit;  /* shown after the number, e.g. "%" or " ms"; may be NULL */
    /* BOOL, CHOICE, INT, FLOAT: what "Set To Default" restores. */
    cmfm_value_t default_value;
    /* Since 1.1 (read when `size` covers them). BUTTON: the text in its value
     * box, e.g. "Reset" (NULL: "Run"), and a question for the game's Yes/No
     * dialog before pressing, e.g. "Reset every DLSS setting?" (NULL: none). */
    const char* button;
    const char* confirm;
} cmfm_setting_t;

/* Reads a setting's current value into *value. On CMFM_ERROR the row shows
 * its first value and CMFM logs it. */
typedef cmfm_result_t(CMFM_CALL* cmfm_get_fn)(void* user, uint32_t id, cmfm_value_t* value);

/* Checks a value the player picked, before CMFM sets it. On CMFM_ERROR write
 * a short reason into `why` (`why_size` bytes including the NUL): CMFM shows
 * it in the help line and the value stays as it was. NULL accepts every
 * value in range. */
typedef cmfm_result_t(CMFM_CALL* cmfm_validate_fn)(void* user, uint32_t id, const cmfm_value_t* value, char* why,
                                                   uint32_t why_size);

/* The player picked a value (validated, in range). Apply it, now or after
 * a restart for CMFM_FLAG_RESTART, and keep it in the plugin's own config
 * (now, or at CMFM_EVENT_CLOSED). CMFM_ERROR keeps the old value. */
typedef cmfm_result_t(CMFM_CALL* cmfm_set_fn)(void* user, uint32_t id, const cmfm_value_t* value);

/* TEXT settings: writes the line's text into `text` (`text_size` bytes
 * including the NUL; longer texts are cut). CMFM asks when a Mods page
 * opens, right after CMFM_EVENT_OPENED. */
typedef void(CMFM_CALL* cmfm_get_text_fn)(void* user, uint32_t id, char* text, uint32_t text_size);

/* Since 1.1. A BUTTON was pressed (and the player said Yes). CMFM reads every
 * value and text of the page again afterwards. */
typedef cmfm_result_t(CMFM_CALL* cmfm_press_fn)(void* user, uint32_t id);

/* Since 1.1. Nonzero if the setting cannot be changed now: CMFM greys the row,
 * refuses changes and shows the reason written into `why` (`why_size` bytes
 * including the NUL) in the help line. CMFM asks when a Mods page opens and
 * after every change it makes, so a lock may depend on other settings. */
typedef uint32_t(CMFM_CALL* cmfm_locked_fn)(void* user, uint32_t id, char* why, uint32_t why_size);

typedef uint32_t cmfm_event_t;
#define CMFM_EVENT_OPENED 1u /* a Mods page opened (title screen or in game) */
#define CMFM_EVENT_CLOSED 2u /* it closed: a good moment to save the config file */

typedef void(CMFM_CALL* cmfm_event_fn)(void* user, cmfm_event_t event);

typedef struct cmfm_page_t {
    uint32_t size;      /* sizeof(cmfm_page_t) */
    uint32_t reserved;  /* 0 */
    const char* plugin; /* the plugin's Astera Loader name, e.g. "mhw_dlss" (status page, logs) */
    const char* title;  /* the tab's name, short: "DLSS" */
    const char* help;   /* the tab's description; may be NULL */
    /* An array of setting_count settings, all built with the same
     * sizeof(cmfm_setting_t): CMFM steps through it by settings[0].size, so
     * an array built for a later minor version still reads correctly. */
    const cmfm_setting_t* settings;
    uint32_t setting_count; /* 1 to CMFM_MAX_SETTINGS */
    uint32_t reserved2;     /* 0 */
    void* user;             /* passed to every callback */
    cmfm_get_fn get;           /* required unless every setting is TEXT */
    cmfm_set_fn set;           /* required unless every setting is TEXT */
    cmfm_validate_fn validate; /* may be NULL */
    cmfm_get_text_fn get_text; /* required if a setting is TEXT */
    cmfm_event_fn on_event;    /* may be NULL */
    /* Since 1.1 (read when `size` covers them). */
    cmfm_press_fn press;   /* required if a setting is BUTTON */
    cmfm_locked_fn locked; /* may be NULL: nothing is ever locked */
    const char* version;   /* shown in the page's header, e.g. "0.15.0"; may be NULL */
} cmfm_page_t;

typedef struct cmfm_menu_t {
    uint32_t size;    /* sizeof(cmfm_menu_t) as CMFM was built */
    uint32_t version; /* CMFM_MENU_VERSION of this CMFM */
    /* Adds a page. Returns its handle, or 0 if CMFM refused it; the reason
     * then goes into `why` (`why_size` bytes including the NUL; `why` may be
     * NULL), CMFM's log and its status page. */
    uint32_t(CMFM_CALL* add_page)(const cmfm_page_t* page, char* why, uint32_t why_size);
} cmfm_menu_t;

#ifdef __cplusplus
}
#endif

#endif /* CMFM_H */
