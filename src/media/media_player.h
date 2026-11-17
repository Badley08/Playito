// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mpv/client.h>
#include <mpv/render.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace playito {

class MediaPlayer {
public:
    using WakeCallback = std::function<void()>;

    MediaPlayer() = default;
    ~MediaPlayer();

    MediaPlayer(const MediaPlayer&) = delete;
    MediaPlayer& operator=(const MediaPlayer&) = delete;

    void initialize(WakeCallback wake_callback);
    void shutdown();
    void load(const std::vector<std::filesystem::path>& files);
    void render(int framebuffer_width, int framebuffer_height);
    void report_swap();
    void process_events();

    void toggle_pause();
    void previous();
    void next();
    void seek(double seconds);
    void set_position(double seconds);
    void set_volume(double volume);
    void toggle_mute();

    bool is_paused() const;
    bool is_muted() const;
    double position() const;
    double duration() const;
    double volume() const;
    std::string title() const;

private:
    static void* get_gl_proc_address(void* context, const char* name);
    static void on_render_update(void* context);
    void set_option(const char* name, const char* value);
    void run_command(const std::vector<const char*>& arguments) const;
    double get_number(const char* property) const;
    bool get_flag(const char* property) const;

    mpv_handle* mpv_ = nullptr;
    mpv_render_context* render_context_ = nullptr;
    WakeCallback wake_callback_;
};

}  // namespace playito
