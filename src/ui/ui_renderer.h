// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "media/media_library.h"

#include <cstddef>
#include <string>
#include <vector>

namespace playito {

enum class UiAction {
    None,
    OpenBrowser,
    Previous,
    TogglePlay,
    Next,
    ToggleMute,
    ToggleFullscreen,
    Update,
    Seek,
    ChangeVolume,
    CloseBrowser,
    BrowserUp,
    BrowserEntry,
};

struct UiHit {
    UiAction action = UiAction::None;
    std::size_t row = 0;
};

struct UiState {
    std::string title;
    std::string status;
    std::string browser_path;
    const std::vector<BrowserEntry>* browser_entries = nullptr;
    std::size_t browser_offset = 0;
    double position = 0.0;
    double duration = 0.0;
    double volume = 100.0;
    std::string update_label = "UPDATE";
    bool paused = true;
    bool muted = false;
    bool browser_open = false;
};

class UiRenderer {
public:
    struct Color {
        float r;
        float g;
        float b;
        float a;
    };

    UiRenderer() = default;
    ~UiRenderer();

    UiRenderer(const UiRenderer&) = delete;
    UiRenderer& operator=(const UiRenderer&) = delete;

    void initialize();
    void shutdown();
    void render(const UiState& state, int width, int height);
    UiHit hit_test(double x, double y, int width, int height,
                   bool browser_open) const;
    double seek_value(double x, int width, double duration) const;
    double volume_value(double x, int width) const;

private:
    struct Vertex {
        float x;
        float y;
        float r;
        float g;
        float b;
        float a;
    };

    void draw_rect(float x, float y, float width, float height, Color color);
    void draw_text(float x, float y, const std::string& text, Color color,
                   float scale = 2.0F);
    void draw_button(float x, float y, float width, float height,
                     const std::string& label, bool active = false);
    void flush(int width, int height);
    void render_controls(const UiState& state, int width, int height);
    void render_browser(const UiState& state, int width, int height);
    unsigned int compile_shader(unsigned int type, const char* source);

    unsigned int program_ = 0;
    unsigned int vertex_array_ = 0;
    unsigned int vertex_buffer_ = 0;
    int viewport_uniform_ = -1;
    std::vector<Vertex> vertices_;
};

}  // namespace playito
