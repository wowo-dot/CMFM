#include "native/mod_list.hpp"

#include <utility>

namespace cmfm::native {

void ModList::reset(std::vector<std::size_t> pages) {
    pages_ = std::move(pages);
    open_ = kNone;
    shown_mod_ = 0;
    shown_page_ = 0;
    rebuild();
}

void ModList::rebuild() {
    rows_.clear();
    for (std::size_t m = 0; m < pages_.size(); ++m) {
        rows_.push_back({m, -1});
        if (m == open_ && pages_[m] > 1) {
            for (std::size_t p = 0; p < pages_[m]; ++p) {
                rows_.push_back({m, static_cast<std::int32_t>(p)});
            }
        }
    }
}

std::size_t ModList::row_of(std::size_t mod, std::int32_t page) const {
    for (std::size_t r = 0; r < rows_.size(); ++r) {
        if (rows_[r].mod == mod && rows_[r].page == page) {
            return r;
        }
    }
    return kNone;
}

std::size_t ModList::shown_row() const {
    if (open_ == shown_mod_ && page_count(shown_mod_) > 1) {
        return row_of(shown_mod_, static_cast<std::int32_t>(shown_page_));
    }
    return row_of(shown_mod_, -1);
}

ModList::Result ModList::preview(std::size_t r) {
    if (r >= rows_.size()) {
        return {};
    }
    const ListRow row = rows_[r];
    std::size_t page = 0;
    if (row.page >= 0) {
        page = static_cast<std::size_t>(row.page);
    } else if (row.mod == shown_mod_) {
        return {};  // the shown mod's entry: its shown page stays
    }
    if (shown_mod_ == row.mod && shown_page_ == page) {
        return {};
    }
    shown_mod_ = row.mod;
    shown_page_ = page;
    return {true, false};
}

ModList::Result ModList::activate(std::size_t r) {
    if (r >= rows_.size()) {
        return {};
    }
    const ListRow row = rows_[r];
    Result result = preview(r);
    result.enter = true;
    const std::size_t open = pages_[row.mod] > 1 ? row.mod : kNone;
    if (open_ != open) {
        open_ = open;  // one dropdown at a time: the shown mod's
        rebuild();
    }
    return result;
}

ModList::Result ModList::step_page(int direction) {
    const std::size_t n = page_count(shown_mod_);
    if (n < 2) {
        return {};
    }
    shown_page_ = direction < 0 ? (shown_page_ + n - 1) % n : (shown_page_ + 1) % n;
    if (open_ != shown_mod_) {
        open_ = shown_mod_;
        rebuild();
    }
    return {true, false};
}

bool ModList::select(std::size_t mod, std::size_t page) {
    if (page >= page_count(mod)) {
        return false;
    }
    shown_mod_ = mod;
    shown_page_ = page;
    open_ = pages_[mod] > 1 ? mod : kNone;
    rebuild();
    return true;
}

}  // namespace cmfm::native
