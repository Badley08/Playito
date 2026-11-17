// SPDX-License-Identifier: GPL-3.0-or-later

#include "media/media_library.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>

namespace playito {
namespace {

constexpr std::array<const char*, 20> kMediaExtensions{
    ".3gp", ".aac", ".avi", ".flac", ".m4a", ".m4v", ".mkv",
    ".mov", ".mp3", ".mp4", ".mpeg", ".mpg", ".ogg", ".opus",
    ".wav", ".webm", ".wma", ".wmv", ".flv", ".ts",
};

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

void append_directory(const std::filesystem::path& directory,
                      std::vector<std::filesystem::path>& result) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    if (error) {
        return;
    }

    std::vector<std::filesystem::path> folder_files;
    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const auto entry = *iterator;
        std::error_code entry_error;
        if (entry.is_regular_file(entry_error) && !entry_error &&
            MediaLibrary::is_supported(entry.path())) {
            folder_files.push_back(entry.path());
        }

        iterator.increment(error);
        if (error) {
            break;
        }
    }
    std::sort(folder_files.begin(), folder_files.end());
    result.insert(result.end(), folder_files.begin(), folder_files.end());
}

}  // namespace

bool MediaLibrary::is_supported(const std::filesystem::path& path) {
    const auto extension = lowercase(path.extension().string());
    return std::any_of(kMediaExtensions.begin(), kMediaExtensions.end(),
                       [&extension](const char* candidate) {
                           return extension == candidate;
                       });
}

std::vector<std::filesystem::path> MediaLibrary::collect(
    const std::vector<std::filesystem::path>& inputs) {
    std::vector<std::filesystem::path> media;
    for (const auto& input : inputs) {
        std::error_code error;
        if (std::filesystem::is_directory(input, error) && !error) {
            append_directory(input, media);
        } else if (!error && std::filesystem::is_regular_file(input, error) &&
                   !error && is_supported(input)) {
            media.push_back(input);
        }
    }
    return media;
}

std::vector<std::filesystem::path> MediaLibrary::playlist_from(
    const std::filesystem::path& selected_file) {
    auto media = collect({selected_file.parent_path()});
    std::sort(media.begin(), media.end());

    const auto selected = std::find(media.begin(), media.end(), selected_file);
    if (selected != media.end()) {
        std::rotate(media.begin(), selected, media.end());
    }
    return media;
}

std::vector<BrowserEntry> MediaLibrary::list_directory(
    const std::filesystem::path& directory, std::string& error_message) {
    std::vector<BrowserEntry> entries;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    if (error) {
        error_message = error.message();
        return entries;
    }

    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const auto entry = *iterator;
        std::error_code entry_error;
        const bool is_directory = entry.is_directory(entry_error);
        if (!entry_error &&
            (is_directory || (entry.is_regular_file(entry_error) &&
                              !entry_error && is_supported(entry.path())))) {
            entries.push_back(
                {entry.path(), entry.path().filename().string(), is_directory});
        }

        iterator.increment(error);
        if (error) {
            error_message = error.message();
            break;
        }
    }

    std::sort(entries.begin(), entries.end(),
              [](const BrowserEntry& left, const BrowserEntry& right) {
                  if (left.is_directory != right.is_directory) {
                      return left.is_directory;
                  }
                  return lowercase(left.name) < lowercase(right.name);
              });
    return entries;
}

}  // namespace playito
