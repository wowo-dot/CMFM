// A description's pages in CMFM's menu (docs/design/config-pages.md): built
// as cmfm.menu pages, as a plugin would add them, whose callbacks read the
// mod's config file when the Mods page opens, keep the player's changes,
// and write them (surgically, verified) when it closes. Pure logic but for
// the file access, which the caller provides. Tested in tests/test_config.cpp.
#pragma once

#include "cmfm/cmfm.h"
#include "config/description.hpp"
#include "config/document.hpp"

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace cmfm::config {

// The game folder's files, by a description's relative path.
class Files {
public:
    virtual ~Files() = default;
    // The file's bytes; false with the reason (for instance, not there).
    virtual bool read(const std::string& relative, std::string& out, std::string& why) = 0;
    // Replaces the file's bytes (safely: a whole new file, a one-time
    // backup); false with the reason.
    virtual bool write(const std::string& relative, const std::string& text, std::string& why) = 0;
};

using LogFn = void (*)(int level, const char* line);  // 0 info, 1 warning

class ConfigPages {
public:
    ConfigPages(Description description, Files& files, LogFn log);
    ConfigPages(const ConfigPages&) = delete;
    ConfigPages& operator=(const ConfigPages&) = delete;

    const Description& description() const { return d_; }

    // At load: the config file reads and parses, holds every `expect` path,
    // and the key defaults are key names. False with the reason (the page is
    // not shown then).
    bool check(std::string& why);

    // The pages to add (valid while this object lives; `user` is this object).
    const std::vector<cmfm_page_t>& pages() const { return pages_; }

    // The Mods page opened / closed (the pages' on_event).
    void opened();
    void closed();
    // While it is open: changes waiting, the last made at least `quiet` ago
    // (a slider dragged writes once, when it stops).
    bool idle_changes(std::chrono::milliseconds quiet) const;
    // Writes the changes now, as closing would, and reads the file again; the
    // page stays open (a crash then loses nothing written).
    void flush();

private:
    struct Row {
        const SettingSpec* spec = nullptr;
        std::uint32_t id = 0;
    };

    void build();
    const Row* row(std::uint32_t id) const;
    const Row* row_by_name(const std::string& id) const;
    // The value's text now: the player's change, else the file's; false with
    // the reason.
    bool current(const Row& r, std::string& text, std::string& why) const;
    bool decode(const Row& r, const std::string& text, cmfm_value_t& out) const;
    bool encode(const Row& r, const cmfm_value_t& value, std::string& out, std::string& why) const;
    // A condition holds: the named setting's value now equals `want`.
    bool holds(const std::string& id, const JsonValue& want) const;
    bool read_file(Document& out, std::string& why) const;
    // The changes into the file (verified, merged with the file as it is now);
    // they are cleared either way (written or logged).
    void write_changes();

    static cmfm_result_t CMFM_CALL get(void* user, std::uint32_t id, cmfm_value_t* value);
    static cmfm_result_t CMFM_CALL set(void* user, std::uint32_t id, const cmfm_value_t* value);
    static cmfm_result_t CMFM_CALL validate(void* user, std::uint32_t id, const cmfm_value_t* value, char* why,
                                            std::uint32_t why_size);
    static void CMFM_CALL get_text(void* user, std::uint32_t id, char* text, std::uint32_t text_size);
    static void CMFM_CALL on_event(void* user, cmfm_event_t event);
    static std::uint32_t CMFM_CALL locked(void* user, std::uint32_t id, char* why, std::uint32_t why_size);

    Description d_;
    Files& files_;
    LogFn log_;
    std::string plugin_;
    std::vector<Row> rows_;  // every setting, ids 1, 2, ... over all pages
    std::vector<std::vector<cmfm_setting_t>> settings_;
    std::vector<std::vector<const char*>> choice_labels_;
    std::vector<cmfm_page_t> pages_;
    // While a Mods page is open: the file as read when it opened, and the
    // player's changes (setting id -> the value's new text).
    Document doc_;
    bool doc_ok_ = false;
    std::string doc_why_;
    std::map<std::uint32_t, std::string> changes_;
    std::chrono::steady_clock::time_point last_change_{};
};

}  // namespace cmfm::config
