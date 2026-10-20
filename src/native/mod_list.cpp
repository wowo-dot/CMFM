#include "native/mod_list.hpp"

#include <utility>

namespace cmfm::native {

void ModList::reset(std::vector<std::size_t> pages) {
    if (pages != pages_) {
        last_page_.assign(pages.size(), 0);
    }
    pages_ = std::move(pages);
    shown_mod_ = 0;
}

ModList::Result ModList::preview(std::size_t mod) {
    if (mod >= pages_.size() || mod == shown_mod_) {
        return {};
    }
    shown_mod_ = mod;
    return {true, false};
}

ModList::Result ModList::activate(std::size_t mod) {
    if (mod >= pages_.size()) {
        return {};
    }
    Result result = preview(mod);
    result.enter = true;
    return result;
}

ModList::Result ModList::show_page(std::size_t page) {
    if (page >= page_count(shown_mod_) || page == last_page_[shown_mod_]) {
        return {};
    }
    last_page_[shown_mod_] = page;
    return {true, false};
}

ModList::Result ModList::step_page(int direction) {
    const std::size_t n = page_count(shown_mod_);
    if (n < 2) {
        return {};
    }
    const std::size_t page = last_page_[shown_mod_];
    return show_page(direction < 0 ? (page + n - 1) % n : (page + 1) % n);
}

bool ModList::select(std::size_t mod, std::size_t page) {
    if (page >= page_count(mod)) {
        return false;
    }
    shown_mod_ = mod;
    last_page_[mod] = page;
    return true;
}

std::size_t tab_window_top(std::size_t top, std::size_t page, std::size_t pages, std::size_t visible) {
    if (visible == 0 || pages <= visible) {
        return 0;
    }
    if (page < top) {
        top = page;
    } else if (page >= top + visible) {
        top = page - visible + 1;
    }
    return top > pages - visible ? pages - visible : top;
}

}  // namespace cmfm::native
