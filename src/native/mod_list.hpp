// The Mods page's mod list (roadmap 1.2 item 7, decisions item 17): one entry
// per mod, flat; a mod's pages are tabs above the panel. As the game's own
// category list: the highlight (the pointer, the arrow keys) shows its mod in
// the panel (preview), and activate (a click, Confirm) goes into the panel.
// Each mod shows the page it showed last (its first page at the start); a tab
// or step_page (Q / E) picks another. The list's rows are the mods, so row r
// is mod r. Pure logic, tested in tests/test_mod_list.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmfm::native {

class ModList {
public:
    // A new menu: each mod's page count (mod 0 the Overview); mod 0 shown.
    // The pages each mod showed last are kept when the mods are the same
    // (the Mods page opening again).
    void reset(std::vector<std::size_t> pages);

    std::size_t mod_count() const { return pages_.size(); }
    std::size_t page_count(std::size_t mod) const { return mod < pages_.size() ? pages_[mod] : 0; }
    std::size_t shown_mod() const { return shown_mod_; }
    std::size_t shown_page() const { return page_of(shown_mod_); }
    // The page mod `mod` shows when it is shown (the one it showed last).
    std::size_t page_of(std::size_t mod) const { return mod < last_page_.size() ? last_page_[mod] : 0; }

    struct Result {
        bool changed = false;  // the panel shows another page
        bool enter = false;    // the panel takes the focus
    };
    // The highlight on mod `mod`: the panel shows it (on its last page).
    Result preview(std::size_t mod);
    // A click or Confirm on mod `mod`: it shows as in preview, and `enter`.
    Result activate(std::size_t mod);
    // A tab: the shown mod's page `page`.
    Result show_page(std::size_t page);
    // Q / E: the shown mod's previous (-1) or next (+1) page, wrapping.
    // Nothing for a mod with one page.
    Result step_page(int direction);
    // Shows (mod, page) directly (a page opening again on what it showed);
    // false (nothing changed) if there is no such page.
    bool select(std::size_t mod, std::size_t page);

private:
    std::vector<std::size_t> pages_;
    std::vector<std::size_t> last_page_;
    std::size_t shown_mod_ = 0;
};

// The tabs' window: the first of `visible` tabs shown so that page `page` of
// `pages` stays in view, moving `top` as little as possible (the mod list's
// rule); 0 when everything fits.
std::size_t tab_window_top(std::size_t top, std::size_t page, std::size_t pages, std::size_t visible);

}  // namespace cmfm::native
