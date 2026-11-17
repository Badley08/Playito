// SPDX-License-Identifier: GPL-3.0-or-later

#include "update/update_manager.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PLAYITO_VERSION
#define PLAYITO_VERSION "0.1.0"
#endif

extern char** environ;

namespace playito {
namespace {

using Json = nlohmann::json;
constexpr char kReleaseApi[] =
    "https://api.github.com/repos/Badley08/Playito/releases/latest";
constexpr char kAppImageAsset[] = "Playito-x86_64.AppImage";
constexpr std::uintmax_t kMaximumAppImageSize =
    static_cast<std::uintmax_t>(1024) * 1024 * 1024;
constexpr std::size_t kMaximumMetadataSize = 2 * 1024 * 1024;

struct MemoryResponse {
    std::string data;
    bool exceeded_limit = false;
};

struct TemporaryFilePath {
    std::filesystem::path path;
    int descriptor = -1;
};

template <typename Value>
void set_curl_option(CURL* handle, CURLoption option, Value value) {
    const auto result = curl_easy_setopt(handle, option, value);
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Configuration HTTPS: ") +
                                 curl_easy_strerror(result));
    }
}

struct FileResponse {
    std::FILE* file = nullptr;
    std::uintmax_t bytes_written = 0;
    std::uintmax_t maximum_size = 0;
};

class TemporaryFile {
public:
    explicit TemporaryFile(std::filesystem::path path) : path_(std::move(path)) {}

    ~TemporaryFile() {
        if (!keep_) {
            std::error_code error;
            std::filesystem::remove(path_, error);
        }
    }

    const std::filesystem::path& path() const {
        return path_;
    }

    void keep() {
        keep_ = true;
    }

private:
    std::filesystem::path path_;
    bool keep_ = false;
};

std::size_t write_metadata(char* data, std::size_t size, std::size_t count,
                           void* context) {
    auto* response = static_cast<MemoryResponse*>(context);
    const auto bytes = size * count;
    if (bytes > kMaximumMetadataSize -
                    std::min(response->data.size(), kMaximumMetadataSize)) {
        response->exceeded_limit = true;
        return 0;
    }
    response->data.append(data, bytes);
    return bytes;
}

std::size_t write_file(char* data, std::size_t size, std::size_t count,
                       void* context) {
    auto* response = static_cast<FileResponse*>(context);
    const auto bytes = static_cast<std::uintmax_t>(size) * count;
    if (bytes > response->maximum_size -
                    std::min(response->bytes_written, response->maximum_size)) {
        return 0;
    }
    const auto written = std::fwrite(data, 1, static_cast<std::size_t>(bytes),
                                     response->file);
    response->bytes_written += written;
    return written;
}

int report_progress(void* context, curl_off_t, curl_off_t, curl_off_t,
                    curl_off_t) {
    const auto* stop_requested = static_cast<const std::atomic<bool>*>(context);
    return stop_requested->load() ? 1 : 0;
}

void configure_curl(CURL* handle, const std::string& url, long timeout_seconds,
                    std::atomic<bool>* stop_requested = nullptr) {
    set_curl_option(handle, CURLOPT_URL, url.c_str());
    set_curl_option(handle, CURLOPT_USERAGENT,
                    "Playito/" PLAYITO_VERSION " (Linux; C++)");
    set_curl_option(handle, CURLOPT_FOLLOWLOCATION, 1L);
    set_curl_option(handle, CURLOPT_MAXREDIRS, 5L);
    set_curl_option(handle, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
    set_curl_option(handle, CURLOPT_REDIR_PROTOCOLS,
                    static_cast<long>(CURLPROTO_HTTPS));
    set_curl_option(handle, CURLOPT_CONNECTTIMEOUT, 15L);
    set_curl_option(handle, CURLOPT_TIMEOUT, timeout_seconds);
    set_curl_option(handle, CURLOPT_FAILONERROR, 1L);
    set_curl_option(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    set_curl_option(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    if (stop_requested) {
        set_curl_option(handle, CURLOPT_NOPROGRESS, 0L);
        set_curl_option(handle, CURLOPT_XFERINFOFUNCTION, report_progress);
        set_curl_option(handle, CURLOPT_XFERINFODATA, stop_requested);
    }
}

void ensure_curl_initialized() {
    static const CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Initialisation HTTPS: ") +
                                 curl_easy_strerror(result));
    }
}

std::array<unsigned int, 3> parse_version(std::string version) {
    if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) {
        version.erase(version.begin());
    }

    std::array<unsigned int, 3> result{};
    std::size_t start = 0;
    for (std::size_t component = 0; component < result.size(); ++component) {
        const auto end = version.find('.', start);
        auto part = version.substr(start, end == std::string::npos
                                              ? std::string::npos
                                              : end - start);
        if (component == 2) {
            const auto suffix = part.find_first_of("-+");
            if (suffix != std::string::npos) {
                part.resize(suffix);
            }
        }
        if (part.empty()) {
            throw std::runtime_error("Version de release invalide.");
        }
        const auto parsed =
            std::from_chars(part.data(), part.data() + part.size(),
                            result[component]);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size()) {
            throw std::runtime_error("Version de release invalide.");
        }
        if (component < 2) {
            if (end == std::string::npos) {
                throw std::runtime_error("Version de release invalide.");
            }
            start = end + 1;
        }
    }
    return result;
}

Json request_release_metadata() {
    ensure_curl_initialized();
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(
        curl_easy_init(), curl_easy_cleanup);
    if (!handle) {
        throw std::runtime_error("Impossible de creer la requete GitHub.");
    }

    MemoryResponse response;
    configure_curl(handle.get(), kReleaseApi, 30L);
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
        curl_slist_append(nullptr, "Accept: application/vnd.github+json"),
        curl_slist_free_all);
    if (!headers) {
        throw std::runtime_error("Impossible de preparer la requete GitHub.");
    }
    set_curl_option(handle.get(), CURLOPT_HTTPHEADER, headers.get());
    set_curl_option(handle.get(), CURLOPT_WRITEFUNCTION, write_metadata);
    set_curl_option(handle.get(), CURLOPT_WRITEDATA, &response);

    const CURLcode result = curl_easy_perform(handle.get());
    long status_code = 0;
    const CURLcode info_result =
        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status_code);
    handle.reset();
    headers.reset();

    if (result != CURLE_OK) {
        if (response.exceeded_limit) {
            throw std::runtime_error("Reponse GitHub trop volumineuse.");
        }
        throw std::runtime_error(std::string("Verification GitHub: ") +
                                 curl_easy_strerror(result));
    }
    if (info_result != CURLE_OK) {
        throw std::runtime_error(std::string("Reponse GitHub: ") +
                                 curl_easy_strerror(info_result));
    }
    if (status_code < 200 || status_code >= 300) {
        throw std::runtime_error("GitHub a renvoye une reponse HTTP invalide.");
    }
    return Json::parse(response.data);
}

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Impossible de verifier le fichier telecharge.");
    }

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) {
        throw std::runtime_error("Impossible d'initialiser SHA-256.");
    }
    const auto cleanup = [&context] { EVP_MD_CTX_free(context); };

    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
        cleanup();
        throw std::runtime_error("Impossible d'initialiser SHA-256.");
    }

    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 &&
            EVP_DigestUpdate(context, buffer.data(),
                             static_cast<std::size_t>(count)) != 1) {
            cleanup();
            throw std::runtime_error("Echec du calcul SHA-256.");
        }
    }
    if (input.bad()) {
        cleanup();
        throw std::runtime_error("Erreur de lecture du fichier telecharge.");
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(context, digest.data(), &digest_size) != 1) {
        cleanup();
        throw std::runtime_error("Echec du calcul SHA-256.");
    }
    cleanup();

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < digest_size; ++index) {
        result << std::setw(2) << static_cast<unsigned int>(digest[index]);
    }
    return result.str();
}

bool is_appimage(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 11> header{};
    input.read(reinterpret_cast<char*>(header.data()),
               static_cast<std::streamsize>(header.size()));
    return input.gcount() == static_cast<std::streamsize>(header.size()) &&
           header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' &&
           header[3] == 'F' && header[8] == 'A' && header[9] == 'I' &&
           header[10] == 2;
}

TemporaryFilePath create_temporary_file(
    const std::filesystem::path& app_image_path) {
    auto pattern = app_image_path.string() + ".update.XXXXXX";
    std::vector<char> writable_pattern(pattern.begin(), pattern.end());
    writable_pattern.push_back('\0');
    const int descriptor = mkstemp(writable_pattern.data());
    if (descriptor < 0) {
        throw std::runtime_error("Impossible de creer le fichier temporaire.");
    }
    return {std::filesystem::path(writable_pattern.data()), descriptor};
}

void download_and_replace(const std::filesystem::path& app_image_path,
                          const std::string& url, const std::string& digest,
                          std::uintmax_t expected_size,
                          std::atomic<bool>& stop_requested) {
    if (expected_size == 0 || expected_size > kMaximumAppImageSize) {
        throw std::runtime_error("Taille de mise a jour invalide.");
    }

    ensure_curl_initialized();
    auto temporary_file = create_temporary_file(app_image_path);
    TemporaryFile temporary(temporary_file.path);
    std::unique_ptr<std::FILE, decltype(&std::fclose)> output(
        ::fdopen(temporary_file.descriptor, "wb"), std::fclose);
    if (!output) {
        ::close(temporary_file.descriptor);
        throw std::runtime_error("Impossible d'ouvrir le fichier temporaire.");
    }

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(
        curl_easy_init(), curl_easy_cleanup);
    if (!handle) {
        throw std::runtime_error("Impossible de creer la requete de telechargement.");
    }

    FileResponse response{output.get(), 0, expected_size};
    configure_curl(handle.get(), url, 600L, &stop_requested);
    set_curl_option(handle.get(), CURLOPT_WRITEFUNCTION, write_file);
    set_curl_option(handle.get(), CURLOPT_WRITEDATA, &response);
    const CURLcode result = curl_easy_perform(handle.get());
    long status_code = 0;
    const CURLcode info_result =
        curl_easy_getinfo(handle.get(), CURLINFO_RESPONSE_CODE, &status_code);
    handle.reset();

    const bool flush_ok = std::fflush(output.get()) == 0;
    const bool sync_ok = flush_ok && ::fsync(::fileno(output.get())) == 0;
    const bool permission_ok =
        sync_ok && ::fchmod(::fileno(output.get()), 0755) == 0;
    const bool close_ok = std::fclose(output.release()) == 0;

    if (result != CURLE_OK) {
        throw std::runtime_error(std::string("Telechargement: ") +
                                 curl_easy_strerror(result));
    }
    if (info_result != CURLE_OK || status_code < 200 || status_code >= 300 ||
        !flush_ok || !sync_ok ||
        !permission_ok || !close_ok || response.bytes_written != expected_size) {
        throw std::runtime_error("Le fichier telecharge est incomplet.");
    }
    if (sha256_file(temporary.path()) != digest) {
        throw std::runtime_error("La verification SHA-256 a echoue.");
    }
    if (!is_appimage(temporary.path())) {
        throw std::runtime_error("Le fichier telecharge n'est pas une AppImage.");
    }
    if (stop_requested.load()) {
        throw std::runtime_error("Mise a jour annulee.");
    }

    std::error_code error;
    std::filesystem::rename(temporary.path(), app_image_path, error);
    if (error) {
        throw std::runtime_error("Impossible d'installer la mise a jour: " +
                                 error.message());
    }
    temporary.keep();

    const int directory =
        ::open(app_image_path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (directory >= 0) {
        ::fsync(directory);
        ::close(directory);
    }
}

}  // namespace

UpdateManager::UpdateManager() {
    const char* app_image = std::getenv("APPIMAGE");
    if (app_image && *app_image) {
        std::error_code error;
        app_image_path_ = std::filesystem::canonical(app_image, error);
        if (error || !std::filesystem::is_regular_file(app_image_path_, error) ||
            error) {
            app_image_path_.clear();
        }
    }
}

UpdateManager::~UpdateManager() {
    shutdown();
}

void UpdateManager::set_wake_callback(WakeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    wake_callback_ = std::move(callback);
}

void UpdateManager::check_for_updates() {
    if (app_image_path_.empty()) {
        publish(UpdateState::Unsupported, "APPIMAGE",
                "Les mises a jour exigent la version AppImage.", false);
        return;
    }
    join_finished_worker();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (busy_) {
            return;
        }
        busy_ = true;
    }
    publish(UpdateState::Checking, "CHECKING", "Recherche de mises a jour...",
            true);
    try {
        worker_ = std::thread(&UpdateManager::check_worker, this);
    } catch (const std::exception& error) {
        publish(UpdateState::Error, "RETRY", error.what(), false);
    }
}

void UpdateManager::download_update() {
    join_finished_worker();
    std::string url;
    std::string digest;
    std::string version;
    std::uintmax_t size = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (busy_ || snapshot_.state != UpdateState::Available) {
            return;
        }
        busy_ = true;
        url = asset_url_;
        digest = asset_digest_;
        version = available_version_;
        size = asset_size_;
    }
    publish(UpdateState::Downloading, "WAIT",
            "Telechargement de la version " + version + "...", true);
    try {
        worker_ = std::thread(&UpdateManager::download_worker, this,
                              std::move(url), std::move(digest), size,
                              std::move(version));
    } catch (const std::exception& error) {
        publish(UpdateState::Error, "RETRY", error.what(), false);
    }
}

bool UpdateManager::restart(const std::vector<std::string>& arguments) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot_.state != UpdateState::Installed ||
            app_image_path_.empty()) {
            return false;
        }
    }

    std::vector<std::string> command_line;
    command_line.reserve(arguments.size() + 1);
    command_line.push_back(app_image_path_.string());
    if (arguments.size() > 1) {
        command_line.insert(command_line.end(), arguments.begin() + 1,
                            arguments.end());
    }
    std::vector<char*> pointers;
    pointers.reserve(command_line.size() + 1);
    for (auto& argument : command_line) {
        pointers.push_back(argument.data());
    }
    pointers.push_back(nullptr);

    pid_t child = 0;
    const int result =
        ::posix_spawn(&child, app_image_path_.c_str(), nullptr, nullptr,
                      pointers.data(), environ);
    if (result != 0) {
        publish(UpdateState::Error, "RETRY",
                std::string("Impossible de relancer Playito: ") +
                    std::strerror(result),
                false);
        return false;
    }
    return true;
}

UpdateSnapshot UpdateManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool UpdateManager::can_update() const {
    return !app_image_path_.empty();
}

void UpdateManager::shutdown() {
    stop_requested_.store(true);
    if (worker_.joinable()) {
        worker_.join();
    }
}

void UpdateManager::check_worker() {
    try {
        const auto release = request_release_metadata();
        const std::string version = release.at("tag_name").get<std::string>();
        const auto latest_version = parse_version(version);
        const auto current_version = parse_version(PLAYITO_VERSION);
        if (latest_version <= current_version) {
            publish(UpdateState::Current, "LATEST",
                    "Playito est deja a jour (" + version + ").", false);
            return;
        }

        const auto& assets = release.at("assets");
        for (const auto& asset : assets) {
            if (asset.value("name", std::string{}) == kAppImageAsset) {
                const std::string digest = asset.value("digest", std::string{});
                const std::string prefix = "sha256:";
                if (digest.compare(0, prefix.size(), prefix) != 0 ||
                    digest.size() != prefix.size() + 64) {
                    throw std::runtime_error(
                        "Cette release ne fournit pas un digest SHA-256 valide.");
                }
                const std::string url =
                    asset.at("browser_download_url").get<std::string>();
                const std::string asset_hash = digest.substr(prefix.size());
                const auto size = asset.at("size").get<std::uintmax_t>();
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    asset_url_ = url;
                    asset_digest_ = asset_hash;
                    asset_size_ = size;
                    available_version_ = version;
                }
                publish(UpdateState::Downloading, "WAIT",
                        "Version " + version + " disponible; telechargement...",
                        true);
                download_and_replace(app_image_path_, url, asset_hash, size,
                                     stop_requested_);
                publish(UpdateState::Installed, "RESTART",
                        "Version " + version +
                            " installee. Redemarrage de Playito...",
                        false);
                return;
            }
        }
        throw std::runtime_error("La release ne contient pas l'AppImage x86_64.");
    } catch (const std::exception& error) {
        publish(UpdateState::Error, "RETRY", error.what(), false);
    }
}

void UpdateManager::download_worker(std::string url, std::string digest,
                                    std::uintmax_t expected_size,
                                    std::string version) {
    try {
        download_and_replace(app_image_path_, url, digest, expected_size,
                             stop_requested_);
        publish(UpdateState::Installed, "RESTART",
                "Version " + version +
                    " installee. Redemarrez Playito pour l'utiliser.",
                false);
    } catch (const std::exception& error) {
        publish(UpdateState::Error, "RETRY", error.what(), false);
    }
}

void UpdateManager::publish(UpdateState state, std::string button_label,
                            std::string message, bool busy) {
    WakeCallback wake;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot_ = {state, std::move(button_label), std::move(message)};
        busy_ = busy;
        wake = wake_callback_;
    }
    if (wake) {
        wake();
    }
}

void UpdateManager::join_finished_worker() {
    bool can_join = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        can_join = !busy_ && worker_.joinable();
    }
    if (can_join) {
        worker_.join();
    }
}

}  // namespace playito
