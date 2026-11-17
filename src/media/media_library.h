// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace playito {

struct BrowserEntry {
    std::filesystem::path path;
    std::string name;
    bool is_directory = false;
};

class MediaLibrary {
public:
    static bool is_supported(const std::filesystem::path& path);
    static std::vector<std::filesystem::path> collect(
        const std::vector<std::filesystem::path>& inputs);
    static std::vector<std::filesystem::path> playlist_from(
        const std::filesystem::path& selected_file);
    static std::vector<BrowserEntry> list_directory(
        const std::filesystem::path& directory, std::string& error);
};

}  // namespace playito
