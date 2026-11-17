// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace playito {

enum class UpdateState {
    Idle,
    Unsupported,
    Checking,
    Available,
    Downloading,
    Installed,
    Current,
    Error,
};

struct UpdateSnapshot {
    UpdateState state = UpdateState::Idle;
    std::string button_label = "UPDATE";
    std::string message;
};

class UpdateManager {
public:
    using WakeCallback = std::function<void()>;

    UpdateManager();
    ~UpdateManager();

    UpdateManager(const UpdateManager&) = delete;
    UpdateManager& operator=(const UpdateManager&) = delete;

    void set_wake_callback(WakeCallback callback);
    void check_for_updates();
    void download_update();
    bool can_update() const;
    bool restart(const std::vector<std::string>& arguments);
    UpdateSnapshot snapshot() const;
    void shutdown();

private:
    void check_worker();
    void download_worker(std::string url, std::string digest,
                        std::uintmax_t expected_size,
                        std::string version);
    void publish(UpdateState state, std::string button_label,
                 std::string message, bool busy);
    void join_finished_worker();

    std::filesystem::path app_image_path_;
    mutable std::mutex mutex_;
    WakeCallback wake_callback_;
    UpdateSnapshot snapshot_;
    std::thread worker_;
    std::atomic<bool> stop_requested_{false};
    std::string asset_url_;
    std::string asset_digest_;
    std::string available_version_;
    std::uintmax_t asset_size_ = 0;
    bool busy_ = false;
};

}  // namespace playito
