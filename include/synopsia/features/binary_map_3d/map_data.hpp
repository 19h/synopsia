/// @file map_data.hpp
/// @brief Data model for 3D binary map visualization

#pragma once

#include <synopsia/common/types.hpp>
#include <vector>
#include <unordered_map>
#include <string>

namespace synopsia {
namespace features {
namespace binary_map_3d {

/// Represents a function node in the 3D visualization
struct FunctionNode {
    ea_t address = BADADDR;
    ea_t end_address = BADADDR;
    std::string name;

    // 3D coordinates (computed)
    float x = 0.0f;  // Hilbert X
    float y = 0.0f;  // Hilbert Y
    float z = 0.0f;  // Call depth

    // Properties
    std::uint32_t size = 0;
    std::uint32_t call_depth = 0;
    std::uint32_t callee_count = 0;
    std::uint32_t caller_count = 0;
    float complexity = 0.0f;  // Cyclomatic or similar

    // For rendering
    float color_r = 1.0f;
    float color_g = 1.0f;
    float color_b = 1.0f;
    float scale = 1.0f;
};

/// Call edge between functions
struct CallEdge {
    ea_t from;
    ea_t to;
};

/// @class BinaryMapData
/// @brief Manages 3D binary map data from IDA database
class BinaryMapData {
public:
    BinaryMapData() = default;
    ~BinaryMapData() = default;

    // Non-copyable
    BinaryMapData(const BinaryMapData&) = delete;
    BinaryMapData& operator=(const BinaryMapData&) = delete;

    /// Refresh all data from database
    bool refresh();

    /// Check if data is valid
    [[nodiscard]] bool is_valid() const { return valid_; }

    /// Get all function nodes
    [[nodiscard]] const std::vector<FunctionNode>& nodes() const { return nodes_; }

    /// Get all call edges
    [[nodiscard]] const std::vector<CallEdge>& edges() const { return edges_; }

    /// Get maximum call depth
    [[nodiscard]] std::uint32_t max_depth() const { return max_depth_; }

    /// Get node by address
    [[nodiscard]] const FunctionNode* find_node(ea_t addr) const;

    /// Get Hilbert curve order used
    [[nodiscard]] int hilbert_order() const { return hilbert_order_; }

private:
    /// Build call graph from xrefs
    void build_call_graph();

    /// Compute call depths via BFS from entry points
    void compute_call_depths();

    /// Map addresses to Hilbert curve coordinates
    void compute_hilbert_layout();

    /// Assign colors based on properties
    void assign_colors();

    /// Hilbert curve coordinate conversion
    static void hilbert_d2xy(int n, int d, int& x, int& y);

    std::vector<FunctionNode> nodes_;
    std::vector<CallEdge> edges_;
    std::unordered_map<ea_t, std::size_t> addr_to_index_;
    std::unordered_map<ea_t, std::vector<ea_t>> callees_;  // func -> functions it calls
    std::unordered_map<ea_t, std::vector<ea_t>> callers_;  // func -> functions that call it

    std::uint32_t max_depth_ = 0;
    int hilbert_order_ = 8;  // 2^8 = 256x256 grid
    bool valid_ = false;
};

} // namespace binary_map_3d
} // namespace features
} // namespace synopsia
