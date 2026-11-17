// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/file_browser.h"
#include "media/media_player.h"
#include "ui/ui_renderer.h"
#include "update/update_manager.h"

#include <GLFW/glfw3.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace playito {

class Application {
public:
    Application(int argc, char** argv);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

private:
    enum class DragMode {
        None,
        Seek,
        Volume,
    };

    static void on_framebuffer_size(GLFWwindow* window, int width, int height);
    static void on_drop(GLFWwindow* window, int count, const char** paths);
    static void on_key(GLFWwindow* window, int key, int scan_code, int action,
                       int modifiers);
    static void on_mouse_button(GLFWwindow* window, int button, int action,
                                int modifiers);
    static void on_cursor_position(GLFWwindow* window, double x, double y);
    static void on_scroll(GLFWwindow* window, double x_offset, double y_offset);

    void initialize();
    void shutdown();
    void request_redraw();
    void render();
    void open_inputs(const std::vector<std::filesystem::path>& inputs);
    void handle_key(int key);
    void handle_click(double x, double y);
    void handle_drag(double x);
    void handle_browser_entry(std::size_t row);
    void handle_update();
    void set_restart_inputs(
        const std::vector<std::filesystem::path>& files);
    void toggle_fullscreen();
    UiState make_ui_state() const;

    GLFWwindow* window_ = nullptr;
    MediaPlayer media_player_;
    UiRenderer ui_renderer_;
    UpdateManager update_manager_;
    FileBrowser file_browser_;
    std::vector<std::filesystem::path> startup_inputs_;
    std::vector<std::string> startup_arguments_;
    std::atomic<bool> redraw_requested_{false};
    std::string status_;
    DragMode drag_mode_ = DragMode::None;
    bool glfw_initialized_ = false;
    bool fullscreen_ = false;
    bool restart_requested_ = false;
    int windowed_x_ = 0;
    int windowed_y_ = 0;
    int windowed_width_ = 960;
    int windowed_height_ = 540;
};

}  // namespace playito
