// SPDX-License-Identifier: GPL-3.0-or-later
#define GL_GLEXT_PROTOTYPES
#include "ui/ui_renderer.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace playito {
namespace {

constexpr UiRenderer::Color kBackground{0.02745F, 0.02745F, 0.02745F, 0.97F};
constexpr UiRenderer::Color kSurface{0.075F, 0.075F, 0.075F, 0.98F};
constexpr UiRenderer::Color kSurfaceActive{0.12549F, 0.32549F, 0.99216F, 1.0F};
constexpr UiRenderer::Color kAccent{0.12549F, 0.32549F, 0.99216F, 1.0F};
constexpr UiRenderer::Color kText{0.91F, 0.95F, 1.0F, 1.0F};
constexpr UiRenderer::Color kMutedText{0.55F, 0.63F, 0.73F, 1.0F};

constexpr char kVertexShader[] = R"(
#version 330 core
layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
uniform vec2 viewport;
out vec4 vertex_color;
void main() {
    vec2 clip = vec2(position.x / viewport.x * 2.0 - 1.0,
                     1.0 - position.y / viewport.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
    vertex_color = color;
}
)";

constexpr char kFragmentShader[] = R"(
#version 330 core
in vec4 vertex_color;
out vec4 fragment_color;
void main() {
    fragment_color = vertex_color;
}
)";

std::array<std::uint8_t, 7> glyph(char value) {
    const char character =
        static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
    switch (character) {
        case 'A': return {14, 17, 17, 31, 17, 17, 17};
        case 'B': return {30, 17, 17, 30, 17, 17, 30};
        case 'C': return {14, 17, 16, 16, 16, 17, 14};
        case 'D': return {30, 17, 17, 17, 17, 17, 30};
        case 'E': return {31, 16, 16, 30, 16, 16, 31};
        case 'F': return {31, 16, 16, 30, 16, 16, 16};
        case 'G': return {14, 17, 16, 23, 17, 17, 15};
        case 'H': return {17, 17, 17, 31, 17, 17, 17};
        case 'I': return {14, 4, 4, 4, 4, 4, 14};
        case 'J': return {7, 2, 2, 2, 18, 18, 12};
        case 'K': return {17, 18, 20, 24, 20, 18, 17};
        case 'L': return {16, 16, 16, 16, 16, 16, 31};
        case 'M': return {17, 27, 21, 21, 17, 17, 17};
        case 'N': return {17, 25, 21, 19, 17, 17, 17};
        case 'O': return {14, 17, 17, 17, 17, 17, 14};
        case 'P': return {30, 17, 17, 30, 16, 16, 16};
        case 'Q': return {14, 17, 17, 17, 21, 18, 13};
        case 'R': return {30, 17, 17, 30, 20, 18, 17};
        case 'S': return {15, 16, 16, 14, 1, 1, 30};
        case 'T': return {31, 4, 4, 4, 4, 4, 4};
        case 'U': return {17, 17, 17, 17, 17, 17, 14};
        case 'V': return {17, 17, 17, 17, 17, 10, 4};
        case 'W': return {17, 17, 17, 21, 21, 21, 10};
        case 'X': return {17, 17, 10, 4, 10, 17, 17};
        case 'Y': return {17, 17, 10, 4, 4, 4, 4};
        case 'Z': return {31, 1, 2, 4, 8, 16, 31};
        case '0': return {14, 17, 19, 21, 25, 17, 14};
        case '1': return {4, 12, 4, 4, 4, 4, 14};
        case '2': return {14, 17, 1, 2, 4, 8, 31};
        case '3': return {30, 1, 1, 14, 1, 1, 30};
        case '4': return {2, 6, 10, 18, 31, 2, 2};
        case '5': return {31, 16, 16, 30, 1, 1, 30};
        case '6': return {14, 16, 16, 30, 17, 17, 14};
        case '7': return {31, 1, 2, 4, 8, 8, 8};
        case '8': return {14, 17, 17, 14, 17, 17, 14};
        case '9': return {14, 17, 17, 15, 1, 1, 14};
        case ':': return {0, 4, 4, 0, 4, 4, 0};
        case '.': return {0, 0, 0, 0, 0, 6, 6};
        case '/': return {1, 2, 2, 4, 8, 8, 16};
        case '-': return {0, 0, 0, 31, 0, 0, 0};
        case '_': return {0, 0, 0, 0, 0, 0, 31};
        case '+': return {0, 4, 4, 31, 4, 4, 0};
        case '?': return {14, 17, 1, 2, 4, 0, 4};
        case ' ': return {0, 0, 0, 0, 0, 0, 0};
        default: return {31, 17, 21, 21, 21, 17, 31};
    }
}

std::string format_time(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    seconds = std::min(seconds,
                       static_cast<double>(std::numeric_limits<int>::max()));
    const auto total_seconds = static_cast<int>(seconds);
    char buffer[24]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total_seconds / 60,
                  total_seconds % 60);
    return buffer;
}

float clamp_x(double x, float left, float width) {
    if (width <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(static_cast<float>((x - left) / width), 0.0F, 1.0F);
}

bool inside(double x, double y, float left, float top, float width,
            float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

}  // namespace

UiRenderer::~UiRenderer() {
    shutdown();
}

void UiRenderer::initialize() {
    const auto vertex_shader = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    unsigned int fragment_shader = 0;
    try {
        fragment_shader = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    } catch (...) {
        glDeleteShader(vertex_shader);
        throw;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vertex_shader);
    glAttachShader(program_, fragment_shader);
    glLinkProgram(program_);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint log_size = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_size);
        std::string log(static_cast<std::size_t>(std::max(1, log_size)), '\0');
        GLsizei written = 0;
        glGetProgramInfoLog(program_, log_size, &written, log.data());
        glDeleteProgram(program_);
        program_ = 0;
        throw std::runtime_error("Impossible de lier le rendu OpenGL: " + log);
    }

    viewport_uniform_ = glGetUniformLocation(program_, "viewport");
    glGenVertexArrays(1, &vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void UiRenderer::shutdown() {
    if (vertex_buffer_) {
        glDeleteBuffers(1, &vertex_buffer_);
        vertex_buffer_ = 0;
    }
    if (vertex_array_) {
        glDeleteVertexArrays(1, &vertex_array_);
        vertex_array_ = 0;
    }
    if (program_) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void UiRenderer::render(const UiState& state, int width, int height) {
    if (!program_ || width <= 0 || height <= 0) {
        return;
    }
    vertices_.clear();
    render_controls(state, width, height);
    if (state.browser_open) {
        render_browser(state, width, height);
    }
    flush(width, height);
}

UiHit UiRenderer::hit_test(double x, double y, int width, int height,
                           bool browser_open) const {
    if (browser_open) {
        const float panel_width = std::min(640.0F, width - 32.0F);
        const float panel_height = std::min(440.0F, height - 32.0F);
        const float panel_x = (width - panel_width) * 0.5F;
        const float panel_y = (height - panel_height) * 0.5F;
        if (inside(x, y, panel_x + panel_width - 58.0F, panel_y + 12.0F,
                   42.0F, 32.0F)) {
            return {UiAction::CloseBrowser, 0};
        }
        if (inside(x, y, panel_x + 18.0F, panel_y + 62.0F, 76.0F, 30.0F)) {
            return {UiAction::BrowserUp, 0};
        }

        const float row_y = panel_y + 104.0F;
        const float row_height = 34.0F;
        const auto row = static_cast<int>((y - row_y) / row_height);
        const int row_count =
            std::max(0, static_cast<int>((panel_height - 128.0F) / row_height));
        if (row >= 0 && row < row_count &&
            inside(x, y, panel_x + 18.0F, row_y + row * row_height,
                   panel_width - 36.0F, row_height)) {
            return {UiAction::BrowserEntry, static_cast<std::size_t>(row)};
        }
        return {};
    }

    const float button_y = height - 72.0F;
    if (inside(x, y, 24.0F, button_y, 78.0F, 40.0F)) {
        return {UiAction::OpenBrowser, 0};
    }
    if (inside(x, y, 110.0F, button_y, 58.0F, 40.0F)) {
        return {UiAction::Previous, 0};
    }
    if (inside(x, y, 176.0F, button_y, 82.0F, 40.0F)) {
        return {UiAction::TogglePlay, 0};
    }
    if (inside(x, y, 266.0F, button_y, 58.0F, 40.0F)) {
        return {UiAction::Next, 0};
    }
    if (inside(x, y, width - 110.0F, button_y, 86.0F, 40.0F)) {
        return {UiAction::ToggleMute, 0};
    }
    if (inside(x, y, width - 100.0F, 14.0F, 76.0F, 34.0F)) {
        return {UiAction::ToggleFullscreen, 0};
    }
    if (inside(x, y, width - 224.0F, 14.0F, 110.0F, 34.0F)) {
        return {UiAction::Update, 0};
    }
    if (inside(x, y, width - 280.0F, button_y, 110.0F, 40.0F)) {
        return {UiAction::ChangeVolume, 0};
    }
    if (inside(x, y, 24.0F, height - 98.0F, width - 48.0F, 8.0F)) {
        return {UiAction::Seek, 0};
    }
    return {};
}

double UiRenderer::seek_value(double x, int width, double duration) const {
    const float track_left = 24.0F;
    const float track_width = width - 48.0F;
    return clamp_x(x, track_left, track_width) * std::max(0.0, duration);
}

double UiRenderer::volume_value(double x, int width) const {
    const float track_left = width - 280.0F;
    const float track_width = 110.0F;
    return clamp_x(x, track_left, track_width) * 100.0;
}

void UiRenderer::draw_rect(float x, float y, float width, float height,
                           Color color) {
    const Vertex top_left{x, y, color.r, color.g, color.b, color.a};
    const Vertex top_right{x + width, y, color.r, color.g, color.b, color.a};
    const Vertex bottom_right{x + width, y + height, color.r, color.g, color.b,
                              color.a};
    const Vertex bottom_left{x, y + height, color.r, color.g, color.b, color.a};
    vertices_.insert(vertices_.end(),
                     {top_left, top_right, bottom_right, top_left, bottom_right,
                      bottom_left});
}

void UiRenderer::draw_text(float x, float y, const std::string& text,
                           Color color, float scale) {
    const float advance = 6.0F * scale;
    float cursor_x = x;
    for (const char character : text) {
        const auto rows = glyph(character);
        for (std::size_t row = 0; row < rows.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1U << (4 - column))) != 0) {
                    draw_rect(cursor_x + column * scale, y + row * scale, scale,
                              scale, color);
                }
            }
        }
        cursor_x += advance;
    }
}

void UiRenderer::draw_button(float x, float y, float width, float height,
                             const std::string& label, bool active) {
    draw_rect(x, y, width, height, active ? kSurfaceActive : kSurface);
    const float text_width = static_cast<float>(label.size()) * 12.0F;
    draw_text(x + (width - text_width) * 0.5F, y + (height - 14.0F) * 0.5F,
              label, kText);
}

void UiRenderer::flush(int width, int height) {
    if (vertices_.empty()) {
        return;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(viewport_uniform_, static_cast<float>(width),
                static_cast<float>(height));
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                 vertices_.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void UiRenderer::render_controls(const UiState& state, int width, int height) {
    draw_rect(0.0F, 0.0F, static_cast<float>(width), 60.0F, kBackground);
    draw_text(24.0F, 22.0F, "PLAYITO", kAccent, 2.0F);

    std::string title = state.title.empty() ? "DROP MEDIA OR OPEN A FILE"
                                            : state.title;
    const auto title_limit =
        static_cast<std::size_t>(std::max(8, (width - 300) / 12));
    if (title.size() > title_limit) {
        title = title.substr(0, title_limit - 3) + "...";
    }
    draw_text(150.0F, 24.0F, title, kText, 1.5F);
    draw_button(static_cast<float>(width - 224), 14.0F, 110.0F, 34.0F,
                state.update_label);
    draw_button(static_cast<float>(width - 100), 14.0F, 76.0F, 34.0F, "FULL");

    if (!state.status.empty()) {
        const auto status_limit =
            static_cast<std::size_t>(std::max(8, (width - 48) / 9));
        const std::string status =
            state.status.size() > status_limit
                ? state.status.substr(0, status_limit - 3) + "..."
                : state.status;
        draw_text(24.0F, 78.0F, status, kMutedText, 1.5F);
    } else if (state.title.empty()) {
        draw_text(static_cast<float>(width / 2 - 132), static_cast<float>(height / 2),
                  "DROP VIDEO OR AUDIO HERE", kMutedText, 2.0F);
    }

    const float panel_top = static_cast<float>(height - 112);
    draw_rect(0.0F, panel_top, static_cast<float>(width), 112.0F, kBackground);

    const float progress_width = static_cast<float>(width - 48);
    const float progress_y = static_cast<float>(height - 96);
    draw_rect(24.0F, progress_y, progress_width, 4.0F, kSurface);
    const double progress = state.duration > 0.0
                                ? std::clamp(state.position / state.duration, 0.0,
                                             1.0)
                                : 0.0;
    draw_rect(24.0F, progress_y, progress_width * static_cast<float>(progress),
              4.0F, kAccent);
    draw_rect(20.0F + progress_width * static_cast<float>(progress), progress_y - 4.0F,
              8.0F, 12.0F, kText);

    const float button_y = static_cast<float>(height - 72);
    draw_button(24.0F, button_y, 78.0F, 40.0F, "OPEN");
    draw_button(110.0F, button_y, 58.0F, 40.0F, "PREV");
    draw_button(176.0F, button_y, 82.0F, 40.0F,
                state.paused || state.title.empty() ? "PLAY" : "PAUSE", true);
    draw_button(266.0F, button_y, 58.0F, 40.0F, "NEXT");

    draw_text(336.0F, button_y + 14.0F,
              format_time(state.position) + "/" + format_time(state.duration),
              kMutedText, 1.5F);
    draw_text(static_cast<float>(width - 280), button_y + 4.0F,
              std::to_string(static_cast<int>(std::clamp(state.volume, 0.0, 100.0))),
              kMutedText, 1.5F);
    draw_rect(static_cast<float>(width - 280), button_y + 27.0F, 110.0F, 4.0F,
              kSurface);
    draw_rect(static_cast<float>(width - 280), button_y + 27.0F,
              110.0F * static_cast<float>(std::clamp(state.volume / 100.0, 0.0, 1.0)),
              4.0F, kAccent);
    draw_button(static_cast<float>(width - 110), button_y, 86.0F, 40.0F,
                state.muted ? "UNMUTE" : "MUTE");
}

void UiRenderer::render_browser(const UiState& state, int width, int height) {
    draw_rect(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height),
              {0.0F, 0.0F, 0.0F, 0.72F});

    const float panel_width = std::min(640.0F, width - 32.0F);
    const float panel_height = std::min(440.0F, height - 32.0F);
    const float panel_x = (width - panel_width) * 0.5F;
    const float panel_y = (height - panel_height) * 0.5F;
    draw_rect(panel_x, panel_y, panel_width, panel_height, kBackground);
    draw_text(panel_x + 20.0F, panel_y + 20.0F, "OPEN MEDIA", kAccent);
    draw_button(panel_x + panel_width - 58.0F, panel_y + 12.0F, 42.0F, 32.0F,
                "X");
    draw_button(panel_x + 18.0F, panel_y + 62.0F, 76.0F, 30.0F, "UP");

    std::string folder = state.browser_path;
    const auto max_chars =
        static_cast<std::size_t>(std::max(8.0F, (panel_width - 130.0F) / 9.0F));
    if (folder.size() > max_chars) {
        folder = "..." + folder.substr(folder.size() - max_chars + 3);
    }
    draw_text(panel_x + 108.0F, panel_y + 72.0F, folder, kMutedText, 1.5F);

    const float rows_top = panel_y + 104.0F;
    const float row_height = 34.0F;
    const auto visible_count = static_cast<std::size_t>(
        std::max(0.0F, std::floor((panel_height - 128.0F) / row_height)));
    const std::vector<BrowserEntry> empty_entries;
    const auto& entries =
        state.browser_entries ? *state.browser_entries : empty_entries;
    for (std::size_t row = 0; row < visible_count; ++row) {
        const auto index = state.browser_offset + row;
        if (index >= entries.size()) {
            break;
        }
        const auto& entry = entries[index];
        const float y = rows_top + static_cast<float>(row) * row_height;
        draw_rect(panel_x + 18.0F, y, panel_width - 36.0F, 30.0F, kSurface);
        const std::string prefix = entry.is_directory ? "+ " : "  ";
        std::string label = prefix + entry.name;
        const auto row_limit =
            static_cast<std::size_t>(std::max(8.0F, (panel_width - 76.0F) / 12.0F));
        if (label.size() > row_limit) {
            label = label.substr(0, row_limit - 3) + "...";
        }
        draw_text(panel_x + 30.0F, y + 8.0F, label,
                  entry.is_directory ? kAccent : kText, 1.5F);
    }

    if (entries.empty()) {
        draw_text(panel_x + 30.0F, rows_top + 12.0F, "NO MEDIA FILES", kMutedText,
                  1.5F);
    }
}

unsigned int UiRenderer::compile_shader(unsigned int type, const char* source) {
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint log_size = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
        std::string log(static_cast<std::size_t>(std::max(1, log_size)), '\0');
        GLsizei written = 0;
        glGetShaderInfoLog(shader, log_size, &written, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Impossible de compiler un shader OpenGL: " +
                                 log);
    }
    return shader;
}

}  // namespace playito
