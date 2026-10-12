#include "native/config_loader.hpp"

#include "config/config_pages.hpp"
#include "config/description.hpp"
#include "menu/service.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace cmfm::native {

namespace {

const al_api_t* g_api = nullptr;
al_plugin_t* g_self = nullptr;
// The loaded descriptions' pages, kept for the session (their callbacks).
std::vector<std::unique_ptr<config::ConfigPages>> g_loaded;

void log_line(int level, const char* line) {
    g_api->log(g_self, level != 0 ? AL_LOG_WARNING : AL_LOG_INFO, line);
}

std::wstring widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string error_text(DWORD code) {
    return "Windows error " + std::to_string(code);
}

bool read_whole(const std::wstring& path, std::string& out, std::string& why) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        why = e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? "not there" : error_text(e);
        return false;
    }
    LARGE_INTEGER size = {};
    constexpr LONGLONG kMaxSize = 4 * 1024 * 1024;  // a config file, not a data file
    bool ok = GetFileSizeEx(f, &size) != 0 && size.QuadPart <= kMaxSize;
    if (ok) {
        out.assign(static_cast<std::size_t>(size.QuadPart), '\0');
        DWORD got = 0;
        ok = out.empty() || (ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) != 0 &&
                             got == out.size());
    }
    if (!ok) {
        why = size.QuadPart > kMaxSize ? "larger than 4 MB" : error_text(GetLastError());
    }
    CloseHandle(f);
    return ok;
}

// The game folder's files. Writes go to the file's real place (under MO2 the
// mod folder it comes from, found through a handle): a new file next to it,
// a one-time backup of the original, then the new file swapped in.
class WinFiles : public config::Files {
public:
    explicit WinFiles(std::wstring game_folder) : game_(std::move(game_folder)) {}

    bool read(const std::string& relative, std::string& out, std::string& why) override {
        return read_whole(path_of(relative), out, why);
    }

    bool write(const std::string& relative, const std::string& text, std::string& why) override {
        const std::wstring target = path_of(relative);
        HANDLE h = CreateFileW(target.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            why = "cannot open it (" + error_text(GetLastError()) + ")";
            return false;
        }
        wchar_t real[32768];
        const DWORD n = GetFinalPathNameByHandleW(h, real, static_cast<DWORD>(std::size(real)), FILE_NAME_NORMALIZED);
        CloseHandle(h);
        if (n == 0 || n >= std::size(real)) {
            why = "cannot find where it is (" + error_text(GetLastError()) + ")";
            return false;
        }
        const std::wstring file(real, n);
        const std::wstring fresh = file + L".cmfm-new";
        const std::wstring backup = file + L".cmfm-backup";
        HANDLE out = CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) {
            why = "cannot write next to it (" + error_text(GetLastError()) + ")";
            return false;
        }
        DWORD wrote = 0;
        const bool written = WriteFile(out, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr) != 0 &&
                             wrote == text.size() && FlushFileBuffers(out) != 0;
        const DWORD write_error = GetLastError();
        CloseHandle(out);
        if (!written) {
            DeleteFileW(fresh.c_str());
            why = "writing failed (" + error_text(write_error) + ")";
            return false;
        }
        if (GetFileAttributesW(backup.c_str()) == INVALID_FILE_ATTRIBUTES && CopyFileW(file.c_str(), backup.c_str(), TRUE) == 0) {
            DeleteFileW(fresh.c_str());
            why = "no backup could be made (" + error_text(GetLastError()) + ")";
            return false;
        }
        if (MoveFileExW(fresh.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
            const DWORD e = GetLastError();
            DeleteFileW(fresh.c_str());
            why = "the new file could not replace the old one (" + error_text(e) + ")";
            return false;
        }
        return true;
    }

private:
    std::wstring path_of(const std::string& relative) const {
        std::wstring w = widen(relative);
        std::replace(w.begin(), w.end(), L'/', L'\\');
        return game_ + L"\\" + w;
    }

    std::wstring game_;
};

std::unique_ptr<WinFiles> g_files;

std::string config_key(const std::string& file) {
    std::string k = file;
    for (char& c : k) {
        c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return k;
}

}  // namespace

void config_pages_load(const al_api_t* api, al_plugin_t* self, const wchar_t* cmfm_folder, const wchar_t* game_folder) {
    g_api = api;
    g_self = self;
    if (cmfm_folder == nullptr || game_folder == nullptr) {
        log_line(1, "config pages: folders unknown; none read");
        return;
    }
    g_files = std::make_unique<WinFiles>(game_folder);
    const std::wstring dir = std::wstring(cmfm_folder) + L"\\pages\\";
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW found = {};
    HANDLE find = FindFirstFileW((dir + L"*.json").c_str(), &found);
    if (find == INVALID_HANDLE_VALUE) {
        log_line(0, "config pages: none (no pages folder or no .json in it)");
        return;
    }
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            names.emplace_back(found.cFileName);
        }
    } while (FindNextFileW(find, &found) != 0);
    FindClose(find);
    std::sort(names.begin(), names.end());

    // Read and check every description.
    std::vector<config::Description> good;
    for (const std::wstring& name : names) {
        const std::string file = narrow(name);
        const std::string plugin = "pages/" + file;
        std::string text;
        std::string why;
        config::Description d;
        if (!read_whole(dir + name, text, why) || !config::parse_description(text, file, d, why)) {
            menu::service_add_refused(plugin, why);
            log_line(1, (plugin + " refused: " + why).c_str());
            continue;
        }
        good.push_back(std::move(d));
    }
    // One page per config file: the author's, else the first by name.
    std::vector<bool> keep(good.size(), true);
    for (std::size_t i = 0; i < good.size(); ++i) {
        for (std::size_t j = 0; j < good.size(); ++j) {
            if (i == j || !keep[j] || config_key(good[i].config_file) != config_key(good[j].config_file)) {
                continue;
            }
            const bool j_wins = (good[j].author && !good[i].author) || (good[j].author == good[i].author && j < i);
            if (j_wins) {
                keep[i] = false;
                const std::string why = "replaced by pages/" + good[j].file +
                                        (good[j].author ? " (the author's page for the same config file)"
                                                        : " (another page for the same config file)");
                menu::service_add_refused("pages/" + good[i].file, why);
                log_line(0, ("pages/" + good[i].file + ": " + why).c_str());
                break;
            }
        }
    }
    std::size_t added = 0;
    for (std::size_t i = 0; i < good.size(); ++i) {
        if (!keep[i]) {
            continue;
        }
        auto pages = std::make_unique<config::ConfigPages>(std::move(good[i]), *g_files, &log_line);
        const std::string plugin = "pages/" + pages->description().file;
        std::string why;
        if (!pages->check(why)) {
            menu::service_add_refused(plugin, why);
            log_line(1, (plugin + " refused: " + why).c_str());
            continue;
        }
        for (const cmfm_page_t& page : pages->pages()) {
            if (menu::service_add_page(&page, why) == 0) {
                log_line(1, (plugin + ": page \"" + page.title + "\" refused: " + why).c_str());
            }
        }
        log_line(0, (plugin + ": " + std::to_string(pages->pages().size()) + " page(s) for " +
                     pages->description().mod + " (" + pages->description().config_file + ")")
                        .c_str());
        g_loaded.push_back(std::move(pages));
        ++added;
    }
    log_line(0, ("config pages: " + std::to_string(added) + " of " + std::to_string(names.size()) + " file(s) shown").c_str());
}

}  // namespace cmfm::native
