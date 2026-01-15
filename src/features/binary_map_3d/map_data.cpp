/// @file map_data.cpp
/// @brief 3D Binary map data implementation

#include <synopsia/features/binary_map_3d/map_data.hpp>
#include <funcs.hpp>
#include <xref.hpp>
#include <name.hpp>
#include <queue>
#include <cmath>
#include <algorithm>

namespace synopsia {
namespace features {
namespace binary_map_3d {

bool BinaryMapData::refresh() {
    nodes_.clear();
    edges_.clear();
    addr_to_index_.clear();
    callees_.clear();
    callers_.clear();
    max_depth_ = 0;
    valid_ = false;

    if (!is_database_loaded()) {
        return false;
    }

    // Count functions and reserve
    std::size_t count = get_func_qty();
    if (count == 0) {
        return false;
    }

    nodes_.reserve(count);
    addr_to_index_.reserve(count);

    // Collect all functions
    for (std::size_t i = 0; i < count; ++i) {
        func_t* func = getn_func(i);
        if (!func) continue;

        FunctionNode node;
        node.address = func->start_ea;
        node.end_address = func->end_ea;
        node.size = static_cast<std::uint32_t>(func->end_ea - func->start_ea);

        // Get function name
        qstring name;
        if (get_func_name(&name, func->start_ea) > 0) {
            node.name = name.c_str();
        } else {
            char buf[32];
            qsnprintf(buf, sizeof(buf), "sub_%llX",
                      static_cast<unsigned long long>(func->start_ea));
            node.name = buf;
        }

        addr_to_index_[node.address] = nodes_.size();
        nodes_.push_back(std::move(node));
    }

    // Build call graph
    build_call_graph();

    // Compute depths
    compute_call_depths();

    // Compute Hilbert layout
    compute_hilbert_layout();

    // Assign colors
    assign_colors();

    valid_ = true;
    return true;
}

void BinaryMapData::build_call_graph() {
    // For each function, find what it calls via code xrefs
    for (auto& node : nodes_) {
        func_t* func = get_func(node.address);
        if (!func) continue;

        // Iterate through function and find call xrefs
        ea_t addr = func->start_ea;
        while (addr < func->end_ea && addr != BADADDR) {
            // Check xrefs from this address
            xrefblk_t xref;
            for (bool ok = xref.first_from(addr, XREF_FAR); ok; ok = xref.next_from()) {
                // Only interested in code xrefs (calls)
                if (xref.type != fl_CN && xref.type != fl_CF) continue;

                // Check if target is a function
                func_t* target_func = get_func(xref.to);
                if (target_func && target_func->start_ea != node.address) {
                    ea_t target = target_func->start_ea;

                    // Add to callees if not already present
                    auto& callee_list = callees_[node.address];
                    if (std::find(callee_list.begin(), callee_list.end(), target) == callee_list.end()) {
                        callee_list.push_back(target);
                        callers_[target].push_back(node.address);

                        // Add edge
                        edges_.push_back({node.address, target});
                    }
                }
            }

            // Move to next head
            addr = next_head(addr, func->end_ea);
        }

        // Update callee count
        node.callee_count = static_cast<std::uint32_t>(callees_[node.address].size());
    }

    // Update caller counts
    for (auto& node : nodes_) {
        auto it = callers_.find(node.address);
        if (it != callers_.end()) {
            node.caller_count = static_cast<std::uint32_t>(it->second.size());
        }
    }
}

void BinaryMapData::compute_call_depths() {
    // Find entry points (functions with no callers or marked as entry)
    std::vector<ea_t> entry_points;

    for (const auto& node : nodes_) {
        auto it = callers_.find(node.address);
        if (it == callers_.end() || it->second.empty()) {
            entry_points.push_back(node.address);
        }
    }

    // If no natural entry points found, use the entry point from IDA
    if (entry_points.empty()) {
        ea_t start = inf_get_start_ea();
        if (start != BADADDR) {
            func_t* entry_func = get_func(start);
            if (entry_func) {
                entry_points.push_back(entry_func->start_ea);
            }
        }
    }

    // BFS from entry points
    std::unordered_map<ea_t, std::uint32_t> depths;
    std::queue<ea_t> queue;

    for (ea_t entry : entry_points) {
        depths[entry] = 0;
        queue.push(entry);
    }

    while (!queue.empty()) {
        ea_t current = queue.front();
        queue.pop();

        std::uint32_t current_depth = depths[current];

        // Process callees
        auto it = callees_.find(current);
        if (it != callees_.end()) {
            for (ea_t callee : it->second) {
                auto depth_it = depths.find(callee);
                if (depth_it == depths.end()) {
                    // Not visited yet
                    depths[callee] = current_depth + 1;
                    queue.push(callee);
                }
                // Don't update if already visited (keeps shortest path)
            }
        }
    }

    // Assign depths to nodes
    max_depth_ = 0;
    for (auto& node : nodes_) {
        auto it = depths.find(node.address);
        if (it != depths.end()) {
            node.call_depth = it->second;
            max_depth_ = std::max(max_depth_, node.call_depth);
        } else {
            // Unreachable functions get depth based on caller count
            node.call_depth = 0;
        }
    }

    // Normalize depths for visualization
    if (max_depth_ > 0) {
        for (auto& node : nodes_) {
            node.z = static_cast<float>(node.call_depth) / static_cast<float>(max_depth_);
        }
    }
}

void BinaryMapData::compute_hilbert_layout() {
    if (nodes_.empty()) return;

    // Get address range
    ea_t min_addr = nodes_.front().address;
    ea_t max_addr = nodes_.front().address;
    for (const auto& node : nodes_) {
        min_addr = std::min(min_addr, node.address);
        max_addr = std::max(max_addr, node.address);
    }

    ea_t range = max_addr - min_addr;
    if (range == 0) range = 1;

    // Hilbert curve grid size
    int n = 1 << hilbert_order_;  // 2^order

    // Map each function address to Hilbert curve position
    for (auto& node : nodes_) {
        // Normalize address to [0, n*n-1]
        double normalized = static_cast<double>(node.address - min_addr) / static_cast<double>(range);
        int d = static_cast<int>(normalized * (n * n - 1));
        d = std::clamp(d, 0, n * n - 1);

        // Convert to x,y via Hilbert curve
        int hx, hy;
        hilbert_d2xy(n, d, hx, hy);

        // Normalize to [-1, 1]
        node.x = (static_cast<float>(hx) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
        node.y = (static_cast<float>(hy) / static_cast<float>(n - 1)) * 2.0f - 1.0f;
    }
}

void BinaryMapData::hilbert_d2xy(int n, int d, int& x, int& y) {
    // Convert Hilbert curve index to x,y coordinates
    // Reference: https://en.wikipedia.org/wiki/Hilbert_curve
    x = 0;
    y = 0;

    for (int s = 1; s < n; s *= 2) {
        int rx = 1 & (d / 2);
        int ry = 1 & (d ^ rx);

        // Rotate
        if (ry == 0) {
            if (rx == 1) {
                x = s - 1 - x;
                y = s - 1 - y;
            }
            std::swap(x, y);
        }

        x += s * rx;
        y += s * ry;
        d /= 4;
    }
}

void BinaryMapData::assign_colors() {
    // Color based on call depth and function size
    for (auto& node : nodes_) {
        // Base color on depth (blue for shallow, red for deep)
        float t = (max_depth_ > 0) ? static_cast<float>(node.call_depth) / static_cast<float>(max_depth_) : 0.0f;

        // Interpolate from blue (0,0.5,1) to red (1,0.3,0.2)
        node.color_r = 0.2f + t * 0.8f;
        node.color_g = 0.5f - t * 0.2f;
        node.color_b = 1.0f - t * 0.8f;

        // Scale based on function size (log scale)
        float size_factor = std::log2(static_cast<float>(node.size) + 1.0f) / 16.0f;
        node.scale = 0.5f + std::clamp(size_factor, 0.0f, 1.0f) * 1.5f;
    }
}

const FunctionNode* BinaryMapData::find_node(ea_t addr) const {
    auto it = addr_to_index_.find(addr);
    if (it != addr_to_index_.end() && it->second < nodes_.size()) {
        return &nodes_[it->second];
    }
    return nullptr;
}

} // namespace binary_map_3d
} // namespace features
} // namespace synopsia
