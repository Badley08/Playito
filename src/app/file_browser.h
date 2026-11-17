// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "media/media_library.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace playito {

class FileBrowser {
public:
    void show();
    void close();
    bool is_open() const;
    void go_up();
    void scroll(int rows);
    std::optional<std::filesystem::path> select_visible(std::size_t row,
                                                        std::size_t row_count);

    const std::filesystem::path& directory() const;
    const std::vector<BrowserEntry>& entries() const;
    std::size_t scroll_offset() const;
    const std::string& error() const;

private:
    void reload();

    std::filesystem::path directory_;
    std::vector<BrowserEntry> entries_;
    std::size_t scroll_offset_ = 0;
    bool open_ = false;
    std::string error_;
};

}  // namespace playito
