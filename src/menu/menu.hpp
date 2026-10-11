// CMFM's menu core: the pages plugins added through cmfm.menu (validated
// copies), laid out as the game's Options rows (native/options_model.hpp):
// tab 0 is the Overview (CMFM's version and every plugin's state), then one
// tab per plugin page (at least 5 tabs for the in-game page, up to 25 for
// the Mods page's list). Values are never stored here: every read and write
// goes through the page's callbacks. The core remembers only, per setting,
// the value the session started with, to tell when a restart is pending.
// Pure logic, single-threaded (the caller serialises), tested in
// tests/test_menu.cpp.
#pragma once

#include "cmfm/cmfm.h"
#include "native/options_model.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cmfm::menu {

inline constexpr std::size_t kMaxPages = native::kMaxTabs - 1;  // tab 0 is the status page
inline constexpr std::uint32_t kMaxTitle = 24;   // bytes; the tab name's space
inline constexpr std::uint32_t kMaxLabel = 64;
inline constexpr std::uint32_t kMaxHelp = 200;   // the help line's space
inline constexpr std::uint32_t kMaxChoice = 32;
inline constexpr std::uint32_t kMaxUnit = 8;
inline constexpr std::uint32_t kMaxPlugin = 64;
inline constexpr std::uint32_t kMaxVersion = 24;  // the page's version in its header
inline constexpr std::uint32_t kArrowChoices = 5;     // more open the pick list
inline constexpr std::uint32_t kMaxListChoices = 14;  // the pick list's rows (btnlist_select)

struct Setting {
    std::uint32_t id = 0;
    cmfm_type_t type = 0;
    cmfm_flags_t flags = 0;
    std::string label;
    std::string help;
    std::vector<std::string> choices;  // CHOICE names
    std::uint32_t decimals = 0;
    double min = 0;
    double max = 0;
    double step = 1;
    std::string unit;
    cmfm_value_t default_value = {};
    std::uint32_t value_count = 0;  // rows' values: 2 for BOOL, 1 for TEXT and SECTION
    // Session state (CMFM_FLAG_RESTART): the value index first read this
    // session (-1: not read yet), and whether a change away from it waits
    // for a restart.
    std::int32_t first_index = -1;
    bool restart_pending = false;
    // BUTTON (since 1.1): the value box's text and the Yes/No question.
    std::string button;
    std::string confirm;
    // Session state: locked now (the page's `locked`), and why.
    bool locked = false;
    std::string locked_why;
};

struct Page {
    std::string plugin;
    std::string title;
    std::string help;
    std::string version;  // since 1.1; may be empty
    std::vector<Setting> settings;
    void* user = nullptr;
    cmfm_get_fn get = nullptr;
    cmfm_set_fn set = nullptr;
    cmfm_validate_fn validate = nullptr;
    cmfm_get_text_fn get_text = nullptr;
    cmfm_event_fn on_event = nullptr;
    cmfm_press_fn press = nullptr;    // since 1.1
    cmfm_locked_fn locked = nullptr;  // since 1.1
    std::uint32_t flags = 0;          // since 1.2: CMFM_PAGE_...
};

// CMFM's own row at the end of a page (CMFM_PAGE_NO_RESET): a BUTTON that
// resets the page's settings to their defaults.
inline constexpr std::uint32_t kResetId = CMFM_RESERVED_IDS + 1;

// Checks a plugin's page against cmfm.h's rules and copies it; false with
// the reason. Reads only the fields `size` covers.
bool copy_page(const cmfm_page_t* page, Page& out, std::string& why);

// Where the core's log lines go (level 0 info, 1 warning).
using LogFn = void (*)(int level, const char* line);

class Menu {
public:
    explicit Menu(const char* version = "", LogFn log = nullptr) : version_(version ? version : ""), log_(log) {}

    // In the order plugins called add_page.
    void add(Page page);
    void add_refused(std::string plugin, std::string why);
    bool dirty() const { return dirty_; }

    // Lays the pages out into `model` (status page, then up to 24 plugin
    // pages). A page that does not fit is listed as not shown. False (with
    // `why`) only if not even the status page fits; `model` is unusable then.
    bool build(native::OptionsModel& model, std::string& why);
    // Tabs in use: the status page plus the plugin pages shown.
    std::size_t used_tabs() const { return 1 + shown_; }

    // While a Mods page is open, on the game's menu thread:
    void refresh();                      // TEXT rows' texts, help lines as written
    void send(cmfm_event_t event);       // to every shown page
    std::uint32_t get(std::uint32_t setting_id);  // the row's value index
    bool set(std::uint32_t setting_id, std::int32_t index);
    void reset_defaults();               // "Set To Default"

    // Rows by setting id (false for ids that are not a plugin setting).
    bool is_section(std::uint32_t setting_id) const;
    bool restart_pending(std::uint32_t setting_id) const;
    // Tab t (1 + page index) has a change waiting for a restart.
    bool tab_restart_pending(std::size_t tab) const;
    // Settings waiting for a restart, over every shown page.
    std::size_t restart_count() const;
    // Changes whenever a setting's restart state does.
    std::uint32_t restart_revision() const { return restart_revision_; }
    // Number rows shown as a slider: INT and FLOAT settings with at least 3
    // values, not locked. The slider's steps (values - 1), or 0 for any other
    // row.
    std::uint32_t slider_steps(std::uint32_t setting_id) const;
    // Changes whenever CMFM sets a value (the player, Set To Default).
    std::uint32_t value_revision() const { return value_revision_; }
    // CHOICE rows that open the game's pick list: more than kArrowChoices
    // values (or CMFM_FLAG_LIST), at most kMaxListChoices, not
    // CMFM_FLAG_ARROWS. The number of choices, or 0 for any other row.
    std::uint32_t list_choices(std::uint32_t setting_id) const;
    // A number row's value text for value `index` ("50%"), as the row shows
    // it; nullptr for other rows. Valid until the next call.
    const char* value_text(std::uint32_t setting_id, std::uint32_t index);

    // BUTTON rows: press() calls the page's `press` (false if it failed or
    // the row is no button); afterwards texts and locks are read again.
    bool is_button(std::uint32_t setting_id) const;
    // A BUTTON drawn as its label alone, centred and coloured, without a value
    // box (like the game's "Advanced Graphics Settings" row): CMFM's reset row.
    bool is_plain_button(std::uint32_t setting_id) const;
    const char* button_question(std::uint32_t setting_id) const;  // nullptr: none
    bool press(std::uint32_t setting_id);
    // Locks (the pages' `locked`), read when a page opens and after every
    // change CMFM makes; a locked row refuses changes.
    bool is_locked(std::uint32_t setting_id) const;
    // Locked with CMFM_FLAG_HIDE_LOCKED: the row is left out of the panel.
    bool is_hidden(std::uint32_t setting_id) const;
    // TEXT rows with CMFM_FLAG_LIVE: reread_text asks get_text again and says
    // whether the line changed.
    bool is_live(std::uint32_t setting_id) const;
    bool reread_text(std::uint32_t setting_id);
    std::uint32_t lock_revision() const { return lock_revision_; }

    // Tab t's header above its rows: "Overview", or the page's title, its
    // version ("v" added before a leading digit) and "Restart needed" while
    // a change waits for a restart; empty for unused tabs. Valid until the
    // next call.
    const char* tab_header(std::size_t tab);

private:
    struct Entry {
        bool page;          // a page (index into pages_) or a refusal
        std::size_t index;  // pages_ or refused_
    };
    struct Row {
        bool status = false;
        std::int32_t status_page = -1;  // Overview rows: the shown page they describe
        std::size_t page = 0;
        std::size_t setting = 0;
        std::string label;
        std::vector<std::string> values;
        std::vector<const char*> value_ptrs;
        std::string base_help;
        char help[kMaxHelp + 64] = {};
        // TEXT rows' and status rows' value; number rows' current value (all
        // their values share this text, native::RowSpec::shared).
        char text[CMFM_MAX_TEXT] = {};
        std::uint32_t value_count = 0;  // the model row's values (value_ptrs.size() unless shared)
        bool shared = false;
        bool get_failed = false;        // logged once per opening
        bool lock_logged = false;       // a refused change while locked, logged once per opening
    };

    void logf(int level, const char* format, ...) const;
    static void set_help(Row& row, const char* text);
    void compose(std::size_t shown, std::vector<native::RowSpec>& specs);
    // The row of a plugin setting (any type): its index in rows_, or
    // rows_.size() (status rows, placeholders, ids not ours).
    std::size_t row_index(std::uint32_t setting_id) const;
    const Row* any_setting_row(std::uint32_t setting_id) const;
    Row* setting_row(std::uint32_t setting_id);  // nullptr for status rows, placeholders, TEXT, SECTION
    // After a value change: the restart state and the Overview's text.
    void note_value(Row& row, std::uint32_t index);
    void update_status_texts();
    void refresh_locks();  // and the locked rows' help lines
    void refresh_texts();  // TEXT rows
    // One setting back to its default (validate, set, the restart state);
    // false if refused or failed, or locked unless `even_locked`.
    bool reset_setting(Row& row, bool even_locked);
    void reset_page(std::size_t page);  // CMFM's "Reset to defaults" row

    std::string version_;
    LogFn log_;
    std::vector<Page> pages_;
    std::vector<std::pair<std::string, std::string>> refused_;  // plugin, why
    std::vector<Entry> entries_;
    bool dirty_ = true;
    std::uint32_t restart_revision_ = 0;
    std::uint32_t value_revision_ = 0;
    std::uint32_t lock_revision_ = 0;

    // The last build.
    const native::OptionsModel* model_ = nullptr;
    std::vector<Row> rows_;  // parallel to the RowSpecs given to the model
    std::size_t shown_ = 0;
    std::vector<std::string> not_shown_why_;  // per page; empty if shown
    std::size_t tab_count_ = native::kTabCount;
    std::string tab_names_[native::kMaxTabs];
    std::string tab_helps_[native::kMaxTabs];
    std::string header_;  // the last tab_header
    std::string value_text_;  // the last value_text
};

}  // namespace cmfm::menu
