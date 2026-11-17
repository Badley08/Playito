// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/application.h"

#include "media/media_library.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace playito {

Application::Application(int argc, char** argv) {
    startup_inputs_.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    startup_arguments_.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        startup_arguments_.emplace_back(argv[index]);
    }
    for (int index = 1; index < argc; ++index) {
        startup_inputs_.emplace_back(argv[index]);
    }

    try {
        initialize();
    } catch (...) {
        shutdown();
        throw;
    }
}

Application::~Application() {
    shutdown();
}

int Application::run() {
    if (!startup_inputs_.empty()) {
        open_inputs(startup_inputs_);
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwWaitEventsTimeout(0.1);
        media_player_.process_events();
        if (update_manager_.snapshot().state == UpdateState::Installed &&
            !restart_requested_) {
            restart_requested_ = true;
            if (update_manager_.restart(startup_arguments_)) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            } else {
                restart_requested_ = false;
            }
        }
        if (redraw_requested_.exchange(false)) {
            render();
        }
    }
    return 0;
}

void Application::on_framebuffer_size(GLFWwindow* window, int, int) {
    static_cast<Application*>(glfwGetWindowUserPointer(window))->request_redraw();
}

void Application::on_drop(GLFWwindow* window, int count, const char** paths) {
    auto* application =
        static_cast<Application*>(glfwGetWindowUserPointer(window));
    try {
        std::vector<std::filesystem::path> inputs;
        inputs.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            inputs.emplace_back(paths[index]);
        }
        application->open_inputs(inputs);
    } catch (const std::exception& error) {
        application->status_ = error.what();
        application->request_redraw();
    }
}

void Application::on_key(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) {
        return;
    }
    if ((key == GLFW_KEY_F || key == GLFW_KEY_O ||
         key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) &&
        action != GLFW_PRESS) {
        return;
    }
    static_cast<Application*>(glfwGetWindowUserPointer(window))->handle_key(key);
}

void Application::on_mouse_button(GLFWwindow* window, int button, int action,
                                  int) {
    auto* application =
        static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    if (action == GLFW_RELEASE) {
        application->drag_mode_ = DragMode::None;
        return;
    }
    if (action != GLFW_PRESS) {
        return;
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    application->handle_click(x, y);
}

void Application::on_cursor_position(GLFWwindow* window, double x, double y) {
    auto* application =
        static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application->drag_mode_ != DragMode::None) {
        application->handle_drag(x);
    }
}

void Application::on_scroll(GLFWwindow* window, double, double y_offset) {
    auto* application =
        static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (application->file_browser_.is_open()) {
        application->file_browser_.scroll(y_offset > 0.0 ? -1 : 1);
        application->request_redraw();
    }
}

void Application::initialize() {
    if (!glfwInit()) {
        throw std::runtime_error("Impossible d'initialiser GLFW.");
    }
    glfw_initialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(960, 540, "Playito", nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("Impossible de creer la fenetre OpenGL.");
    }
    glfwSetWindowSizeLimits(window_, 720, 480, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, on_framebuffer_size);
    glfwSetDropCallback(window_, on_drop);
    glfwSetKeyCallback(window_, on_key);
    glfwSetMouseButtonCallback(window_, on_mouse_button);
    glfwSetCursorPosCallback(window_, on_cursor_position);
    glfwSetScrollCallback(window_, on_scroll);

    media_player_.initialize([this] {
        redraw_requested_.store(true);
        glfwPostEmptyEvent();
    });
    ui_renderer_.initialize();
    update_manager_.set_wake_callback([this] { request_redraw(); });
    if (update_manager_.can_update()) {
        update_manager_.check_for_updates();
    }
    request_redraw();
}

void Application::shutdown() {
    update_manager_.shutdown();
    update_manager_.set_wake_callback({});
    if (window_) {
        glfwMakeContextCurrent(window_);
    }
    media_player_.shutdown();
    ui_renderer_.shutdown();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfw_initialized_) {
        glfwTerminate();
        glfw_initialized_ = false;
    }
}

void Application::request_redraw() {
    redraw_requested_.store(true);
    glfwPostEmptyEvent();
}

void Application::render() {
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);
    media_player_.render(framebuffer_width, framebuffer_height);
    ui_renderer_.render(make_ui_state(), framebuffer_width, framebuffer_height);
    glfwSwapBuffers(window_);
    media_player_.report_swap();
}

void Application::open_inputs(
    const std::vector<std::filesystem::path>& inputs) {
    const auto files = MediaLibrary::collect(inputs);
    if (files.empty()) {
        status_ = "Aucun fichier audio ou video compatible";
    } else {
        media_player_.load(files);
        set_restart_inputs(files);
        status_.clear();
        file_browser_.close();
    }
    request_redraw();
}

void Application::handle_key(int key) {
    if (file_browser_.is_open()) {
        if (key == GLFW_KEY_ESCAPE) {
            file_browser_.close();
            request_redraw();
        }
        return;
    }

    switch (key) {
        case GLFW_KEY_SPACE:
            media_player_.toggle_pause();
            break;
        case GLFW_KEY_LEFT:
            media_player_.seek(-5.0);
            break;
        case GLFW_KEY_RIGHT:
            media_player_.seek(5.0);
            break;
        case GLFW_KEY_UP:
            media_player_.set_volume(media_player_.volume() + 5.0);
            break;
        case GLFW_KEY_DOWN:
            media_player_.set_volume(media_player_.volume() - 5.0);
            break;
        case GLFW_KEY_F:
            toggle_fullscreen();
            break;
        case GLFW_KEY_O:
            file_browser_.show();
            status_ = file_browser_.error();
            break;
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
            break;
        default:
            return;
    }
    request_redraw();
}

void Application::handle_click(double x, double y) {
    int width = 0;
    int height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetWindowSize(window_, &width, &height);
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);
    static_cast<void>(height);
    static_cast<void>(framebuffer_height);
    if (width <= 0 || height <= 0) {
        return;
    }

    x *= static_cast<double>(framebuffer_width) / width;
    y *= static_cast<double>(framebuffer_height) / height;
    const auto hit = ui_renderer_.hit_test(
        x, y, framebuffer_width, framebuffer_height, file_browser_.is_open());

    switch (hit.action) {
        case UiAction::OpenBrowser:
            file_browser_.show();
            status_ = file_browser_.error();
            break;
        case UiAction::Previous:
            media_player_.previous();
            break;
        case UiAction::TogglePlay:
            media_player_.toggle_pause();
            break;
        case UiAction::Next:
            media_player_.next();
            break;
        case UiAction::ToggleMute:
            media_player_.toggle_mute();
            break;
        case UiAction::ToggleFullscreen:
            toggle_fullscreen();
            break;
        case UiAction::Update:
            handle_update();
            break;
        case UiAction::Seek:
            drag_mode_ = DragMode::Seek;
            handle_drag(x);
            break;
        case UiAction::ChangeVolume:
            drag_mode_ = DragMode::Volume;
            handle_drag(x);
            break;
        case UiAction::CloseBrowser:
            file_browser_.close();
            break;
        case UiAction::BrowserUp:
            file_browser_.go_up();
            status_ = file_browser_.error();
            break;
        case UiAction::BrowserEntry:
            handle_browser_entry(hit.row);
            break;
        case UiAction::None:
            return;
    }
    request_redraw();
}

void Application::handle_drag(double x) {
    int width = 0;
    int height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetWindowSize(window_, &width, &height);
    glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);
    static_cast<void>(height);
    static_cast<void>(framebuffer_height);
    if (width <= 0) {
        return;
    }
    x *= static_cast<double>(framebuffer_width) / width;

    if (drag_mode_ == DragMode::Seek) {
        media_player_.set_position(
            ui_renderer_.seek_value(x, framebuffer_width,
                                    media_player_.duration()));
    } else if (drag_mode_ == DragMode::Volume) {
        media_player_.set_volume(
            ui_renderer_.volume_value(x, framebuffer_width));
    }
    request_redraw();
}

void Application::handle_browser_entry(std::size_t row) {
    auto selected = file_browser_.select_visible(row, 9);
    if (selected) {
        const auto files = MediaLibrary::playlist_from(*selected);
        if (files.empty()) {
            status_ = "Impossible de creer la liste de lecture";
        } else {
            media_player_.load(files);
            set_restart_inputs(files);
            status_.clear();
            file_browser_.close();
        }
    } else {
        status_ = file_browser_.error();
    }
}

void Application::handle_update() {
    const auto state = update_manager_.snapshot().state;
    switch (state) {
        case UpdateState::Idle:
        case UpdateState::Current:
        case UpdateState::Error:
        case UpdateState::Unsupported:
            update_manager_.check_for_updates();
            break;
        case UpdateState::Available:
            update_manager_.download_update();
            break;
        case UpdateState::Installed:
            if (update_manager_.restart(startup_arguments_)) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            break;
        case UpdateState::Checking:
        case UpdateState::Downloading:
            break;
    }
}

void Application::set_restart_inputs(
    const std::vector<std::filesystem::path>& files) {
    if (startup_arguments_.empty()) {
        startup_arguments_.push_back("playito");
    }
    startup_arguments_.resize(1);
    for (const auto& file : files) {
        startup_arguments_.push_back(file.string());
    }
}

void Application::toggle_fullscreen() {
    if (!fullscreen_) {
        glfwGetWindowPos(window_, &windowed_x_, &windowed_y_);
        glfwGetWindowSize(window_, &windowed_width_, &windowed_height_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (monitor && mode) {
            glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width,
                                 mode->height, mode->refreshRate);
            fullscreen_ = true;
        }
    } else {
        glfwSetWindowMonitor(window_, nullptr, windowed_x_, windowed_y_,
                             windowed_width_, windowed_height_,
                             GLFW_DONT_CARE);
        fullscreen_ = false;
    }
}

UiState Application::make_ui_state() const {
    UiState state;
    state.title = media_player_.title();
    state.status = status_;
    state.position = media_player_.position();
    state.duration = media_player_.duration();
    state.volume = media_player_.volume();
    state.paused = media_player_.is_paused();
    state.muted = media_player_.is_muted();
    const auto update = update_manager_.snapshot();
    state.update_label = update.button_label;
    if (state.status.empty()) {
        state.status = update.message;
    }
    state.browser_open = file_browser_.is_open();
    state.browser_path = file_browser_.directory().string();
    state.browser_entries = &file_browser_.entries();
    state.browser_offset = file_browser_.scroll_offset();
    return state;
}

}  // namespace playito
