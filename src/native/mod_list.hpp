// The Mods page's mod list (roadmap 1.2 item 7): one entry per mod; a mod
// with several pages opens a dropdown of them under its entry, every page
// listed, the first the default. As the game's own category list: the
// highlight (the pointer, the arrow keys) shows its row in the panel
// (preview), and activate (a click, Confirm) goes into the panel. Only
// activate and step_page (Q / E) open or fold a dropdown, so no row moves
// under the pointer while it only moves; one dropdown open at a time, the one
// of the mod last activated or stepped. Pure logic, tested in
// tests/test_mod_list.cpp.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cmfm::native {

struct ListRow {
    std::size_t mod;
    std::int32_t page;  // -1: the mod's own entry; else its page (0: the default)
};

class ModList {
public:
    static constexpr std::size_t kNone = static_cast<std::size_t>(-1);

    // A new menu: each mod's page count (mod 0 the Overview); nothing open,
    // mod 0's default page shown.
    void reset(std::vector<std::size_t> pages);

    const std::vector<ListRow>& rows() const { return rows_; }
    std::size_t mod_count() const { return pages_.size(); }
    std::size_t page_count(std::size_t mod) const { return mod < pages_.size() ? pages_[mod] : 0; }
    std::size_t open() const { return open_; }
    std::size_t shown_mod() const { return shown_mod_; }
    std::size_t shown_page() const { return shown_page_; }
    // The row that marks what the panel shows: the shown page's own row
    // while its mod's dropdown is open, else the mod's entry.
    std::size_t shown_row() const;
    // The row of (mod, page) (page -1: the mod's entry), or kNone if it is
    // not listed (a folded dropdown).
    std::size_t row_of(std::size_t mod, std::int32_t page) const;

    struct Result {
        bool changed = false;  // the panel shows another page
        bool enter = false;    // the panel takes the focus
    };
    // The highlight on row `r`: the panel shows that row's page (a mod's
    // entry: its default page, or the page it shows already); no dropdown
    // opens or folds.
    Result preview(std::size_t r);
    // A click or Confirm on row `r`: it shows as in preview, and `enter`. A
    // mod with several pages gets its dropdown open; any other open dropdown
    // folds (a page of the open dropdown keeps it).
    Result activate(std::size_t r);
    // Q / E: the shown mod's previous (-1) or next (+1) page, wrapping; its
    // dropdown opens. Nothing for a mod with one page.
    Result step_page(int direction);
    // Shows (mod, page) directly, its dropdown open (a page opening again on
    // what it showed); false (nothing changed) if there is no such page.
    bool select(std::size_t mod, std::size_t page);

private:
    void rebuild();

    std::vector<std::size_t> pages_;
    std::vector<ListRow> rows_;
    std::size_t open_ = kNone;
    std::size_t shown_mod_ = 0;
    std::size_t shown_page_ = 0;
};

}  // namespace cmfm::native
