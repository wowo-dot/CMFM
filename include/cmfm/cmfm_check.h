/* CMFM page check, header-only C, for "cmfm.menu" (cmfm.h).
 *
 * cmfm_check_page applies exactly the rules CMFM's add_page applies (CMFM's
 * own add_page calls it), so a plugin can check its pages outside the game,
 * in a unit test or a static check at start-up, and see the same reason
 * CMFM would log:
 *
 *   char why[160];
 *   if (cmfm_check_page(&page, why, sizeof why) != CMFM_OK) { ... why ... }
 *
 * It reads only the page and its settings (no callbacks are called) and
 * needs nothing but the C standard library. Copy it next to cmfm.h, from the
 * same CMFM commit. A check newer than the CMFM a player runs may accept a
 * page that CMFM refuses for using a later feature; the reverse does not
 * happen within a major version.
 */
#ifndef CMFM_CHECK_H
#define CMFM_CHECK_H

#include "cmfm.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus) || !defined(_MSC_VER)
#define CMFM_CHECK_INLINE static inline
#else
#define CMFM_CHECK_INLINE static __inline
#endif

/* A struct field present for the struct's size (fields added in later minor
 * versions are read only when `size` covers them). */
#define CMFM_CHECK_HAS(object, type, field) \
    ((object)->size >= offsetof(type, field) + sizeof(((type*)0)->field))

/* A text of 1 to `limit` bytes, or 0 to `limit` if it may be empty (NULL
 * counts as empty). */
CMFM_CHECK_INLINE int cmfm_check_text(const char* s, uint32_t limit, int may_be_empty) {
    size_t n = 0;
    if (s == NULL) {
        return may_be_empty;
    }
    while (n <= limit && s[n] != '\0') {
        ++n;
    }
    return n <= limit && (n > 0 || may_be_empty);
}

/* A virtual-key code a KEY setting can hold: not the left or right mouse
 * button, not a modifier key on its own, not a Windows key. */
CMFM_CHECK_INLINE int cmfm_check_key_code(uint32_t vk) {
    switch (vk) {
        case 0x00:
        case 0x01: /* left mouse button */
        case 0x02: /* right mouse button */
        case 0x10: /* Shift, Ctrl, Alt */
        case 0x11:
        case 0x12:
        case 0x5B: /* Windows keys */
        case 0x5C:
            return 0;
        default:
            return vk <= 0xFE && !(vk >= 0xA0 && vk <= 0xA5); /* left and right Shift, Ctrl, Alt */
    }
}

/* A KEY value: 0 (no key), or a key code above with CMFM_KEY_* modifiers. */
CMFM_CHECK_INLINE int cmfm_check_key(int32_t key) {
    const uint32_t k = (uint32_t)key;
    if (key == 0) {
        return 1;
    }
    return (k & ~(uint32_t)(0xFFu | CMFM_KEY_CTRL | CMFM_KEY_SHIFT | CMFM_KEY_ALT)) == 0 &&
           cmfm_check_key_code(CMFM_KEY_VK(k));
}

CMFM_CHECK_INLINE int cmfm_check_whole_(double v) {
    return isfinite(v) && v == floor(v) && fabs(v) <= 2147483647.0;
}

CMFM_CHECK_INLINE uint32_t cmfm_check_fail_(char* why, uint32_t why_size, const cmfm_setting_t* s, const char* what) {
    if (why != NULL && why_size > 0) {
        if (s != NULL) {
            snprintf(why, why_size, "setting %u: %s", (unsigned)s->id, what);
        } else {
            snprintf(why, why_size, "%s", what);
        }
    }
    return CMFM_ERROR;
}

/* One setting; `size` is the array's stride (settings[0].size). */
CMFM_CHECK_INLINE uint32_t cmfm_check_setting(const cmfm_setting_t* s, uint32_t size, char* why, uint32_t why_size) {
    const uint32_t all_flags =
        CMFM_FLAG_RESTART | CMFM_FLAG_LIST | CMFM_FLAG_ARROWS | CMFM_FLAG_HIDE_LOCKED | CMFM_FLAG_LIVE;
    if (!cmfm_check_text(s->label, CMFM_MAX_LABEL, 0)) {
        return cmfm_check_fail_(why, why_size, s, "label missing or longer than 64 bytes");
    }
    if (!cmfm_check_text(s->help, CMFM_MAX_HELP, 1)) {
        return cmfm_check_fail_(why, why_size, s, "help longer than 200 bytes");
    }
    if ((s->flags & ~all_flags) != 0) {
        return cmfm_check_fail_(why, why_size, s, "unknown flags");
    }
    if ((s->flags & CMFM_FLAG_LIVE) != 0 && s->type != CMFM_TYPE_TEXT) {
        return cmfm_check_fail_(why, why_size, s, "LIVE is for TEXT settings");
    }
    if ((s->flags & (CMFM_FLAG_LIST | CMFM_FLAG_ARROWS)) != 0 && s->type != CMFM_TYPE_CHOICE) {
        return cmfm_check_fail_(why, why_size, s, "LIST and ARROWS are for CHOICE settings");
    }
    if ((s->flags & CMFM_FLAG_LIST) != 0 && (s->flags & CMFM_FLAG_ARROWS) != 0) {
        return cmfm_check_fail_(why, why_size, s, "LIST and ARROWS together");
    }
    switch (s->type) {
        case CMFM_TYPE_BOOL:
            if (s->default_value.i != 0 && s->default_value.i != 1) {
                return cmfm_check_fail_(why, why_size, s, "default is not 0 or 1");
            }
            return CMFM_OK;
        case CMFM_TYPE_CHOICE: {
            uint32_t c;
            if (s->choices == NULL || s->choice_count == 0 || s->choice_count > CMFM_MAX_CHOICES) {
                return cmfm_check_fail_(why, why_size, s, "needs 1 to 32 choices");
            }
            for (c = 0; c < s->choice_count; ++c) {
                if (!cmfm_check_text(s->choices[c], CMFM_MAX_CHOICE_NAME, 0)) {
                    return cmfm_check_fail_(why, why_size, s, "a choice name is missing or longer than 32 bytes");
                }
            }
            if (s->default_value.i < 0 || (uint32_t)s->default_value.i >= s->choice_count) {
                return cmfm_check_fail_(why, why_size, s, "default is not one of the choices");
            }
            return CMFM_OK;
        }
        case CMFM_TYPE_INT:
        case CMFM_TYPE_FLOAT: {
            const int is_int = s->type == CMFM_TYPE_INT;
            const double d = is_int ? (double)s->default_value.i : s->default_value.f;
            if (!isfinite(s->min) || !isfinite(s->max) || !isfinite(s->step) || s->step <= 0 || s->min > s->max) {
                return cmfm_check_fail_(why, why_size, s, "needs min <= max and step > 0");
            }
            if (is_int && (!cmfm_check_whole_(s->min) || !cmfm_check_whole_(s->max) || !cmfm_check_whole_(s->step))) {
                return cmfm_check_fail_(why, why_size, s, "INT needs whole min, max and step");
            }
            if (!is_int && s->decimals > 3) {
                return cmfm_check_fail_(why, why_size, s, "decimals above 3");
            }
            if ((s->max - s->min) / s->step + 1 > CMFM_MAX_STEPS + 0.5) {
                return cmfm_check_fail_(why, why_size, s, "more than 101 values (the slider has positions 0 to 100)");
            }
            if (!cmfm_check_text(s->unit, CMFM_MAX_UNIT, 1)) {
                return cmfm_check_fail_(why, why_size, s, "unit longer than 8 bytes");
            }
            if (!(d >= s->min - 1e-9 && d <= s->max + 1e-9)) {
                return cmfm_check_fail_(why, why_size, s, "default outside min..max");
            }
            return CMFM_OK;
        }
        case CMFM_TYPE_TEXT:
            return CMFM_OK;
        case CMFM_TYPE_SECTION:
            if ((s->flags & ~CMFM_FLAG_HIDE_LOCKED) != 0) {
                return cmfm_check_fail_(why, why_size, s, "a SECTION takes no flags but HIDE_LOCKED");
            }
            return CMFM_OK;
        case CMFM_TYPE_KEY:
            if ((s->flags & ~(CMFM_FLAG_RESTART | CMFM_FLAG_HIDE_LOCKED)) != 0) {
                return cmfm_check_fail_(why, why_size, s, "a KEY takes no flags but RESTART and HIDE_LOCKED");
            }
            if (!cmfm_check_key(s->default_value.i)) {
                return cmfm_check_fail_(why, why_size, s,
                                        "default is not a key (a virtual-key code with CMFM_KEY_* modifiers, or 0)");
            }
            return CMFM_OK;
        case CMFM_TYPE_BUTTON: {
            const int sized = size >= offsetof(cmfm_setting_t, confirm) + sizeof s->confirm;
            if ((s->flags & ~CMFM_FLAG_HIDE_LOCKED) != 0) {
                return cmfm_check_fail_(why, why_size, s, "a BUTTON takes no flags but HIDE_LOCKED");
            }
            if (!cmfm_check_text(sized ? s->button : NULL, CMFM_MAX_BUTTON, 1)) {
                return cmfm_check_fail_(why, why_size, s, "button text longer than 32 bytes");
            }
            if (!cmfm_check_text(sized ? s->confirm : NULL, CMFM_MAX_QUESTION, 1)) {
                return cmfm_check_fail_(why, why_size, s, "confirm question longer than 200 bytes");
            }
            return CMFM_OK;
        }
        default:
            return cmfm_check_fail_(why, why_size, s, "unknown type");
    }
}

/* The setting at `index` of the page's array (stepped by settings[0].size). */
CMFM_CHECK_INLINE const cmfm_setting_t* cmfm_check_at_(const cmfm_page_t* page, uint32_t index) {
    return (const cmfm_setting_t*)((const char*)page->settings + (size_t)index * page->settings[0].size);
}

/* CMFM_OK if CMFM's add_page would accept the page; otherwise CMFM_ERROR and
 * the reason in `why` (`why_size` bytes including the NUL; `why` may be
 * NULL). */
CMFM_CHECK_INLINE uint32_t cmfm_check_page(const cmfm_page_t* page, char* why, uint32_t why_size) {
    uint32_t i, j, stride;
    int any_value = 0, any_text = 0, any_button = 0;
    if (page == NULL || page->size < offsetof(cmfm_page_t, press)) {
        return cmfm_check_fail_(why, why_size, NULL, "no page, or a page smaller than cmfm.menu 1.0's");
    }
    if (!cmfm_check_text(page->plugin, CMFM_MAX_PLUGIN, 0)) {
        return cmfm_check_fail_(why, why_size, NULL, "plugin name missing or longer than 64 bytes");
    }
    if (!cmfm_check_text(page->title, CMFM_MAX_TITLE, 0)) {
        return cmfm_check_fail_(why, why_size, NULL, "title missing or longer than 24 bytes");
    }
    if (!cmfm_check_text(page->help, CMFM_MAX_HELP, 1)) {
        return cmfm_check_fail_(why, why_size, NULL, "page help longer than 200 bytes");
    }
    if (page->settings == NULL || page->setting_count == 0 || page->setting_count > CMFM_MAX_SETTINGS) {
        return cmfm_check_fail_(why, why_size, NULL, "needs 1 to 1024 settings");
    }
    stride = page->settings[0].size;
    if (stride < offsetof(cmfm_setting_t, button)) {
        return cmfm_check_fail_(why, why_size, NULL, "settings smaller than cmfm.menu 1.0's");
    }
    for (i = 0; i < page->setting_count; ++i) {
        const cmfm_setting_t* s = cmfm_check_at_(page, i);
        if (s->size != stride) {
            return cmfm_check_fail_(why, why_size, NULL, "settings of different sizes in one array");
        }
        if (cmfm_check_setting(s, stride, why, why_size) != CMFM_OK) {
            return CMFM_ERROR;
        }
        if (s->id >= CMFM_RESERVED_IDS) {
            return cmfm_check_fail_(why, why_size, s, "ids from 0xFFFFFF00 are CMFM's");
        }
        for (j = 0; j < i; ++j) {
            if (cmfm_check_at_(page, j)->id == s->id) {
                return cmfm_check_fail_(why, why_size, s, "id used twice");
            }
        }
        if (s->type == CMFM_TYPE_TEXT) {
            any_text = 1;
        } else if (s->type == CMFM_TYPE_BUTTON) {
            any_button = 1;
        } else if (s->type != CMFM_TYPE_SECTION) {
            any_value = 1;
        }
    }
    if (!any_value && !any_text && !any_button) {
        return cmfm_check_fail_(why, why_size, NULL, "needs a setting besides its sections");
    }
    if (any_value && (page->get == NULL || page->set == NULL)) {
        return cmfm_check_fail_(why, why_size, NULL, "get and set callbacks needed");
    }
    if (any_text && page->get_text == NULL) {
        return cmfm_check_fail_(why, why_size, NULL, "get_text callback needed for TEXT settings");
    }
    if (CMFM_CHECK_HAS(page, cmfm_page_t, flags) && (page->flags & ~CMFM_PAGE_NO_RESET) != 0) {
        return cmfm_check_fail_(why, why_size, NULL, "unknown page flags");
    }
    if (CMFM_CHECK_HAS(page, cmfm_page_t, mod) && !cmfm_check_text(page->mod, CMFM_MAX_MOD, 1)) {
        return cmfm_check_fail_(why, why_size, NULL, "mod name longer than 24 bytes");
    }
    if (CMFM_CHECK_HAS(page, cmfm_page_t, version) && !cmfm_check_text(page->version, CMFM_MAX_VERSION, 1)) {
        return cmfm_check_fail_(why, why_size, NULL, "version longer than 24 bytes");
    }
    if (any_button && !(CMFM_CHECK_HAS(page, cmfm_page_t, press) && page->press != NULL)) {
        return cmfm_check_fail_(why, why_size, NULL, "press callback needed for BUTTON settings");
    }
    if (why != NULL && why_size > 0) {
        why[0] = '\0';
    }
    return CMFM_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* CMFM_CHECK_H */
