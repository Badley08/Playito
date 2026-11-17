// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/file_browser.h"

#include <algorithm>
#include <system_error>

namespace playito {

void FileBrowser::show() {
    std::error_code error;
    directory_ = std::filesystem::current_path(error);
    if (error) {
        directory_ = std::filesystem::path(".");
    }
    open_ = true;
    scroll_offset_ = 0;
    reload();
}

void FileBrowser::close() {
    open_ = false;
}

bool FileBrowser::is_open() const {
    return open_;
}

void FileBrowser::go_up() {
    const auto parent = directory_.parent_path();
    if (!parent.empty() && parent != directory_) {
        directory_ = parent;
        scroll_offset_ = 0;
        reload();
    }
}

void FileBrowser::scroll(int rows) {
    if (rows < 0) {
        const auto amount = static_cast<std::size_t>(-rows);
        scroll_offset_ = amount > scroll_offset_ ? 0 : scroll_offset_ - amount;
        return;
    }
    const auto max_offset =
        entries_.size() > 9 ? entries_.size() - 9 : static_cast<std::size_t>(0);
    scroll_offset_ =
        std::min(scroll_offset_ + static_cast<std::size_t>(rows), max_offset);
}

std::optional<std::filesystem::path> FileBrowser::select_visible(
    std::size_t row, std::size_t row_count) {
    const auto index = scroll_offset_ + row;
    if (row >= row_count || index >= entries_.size()) {
        return std::nullopt;
    }

    const auto& entry = entries_[index];
    if (entry.is_directory) {
        directory_ = entry.path;
        scroll_offset_ = 0;
        reload();
        return std::nullopt;
    }
    return entry.path;
}

const std::filesystem::path& FileBrowser::directory() const {
    return directory_;
}

const std::vector<BrowserEntry>& FileBrowser::entries() const {
    return entries_;
}

std::size_t FileBrowser::scroll_offset() const {
    return scroll_offset_;
}

const std::string& FileBrowser::error() const {
    return error_;
}

void FileBrowser::reload() {
    error_.clear();
    entries_ = MediaLibrary::list_directory(directory_, error_);
    if (scroll_offset_ >= entries_.size()) {
        scroll_offset_ = entries_.size() > 9 ? entries_.size() - 9 : 0;
    }
}

}  // namespace playito
