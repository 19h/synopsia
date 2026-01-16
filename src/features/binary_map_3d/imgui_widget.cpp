/// @file imgui_widget.cpp
/// @brief Force-directed 3D call graph visualization

#include <synopsia/features/binary_map_3d/map_data.hpp>
#include <synopsia/imgui/qt_imgui_widget.hpp>
#include <funcs.hpp>
#include <xref.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <algorithm>
#include <queue>

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
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }

    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length_sq() const { return x * x + y * y + z * z; }

    Vec3 normalized() const {
        float len = length();
        if (len < 0.0001f) return {0, 0, 0};
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
    Vec3 target{0, 0, 0};
    float distance = 8.0f;
    float yaw = 0.4f;
    float pitch = 0.3f;
    float fov = 60.0f;

    // Free flight mode
    bool free_flight = false;
    Vec3 position{0, 0, 8};  // Used in free flight mode

    // 2D mode settings
    float zoom_2d = 50.0f;   // Pixels per unit in 2D mode
    Vec3 pan_2d{0, 0, 0};    // Pan offset in 2D mode (only x, y used)

    Vec3 get_position() const {
        if (free_flight) {
            return position;
        }
        float cos_pitch = std::cos(pitch);
        float sin_pitch = std::sin(pitch);
        float cos_yaw = std::cos(yaw);
        float sin_yaw = std::sin(yaw);

        return {
            target.x + distance * cos_pitch * sin_yaw,
            target.y + distance * sin_pitch,
            target.z + distance * cos_pitch * cos_yaw
        };
    }

    Vec3 get_forward() const {
        float cos_pitch = std::cos(pitch);
        float sin_pitch = std::sin(pitch);
        float cos_yaw = std::cos(yaw);
        float sin_yaw = std::sin(yaw);
        return Vec3(-sin_yaw * cos_pitch, -sin_pitch, -cos_yaw * cos_pitch).normalized();
    }

    Vec3 get_right() const {
        Vec3 forward = get_forward();
        Vec3 up{0, 1, 0};
        return forward.cross(up).normalized();
    }

    Vec3 get_up() const {
        return get_right().cross(get_forward()).normalized();
    }

    void enter_free_flight() {
        if (!free_flight) {
            position = get_position();
            free_flight = true;
        }
    }

    void exit_free_flight() {
        if (free_flight) {
            // Compute new target from current position/orientation
            Vec3 forward = get_forward();
            target = position + forward * distance;
            free_flight = false;
        }
    }

    ImVec2 project(const Vec3& point, const ImVec2& screen_size) const {
        Vec3 cam_pos = get_position();
        Vec3 up{0, 1, 0};

        Vec3 forward = free_flight ? get_forward() : (target - cam_pos).normalized();
        Vec3 right = forward.cross(up).normalized();
        Vec3 cam_up = right.cross(forward).normalized();

        Vec3 p = point - cam_pos;
        float x = p.dot(right);
        float y = p.dot(cam_up);
        float z = p.dot(forward);

        if (z <= 0.1f) {
            return {-10000, -10000};
        }

        float fov_rad = fov * 3.14159f / 180.0f;
        float scale = 1.0f / std::tan(fov_rad * 0.5f);
        float aspect = screen_size.x / screen_size.y;

        float px = (x * scale / z / aspect + 1.0f) * 0.5f * screen_size.x;
        float py = (-y * scale / z + 1.0f) * 0.5f * screen_size.y;

        return {px, py};
    }

    // 2D orthographic projection (top-down view, x-y plane)
    ImVec2 project_2d(const Vec3& point, const ImVec2& screen_size) const {
        // Center of screen + (point position - pan offset) * zoom
        float px = screen_size.x * 0.5f + (point.x - pan_2d.x) * zoom_2d;
        float py = screen_size.y * 0.5f - (point.y - pan_2d.y) * zoom_2d;  // Flip Y for screen coords
        return {px, py};
    }

    // For 2D mode, depth is just used for ordering - use Y coordinate
    float get_depth_2d(const Vec3& point) const {
        return -point.y;  // Lower Y = further back
    }

    float get_depth(const Vec3& point) const {
        Vec3 cam_pos = get_position();
        Vec3 forward = free_flight ? get_forward() : (target - cam_pos).normalized();
        Vec3 p = point - cam_pos;
        return p.dot(forward);
    }
};

// =============================================================================
// Force-Directed Graph Node
// =============================================================================

struct GraphNode {
    ea_t address = BADADDR;
    std::string name;
    std::uint32_t size = 0;

    // Position and velocity for physics simulation
    Vec3 pos;
    Vec3 vel;

    // Graph properties
    std::uint32_t caller_count = 0;
    std::uint32_t callee_count = 0;
    int graph_distance = -1;  // Distance from selected node (-1 = not computed)
    int follow_distance = -1; // Distance from nearest followed node (-1 = not computed)
    bool is_hub = false;      // True if node has too many connections (not traversed)
    bool is_followed = false; // True if this node is being followed

    // Visual properties
    float importance = 0.0f;  // 0-1, relative to selected node
    float opacity = 1.0f;
    float scale = 1.0f;
};

// =============================================================================
// Force Graph State
// =============================================================================

class ForceGraphState {
public:
    ForceGraphState() = default;

    void refresh_data() {
        data_.refresh();

        // Get current EA from IDA if not established
        if (current_ea_ == BADADDR) {
            current_ea_ = get_screen_ea();
        }

        // In focused mode with valid EA, do targeted load (much faster for large binaries)
        if (only_show_neighbors_ && current_ea_ != BADADDR) {
            func_t* func = get_func(current_ea_);
            if (func) {
                selected_addr_ = func->start_ea;
                load_neighbors_from_ea(selected_addr_);
                restart_simulation();
                return;
            }
        }

        // Normal mode: build full graph (with limit)
        build_full_graph();
        selected_addr_ = BADADDR;
        selected_node_idx_ = -1;
        apply_filter();
        restart_simulation();
    }

    void on_ea_changed(ea_t ea) {
        current_ea_ = ea;
        // In locked mode, don't update the graph
        if (graph_locked_) return;
        if (track_ea_) {
            select_node_at_ea(ea);
        }
    }

    void set_focused_mode(bool enabled) {
        track_ea_ = enabled;
        only_show_neighbors_ = enabled;

        // Get current EA from IDA if not established
        if (current_ea_ == BADADDR) {
            current_ea_ = get_screen_ea();
        }

        if (enabled && current_ea_ != BADADDR) {
            // Focused mode: do targeted load
            func_t* func = get_func(current_ea_);
            if (func) {
                selected_addr_ = func->start_ea;
                load_neighbors_from_ea(selected_addr_);
                restart_simulation();
            }
        } else if (!enabled) {
            // Switching to full mode: need to build full graph
            build_full_graph();
            apply_filter();
            restart_simulation();
        }
    }

    bool is_focused_mode() const {
        return track_ea_ && only_show_neighbors_;
    }

    void render() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 display_size = io.DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display_size);

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("ForceGraphWindow", nullptr, window_flags);

        if (ImGui::BeginTable("##main-layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableSetupColumn("View", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextColumn();
            render_info_panel();

            ImGui::TableNextColumn();
            render_graph_view();

            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    static constexpr std::size_t MAX_NODES = 2000;
    static constexpr int HUB_NODE_THRESHOLD = 20;  // Nodes with 20+ connections are "hubs"

    void build_full_graph() {
        all_nodes_.clear();
        all_edges_.clear();
        all_addr_to_idx_.clear();

        if (!data_.is_valid()) return;

        // Create all nodes from data (with limit)
        const auto& data_nodes = data_.nodes();
        std::size_t node_count = std::min(data_nodes.size(), MAX_NODES);
        all_nodes_.reserve(node_count);

        for (std::size_t i = 0; i < node_count; ++i) {
            const auto& dn = data_nodes[i];
            GraphNode node;
            node.address = dn.address;
            node.name = dn.name;
            node.size = dn.size;
            node.caller_count = dn.caller_count;
            node.callee_count = dn.callee_count;

            // Scale based on connectivity
            float connectivity = static_cast<float>(node.caller_count + node.callee_count);
            node.scale = 0.8f + std::min(connectivity / 20.0f, 2.0f);

            all_addr_to_idx_[node.address] = all_nodes_.size();
            all_nodes_.push_back(node);
        }

        // Copy edges (only between nodes we included)
        const auto& data_edges = data_.edges();
        for (const auto& edge : data_edges) {
            if (all_addr_to_idx_.find(edge.from) != all_addr_to_idx_.end() &&
                all_addr_to_idx_.find(edge.to) != all_addr_to_idx_.end()) {
                all_edges_.push_back(edge);
            }
        }
    }

    /// Count xrefs for a function (callers + callees)
    int count_function_xrefs(ea_t func_ea) {
        int count = 0;
        xrefblk_t xb;
        // Count callers
        for (bool ok = xb.first_to(func_ea, XREF_FAR); ok; ok = xb.next_to()) {
            if (xb.type == fl_CF || xb.type == fl_CN || xb.type == fl_JF || xb.type == fl_JN)
                count++;
        }
        // Count callees (approximate - just from function start)
        for (bool ok = xb.first_from(func_ea, XREF_FAR); ok; ok = xb.next_from()) {
            if (xb.type == fl_CF || xb.type == fl_CN || xb.type == fl_JF || xb.type == fl_JN)
                count++;
        }
        return count;
    }

    /// Load only neighbors within max_depth from a given EA (for focused mode)
    /// This is much faster than building full graph for large binaries
    void load_neighbors_from_ea(ea_t center_ea) {
        nodes_.clear();
        edges_.clear();
        addr_to_idx_.clear();
        filtered_to_full_.clear();

        if (center_ea == BADADDR) return;

        // BFS to find all functions within max_depth
        std::unordered_set<ea_t> visited;
        std::unordered_set<ea_t> hub_nodes;  // Nodes with too many connections
        std::vector<std::pair<ea_t, int>> queue;  // (address, distance)
        std::unordered_map<ea_t, int> distances;

        queue.push_back({center_ea, 0});
        visited.insert(center_ea);
        distances[center_ea] = 0;

        std::size_t head = 0;
        while (head < queue.size() && visited.size() < MAX_NODES) {
            auto [current_ea, current_dist] = queue[head++];

            if (current_dist >= max_depth_) continue;

            // Check if this is a hub node (skip traversing but keep the node)
            if (skip_hub_nodes_ && current_ea != center_ea) {
                int xref_count = count_function_xrefs(current_ea);
                if (xref_count >= HUB_NODE_THRESHOLD) {
                    hub_nodes.insert(current_ea);
                    continue;  // Don't traverse from hub nodes
                }
            }

            // Get function at this EA
            func_t* func = get_func(current_ea);
            if (!func) continue;

            // Find callers (xrefs TO this function)
            xrefblk_t xb;
            for (bool ok = xb.first_to(func->start_ea, XREF_FAR); ok; ok = xb.next_to()) {
                if (xb.type != fl_CF && xb.type != fl_CN && xb.type != fl_JF && xb.type != fl_JN)
                    continue;

                func_t* caller_func = get_func(xb.from);
                if (!caller_func) continue;

                ea_t caller_ea = caller_func->start_ea;
                if (visited.size() < MAX_NODES && visited.find(caller_ea) == visited.end()) {
                    visited.insert(caller_ea);
                    distances[caller_ea] = current_dist + 1;
                    queue.push_back({caller_ea, current_dist + 1});
                }
            }

            // Find callees (xrefs FROM this function)
            func_item_iterator_t fii;
            for (bool ok = fii.set(func); ok; ok = fii.next_code()) {
                ea_t item_ea = fii.current();
                xrefblk_t xb2;
                for (bool ok2 = xb2.first_from(item_ea, XREF_FAR); ok2; ok2 = xb2.next_from()) {
                    if (xb2.type != fl_CF && xb2.type != fl_CN && xb2.type != fl_JF && xb2.type != fl_JN)
                        continue;

                    func_t* callee_func = get_func(xb2.to);
                    if (!callee_func) continue;

                    ea_t callee_ea = callee_func->start_ea;
                    if (visited.size() < MAX_NODES && visited.find(callee_ea) == visited.end()) {
                        visited.insert(callee_ea);
                        distances[callee_ea] = current_dist + 1;
                        queue.push_back({callee_ea, current_dist + 1});
                    }
                }
            }
        }

        // Build nodes from visited functions
        for (ea_t func_ea : visited) {
            func_t* func = get_func(func_ea);
            if (!func) continue;

            GraphNode node;
            node.address = func_ea;

            // Get function name
            qstring name;
            if (get_func_name(&name, func_ea) > 0) {
                node.name = name.c_str();
            } else {
                char buf[32];
                qsnprintf(buf, sizeof(buf), "sub_%llX", (unsigned long long)func_ea);
                node.name = buf;
            }

            node.size = static_cast<std::size_t>(func->end_ea - func->start_ea);

            // Count callers/callees (approximate)
            int caller_count = 0, callee_count = 0;
            xrefblk_t xb;
            for (bool ok = xb.first_to(func_ea, XREF_FAR); ok; ok = xb.next_to()) {
                if (xb.type == fl_CF || xb.type == fl_CN) caller_count++;
            }
            for (bool ok = xb.first_from(func_ea, XREF_FAR); ok; ok = xb.next_from()) {
                if (xb.type == fl_CF || xb.type == fl_CN) callee_count++;
            }
            node.caller_count = caller_count;
            node.callee_count = callee_count;

            // Set distance and importance
            auto dist_it = distances.find(func_ea);
            if (dist_it != distances.end()) {
                node.graph_distance = dist_it->second;
                node.importance = 1.0f - (static_cast<float>(dist_it->second) / static_cast<float>(max_depth_ + 1));
            }
            node.opacity = 1.0f;

            // Mark as hub if it has too many connections
            node.is_hub = hub_nodes.find(func_ea) != hub_nodes.end();

            float connectivity = static_cast<float>(caller_count + callee_count);
            // Hub nodes get larger scale to indicate they're connection hubs
            if (node.is_hub) {
                node.scale = 2.0f;  // Fixed larger size for hubs
            } else {
                node.scale = 0.8f + std::min(connectivity / 20.0f, 2.0f);
            }

            addr_to_idx_[func_ea] = nodes_.size();
            nodes_.push_back(node);
        }

        // Build edges between loaded nodes
        for (const auto& node : nodes_) {
            func_t* func = get_func(node.address);
            if (!func) continue;

            // Find calls from this function
            func_item_iterator_t fii;
            for (bool ok = fii.set(func); ok; ok = fii.next_code()) {
                ea_t item_ea = fii.current();
                xrefblk_t xb;
                for (bool ok2 = xb.first_from(item_ea, XREF_FAR); ok2; ok2 = xb.next_from()) {
                    if (xb.type != fl_CF && xb.type != fl_CN && xb.type != fl_JF && xb.type != fl_JN)
                        continue;

                    func_t* callee_func = get_func(xb.to);
                    if (!callee_func) continue;

                    ea_t callee_ea = callee_func->start_ea;
                    // Only add edge if both endpoints are in our loaded set
                    if (addr_to_idx_.find(callee_ea) != addr_to_idx_.end()) {
                        CallEdge edge;
                        edge.from = node.address;
                        edge.to = callee_ea;
                        edges_.push_back(edge);
                    }
                }
            }
        }

        // Update selected node index
        auto sel_it = addr_to_idx_.find(center_ea);
        selected_node_idx_ = (sel_it != addr_to_idx_.end()) ? static_cast<int>(sel_it->second) : -1;

        // Also update filtered_to_full_ for consistency (identity mapping since no full graph)
        filtered_to_full_.resize(nodes_.size());
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            filtered_to_full_[i] = i;
        }
    }

    void apply_filter() {
        nodes_.clear();
        edges_.clear();
        addr_to_idx_.clear();
        filtered_to_full_.clear();

        if (all_nodes_.empty()) return;

        // If not filtering or no selection, use all nodes
        if (!only_show_neighbors_ || selected_addr_ == BADADDR) {
            // Copy all nodes
            nodes_ = all_nodes_;
            edges_ = all_edges_;
            addr_to_idx_ = all_addr_to_idx_;
            filtered_to_full_.resize(all_nodes_.size());
            for (std::size_t i = 0; i < all_nodes_.size(); ++i) {
                filtered_to_full_[i] = i;
            }
            // Update selected_node_idx_ to point to correct index in nodes_
            if (selected_addr_ != BADADDR) {
                auto it = addr_to_idx_.find(selected_addr_);
                selected_node_idx_ = (it != addr_to_idx_.end()) ? static_cast<int>(it->second) : -1;
            } else {
                selected_node_idx_ = -1;
            }
            return;
        }

        // Use selected_addr_ which is stable across filter operations
        ea_t selected_addr = selected_addr_;

        // BFS to find neighbors within max_depth
        std::unordered_set<ea_t> neighbor_addrs;
        std::vector<ea_t> queue;
        std::unordered_map<ea_t, int> distances;

        queue.push_back(selected_addr);
        distances[selected_addr] = 0;
        neighbor_addrs.insert(selected_addr);

        std::size_t head = 0;
        while (head < queue.size()) {
            ea_t current = queue[head++];
            int current_dist = distances[current];

            if (current_dist >= max_depth_) continue;

            // Find neighbors via edges
            for (const auto& edge : all_edges_) {
                ea_t neighbor = BADADDR;
                if (edge.from == current) {
                    neighbor = edge.to;
                } else if (edge.to == current) {
                    neighbor = edge.from;
                }

                if (neighbor != BADADDR && neighbor_addrs.find(neighbor) == neighbor_addrs.end()) {
                    neighbor_addrs.insert(neighbor);
                    distances[neighbor] = current_dist + 1;
                    queue.push_back(neighbor);
                }
            }
        }

        // Build filtered nodes
        std::unordered_map<ea_t, std::size_t> new_addr_to_idx;
        for (std::size_t i = 0; i < all_nodes_.size(); ++i) {
            const auto& node = all_nodes_[i];
            if (neighbor_addrs.find(node.address) != neighbor_addrs.end()) {
                new_addr_to_idx[node.address] = nodes_.size();
                filtered_to_full_.push_back(i);

                GraphNode filtered_node = node;
                // Set graph distance for coloring
                auto dist_it = distances.find(node.address);
                if (dist_it != distances.end()) {
                    filtered_node.graph_distance = dist_it->second;
                    filtered_node.importance = 1.0f - (static_cast<float>(dist_it->second) / static_cast<float>(max_depth_ + 1));
                }
                filtered_node.opacity = 1.0f;
                nodes_.push_back(filtered_node);
            }
        }

        addr_to_idx_ = new_addr_to_idx;

        // Build filtered edges (only between filtered nodes)
        for (const auto& edge : all_edges_) {
            if (new_addr_to_idx.find(edge.from) != new_addr_to_idx.end() &&
                new_addr_to_idx.find(edge.to) != new_addr_to_idx.end()) {
                edges_.push_back(edge);
            }
        }

        // Update selected_node_idx_ to point to filtered index
        auto sel_it = new_addr_to_idx.find(selected_addr);
        if (sel_it != new_addr_to_idx.end()) {
            selected_node_idx_ = static_cast<int>(sel_it->second);
        }
    }

    /// Toggle follow on a node (Alt+click in locked mode)
    void toggle_follow_node(ea_t addr) {
        if (addr == BADADDR) return;

        // Check if node exists in our current graph (may be in base or expanded)
        auto it = addr_to_idx_.find(addr);
        if (it == addr_to_idx_.end()) {
            // Node might not be in current graph but could be in base - check base
            auto base_it = base_addr_to_idx_.find(addr);
            if (base_it == base_addr_to_idx_.end()) return;
        }

        if (followed_nodes_.find(addr) != followed_nodes_.end()) {
            // Unfollow: remove from set and rebuild graph from base + remaining follows
            followed_nodes_.erase(addr);
            rebuild_from_base_with_follows();
        } else {
            // Follow: add to set and expand graph with neighbors
            followed_nodes_.insert(addr);
            if (it != addr_to_idx_.end()) {
                nodes_[it->second].is_followed = true;
            }
            add_neighbors_to_graph(addr);
        }

        // Recompute distances and opacity
        compute_follow_distances();
    }

    /// Add immediate neighbors of a node to the current graph (for follow mode)
    void add_neighbors_to_graph(ea_t center_ea) {
        if (center_ea == BADADDR) return;

        func_t* func = get_func(center_ea);
        if (!func) return;

        std::vector<ea_t> new_neighbors;

        // Find callers (xrefs TO this function)
        xrefblk_t xb;
        for (bool ok = xb.first_to(func->start_ea, XREF_FAR); ok; ok = xb.next_to()) {
            if (xb.type != fl_CF && xb.type != fl_CN && xb.type != fl_JF && xb.type != fl_JN)
                continue;

            func_t* caller_func = get_func(xb.from);
            if (!caller_func) continue;

            ea_t caller_ea = caller_func->start_ea;
            if (addr_to_idx_.find(caller_ea) == addr_to_idx_.end()) {
                new_neighbors.push_back(caller_ea);
            }
        }

        // Find callees (xrefs FROM this function)
        func_item_iterator_t fii;
        for (bool ok = fii.set(func); ok; ok = fii.next_code()) {
            ea_t item_ea = fii.current();
            xrefblk_t xb2;
            for (bool ok2 = xb2.first_from(item_ea, XREF_FAR); ok2; ok2 = xb2.next_from()) {
                if (xb2.type != fl_CF && xb2.type != fl_CN && xb2.type != fl_JF && xb2.type != fl_JN)
                    continue;

                func_t* callee_func = get_func(xb2.to);
                if (!callee_func) continue;

                ea_t callee_ea = callee_func->start_ea;
                if (addr_to_idx_.find(callee_ea) == addr_to_idx_.end()) {
                    new_neighbors.push_back(callee_ea);
                }
            }
        }

        // Add new nodes to the graph
        for (ea_t neighbor_ea : new_neighbors) {
            func_t* nfunc = get_func(neighbor_ea);
            if (!nfunc) continue;

            // Check MAX_NODES limit
            if (nodes_.size() >= MAX_NODES) break;

            GraphNode node;
            node.address = neighbor_ea;

            // Get function name
            qstring name;
            if (get_func_name(&name, neighbor_ea) > 0) {
                node.name = name.c_str();
            } else {
                char buf[32];
                qsnprintf(buf, sizeof(buf), "sub_%llX", (unsigned long long)neighbor_ea);
                node.name = buf;
            }

            node.size = static_cast<std::size_t>(nfunc->end_ea - nfunc->start_ea);

            // Random position near the parent
            auto center_it = addr_to_idx_.find(center_ea);
            Vec3 parent_pos = (center_it != addr_to_idx_.end()) ? nodes_[center_it->second].pos : Vec3(0, 0, 0);
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
            node.pos = parent_pos + Vec3(dist(rng), dist(rng), dist(rng));
            node.vel = Vec3(0, 0, 0);

            // Count xrefs
            int caller_count = 0, callee_count = 0;
            xrefblk_t xb3;
            for (bool ok = xb3.first_to(neighbor_ea, XREF_FAR); ok; ok = xb3.next_to()) {
                if (xb3.type == fl_CF || xb3.type == fl_CN) caller_count++;
            }
            for (bool ok = xb3.first_from(neighbor_ea, XREF_FAR); ok; ok = xb3.next_from()) {
                if (xb3.type == fl_CF || xb3.type == fl_CN) callee_count++;
            }
            node.caller_count = caller_count;
            node.callee_count = callee_count;

            float connectivity = static_cast<float>(caller_count + callee_count);
            node.scale = 0.8f + std::min(connectivity / 20.0f, 2.0f);

            addr_to_idx_[neighbor_ea] = nodes_.size();
            nodes_.push_back(node);
        }

        // Add edges for new nodes
        for (ea_t neighbor_ea : new_neighbors) {
            if (addr_to_idx_.find(neighbor_ea) == addr_to_idx_.end()) continue;

            func_t* nfunc = get_func(neighbor_ea);
            if (!nfunc) continue;

            // Find calls from this function
            func_item_iterator_t fii2;
            for (bool ok = fii2.set(nfunc); ok; ok = fii2.next_code()) {
                ea_t item_ea = fii2.current();
                xrefblk_t xb4;
                for (bool ok2 = xb4.first_from(item_ea, XREF_FAR); ok2; ok2 = xb4.next_from()) {
                    if (xb4.type != fl_CF && xb4.type != fl_CN && xb4.type != fl_JF && xb4.type != fl_JN)
                        continue;

                    func_t* callee_func = get_func(xb4.to);
                    if (!callee_func) continue;

                    ea_t callee_ea = callee_func->start_ea;
                    if (addr_to_idx_.find(callee_ea) != addr_to_idx_.end()) {
                        CallEdge edge;
                        edge.from = neighbor_ea;
                        edge.to = callee_ea;
                        edges_.push_back(edge);
                    }
                }
            }

            // Find calls to this function (from existing nodes)
            xrefblk_t xb5;
            for (bool ok = xb5.first_to(neighbor_ea, XREF_FAR); ok; ok = xb5.next_to()) {
                if (xb5.type != fl_CF && xb5.type != fl_CN && xb5.type != fl_JF && xb5.type != fl_JN)
                    continue;

                func_t* caller_func = get_func(xb5.from);
                if (!caller_func) continue;

                ea_t caller_ea = caller_func->start_ea;
                if (addr_to_idx_.find(caller_ea) != addr_to_idx_.end()) {
                    CallEdge edge;
                    edge.from = caller_ea;
                    edge.to = neighbor_ea;
                    edges_.push_back(edge);
                }
            }
        }

        // Restart simulation to settle new nodes
        simulation_running_ = true;
        simulation_iterations_ = std::max(0, simulation_iterations_ - 100);
    }

    /// Rebuild graph from base state plus neighbors of all followed nodes
    /// Called when unfollowing a node to properly remove dynamically added nodes
    void rebuild_from_base_with_follows() {
        if (base_nodes_.empty()) return;

        // Start from base state
        nodes_ = base_nodes_;
        edges_ = base_edges_;
        addr_to_idx_ = base_addr_to_idx_;

        // Re-add neighbors for all remaining followed nodes
        for (ea_t followed_addr : followed_nodes_) {
            // Mark the followed node
            auto it = addr_to_idx_.find(followed_addr);
            if (it != addr_to_idx_.end()) {
                nodes_[it->second].is_followed = true;
            }
            // Add its neighbors
            add_neighbors_to_graph(followed_addr);
        }

        // Update selected_node_idx_ to match new addr_to_idx_
        if (selected_addr_ != BADADDR) {
            auto sel_it = addr_to_idx_.find(selected_addr_);
            if (sel_it != addr_to_idx_.end()) {
                selected_node_idx_ = static_cast<int>(sel_it->second);
            } else {
                selected_node_idx_ = -1;
            }
        }
    }

    /// Compute distances from all followed nodes (BFS) and update opacity
    void compute_follow_distances() {
        // Reset follow distances
        for (auto& node : nodes_) {
            node.follow_distance = -1;
            node.is_followed = (followed_nodes_.find(node.address) != followed_nodes_.end());
        }

        if (followed_nodes_.empty()) {
            // No followed nodes - full opacity for all
            for (auto& node : nodes_) {
                node.opacity = 1.0f;
            }
            return;
        }

        // BFS from all followed nodes simultaneously
        std::vector<std::size_t> queue;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].is_followed) {
                nodes_[i].follow_distance = 0;
                queue.push_back(i);
            }
        }

        std::size_t head = 0;
        int max_follow_dist = 0;
        while (head < queue.size()) {
            std::size_t current_idx = queue[head++];
            int current_dist = nodes_[current_idx].follow_distance;
            ea_t current_addr = nodes_[current_idx].address;

            // Find neighbors via edges
            for (const auto& edge : edges_) {
                std::size_t neighbor_idx = SIZE_MAX;
                if (edge.from == current_addr) {
                    auto it = addr_to_idx_.find(edge.to);
                    if (it != addr_to_idx_.end()) neighbor_idx = it->second;
                } else if (edge.to == current_addr) {
                    auto it = addr_to_idx_.find(edge.from);
                    if (it != addr_to_idx_.end()) neighbor_idx = it->second;
                }

                if (neighbor_idx != SIZE_MAX && nodes_[neighbor_idx].follow_distance < 0) {
                    int new_dist = current_dist + 1;
                    nodes_[neighbor_idx].follow_distance = new_dist;
                    max_follow_dist = std::max(max_follow_dist, new_dist);
                    queue.push_back(neighbor_idx);
                }
            }
        }

        // Update opacity based on follow distance
        // Followed nodes = full opacity, further = lower opacity
        for (auto& node : nodes_) {
            if (node.is_followed) {
                node.opacity = 1.0f;
            } else if (node.follow_distance >= 0) {
                // Fade out with distance: 1.0 at dist 0, down to min_opacity at max distance
                float t = static_cast<float>(node.follow_distance) / static_cast<float>(std::max(max_follow_dist, 1));
                node.opacity = 1.0f - t * (1.0f - 0.15f);  // 0.15 minimum opacity
            } else {
                // Disconnected from any followed node
                node.opacity = 0.08f;
            }
        }
    }

    void restart_simulation() {
        init_positions();
        simulation_running_ = true;
        simulation_iterations_ = 0;
    }

    void init_positions() {
        if (mode_2d_) {
            // 2D mode: use KK + modified FR layout (Gabriel et al.)
            compute_2d_force_layout();
            return;
        }

        // 3D mode: initialize with random positions on a sphere
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        float radius = std::sqrt(static_cast<float>(nodes_.size())) * 0.5f;
        if (radius < 1.0f) radius = 1.0f;

        for (auto& node : nodes_) {
            Vec3 p;
            do {
                p = Vec3(dist(rng), dist(rng), dist(rng));
            } while (p.length_sq() > 1.0f);
            node.pos = p * radius;
            node.vel = Vec3(0, 0, 0);
        }
    }

    /// 2D Force-directed layout using Kamada-Kawai + Modified Fruchterman-Reingold
    /// Based on "Summarization meets Visualization on Online Social Networks" (Gabriel et al.)
    /// KK provides initial layout, then modified FR refines with similarity-based forces
    void compute_2d_force_layout() {
        if (nodes_.empty()) return;
        
        const std::size_t n = nodes_.size();
        if (n == 1) {
            nodes_[0].pos = Vec3(0, 0, 0);
            return;
        }

        // Build adjacency set for fast lookup
        std::vector<std::unordered_set<std::size_t>> adj(n);
        for (const auto& edge : edges_) {
            auto it_from = addr_to_idx_.find(edge.from);
            auto it_to = addr_to_idx_.find(edge.to);
            if (it_from != addr_to_idx_.end() && it_to != addr_to_idx_.end()) {
                adj[it_from->second].insert(it_to->second);
                adj[it_to->second].insert(it_from->second);  // Undirected for layout
            }
        }

        // =====================================================================
        // Step 1: Compute shortest path distances using BFS (for KK ideal lengths)
        // =====================================================================
        std::vector<std::vector<int>> dist(n, std::vector<int>(n, static_cast<int>(n + 1)));
        
        for (std::size_t src = 0; src < n; ++src) {
            dist[src][src] = 0;
            std::queue<std::size_t> bfs;
            bfs.push(src);
            while (!bfs.empty()) {
                std::size_t u = bfs.front();
                bfs.pop();
                for (std::size_t v : adj[u]) {
                    if (dist[src][v] > dist[src][u] + 1) {
                        dist[src][v] = dist[src][u] + 1;
                        bfs.push(v);
                    }
                }
            }
        }

        // =====================================================================
        // Step 2: Compute Jaccard similarity between nodes (shared neighbors)
        // =====================================================================
        std::vector<std::vector<float>> similarity(n, std::vector<float>(n, 0.0f));
        
        for (std::size_t i = 0; i < n; ++i) {
            similarity[i][i] = 1.0f;
            for (std::size_t j = i + 1; j < n; ++j) {
                // Jaccard: |intersection| / |union|
                std::size_t intersect = 0;
                for (std::size_t nb : adj[i]) {
                    if (adj[j].count(nb)) intersect++;
                }
                // Also count direct edge as similarity
                if (adj[i].count(j)) intersect++;
                
                std::size_t union_size = adj[i].size() + adj[j].size() - intersect;
                if (union_size > 0) {
                    similarity[i][j] = static_cast<float>(intersect) / static_cast<float>(union_size);
                }
                similarity[j][i] = similarity[i][j];
                
                // Ensure minimum similarity to avoid division issues
                if (similarity[i][j] < 0.01f) similarity[i][j] = 0.01f;
                if (similarity[j][i] < 0.01f) similarity[j][i] = 0.01f;
            }
        }

        // =====================================================================
        // Step 3: Kamada-Kawai for initial layout
        // Energy-based spring system with ideal lengths = graph distances
        // =====================================================================
        const float L = 1.5f;  // Ideal edge length
        const float K = 1.0f;  // Spring constant
        const int kk_iterations = std::min(300, static_cast<int>(n * 10));
        
        // Initialize positions in a circle
        for (std::size_t i = 0; i < n; ++i) {
            float angle = 2.0f * 3.14159f * static_cast<float>(i) / static_cast<float>(n);
            float radius = std::sqrt(static_cast<float>(n)) * 0.8f;
            nodes_[i].pos = Vec3(radius * std::cos(angle), radius * std::sin(angle), 0.0f);
        }

        // KK main loop - move node with highest energy
        for (int iter = 0; iter < kk_iterations; ++iter) {
            // Find node with maximum delta (energy gradient)
            std::size_t max_node = 0;
            float max_delta = 0.0f;
            
            for (std::size_t m = 0; m < n; ++m) {
                float dx = 0.0f, dy = 0.0f;
                for (std::size_t i = 0; i < n; ++i) {
                    if (i == m) continue;
                    
                    float d_mi = dist[m][i];
                    if (d_mi > static_cast<int>(n)) d_mi = static_cast<float>(n);  // Disconnected
                    float l_mi = L * d_mi;  // Ideal distance
                    float k_mi = K / (d_mi * d_mi + 0.1f);  // Spring strength
                    
                    float diff_x = nodes_[m].pos.x - nodes_[i].pos.x;
                    float diff_y = nodes_[m].pos.y - nodes_[i].pos.y;
                    float actual_dist = std::sqrt(diff_x * diff_x + diff_y * diff_y);
                    if (actual_dist < 0.01f) actual_dist = 0.01f;
                    
                    float factor = k_mi * (actual_dist - l_mi) / actual_dist;
                    dx += factor * diff_x;
                    dy += factor * diff_y;
                }
                
                float delta = std::sqrt(dx * dx + dy * dy);
                if (delta > max_delta) {
                    max_delta = delta;
                    max_node = m;
                }
            }
            
            if (max_delta < 0.01f) break;  // Converged
            
            // Move the node with highest energy
            float dx = 0.0f, dy = 0.0f;
            float dxx = 0.0f, dxy = 0.0f, dyy = 0.0f;
            
            for (std::size_t i = 0; i < n; ++i) {
                if (i == max_node) continue;
                
                float d_mi = static_cast<float>(dist[max_node][i]);
                if (d_mi > static_cast<float>(n)) d_mi = static_cast<float>(n);
                float l_mi = L * d_mi;
                float k_mi = K / (d_mi * d_mi + 0.1f);
                
                float diff_x = nodes_[max_node].pos.x - nodes_[i].pos.x;
                float diff_y = nodes_[max_node].pos.y - nodes_[i].pos.y;
                float dist_sq = diff_x * diff_x + diff_y * diff_y;
                float actual_dist = std::sqrt(dist_sq);
                if (actual_dist < 0.01f) actual_dist = 0.01f;
                
                dx += k_mi * (1.0f - l_mi / actual_dist) * diff_x;
                dy += k_mi * (1.0f - l_mi / actual_dist) * diff_y;
                
                dxx += k_mi * (1.0f - l_mi * diff_y * diff_y / (dist_sq * actual_dist));
                dxy += k_mi * l_mi * diff_x * diff_y / (dist_sq * actual_dist);
                dyy += k_mi * (1.0f - l_mi * diff_x * diff_x / (dist_sq * actual_dist));
            }
            
            // Solve 2x2 system to find displacement
            float det = dxx * dyy - dxy * dxy;
            if (std::abs(det) > 0.0001f) {
                float move_x = (dyy * dx - dxy * dy) / det;
                float move_y = (dxx * dy - dxy * dx) / det;
                nodes_[max_node].pos.x -= move_x;
                nodes_[max_node].pos.y -= move_y;
            }
        }

        // =====================================================================
        // Step 4: Modified Fruchterman-Reingold with similarity-based forces
        // Repulsion: F_r = f² * distance / similarity
        // Attraction: F_a = distance * similarity / f²
        // =====================================================================
        const float area = static_cast<float>(n) * 4.0f;
        const float f = std::sqrt(area / static_cast<float>(n));  // Optimal distance
        float temperature = std::sqrt(area) * 0.5f;
        const float cooling = 0.95f;
        const int fr_iterations = std::min(200, static_cast<int>(n * 5));
        
        std::vector<Vec3> displacement(n);
        
        for (int iter = 0; iter < fr_iterations; ++iter) {
            // Reset displacements
            for (std::size_t i = 0; i < n; ++i) {
                displacement[i] = Vec3(0, 0, 0);
            }
            
            // Repulsive forces between all pairs (modified by similarity)
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = i + 1; j < n; ++j) {
                    Vec3 delta = nodes_[i].pos - nodes_[j].pos;
                    float d = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                    if (d < 0.01f) d = 0.01f;
                    
                    // Modified repulsion: inversely proportional to similarity
                    // Higher similarity = less repulsion (they should be closer)
                    float sim = similarity[i][j];
                    float repulsion = (f * f) / (d * sim);  // Paper formula
                    
                    Vec3 dir = Vec3(delta.x / d, delta.y / d, 0);
                    displacement[i] = displacement[i] + dir * repulsion;
                    displacement[j] = displacement[j] - dir * repulsion;
                }
            }
            
            // Attractive forces along edges (modified by similarity)
            for (const auto& edge : edges_) {
                auto it_from = addr_to_idx_.find(edge.from);
                auto it_to = addr_to_idx_.find(edge.to);
                if (it_from == addr_to_idx_.end() || it_to == addr_to_idx_.end()) continue;
                
                std::size_t i = it_from->second;
                std::size_t j = it_to->second;
                
                Vec3 delta = nodes_[i].pos - nodes_[j].pos;
                float d = std::sqrt(delta.x * delta.x + delta.y * delta.y);
                if (d < 0.01f) continue;
                
                // Modified attraction: proportional to similarity
                // Higher similarity = more attraction
                float sim = similarity[i][j];
                float attraction = (d * sim) / (f * f);  // Paper formula
                
                Vec3 dir = Vec3(delta.x / d, delta.y / d, 0);
                displacement[i] = displacement[i] - dir * attraction;
                displacement[j] = displacement[j] + dir * attraction;
            }
            
            // Apply displacements with temperature limiting
            float max_disp = 0.0f;
            for (std::size_t i = 0; i < n; ++i) {
                float d = std::sqrt(displacement[i].x * displacement[i].x + 
                                    displacement[i].y * displacement[i].y);
                if (d > 0.001f) {
                    float capped = std::min(d, temperature);
                    nodes_[i].pos.x += (displacement[i].x / d) * capped;
                    nodes_[i].pos.y += (displacement[i].y / d) * capped;
                    max_disp = std::max(max_disp, capped);
                }
                nodes_[i].pos.z = 0.0f;  // Keep in 2D plane
            }
            
            // Cool down
            temperature *= cooling;
            
            // Early termination if converged
            if (max_disp < 0.01f) break;
        }

        // Center the layout
        Vec3 center(0, 0, 0);
        for (const auto& node : nodes_) {
            center += node.pos;
        }
        center = center * (1.0f / static_cast<float>(n));
        for (auto& node : nodes_) {
            node.pos = node.pos - center;
            node.vel = Vec3(0, 0, 0);
        }
    }

    void step_simulation() {
        // 2D mode uses static hierarchical layout, no force simulation needed
        if (mode_2d_) {
            simulation_running_ = false;
            return;
        }
        if (!simulation_running_ || nodes_.empty()) return;

        const float repulsion = 50.0f;
        const float attraction = 0.05f;
        const float damping = 0.85f;
        const float min_dist = 0.5f;
        const float dt = 0.1f;

        // Repulsion between all nodes (use spatial hashing for large graphs)
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            for (std::size_t j = i + 1; j < nodes_.size(); ++j) {
                Vec3 delta = nodes_[i].pos - nodes_[j].pos;
                float dist_sq = delta.length_sq();
                if (dist_sq < 0.01f) dist_sq = 0.01f;

                float force = repulsion / dist_sq;
                Vec3 dir = delta.normalized();

                nodes_[i].vel += dir * force * dt;
                nodes_[j].vel += dir * (-force) * dt;
            }
        }

        // Attraction along edges
        for (const auto& edge : edges_) {
            auto it_from = addr_to_idx_.find(edge.from);
            auto it_to = addr_to_idx_.find(edge.to);
            if (it_from == addr_to_idx_.end() || it_to == addr_to_idx_.end()) continue;

            std::size_t i = it_from->second;
            std::size_t j = it_to->second;

            Vec3 delta = nodes_[j].pos - nodes_[i].pos;
            float dist = delta.length();
            if (dist < min_dist) continue;

            Vec3 dir = delta.normalized();
            float force = (dist - min_dist) * attraction;

            nodes_[i].vel += dir * force * dt;
            nodes_[j].vel += dir * (-force) * dt;
        }

        // Center gravity (pull towards origin)
        for (auto& node : nodes_) {
            node.vel += node.pos * (-0.01f) * dt;
        }

        // Apply velocities with damping
        float max_vel = 0.0f;
        for (auto& node : nodes_) {
            node.vel = node.vel * damping;
            node.pos += node.vel * dt;
            max_vel = std::max(max_vel, node.vel.length());
        }

        ++simulation_iterations_;

        // Stop simulation when stable
        if (max_vel < 0.01f || simulation_iterations_ > 500) {
            simulation_running_ = false;
        }
    }

    void compute_distances_from_selection() {
        // Reset all distances
        for (auto& node : nodes_) {
            node.graph_distance = -1;
            node.importance = 0.0f;
            node.opacity = unselected_opacity_;
        }

        if (selected_node_idx_ < 0 || selected_node_idx_ >= static_cast<int>(nodes_.size())) {
            // No selection - show all nodes fully
            for (auto& node : nodes_) {
                node.opacity = 1.0f;
            }
            return;
        }

        // BFS from selected node
        std::vector<int> queue;
        queue.push_back(selected_node_idx_);
        nodes_[selected_node_idx_].graph_distance = 0;
        nodes_[selected_node_idx_].importance = 1.0f;
        nodes_[selected_node_idx_].opacity = 1.0f;

        std::size_t head = 0;
        while (head < queue.size()) {
            int current_idx = queue[head++];
            int current_dist = nodes_[current_idx].graph_distance;

            if (current_dist >= max_depth_) continue;

            ea_t current_addr = nodes_[current_idx].address;

            // Find neighbors via edges
            for (const auto& edge : edges_) {
                int neighbor_idx = -1;
                if (edge.from == current_addr) {
                    auto it = addr_to_idx_.find(edge.to);
                    if (it != addr_to_idx_.end()) neighbor_idx = static_cast<int>(it->second);
                } else if (edge.to == current_addr) {
                    auto it = addr_to_idx_.find(edge.from);
                    if (it != addr_to_idx_.end()) neighbor_idx = static_cast<int>(it->second);
                }

                if (neighbor_idx >= 0 && nodes_[neighbor_idx].graph_distance < 0) {
                    int new_dist = current_dist + 1;
                    nodes_[neighbor_idx].graph_distance = new_dist;
                    nodes_[neighbor_idx].importance = 1.0f - (static_cast<float>(new_dist) / static_cast<float>(max_depth_ + 1));
                    nodes_[neighbor_idx].opacity = 1.0f;
                    queue.push_back(neighbor_idx);
                }
            }
        }
    }

    void render_info_panel() {
        ImGui::Text("Call Graph");
        ImGui::Separator();

        if (!data_.is_valid()) {
            ImGui::TextDisabled("No data loaded");
            return;
        }

        // Search box
        ImGui::Text("Search:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##search", search_buffer_, sizeof(search_buffer_))) {
            update_search_results();
        }

        // Search results
        if (search_buffer_[0] != '\0' && !search_results_.empty()) {
            ImGui::BeginChild("##search-results", ImVec2(-1, 120), true);
            for (std::size_t i = 0; i < search_results_.size() && i < 20; ++i) {
                int node_idx = search_results_[i];
                const auto& node = nodes_[node_idx];
                bool is_selected = (node_idx == selected_node_idx_);

                if (ImGui::Selectable(node.name.c_str(), is_selected)) {
                    // Jump to and select this node
                    selected_addr_ = node.address;
                    if (only_show_neighbors_) {
                        // In focused mode: do targeted load around selected node
                        load_neighbors_from_ea(node.address);
                        restart_simulation();
                        if (selected_node_idx_ >= 0) {
                            jump_to_node(selected_node_idx_);
                        }
                    } else {
                        selected_node_idx_ = node_idx;
                        compute_distances_from_selection();
                        jump_to_node(node_idx);
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::Separator();

        ImGui::Text("Functions: %zu", nodes_.size());
        ImGui::Text("Edges: %zu", edges_.size());

        if (simulation_running_) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Simulating... (%d)", simulation_iterations_);
        } else {
            ImGui::TextDisabled("Simulation complete");
        }

        ImGui::Separator();
        ImGui::Text("Settings:");

        if (ImGui::SliderInt("Max Depth", &max_depth_, 1, 10)) {
            if (only_show_neighbors_ && selected_addr_ != BADADDR) {
                // Re-load neighbors with new depth
                load_neighbors_from_ea(selected_addr_);
                restart_simulation();
            } else {
                compute_distances_from_selection();
            }
        }

        ImGui::SliderFloat("Node Size", &base_node_size_, 3.0f, 15.0f);
        ImGui::SliderFloat("Unselected Opacity", &unselected_opacity_, 0.05f, 0.5f);
        ImGui::SliderFloat("Move Speed", &move_speed_, 0.1f, 2.0f);

        ImGui::Checkbox("Show Edges", &show_edges_);
        ImGui::Checkbox("Show Labels", &show_labels_);
        if (show_labels_) {
            ImGui::SliderFloat("Label Distance", &label_distance_, 5.0f, 100.0f);
        }

        ImGui::Separator();
        ImGui::Text("EA Tracking:");

        if (ImGui::Checkbox("Track Current EA", &track_ea_)) {
            if (track_ea_ && current_ea_ != BADADDR) {
                select_node_at_ea(current_ea_);
            }
        }
        if (track_ea_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(follows IDA cursor)");
        }

        bool prev_only_neighbors = only_show_neighbors_;
        ImGui::Checkbox("Only Callers/Callees", &only_show_neighbors_);
        if (only_show_neighbors_ != prev_only_neighbors) {
            // Checkbox was toggled
            if (only_show_neighbors_ && selected_addr_ != BADADDR) {
                // Switched to focused mode: do targeted load
                load_neighbors_from_ea(selected_addr_);
                restart_simulation();
            } else if (!only_show_neighbors_) {
                // Switched to full mode: need to build full graph
                build_full_graph();
                apply_filter();
                restart_simulation();
            }
        }
        if (only_show_neighbors_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(removes unrelated)");
        }

        bool prev_skip_hubs = skip_hub_nodes_;
        ImGui::Checkbox("Skip Hub Nodes", &skip_hub_nodes_);
        if (skip_hub_nodes_ != prev_skip_hubs && only_show_neighbors_ && selected_addr_ != BADADDR) {
            // Re-load with new hub setting
            load_neighbors_from_ea(selected_addr_);
            restart_simulation();
        }
        if (skip_hub_nodes_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(20+ conns)");
        }

        ImGui::Separator();
        ImGui::Text("Lock Mode:");

        bool prev_locked = graph_locked_;
        ImGui::Checkbox("Lock Graph", &graph_locked_);
        if (graph_locked_ != prev_locked) {
            if (graph_locked_) {
                // Locking: capture current graph as base state for follow mode
                base_nodes_ = nodes_;
                base_edges_ = edges_;
                base_addr_to_idx_ = addr_to_idx_;
            } else {
                // Unlocking: clear followed nodes and restore base state
                followed_nodes_.clear();
                nodes_ = base_nodes_;
                edges_ = base_edges_;
                addr_to_idx_ = base_addr_to_idx_;
                for (auto& node : nodes_) {
                    node.is_followed = false;
                    node.follow_distance = -1;
                }
                // Update selected_node_idx_ to match restored addr_to_idx_
                if (selected_addr_ != BADADDR) {
                    auto sel_it = addr_to_idx_.find(selected_addr_);
                    if (sel_it != addr_to_idx_.end()) {
                        selected_node_idx_ = static_cast<int>(sel_it->second);
                    } else {
                        selected_node_idx_ = -1;
                    }
                }
                compute_distances_from_selection();
                // Clear base state
                base_nodes_.clear();
                base_edges_.clear();
                base_addr_to_idx_.clear();
            }
        }
        if (graph_locked_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(Alt+click to follow)");

            if (!followed_nodes_.empty()) {
                ImGui::Text("Following: %zu nodes", followed_nodes_.size());
            } else {
                ImGui::TextDisabled("Alt+click nodes to follow");
            }
        }

        ImGui::Separator();
        ImGui::Text("View Mode:");

        bool prev_2d = mode_2d_;
        if (ImGui::Checkbox("2D Flowgraph", &mode_2d_)) {
            if (mode_2d_ != prev_2d) {
                // Exit free flight when switching to 2D
                if (mode_2d_ && camera_.free_flight) {
                    camera_.exit_free_flight();
                }
                // Restart simulation to re-layout in 2D/3D
                restart_simulation();
            }
        }
        if (mode_2d_) {
            ImGui::SameLine();
            ImGui::TextDisabled("(hierarchical)");
        }

        // Free Flight only available in 3D mode
        if (!mode_2d_) {
            if (ImGui::Checkbox("Free Flight", &camera_.free_flight)) {
                if (camera_.free_flight) {
                    camera_.enter_free_flight();
                } else {
                    camera_.exit_free_flight();
                }
            }
        }

        if (ImGui::Button("Reset Layout")) {
            init_positions();
            simulation_running_ = true;
            simulation_iterations_ = 0;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset View")) {
            if (mode_2d_) {
                camera_.pan_2d = Vec3(0, 0, 0);
                camera_.zoom_2d = 50.0f;
            } else {
                camera_.target = Vec3(0, 0, 0);
                camera_.distance = 8.0f;
                camera_.yaw = 0.4f;
                camera_.pitch = 0.3f;
                if (camera_.free_flight) {
                    camera_.exit_free_flight();
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Controls:");
        if (mode_2d_) {
            ImGui::BulletText("Drag: Pan");
            ImGui::BulletText("Scroll: Zoom");
        } else if (camera_.free_flight) {
            ImGui::BulletText("WASD/Arrows: Move");
            ImGui::BulletText("Q/E: Up/Down");
            ImGui::BulletText("Mouse: Look");
            ImGui::BulletText("Scroll: Speed");
        } else {
            ImGui::BulletText("Drag: Rotate");
            ImGui::BulletText("Shift+Drag: Pan");
            ImGui::BulletText("Scroll: Zoom");
        }
        ImGui::BulletText("Click node: Select");
        ImGui::BulletText("Click empty: Deselect");
        if (graph_locked_) {
            ImGui::BulletText("Alt+Click: Follow/Unfollow");
        }

        ImGui::Separator();

        if (selected_node_idx_ >= 0 && selected_node_idx_ < static_cast<int>(nodes_.size())) {
            const auto& node = nodes_[selected_node_idx_];
            ImGui::Text("Selected:");
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "  %s", node.name.c_str());
            ImGui::Text("  Addr: %llX", static_cast<unsigned long long>(node.address));
            ImGui::Text("  Size: %u bytes", node.size);
            ImGui::Text("  Calls: %u", node.callee_count);
            ImGui::Text("  Called by: %u", node.caller_count);

            // Count visible neighbors
            int visible_count = 0;
            for (const auto& n : nodes_) {
                if (n.graph_distance >= 0 && n.graph_distance <= max_depth_) {
                    ++visible_count;
                }
            }
            ImGui::Text("  Neighbors (d<=%d): %d", max_depth_, visible_count - 1);
        } else if (hovered_node_idx_ >= 0 && hovered_node_idx_ < static_cast<int>(nodes_.size())) {
            const auto& node = nodes_[hovered_node_idx_];
            ImGui::Text("Hover:");
            ImGui::Text("  %s", node.name.c_str());
            ImGui::Text("  Calls: %u | Called by: %u", node.callee_count, node.caller_count);
        } else {
            ImGui::TextDisabled("Click a node to select");
        }
    }

    void update_search_results() {
        search_results_.clear();
        if (search_buffer_[0] == '\0') return;

        std::string query(search_buffer_);
        // Convert to lowercase for case-insensitive search
        for (auto& c : query) c = static_cast<char>(std::tolower(c));

        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            std::string name = nodes_[i].name;
            for (auto& c : name) c = static_cast<char>(std::tolower(c));

            if (name.find(query) != std::string::npos) {
                search_results_.push_back(static_cast<int>(i));
            }
        }
    }

    void jump_to_node(int node_idx) {
        if (node_idx < 0 || node_idx >= static_cast<int>(nodes_.size())) return;

        const auto& node = nodes_[node_idx];
        if (camera_.free_flight) {
            // In free flight, move camera to look at node
            Vec3 offset = camera_.get_forward() * (-camera_.distance);
            camera_.position = node.pos + offset;
        } else {
            // In orbit mode, set target to node position
            camera_.target = node.pos;
        }
    }

    void select_node_at_ea(ea_t ea) {
        if (ea == BADADDR) return;

        // Find the function containing this EA
        func_t* func = get_func(ea);
        if (!func) return;

        ea_t func_ea = func->start_ea;

        // Check if selection actually changed
        if (func_ea == selected_addr_) return;

        // Update selection
        selected_addr_ = func_ea;

        if (only_show_neighbors_) {
            // In focused mode: do targeted load directly from IDA
            load_neighbors_from_ea(func_ea);
            restart_simulation();
        } else {
            // Not filtering: check if node exists in full graph
            auto it = all_addr_to_idx_.find(func_ea);
            if (it == all_addr_to_idx_.end()) return;
            selected_node_idx_ = static_cast<int>(it->second);
            compute_distances_from_selection();
        }

        // Jump to the selected node
        if (selected_node_idx_ >= 0 && selected_node_idx_ < static_cast<int>(nodes_.size())) {
            jump_to_node(selected_node_idx_);
        }
    }

    void render_graph_view() {
        // Step physics simulation
        step_simulation();

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();

        if (canvas_size.x < 50 || canvas_size.y < 50) return;

        ImGui::InvisibleButton("##graph-canvas", canvas_size,
                               ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);

        bool is_hovered = ImGui::IsItemHovered();
        bool is_active = ImGui::IsItemActive();

        // Capture keyboard when in free flight mode and canvas is hovered/active
        if (camera_.free_flight && (is_hovered || is_active)) {
            ImGui::GetIO().WantCaptureKeyboard = true;
        }
        bool was_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        handle_input(is_hovered, is_active, canvas_pos, canvas_size);

        // Handle click on canvas (deselect) vs click on node
        // Only trigger on mouse release (was_clicked) and only if it was a short click (not a drag)
        bool is_short_click = was_clicked && was_short_click();

        ImGuiIO& io = ImGui::GetIO();
        bool alt_pressed = io.KeyAlt;

        if (is_short_click && hovered_node_idx_ < 0) {
            // Short click on empty canvas - deselect
            if (only_show_neighbors_) {
                // In focused mode, clicking empty space does nothing (graph IS the selection)
            } else {
                selected_addr_ = BADADDR;
                selected_node_idx_ = -1;
                compute_distances_from_selection();
            }
        } else if (is_short_click && hovered_node_idx_ >= 0) {
            // Short click on a node
            if (hovered_node_idx_ < static_cast<int>(nodes_.size())) {
                ea_t clicked_addr = nodes_[hovered_node_idx_].address;

                if (graph_locked_) {
                    // In locked mode: Alt+click toggles follow, regular click just navigates
                    if (alt_pressed) {
                        toggle_follow_node(clicked_addr);
                    }
                    // Always navigate to the node in IDA when clicking in locked mode
                    jumpto(clicked_addr);
                } else if (clicked_addr != selected_addr_) {
                    // Normal mode: select node and update graph
                    selected_addr_ = clicked_addr;
                    if (only_show_neighbors_) {
                        // In focused mode: do targeted load around clicked node
                        load_neighbors_from_ea(clicked_addr);
                        restart_simulation();
                    } else {
                        selected_node_idx_ = hovered_node_idx_;
                        compute_distances_from_selection();
                    }
                }
            }
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Background
        draw_list->AddRectFilled(canvas_pos,
                                 ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                                 IM_COL32(15, 15, 20, 255));

        if (nodes_.empty()) {
            draw_list->AddText(
                ImVec2(canvas_pos.x + canvas_size.x * 0.5f - 30,
                       canvas_pos.y + canvas_size.y * 0.5f),
                IM_COL32(128, 128, 128, 255),
                "No data");
            return;
        }

        // Draw edges
        if (show_edges_) {
            draw_edges(draw_list, canvas_pos, canvas_size);
        }

        // Draw nodes
        draw_nodes(draw_list, canvas_pos, canvas_size);
    }

    void handle_input(bool is_hovered, bool is_active,
                      const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        ImGuiIO& io = ImGui::GetIO();

        // Track mouse down for click vs drag detection
        bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (mouse_down && !mouse_was_down_ && is_hovered) {
            mouse_down_pos_ = io.MousePos;
            mouse_down_time_ = static_cast<float>(ImGui::GetTime());
        }
        mouse_was_down_ = mouse_down;

        if (camera_.free_flight) {
            // Free flight controls
            // Mouse look (when holding right mouse or when canvas is active)
            if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                camera_.yaw -= io.MouseDelta.x * 0.003f;
                camera_.pitch += io.MouseDelta.y * 0.003f;
                camera_.pitch = std::clamp(camera_.pitch, -1.55f, 1.55f);
            }
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                camera_.yaw -= io.MouseDelta.x * 0.003f;
                camera_.pitch += io.MouseDelta.y * 0.003f;
                camera_.pitch = std::clamp(camera_.pitch, -1.55f, 1.55f);
            }

            // WASD / Arrow keys for movement
            Vec3 forward = camera_.get_forward();
            Vec3 right = camera_.get_right();
            Vec3 up{0, 1, 0};
            float speed = move_speed_ * io.DeltaTime * 20.0f;

            if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
                camera_.position += forward * speed;
            }
            if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
                camera_.position += forward * (-speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow)) {
                camera_.position += right * (-speed);
            }
            if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) {
                camera_.position += right * speed;
            }
            if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_Space)) {
                camera_.position += up * speed;
            }
            if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                camera_.position += up * (-speed);
            }

            // Scroll to adjust speed
            if (is_hovered && std::abs(io.MouseWheel) > 0.01f) {
                move_speed_ *= (1.0f + io.MouseWheel * 0.1f);
                move_speed_ = std::clamp(move_speed_, 0.05f, 5.0f);
            }
        } else if (mode_2d_) {
            // 2D camera controls: pan and zoom
            if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                // Pan with left mouse drag
                float pan_speed = 1.0f / camera_.zoom_2d;
                camera_.pan_2d.x -= io.MouseDelta.x * pan_speed;
                camera_.pan_2d.y += io.MouseDelta.y * pan_speed;  // Flip Y
            }

            // Zoom with scroll wheel
            if (is_hovered && std::abs(io.MouseWheel) > 0.01f) {
                // Zoom towards mouse position
                ImVec2 mouse_pos = io.MousePos;
                float rel_x = (mouse_pos.x - canvas_pos.x - canvas_size.x * 0.5f) / camera_.zoom_2d;
                float rel_y = -(mouse_pos.y - canvas_pos.y - canvas_size.y * 0.5f) / camera_.zoom_2d;

                float old_zoom = camera_.zoom_2d;
                camera_.zoom_2d *= (1.0f + io.MouseWheel * 0.1f);
                camera_.zoom_2d = std::clamp(camera_.zoom_2d, 5.0f, 500.0f);

                // Adjust pan to keep point under mouse stationary
                float zoom_ratio = camera_.zoom_2d / old_zoom;
                camera_.pan_2d.x += rel_x * (1.0f - 1.0f / zoom_ratio);
                camera_.pan_2d.y += rel_y * (1.0f - 1.0f / zoom_ratio);
            }
        } else {
            // 3D orbit camera controls
            if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                if (io.KeyShift) {
                    float pan_speed = 0.01f * camera_.distance;
                    camera_.target.x -= io.MouseDelta.x * pan_speed;
                    camera_.target.y += io.MouseDelta.y * pan_speed;
                } else {
                    camera_.yaw -= io.MouseDelta.x * 0.01f;
                    camera_.pitch += io.MouseDelta.y * 0.01f;
                    camera_.pitch = std::clamp(camera_.pitch, -1.5f, 1.5f);
                }
            }

            // Zoom (no limits)
            if (is_hovered && std::abs(io.MouseWheel) > 0.01f) {
                camera_.distance *= (1.0f - io.MouseWheel * 0.1f);
                // No clamp - allow unlimited zoom
                if (camera_.distance < 0.1f) camera_.distance = 0.1f;  // Just prevent negative/zero
            }
        }

        // Hover detection
        hovered_node_idx_ = -1;
        if (is_hovered) {
            ImVec2 mouse_pos = io.MousePos;
            float best_dist_sq = 20.0f * 20.0f;

            for (std::size_t i = 0; i < nodes_.size(); ++i) {
                const auto& node = nodes_[i];

                // Skip very faded nodes
                if (node.opacity < 0.1f) continue;

                ImVec2 screen_pos = mode_2d_ ? camera_.project_2d(node.pos, canvas_size)
                                             : camera_.project(node.pos, canvas_size);
                screen_pos.x += canvas_pos.x;
                screen_pos.y += canvas_pos.y;

                float dx = mouse_pos.x - screen_pos.x;
                float dy = mouse_pos.y - screen_pos.y;
                float dist_sq = dx * dx + dy * dy;

                if (dist_sq < best_dist_sq) {
                    best_dist_sq = dist_sq;
                    hovered_node_idx_ = static_cast<int>(i);
                }
            }
        }
    }

    bool was_short_click() const {
        ImGuiIO& io = ImGui::GetIO();
        // Check if mouse moved significantly since press
        float dx = io.MousePos.x - mouse_down_pos_.x;
        float dy = io.MousePos.y - mouse_down_pos_.y;
        float dist_sq = dx * dx + dy * dy;

        // Short click: didn't drag more than 5 pixels
        return dist_sq < 25.0f;
    }

    void draw_edges(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        for (const auto& edge : edges_) {
            auto it_from = addr_to_idx_.find(edge.from);
            auto it_to = addr_to_idx_.find(edge.to);
            if (it_from == addr_to_idx_.end() || it_to == addr_to_idx_.end()) continue;

            const auto& from_node = nodes_[it_from->second];
            const auto& to_node = nodes_[it_to->second];

            // Edge opacity based on node visibility
            float edge_opacity = std::min(from_node.opacity, to_node.opacity);
            if (edge_opacity < 0.05f) continue;

            // Note: In "only neighbors" mode, edges_ is already filtered to only contain
            // edges between filtered nodes, so no additional visibility check needed

            ImVec2 from_screen = mode_2d_ ? camera_.project_2d(from_node.pos, canvas_size)
                                          : camera_.project(from_node.pos, canvas_size);
            ImVec2 to_screen = mode_2d_ ? camera_.project_2d(to_node.pos, canvas_size)
                                        : camera_.project(to_node.pos, canvas_size);

            from_screen.x += canvas_pos.x;
            from_screen.y += canvas_pos.y;
            to_screen.x += canvas_pos.x;
            to_screen.y += canvas_pos.y;

            if (!mode_2d_ && (from_screen.x < -5000 || to_screen.x < -5000)) continue;

            // Color based on importance
            ImU32 edge_color;
            if (selected_node_idx_ >= 0) {
                int alpha = static_cast<int>(edge_opacity * 100);
                if (from_node.importance > 0.5f || to_node.importance > 0.5f) {
                    edge_color = IM_COL32(100, 150, 255, alpha);
                } else {
                    edge_color = IM_COL32(60, 60, 80, alpha);
                }
            } else {
                edge_color = IM_COL32(60, 70, 90, 60);
            }

            draw_list->AddLine(from_screen, to_screen, edge_color, 1.0f);
        }
    }

    void draw_nodes(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size) {
        // Sort by depth for proper rendering order (in 3D) or just render in order (in 2D)
        struct NodeRender {
            int idx;
            float depth;
        };
        std::vector<NodeRender> sorted;
        sorted.reserve(nodes_.size());

        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            float depth = mode_2d_ ? camera_.get_depth_2d(nodes_[i].pos)
                                   : camera_.get_depth(nodes_[i].pos);
            sorted.push_back({static_cast<int>(i), depth});
        }

        std::sort(sorted.begin(), sorted.end(),
                  [](const NodeRender& a, const NodeRender& b) { return a.depth > b.depth; });

        for (const auto& nr : sorted) {
            const auto& node = nodes_[nr.idx];

            // Skip very faded nodes
            if (node.opacity < 0.05f) continue;

            // Note: In "only neighbors" mode, nodes_ is already filtered to only contain
            // nodes within max_depth of selection, so no additional visibility check needed

            ImVec2 screen_pos = mode_2d_ ? camera_.project_2d(node.pos, canvas_size)
                                         : camera_.project(node.pos, canvas_size);
            screen_pos.x += canvas_pos.x;
            screen_pos.y += canvas_pos.y;

            // Off-screen culling
            if (screen_pos.x < canvas_pos.x - 50 ||
                screen_pos.x > canvas_pos.x + canvas_size.x + 50 ||
                screen_pos.y < canvas_pos.y - 50 ||
                screen_pos.y > canvas_pos.y + canvas_size.y + 50) {
                continue;
            }

            // Size with depth perspective (no perspective in 2D mode)
            float depth_scale = mode_2d_ ? 1.0f : (1.0f / (1.0f + nr.depth * 0.05f));
            float size = base_node_size_ * node.scale * depth_scale;
            size = std::clamp(size, 2.0f, 30.0f);

            // Color based on importance and selection state
            ImU32 color;
            int alpha = static_cast<int>(node.opacity * 255);
            bool is_hub_node = node.is_hub;
            bool is_followed_node = node.is_followed;

            if (nr.idx == selected_node_idx_) {
                // Selected node - bright green
                color = IM_COL32(100, 255, 150, alpha);
                size *= 1.4f;
            } else if (nr.idx == hovered_node_idx_) {
                // Hovered node - yellow
                color = IM_COL32(255, 255, 100, alpha);
                size *= 1.3f;
            } else if (is_followed_node) {
                // Followed node (in lock mode) - bright magenta/purple
                color = IM_COL32(220, 100, 255, alpha);
                size *= 1.3f;
            } else if (is_hub_node) {
                // Hub node (many connections, not traversed) - orange/amber
                color = IM_COL32(255, 165, 50, alpha);
                size *= 1.2f;
            } else if (graph_locked_ && !followed_nodes_.empty() && node.follow_distance >= 0) {
                // In lock mode with followed nodes: color by distance from followed
                float max_dist = 10.0f;  // Assume reasonable max
                float t = 1.0f - std::min(static_cast<float>(node.follow_distance) / max_dist, 1.0f);
                // Gradient: gray (far) -> purple tint (close to followed)
                int r = static_cast<int>(80 + t * 100);
                int g = static_cast<int>(80 + t * 60);
                int b = static_cast<int>(120 + t * 100);
                color = IM_COL32(r, g, b, alpha);
            } else if (selected_node_idx_ >= 0 && node.graph_distance >= 0) {
                // Connected to selected node - color by distance
                float t = node.importance;
                // Gradient: blue (far) -> cyan -> green (close)
                int r = static_cast<int>((1.0f - t) * 80 + t * 100);
                int g = static_cast<int>((1.0f - t) * 120 + t * 230);
                int b = static_cast<int>((1.0f - t) * 220 + t * 180);
                color = IM_COL32(r, g, b, alpha);
            } else if (selected_node_idx_ >= 0) {
                // Not connected - gray
                color = IM_COL32(80, 80, 90, alpha);
            } else {
                // No selection - default blue gradient based on connectivity
                float conn = std::min(static_cast<float>(node.caller_count + node.callee_count) / 10.0f, 1.0f);
                int r = static_cast<int>(80 + conn * 100);
                int g = static_cast<int>(120 + conn * 80);
                int b = static_cast<int>(200 - conn * 50);
                color = IM_COL32(r, g, b, alpha);
            }

            // Draw node - hub nodes as rings, others as filled circles
            if (is_hub_node) {
                draw_list->AddCircle(screen_pos, size, color, 0, 3.0f);
                draw_list->AddCircleFilled(screen_pos, size * 0.5f, color);
            } else {
                draw_list->AddCircleFilled(screen_pos, size, color);
            }

            // Outline for selected/hovered/followed
            if (nr.idx == selected_node_idx_ || nr.idx == hovered_node_idx_) {
                draw_list->AddCircle(screen_pos, size + 2, IM_COL32(255, 255, 255, alpha), 0, 2.0f);
            } else if (is_followed_node) {
                // Double ring for followed nodes
                draw_list->AddCircle(screen_pos, size + 3, IM_COL32(220, 100, 255, alpha), 0, 2.0f);
                draw_list->AddCircle(screen_pos, size + 6, IM_COL32(180, 80, 220, static_cast<int>(alpha * 0.6f)), 0, 1.5f);
            }

            // Labels - show if:
            // 1. Labels enabled AND within label distance, OR
            // 2. This is selected/hovered node, OR
            // 3. This is a direct neighbor (distance 1) of selected node, OR
            // 4. This is a followed node (in lock mode)
            bool is_direct_neighbor = (selected_node_idx_ >= 0 && node.graph_distance == 1);
            bool is_selected_or_hovered = (nr.idx == selected_node_idx_ || nr.idx == hovered_node_idx_);
            bool within_label_distance = (show_labels_ && node.opacity > 0.5f && nr.depth < label_distance_);
            bool is_followed_for_label = is_followed_node;

            if (within_label_distance || is_selected_or_hovered || is_direct_neighbor || is_followed_for_label) {
                int text_alpha = static_cast<int>(node.opacity * 200);
                if ((is_direct_neighbor || is_followed_for_label) && !within_label_distance) {
                    text_alpha = 220;  // Ensure direct neighbors and followed are visible
                }
                ImU32 text_color = IM_COL32(200, 200, 200, text_alpha);
                draw_list->AddText(
                    ImVec2(screen_pos.x + size + 3, screen_pos.y - 6),
                    text_color,
                    node.name.c_str()
                );
            }
        }
    }

    BinaryMapData data_;

    // Full graph (all nodes/edges from the binary)
    std::vector<GraphNode> all_nodes_;
    std::vector<CallEdge> all_edges_;
    std::unordered_map<ea_t, std::size_t> all_addr_to_idx_;

    // Active/filtered graph (what we actually render and simulate)
    std::vector<GraphNode> nodes_;
    std::vector<CallEdge> edges_;
    std::unordered_map<ea_t, std::size_t> addr_to_idx_;

    // Mapping from filtered index to full graph index
    std::vector<std::size_t> filtered_to_full_;

    Camera camera_;

    // Simulation state
    bool simulation_running_ = false;
    int simulation_iterations_ = 0;

    // Selection state
    int selected_node_idx_ = -1;   // Index into nodes_ (filtered graph)
    ea_t selected_addr_ = BADADDR; // Address of selected node (persistent across filters)
    int hovered_node_idx_ = -1;

    // Click detection for deselect (only on short clicks, not drags)
    ImVec2 mouse_down_pos_{0, 0};
    float mouse_down_time_ = 0.0f;
    bool mouse_was_down_ = false;

    // Search
    char search_buffer_[256] = {0};
    std::vector<int> search_results_;

    // Settings
    int max_depth_ = 3;
    float base_node_size_ = 6.0f;
    float unselected_opacity_ = 0.15f;
    float move_speed_ = 0.5f;
    float label_distance_ = 15.0f;
    bool show_edges_ = true;
    bool show_labels_ = false;
    bool skip_hub_nodes_ = true;  // Don't traverse nodes with 20+ connections
    bool mode_2d_ = false;        // 2D mode (default is 3D)

    // EA tracking
    bool track_ea_ = false;
    ea_t current_ea_ = BADADDR;
    bool only_show_neighbors_ = false;

    // Lock mode - prevents graph from updating, allows following nodes
    bool graph_locked_ = false;
    std::unordered_set<ea_t> followed_nodes_;  // Nodes being followed (Alt+click to toggle)

    // Base state for follow mode - captured when lock mode is enabled
    // Used to rebuild graph when unfollowing nodes
    std::vector<GraphNode> base_nodes_;
    std::vector<CallEdge> base_edges_;
    std::unordered_map<ea_t, std::size_t> base_addr_to_idx_;
};

// =============================================================================
// Global State and Bridge Functions
// =============================================================================

static std::unique_ptr<ForceGraphState> g_state;

void init_binary_map_3d_state() {
    if (!g_state) {
        g_state = std::make_unique<ForceGraphState>();
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

void on_binary_map_3d_cursor_changed(ea_t ea) {
    if (g_state) {
        g_state->on_ea_changed(ea);
    }
}

void set_binary_map_3d_focused_mode(bool enabled) {
    if (g_state) {
        g_state->set_focused_mode(enabled);
    }
}

} // namespace binary_map_3d
} // namespace features
} // namespace synopsia
