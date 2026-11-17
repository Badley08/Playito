// SPDX-License-Identifier: GPL-3.0-or-later

#include "media/media_library.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void create_file(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "Could not create test media file");
}

}  // namespace

int main() {
    const auto unique_id = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
    const auto directory = std::filesystem::temp_directory_path() /
                           ("playito-media-test-" + std::to_string(unique_id));

    try {
        std::filesystem::create_directories(directory / "subfolder");
        create_file(directory / "b.MKV");
        create_file(directory / "a.mp4");
        create_file(directory / "notes.txt");
        create_file(directory / "subfolder" / "nested.mp4");

        require(playito::MediaLibrary::is_supported(directory / "clip.MP4"),
                "Media extensions should be matched without case sensitivity");
        require(!playito::MediaLibrary::is_supported(directory / "notes.txt"),
                "Non-media files should not be accepted");

        const auto files = playito::MediaLibrary::collect({directory});
        require(files.size() == 2, "Directory scans should be non-recursive");
        require(files[0].filename() == "a.mp4" &&
                    files[1].filename() == "b.MKV",
                "Folder playback should be sorted by file name");

        const auto playlist =
            playito::MediaLibrary::playlist_from(directory / "b.MKV");
        require(playlist.size() == 2, "Selected files should create a playlist");
        require(playlist.front().filename() == "b.MKV",
                "Playback should begin with the selected file");

        std::string error;
        const auto entries = playito::MediaLibrary::list_directory(directory, error);
        require(error.empty(), "A readable folder should not report an error");
        require(entries.size() == 3 && entries.front().is_directory,
                "The browser should list folders before supported media files");
    } catch (const std::exception& error) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(directory, cleanup_error);
        std::cerr << error.what() << '\n';
        return 1;
    }

    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);
    if (cleanup_error) {
        std::cerr << "Could not remove test directory: "
                  << cleanup_error.message() << '\n';
        return 1;
    }
    return 0;
}
