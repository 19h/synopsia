/// @file qt_imgui_widget.hpp
/// @brief Qt-OpenGL-ImGui integration widget for GPU-accelerated ImGui rendering

#pragma once

#include <imgui.h>

#include <functional>
#include <memory>
#include <string>

// Forward declarations - use C linkage bridge pattern to avoid Qt/IDA header conflicts
extern "C" {
    void* synopsia_imgui_create_widget(
        const char* ini_prefix,
        void (*render_callback)(void* user_data),
        void* user_data
    );
    void synopsia_imgui_destroy_widget(void* widget);
    void synopsia_imgui_set_render_callback(
        void* widget,
        void (*render_callback)(void* user_data),
        void* user_data
    );
}

namespace synopsia {
namespace imgui {

/// @class QtImGuiWidget
/// @brief Wrapper for Qt-OpenGL-ImGui integration widget
///
/// This class wraps a Qt widget that provides GPU-accelerated ImGui rendering.
/// It uses OpenGL for rendering and integrates with Qt's event system.
class QtImGuiWidget {
public:
    using RenderCallback = std::function<void()>;

    /// Create the widget with an INI file prefix for ImGui settings
    explicit QtImGuiWidget(const std::string& ini_prefix);
    ~QtImGuiWidget();

    // Non-copyable
    QtImGuiWidget(const QtImGuiWidget&) = delete;
    QtImGuiWidget& operator=(const QtImGuiWidget&) = delete;

    // Movable
    QtImGuiWidget(QtImGuiWidget&& other) noexcept;
    QtImGuiWidget& operator=(QtImGuiWidget&& other) noexcept;

    /// Set the callback for rendering ImGui content
    void set_render_callback(RenderCallback callback);

    /// Get the underlying Qt widget pointer (for embedding in IDA)
    [[nodiscard]] void* get_widget() const noexcept { return widget_; }

private:
    static void render_thunk(void* user_data);

    void* widget_ = nullptr;
    RenderCallback render_callback_;
};

} // namespace imgui
} // namespace synopsia
