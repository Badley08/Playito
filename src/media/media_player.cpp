// SPDX-License-Identifier: GPL-3.0-or-later

#include "media/media_player.h"

#include <GLFW/glfw3.h>
#include <mpv/render_gl.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace playito {

MediaPlayer::~MediaPlayer() {
    shutdown();
}

void MediaPlayer::initialize(WakeCallback wake_callback) {
    wake_callback_ = std::move(wake_callback);
    mpv_ = mpv_create();
    if (!mpv_) {
        throw std::runtime_error("Impossible de creer le lecteur mpv.");
    }

    try {
        set_option("vo", "libmpv");
        set_option("gpu-api", "opengl");
        set_option("hwdec", "auto-safe");
        set_option("vd-lavc-threads", "2");
        set_option("osc", "no");
        set_option("msg-level", "all=warn");

        const int initialize_result = mpv_initialize(mpv_);
        if (initialize_result < 0) {
            throw std::runtime_error(std::string("Initialisation mpv: ") +
                                     mpv_error_string(initialize_result));
        }

        mpv_opengl_init_params gl_init{
            get_gl_proc_address,
            nullptr,
        };
        mpv_render_param render_params[] = {
            {MPV_RENDER_PARAM_API_TYPE,
             const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        const int render_result =
            mpv_render_context_create(&render_context_, mpv_, render_params);
        if (render_result < 0) {
            throw std::runtime_error(std::string("Rendu mpv: ") +
                                     mpv_error_string(render_result));
        }

        mpv_render_context_set_update_callback(render_context_,
                                               on_render_update, this);
    } catch (...) {
        shutdown();
        throw;
    }
}

void MediaPlayer::shutdown() {
    if (render_context_) {
        mpv_render_context_set_update_callback(render_context_, nullptr,
                                               nullptr);
        mpv_render_context_free(render_context_);
        render_context_ = nullptr;
    }
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
    wake_callback_ = {};
}

void MediaPlayer::load(const std::vector<std::filesystem::path>& files) {
    for (std::size_t index = 0; index < files.size(); ++index) {
        const std::string path = files[index].string();
        const char* arguments[] = {
            "loadfile",
            path.c_str(),
            index == 0 ? "replace" : "append-play",
            nullptr,
        };
        const int result = mpv_command(mpv_, arguments);
        if (result < 0) {
            std::cerr << "Impossible de lire " << path << ": "
                      << mpv_error_string(result) << '\n';
        }
    }
}

void MediaPlayer::render(int framebuffer_width, int framebuffer_height) {
    if (!render_context_ || framebuffer_width <= 0 || framebuffer_height <= 0) {
        return;
    }

    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.02745F, 0.02745F, 0.02745F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    mpv_opengl_fbo framebuffer{0, framebuffer_width, framebuffer_height, 0};
    int flip_y = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(render_context_, params);
}

void MediaPlayer::report_swap() {
    if (render_context_) {
        mpv_render_context_report_swap(render_context_);
    }
}

void MediaPlayer::process_events() {
    if (!mpv_) {
        return;
    }
    while (mpv_wait_event(mpv_, 0.0)->event_id != MPV_EVENT_NONE) {
    }
}

void MediaPlayer::toggle_pause() {
    run_command({"cycle", "pause"});
}

void MediaPlayer::previous() {
    run_command({"playlist-prev", "weak"});
}

void MediaPlayer::next() {
    run_command({"playlist-next", "weak"});
}

void MediaPlayer::seek(double seconds) {
    const std::string value = std::to_string(seconds);
    run_command({"seek", value.c_str(), "relative"});
}

void MediaPlayer::set_position(double seconds) {
    const std::string value = std::to_string(seconds);
    run_command({"seek", value.c_str(), "absolute"});
}

void MediaPlayer::set_volume(double volume_value) {
    double clamped = volume_value < 0.0
                         ? 0.0
                         : volume_value > 100.0 ? 100.0 : volume_value;
    if (mpv_) {
        const int result =
            mpv_set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &clamped);
        if (result < 0) {
            std::cerr << "Impossible de regler le volume: "
                      << mpv_error_string(result) << '\n';
        }
    }
}

void MediaPlayer::toggle_mute() {
    run_command({"cycle", "mute"});
}

bool MediaPlayer::is_paused() const {
    return get_flag("pause");
}

bool MediaPlayer::is_muted() const {
    return get_flag("mute");
}

double MediaPlayer::position() const {
    return get_number("time-pos");
}

double MediaPlayer::duration() const {
    return get_number("duration");
}

double MediaPlayer::volume() const {
    return get_number("volume");
}

std::string MediaPlayer::title() const {
    if (!mpv_) {
        return {};
    }
    char* value = nullptr;
    if (mpv_get_property(mpv_, "media-title", MPV_FORMAT_STRING, &value) < 0 ||
        !value) {
        return {};
    }
    std::string result(value);
    mpv_free(value);
    return result;
}

void* MediaPlayer::get_gl_proc_address(void*, const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

void MediaPlayer::on_render_update(void* context) {
    auto* player = static_cast<MediaPlayer*>(context);
    if (player->wake_callback_) {
        player->wake_callback_();
    }
}

void MediaPlayer::set_option(const char* name, const char* value) {
    const int result = mpv_set_option_string(mpv_, name, value);
    if (result < 0) {
        throw std::runtime_error(std::string("Option mpv ") + name + ": " +
                                 mpv_error_string(result));
    }
}

void MediaPlayer::run_command(
    const std::vector<const char*>& arguments) const {
    if (!mpv_) {
        return;
    }
    std::vector<const char*> command_arguments = arguments;
    command_arguments.push_back(nullptr);
    const int result = mpv_command(mpv_, command_arguments.data());
    if (result < 0) {
        std::cerr << "Commande mpv impossible: " << mpv_error_string(result)
                  << '\n';
    }
}

double MediaPlayer::get_number(const char* property) const {
    double value = 0.0;
    if (mpv_) {
        mpv_get_property(mpv_, property, MPV_FORMAT_DOUBLE, &value);
    }
    return value;
}

bool MediaPlayer::get_flag(const char* property) const {
    int value = 0;
    if (mpv_) {
        mpv_get_property(mpv_, property, MPV_FORMAT_FLAG, &value);
    }
    return value != 0;
}

}  // namespace playito
