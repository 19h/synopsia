/// @file qt_imgui_widget.cpp
/// @brief Qt-OpenGL-ImGui integration implementation

// Qt headers - NO IDA headers in this file
#include <QWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QFocusEvent>

// ImGui
#include <imgui.h>
#include <imgui_impl_opengl3.h>

// OpenGL
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <cstdio>

namespace {

// =============================================================================
// Qt Key to ImGui Key Conversion
// =============================================================================

ImGuiKey qt_key_to_imgui(int key) {
    switch (key) {
        case Qt::Key_Tab: return ImGuiKey_Tab;
        case Qt::Key_Left: return ImGuiKey_LeftArrow;
        case Qt::Key_Right: return ImGuiKey_RightArrow;
        case Qt::Key_Up: return ImGuiKey_UpArrow;
        case Qt::Key_Down: return ImGuiKey_DownArrow;
        case Qt::Key_PageUp: return ImGuiKey_PageUp;
        case Qt::Key_PageDown: return ImGuiKey_PageDown;
        case Qt::Key_Home: return ImGuiKey_Home;
        case Qt::Key_End: return ImGuiKey_End;
        case Qt::Key_Insert: return ImGuiKey_Insert;
        case Qt::Key_Delete: return ImGuiKey_Delete;
        case Qt::Key_Backspace: return ImGuiKey_Backspace;
        case Qt::Key_Space: return ImGuiKey_Space;
        case Qt::Key_Return: return ImGuiKey_Enter;
        case Qt::Key_Enter: return ImGuiKey_KeypadEnter;
        case Qt::Key_Escape: return ImGuiKey_Escape;
        case Qt::Key_A: return ImGuiKey_A;
        case Qt::Key_C: return ImGuiKey_C;
        case Qt::Key_V: return ImGuiKey_V;
        case Qt::Key_X: return ImGuiKey_X;
        case Qt::Key_Y: return ImGuiKey_Y;
        case Qt::Key_Z: return ImGuiKey_Z;
        case Qt::Key_Control: return ImGuiKey_LeftCtrl;
        case Qt::Key_Shift: return ImGuiKey_LeftShift;
        case Qt::Key_Alt: return ImGuiKey_LeftAlt;
        case Qt::Key_Super_L: return ImGuiKey_LeftSuper;
        case Qt::Key_Super_R: return ImGuiKey_RightSuper;
        case Qt::Key_Menu: return ImGuiKey_Menu;
        case Qt::Key_F1: return ImGuiKey_F1;
        case Qt::Key_F2: return ImGuiKey_F2;
        case Qt::Key_F3: return ImGuiKey_F3;
        case Qt::Key_F4: return ImGuiKey_F4;
        case Qt::Key_F5: return ImGuiKey_F5;
        case Qt::Key_F6: return ImGuiKey_F6;
        case Qt::Key_F7: return ImGuiKey_F7;
        case Qt::Key_F8: return ImGuiKey_F8;
        case Qt::Key_F9: return ImGuiKey_F9;
        case Qt::Key_F10: return ImGuiKey_F10;
        case Qt::Key_F11: return ImGuiKey_F11;
        case Qt::Key_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

int qt_mouse_button_to_imgui(Qt::MouseButton button) {
    switch (button) {
        case Qt::LeftButton: return 0;
        case Qt::RightButton: return 1;
        case Qt::MiddleButton: return 2;
        default: return -1;
    }
}

// =============================================================================
// ImGuiGLWindow - QWindow with OpenGL surface that forwards events
// =============================================================================

class ImGuiGLWindow : public QWindow {
public:
    using EventHandler = std::function<void(QEvent*)>;

    explicit ImGuiGLWindow(QWindow* parent = nullptr)
        : QWindow(parent)
    {
        setSurfaceType(QSurface::OpenGLSurface);
    }

    void setEventHandler(EventHandler handler) {
        event_handler_ = std::move(handler);
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::mousePressEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::mouseReleaseEvent(e);
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::mouseMoveEvent(e);
    }

    void wheelEvent(QWheelEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::wheelEvent(e);
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::keyPressEvent(e);
    }

    void keyReleaseEvent(QKeyEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::keyReleaseEvent(e);
    }

    void focusInEvent(QFocusEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::focusInEvent(e);
    }

    void focusOutEvent(QFocusEvent* e) override {
        if (event_handler_) event_handler_(e);
        QWindow::focusOutEvent(e);
    }

private:
    EventHandler event_handler_;
};

// =============================================================================
// ImGuiOpenGLWidget - Main widget that manages ImGui context and rendering
// =============================================================================

class ImGuiOpenGLWidget : public QWidget {
public:
    using RenderCallback = void(*)(void*);

    explicit ImGuiOpenGLWidget(const char* ini_prefix, QWidget* parent = nullptr)
        : QWidget(parent)
        , ini_filename_(ini_prefix ? std::string(ini_prefix) + ".ini" : "imgui.ini")
    {
        // Create GL window
        gl_window_ = new ImGuiGLWindow();
        gl_window_->setEventHandler([this](QEvent* e) { handleEvent(e); });

        // Set up OpenGL format
        QSurfaceFormat fmt;
        fmt.setMajorVersion(3);
        fmt.setMinorVersion(3);
        fmt.setProfile(QSurfaceFormat::CoreProfile);
        gl_window_->setFormat(fmt);

        // Wrap window in container widget
        QWidget* container = QWidget::createWindowContainer(gl_window_, this);
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(container);

        // Create OpenGL context
        context_ = new QOpenGLContext();
        context_->setFormat(fmt);
        if (!context_->create()) {
            fprintf(stderr, "[Synopsia] Failed to create OpenGL context\n");
        }

        // Create ImGui context
        imgui_context_ = ImGui::CreateContext();
        ImGui::SetCurrentContext(imgui_context_);

        // Configure ImGui IO
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = ini_filename_.c_str();

        // Get DPI scale factor
        dpr_ = static_cast<float>(devicePixelRatio());

        // Set framebuffer scale for HiDPI
        io.DisplayFramebufferScale = ImVec2(dpr_, dpr_);

        // Load font at scaled size for crisp HiDPI rendering
        // Default ImGui font is 13px, scale it up
        float font_size = 14.0f * dpr_;
        ImFontConfig font_config;
        font_config.OversampleH = 2;
        font_config.OversampleV = 2;
        font_config.PixelSnapH = true;

        // Use default font but at higher resolution
        io.Fonts->AddFontDefault(&font_config);

        // Build font atlas at scaled size
        io.Fonts->Clear();
        font_config.SizePixels = font_size;
        io.Fonts->AddFontDefault(&font_config);
        io.Fonts->Build();

        // Scale back down so UI elements remain the same logical size
        io.FontGlobalScale = 1.0f / dpr_;

        // Start render timer (60 FPS)
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &ImGuiOpenGLWidget::render);
        timer_->start(16);
    }

    ~ImGuiOpenGLWidget() override {
        if (imgui_context_) {
            ImGui::SetCurrentContext(imgui_context_);
            if (renderer_initialized_) {
                ImGui_ImplOpenGL3_Shutdown();
            }
            ImGui::DestroyContext(imgui_context_);
        }
    }

    void setRenderCallback(RenderCallback callback, void* user_data) {
        render_callback_ = callback;
        render_user_data_ = user_data;
    }

private:
    void handleEvent(QEvent* event) {
        ImGui::SetCurrentContext(imgui_context_);
        ImGuiIO& io = ImGui::GetIO();

        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                auto* e = static_cast<QMouseEvent*>(event);
                int button = qt_mouse_button_to_imgui(e->button());
                if (button >= 0) {
                    io.AddMouseButtonEvent(button, true);
                }
                break;
            }
            case QEvent::MouseButtonRelease: {
                auto* e = static_cast<QMouseEvent*>(event);
                int button = qt_mouse_button_to_imgui(e->button());
                if (button >= 0) {
                    io.AddMouseButtonEvent(button, false);
                }
                break;
            }
            case QEvent::MouseMove: {
                auto* e = static_cast<QMouseEvent*>(event);
                QPointF pos = e->position();
                io.AddMousePosEvent(static_cast<float>(pos.x()), static_cast<float>(pos.y()));
                break;
            }
            case QEvent::Wheel: {
                auto* e = static_cast<QWheelEvent*>(event);
                io.AddMouseWheelEvent(0.0f, e->angleDelta().y() / 120.0f);
                break;
            }
            case QEvent::KeyPress: {
                auto* e = static_cast<QKeyEvent*>(event);

                // Update modifier states
                Qt::KeyboardModifiers mods = e->modifiers();
                io.AddKeyEvent(ImGuiMod_Ctrl, (mods & Qt::ControlModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Shift, (mods & Qt::ShiftModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Alt, (mods & Qt::AltModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Super, (mods & Qt::MetaModifier) != 0);

                ImGuiKey key = qt_key_to_imgui(e->key());
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, true);
                }
                QString text = e->text();
                if (!text.isEmpty()) {
                    for (QChar c : text) {
                        io.AddInputCharacter(c.unicode());
                    }
                }
                break;
            }
            case QEvent::KeyRelease: {
                auto* e = static_cast<QKeyEvent*>(event);

                // Update modifier states
                Qt::KeyboardModifiers mods = e->modifiers();
                io.AddKeyEvent(ImGuiMod_Ctrl, (mods & Qt::ControlModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Shift, (mods & Qt::ShiftModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Alt, (mods & Qt::AltModifier) != 0);
                io.AddKeyEvent(ImGuiMod_Super, (mods & Qt::MetaModifier) != 0);

                ImGuiKey key = qt_key_to_imgui(e->key());
                if (key != ImGuiKey_None) {
                    io.AddKeyEvent(key, false);
                }
                break;
            }
            case QEvent::FocusIn:
                io.AddFocusEvent(true);
                break;
            case QEvent::FocusOut:
                io.AddFocusEvent(false);
                break;
            default:
                break;
        }
    }

    void render() {
        if (!gl_window_->isExposed()) return;
        if (!context_->makeCurrent(gl_window_)) return;

        // Initialize OpenGL backend on first render
        if (!renderer_initialized_) {
            ImGui::SetCurrentContext(imgui_context_);
            ImGui_ImplOpenGL3_Init("#version 330 core");
            renderer_initialized_ = true;
        }

        ImGui::SetCurrentContext(imgui_context_);

        // Get logical size and compute physical size for HiDPI
        QSize logical_size = gl_window_->size();
        int physical_width = static_cast<int>(logical_size.width() * dpr_);
        int physical_height = static_cast<int>(logical_size.height() * dpr_);

        // Set up viewport with physical pixels
        glViewport(0, 0, physical_width, physical_height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Set display size in logical pixels (ImGui handles the scaling via DisplayFramebufferScale)
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(static_cast<float>(logical_size.width()), static_cast<float>(logical_size.height()));

        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();

        // Call user render callback
        if (render_callback_) {
            render_callback_(render_user_data_);
        }

        // Render
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        context_->swapBuffers(gl_window_);
    }

    ImGuiGLWindow* gl_window_ = nullptr;
    QOpenGLContext* context_ = nullptr;
    QTimer* timer_ = nullptr;
    ImGuiContext* imgui_context_ = nullptr;
    bool renderer_initialized_ = false;
    std::string ini_filename_;
    float dpr_ = 1.0f;

    RenderCallback render_callback_ = nullptr;
    void* render_user_data_ = nullptr;
};

} // anonymous namespace

// =============================================================================
// C Linkage Bridge Functions
// =============================================================================

extern "C" {

void* synopsia_imgui_create_widget(
    const char* ini_prefix,
    void (*render_callback)(void* user_data),
    void* user_data)
{
    auto* widget = new ImGuiOpenGLWidget(ini_prefix);
    if (render_callback) {
        widget->setRenderCallback(render_callback, user_data);
    }
    return widget;
}

void synopsia_imgui_destroy_widget(void* widget) {
    delete static_cast<ImGuiOpenGLWidget*>(widget);
}

void synopsia_imgui_set_render_callback(
    void* widget,
    void (*render_callback)(void* user_data),
    void* user_data)
{
    static_cast<ImGuiOpenGLWidget*>(widget)->setRenderCallback(render_callback, user_data);
}

void synopsia_add_widget_to_layout(void* parent, void* child) {
    auto* parent_widget = static_cast<QWidget*>(parent);
    auto* child_widget = static_cast<QWidget*>(child);

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent_widget->layout());
    if (!layout) {
        layout = new QVBoxLayout(parent_widget);
        layout->setContentsMargins(0, 0, 0, 0);
    }
    layout->addWidget(child_widget);
}

} // extern "C"

// =============================================================================
// QtImGuiWidget Wrapper Implementation
// =============================================================================

#include <synopsia/imgui/qt_imgui_widget.hpp>

namespace synopsia {
namespace imgui {

QtImGuiWidget::QtImGuiWidget(const std::string& ini_prefix) {
    widget_ = synopsia_imgui_create_widget(
        ini_prefix.c_str(),
        &QtImGuiWidget::render_thunk,
        this
    );
}

QtImGuiWidget::~QtImGuiWidget() {
    if (widget_) {
        synopsia_imgui_destroy_widget(widget_);
    }
}

QtImGuiWidget::QtImGuiWidget(QtImGuiWidget&& other) noexcept
    : widget_(other.widget_)
    , render_callback_(std::move(other.render_callback_))
{
    other.widget_ = nullptr;
    if (widget_) {
        synopsia_imgui_set_render_callback(widget_, &QtImGuiWidget::render_thunk, this);
    }
}

QtImGuiWidget& QtImGuiWidget::operator=(QtImGuiWidget&& other) noexcept {
    if (this != &other) {
        if (widget_) {
            synopsia_imgui_destroy_widget(widget_);
        }
        widget_ = other.widget_;
        render_callback_ = std::move(other.render_callback_);
        other.widget_ = nullptr;
        if (widget_) {
            synopsia_imgui_set_render_callback(widget_, &QtImGuiWidget::render_thunk, this);
        }
    }
    return *this;
}

void QtImGuiWidget::set_render_callback(RenderCallback callback) {
    render_callback_ = std::move(callback);
}

void QtImGuiWidget::render_thunk(void* user_data) {
    auto* self = static_cast<QtImGuiWidget*>(user_data);
    if (self->render_callback_) {
        self->render_callback_();
    }
}

} // namespace imgui
} // namespace synopsia
