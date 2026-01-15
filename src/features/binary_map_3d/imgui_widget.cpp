/// @file imgui_widget.cpp
/// @brief ImGui-based 3D binary map visualization

#include <synopsia/features/binary_map_3d/map_data.hpp>
#include <synopsia/imgui/qt_imgui_widget.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <memory>
#include <string>

namespace synopsia {
namespace features {
namespace binary_map_3d {

// =============================================================================
// 3D Math Utilities
// =============================================================================

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }

    Vec3 normalized() const {
        float len = length();
        if (len < 0.0001f) return {0, 0, 1};
        return {x / len, y / len, z / len};
    }

    Vec3 cross(const Vec3& v) const {
        return {
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }
};

struct Camera {
    Vec3 position{0, 0, 5};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 1, 0};

    float fov = 60.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    // Orbit parameters
    float distance = 5.0f;
    float yaw = 0.0f;      // Rotation around Y axis
    float pitch = 0.3f;    // Rotation around X axis

    void update_from_orbit() {
        float cos_pitch = std::cos(pitch);
        float sin_pitch = std::sin(pitch);
        float cos_yaw = std::cos(yaw);
        float sin_yaw = std::sin(yaw);

        position.x = target.x + distance * cos_pitch * sin_yaw;
        position.y = target.y + distance * sin_pitch;
        position.z = target.z + distance * cos_pitch * cos_yaw;
    }

    // Project 3D point to screen coordinates
    ImVec2 project(const Vec3& point, const ImVec2& screen_size) const {
        // View direction
        Vec3 forward = (target - position).normalized();
        Vec3 right = forward.cross(up).normalized();
        Vec3 cam_up = right.cross(forward).normalized();

        // Transform to camera space
        Vec3 p = point - position;
        float x = p.dot(right);
        float y = p.dot(cam_up);
        float z = p.dot(forward);

        // Perspective projection
        if (z <= near_plane) {
            return {-10000, -10000};  // Behind camera
        }

        float fov_rad = fov * 3.14159f / 180.0f;
        float scale = 1.0f / std::tan(fov_rad * 0.5f);
        float aspect = screen_size.x / screen_size.y;

        float px = (x * scale / z / aspect + 1.0f) * 0.5f * screen_size.x;
        float py = (-y * scale / z + 1.0f) * 0.5f * screen_size.y;

        return {px, py};
    }

    // Get depth for sorting
    float get_depth(const Vec3& point) const {
        Vec3 forward = (target - position).normalized();
        Vec3 p = point - position;
        return p.dot(forward);
    }
};

// =============================================================================
// 3D Binary Map State
// =============================================================================

class BinaryMap3DState {
public:
    BinaryMap3DState() = default;

    void refresh_data() {
        data_.refresh();
    }

    void render() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 display_size = io.DisplaySize;

        // Set window to fullscreen with no decorations
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display_size);

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("BinaryMap3DWindow", nullptr, window_flags);

        // Layout: info panel on left, 3D view on right
        if (ImGui::BeginTable("##main-layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 200.0f);
            ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextColumn();
            render_info_panel();

            ImGui::TableNextColumn();
            render_3d_view();

            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    void render_info_panel() {
        ImGui::Text("3D Binary Map");
        ImGui::Separator();

        if (!data_.is_valid()) {
            ImGui::TextDisabled("No data loaded");
            return;
        }

        ImGui::Text("Functions: %zu", data_.nodes().size());
        ImGui::Text("Call edges: %zu", data_.edges().size());
        ImGui::Text("Max depth: %u", data_.max_depth());

        ImGui::Separator();

        ImGui::Text("Controls:");
        ImGui::BulletText("Drag: Rotate");
        ImGui::BulletText("Scroll: Zoom");
        ImGui::BulletText("Shift+Drag: Pan");

        ImGui::Separator();

        // Display options
        ImGui::Text("Display:");
        ImGui::Checkbox("Show edges", &show_edges_);
        ImGui::Checkbox("Show labels", &show_labels_);
        ImGui::SliderFloat("Point size", &point_size_, 2.0f, 20.0f);
        ImGui::SliderFloat("Z scale", &z_scale_, 0.1f, 5.0f);

        ImGui::Separator();

        // Selected function info
        if (selected_node_) {
            ImGui::Text("Selected:");
            ImGui::Text("  %s", selected_node_->name.c_str());
            ImGui::Text("  Addr: %llX", static_cast<unsigned long long>(selected_node_->address));
            ImGui::Text("  Size: %u bytes", selected_node_->size);
            ImGui::Text("  Depth: %u", selected_node_->call_depth);
            ImGui::Text("  Calls: %u", selected_node_->callee_count);
            ImGui::Text("  Called by: %u", selected_node_->caller_count);
        } else if (hovered_node_) {
            ImGui::Text("Hover:");
            ImGui::Text("  %s", hovered_node_->name.c_str());
            ImGui::Text("  Depth: %u", hovered_node_->call_depth);
        }
    }

    void render_3d_view() {
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();

        if (canvas_size.x < 50 || canvas_size.y < 50) return;

        // Create invisible button for input handling
        ImGui::InvisibleButton("##3d-canvas", canvas_size,
                               ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);

        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        // Handle input
        handle_input(is_hovered, is_active, canvas_pos, canvas_size);

        // Update camera
        camera_.update_from_orbit();

        // Get draw list
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Draw background
        draw_list->AddRectFilled(canvas_pos,
                                 ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                                 IM_COL32(20, 20, 30, 255));

        if (!data_.is_valid()) {
            draw_list->AddText(
                ImVec2(canvas_pos.x + canvas_size.x * 0.5f - 50,
                       canvas_pos.y + canvas_size.y * 0.5f),
                IM_COL32(128, 128, 128, 255),
                "No data");
            return;
        }

        // Draw coordinate axes
        draw_axes(draw_list, canvas_pos, canvas_size);

        // Draw edges first (behind nodes)
        if (show_edges_) {
            draw_edges(draw_list, canvas_pos, canvas_size);
        }

        // Draw nodes sorted by depth (back to front)
        draw_nodes(draw_list, canvas_pos, canvas_size);
    }

    void handle_input(bool is_hovered, bool is_active,
                      const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        ImGuiIO& io = ImGui::GetIO();

        // Rotation (left mouse drag)
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (io.KeyShift) {
                // Pan
                float pan_speed = 0.01f * camera_.distance;
                camera_.target.x -= io.MouseDelta.x * pan_speed;
                camera_.target.y += io.MouseDelta.y * pan_speed;
            } else {
                // Rotate
                camera_.yaw -= io.MouseDelta.x * 0.01f;
                camera_.pitch += io.MouseDelta.y * 0.01f;
                camera_.pitch = std::clamp(camera_.pitch, -1.5f, 1.5f);
            }
        }

        // Zoom (scroll)
        if (is_hovered && std::abs(io.MouseWheel) > 0.01f) {
            camera_.distance *= (1.0f - io.MouseWheel * 0.1f);
            camera_.distance = std::clamp(camera_.distance, 1.0f, 50.0f);
        }

        // Hover detection
        hovered_node_ = nullptr;
        if (is_hovered && !is_active) {
            ImVec2 mouse_pos = io.MousePos;
            float best_dist_sq = 15.0f * 15.0f;  // Max hover distance

            for (const auto& node : data_.nodes()) {
                Vec3 pos = get_node_position(node);
                ImVec2 screen_pos = camera_.project(pos, canvas_size);
                screen_pos.x += canvas_pos.x;
                screen_pos.y += canvas_pos.y;

                float dx = mouse_pos.x - screen_pos.x;
                float dy = mouse_pos.y - screen_pos.y;
                float dist_sq = dx * dx + dy * dy;

                if (dist_sq < best_dist_sq) {
                    best_dist_sq = dist_sq;
                    hovered_node_ = &node;
                }
            }
        }

        // Selection (click)
        if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyShift) {
            selected_node_ = hovered_node_;
        }
    }

    Vec3 get_node_position(const FunctionNode& node) const {
        return Vec3(node.x * 2.0f, node.y * 2.0f, node.z * z_scale_ * 2.0f - z_scale_);
    }

    void draw_axes(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        // Draw X, Y, Z axes
        Vec3 origin(0, 0, 0);
        Vec3 x_end(1, 0, 0);
        Vec3 y_end(0, 1, 0);
        Vec3 z_end(0, 0, 1);

        ImVec2 origin_screen = camera_.project(origin, canvas_size);
        origin_screen.x += canvas_pos.x;
        origin_screen.y += canvas_pos.y;

        // X axis (red)
        ImVec2 x_screen = camera_.project(x_end, canvas_size);
        x_screen.x += canvas_pos.x;
        x_screen.y += canvas_pos.y;
        draw_list->AddLine(origin_screen, x_screen, IM_COL32(255, 100, 100, 128), 1.5f);

        // Y axis (green)
        ImVec2 y_screen = camera_.project(y_end, canvas_size);
        y_screen.x += canvas_pos.x;
        y_screen.y += canvas_pos.y;
        draw_list->AddLine(origin_screen, y_screen, IM_COL32(100, 255, 100, 128), 1.5f);

        // Z axis (blue) - call depth
        ImVec2 z_screen = camera_.project(z_end, canvas_size);
        z_screen.x += canvas_pos.x;
        z_screen.y += canvas_pos.y;
        draw_list->AddLine(origin_screen, z_screen, IM_COL32(100, 100, 255, 128), 1.5f);
    }

    void draw_edges(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        ImU32 edge_color = IM_COL32(80, 80, 100, 60);

        for (const auto& edge : data_.edges()) {
            const FunctionNode* from_node = data_.find_node(edge.from);
            const FunctionNode* to_node = data_.find_node(edge.to);

            if (!from_node || !to_node) continue;

            Vec3 from_pos = get_node_position(*from_node);
            Vec3 to_pos = get_node_position(*to_node);

            ImVec2 from_screen = camera_.project(from_pos, canvas_size);
            ImVec2 to_screen = camera_.project(to_pos, canvas_size);

            from_screen.x += canvas_pos.x;
            from_screen.y += canvas_pos.y;
            to_screen.x += canvas_pos.x;
            to_screen.y += canvas_pos.y;

            // Skip if behind camera
            if (from_screen.x < -5000 || to_screen.x < -5000) continue;

            draw_list->AddLine(from_screen, to_screen, edge_color, 0.5f);
        }
    }

    void draw_nodes(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        // Sort nodes by depth (back to front)
        struct NodeDepth {
            const FunctionNode* node;
            float depth;
        };

        std::vector<NodeDepth> sorted_nodes;
        sorted_nodes.reserve(data_.nodes().size());

        for (const auto& node : data_.nodes()) {
            Vec3 pos = get_node_position(node);
            sorted_nodes.push_back({&node, camera_.get_depth(pos)});
        }

        std::sort(sorted_nodes.begin(), sorted_nodes.end(),
                  [](const NodeDepth& a, const NodeDepth& b) {
                      return a.depth > b.depth;  // Far to near
                  });

        // Draw nodes
        for (const auto& nd : sorted_nodes) {
            const FunctionNode* node = nd.node;
            Vec3 pos = get_node_position(*node);
            ImVec2 screen_pos = camera_.project(pos, canvas_size);

            screen_pos.x += canvas_pos.x;
            screen_pos.y += canvas_pos.y;

            // Skip if behind camera or off screen
            if (screen_pos.x < canvas_pos.x - 50 ||
                screen_pos.x > canvas_pos.x + canvas_size.x + 50 ||
                screen_pos.y < canvas_pos.y - 50 ||
                screen_pos.y > canvas_pos.y + canvas_size.y + 50) {
                continue;
            }

            // Size based on depth (perspective) and node scale
            float depth_scale = 1.0f / (1.0f + nd.depth * 0.1f);
            float size = point_size_ * node->scale * depth_scale;
            size = std::clamp(size, 2.0f, 30.0f);

            // Color
            ImU32 color = IM_COL32(
                static_cast<int>(node->color_r * 255),
                static_cast<int>(node->color_g * 255),
                static_cast<int>(node->color_b * 255),
                200
            );

            // Highlight if hovered or selected
            if (node == hovered_node_) {
                color = IM_COL32(255, 255, 100, 255);
                size *= 1.5f;
            } else if (node == selected_node_) {
                color = IM_COL32(100, 255, 100, 255);
                size *= 1.3f;
            }

            // Draw filled circle
            draw_list->AddCircleFilled(screen_pos, size, color);

            // Draw outline
            draw_list->AddCircle(screen_pos, size, IM_COL32(255, 255, 255, 100), 0, 1.0f);

            // Draw label if enabled and close enough
            if (show_labels_ && nd.depth < 8.0f) {
                draw_list->AddText(
                    ImVec2(screen_pos.x + size + 2, screen_pos.y - 6),
                    IM_COL32(200, 200, 200, 180),
                    node->name.c_str()
                );
            }
        }
    }

    BinaryMapData data_;
    Camera camera_;

    // Display options
    bool show_edges_ = true;
    bool show_labels_ = false;
    float point_size_ = 6.0f;
    float z_scale_ = 2.0f;

    // Interaction state
    const FunctionNode* hovered_node_ = nullptr;
    const FunctionNode* selected_node_ = nullptr;
};

// =============================================================================
// Global State and Bridge Functions
// =============================================================================

static std::unique_ptr<BinaryMap3DState> g_state;

void init_binary_map_3d_state() {
    if (!g_state) {
        g_state = std::make_unique<BinaryMap3DState>();
        g_state->refresh_data();
    }
}

void cleanup_binary_map_3d_state() {
    g_state.reset();
}

void refresh_binary_map_3d_data() {
    if (g_state) {
        g_state->refresh_data();
    }
}

void render_binary_map_3d() {
    if (g_state) {
        g_state->render();
    }
}

} // namespace binary_map_3d
} // namespace features
} // namespace synopsia
