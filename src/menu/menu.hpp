// CMFM's menu core: the pages plugins added through cmfm.menu (validated
// copies), laid out as the game's Options rows (native/options_model.hpp):
// tab 0 is CMFM's status page, then one tab per plugin page (at least 5 tabs
// for the in-game page, up to 25 for the title page's scrolling list).
// Values are never stored here: every read and write goes through the page's
// callbacks.
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
    std::uint32_t value_count = 0;  // rows' values: 2 for BOOL, 1 for TEXT
};

struct Page {
    std::string plugin;
    std::string title;
    std::string help;
    std::vector<Setting> settings;
    void* user = nullptr;
    cmfm_get_fn get = nullptr;
    cmfm_set_fn set = nullptr;
    cmfm_validate_fn validate = nullptr;
    cmfm_get_text_fn get_text = nullptr;
    cmfm_event_fn on_event = nullptr;
};

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

private:
    struct Entry {
        bool page;          // a page (index into pages_) or a refusal
        std::size_t index;  // pages_ or refused_
    };
    struct Row {
        bool status = false;
        std::size_t page = 0;
        std::size_t setting = 0;
        std::string label;
        std::vector<std::string> values;
        std::vector<const char*> value_ptrs;
        std::string base_help;
        char help[kMaxHelp + 64] = {};
        char text[CMFM_MAX_TEXT] = {};  // TEXT rows' and status rows' value
        bool get_failed = false;        // logged once per opening
    };

    void logf(int level, const char* format, ...) const;
    static void set_help(Row& row, const char* text);
    void compose(std::size_t shown, std::vector<native::RowSpec>& specs);
    Row* setting_row(std::uint32_t setting_id);  // nullptr for status rows, placeholders, TEXT

    std::string version_;
    LogFn log_;
    std::vector<Page> pages_;
    std::vector<std::pair<std::string, std::string>> refused_;  // plugin, why
    std::vector<Entry> entries_;
    bool dirty_ = true;

    // The last build.
    const native::OptionsModel* model_ = nullptr;
    std::vector<Row> rows_;  // parallel to the RowSpecs given to the model
    std::size_t shown_ = 0;
    std::vector<std::string> not_shown_why_;  // per page; empty if shown
    std::size_t tab_count_ = native::kTabCount;
    std::string tab_names_[native::kMaxTabs];
    std::string tab_helps_[native::kMaxTabs];
};

}  // namespace cmfm::menu
