/// @file minimap_data.hpp
/// @brief Minimap data model and coordinate transformation

#pragma once

#include "types.hpp"
#include "entropy.hpp"
#include "color.hpp"
#include "minimap_data_interface.hpp"
#include <mutex>
#include <atomic>
#include <string>

namespace synopsia {

/// @class MinimapData
/// @brief Manages entropy data and coordinate transformations for the minimap
///
/// This class is responsible for:
/// - Storing computed entropy blocks
/// - Mapping between screen coordinates and addresses
/// - Managing viewport (pan/zoom)
/// - Caching rendered image data
///
/// It implements IMinimapDataSource for Qt widget compatibility.
class MinimapData : public IMinimapDataSource {
public:
    MinimapData();
    ~MinimapData() override = default;
    
    // Non-copyable, non-movable (contains std::atomic)
    MinimapData(const MinimapData&) = delete;
    MinimapData& operator=(const MinimapData&) = delete;
    MinimapData(MinimapData&&) = delete;
    MinimapData& operator=(MinimapData&&) = delete;
    
    // =========================================================================
    // Data Management
    // =========================================================================
    
    /// @brief Refresh entropy data from the database
    /// @param block_size Block size for entropy calculation
    /// @return true if data was successfully refreshed
    bool refresh(std::size_t block_size = DEFAULT_BLOCK_SIZE);
    
    /// @brief Check if data is currently valid
    [[nodiscard]] bool is_valid() const noexcept override { return valid_.load(); }
    
    /// @brief Mark data as needing refresh
    void invalidate() noexcept { valid_.store(false); }
    
    /// @brief Get the entropy blocks (IDA version)
    [[nodiscard]] const std::vector<EntropyBlock>& blocks() const noexcept { return blocks_; }
    
    /// @brief Get the memory regions (IDA version)
    [[nodiscard]] const std::vector<MemoryRegion>& regions() const noexcept { return regions_; }
    
    /// @brief Get database address range
    [[nodiscard]] std::pair<ea_t, ea_t> address_range() const noexcept {
        return {db_start_, db_end_};
    }
    
    // =========================================================================
    // IMinimapDataSource Interface Implementation
    // =========================================================================
    
    [[nodiscard]] std::size_t block_count() const override { return blocks_.size(); }
    
    [[nodiscard]] EntropyBlockData get_block(std::size_t index) const override {
        if (index >= blocks_.size()) {
            return {0, 0, 0.0};
        }
        const auto& b = blocks_[index];
        return {
            static_cast<data_addr_t>(b.start_ea),
            static_cast<data_addr_t>(b.end_ea),
            b.entropy
        };
    }
    
    [[nodiscard]] std::size_t region_count() const override { return regions_.size(); }
    
    [[nodiscard]] RegionData get_region(std::size_t index) const override {
        if (index >= regions_.size()) {
            return {0, 0};
        }
        const auto& r = regions_[index];
        return {
            static_cast<data_addr_t>(r.start_ea),
            static_cast<data_addr_t>(r.end_ea)
        };
    }
    
    [[nodiscard]] std::string get_region_name_at(std::size_t index) const override {
        if (index >= regions_.size()) {
            return {};
        }
        const auto& r = regions_[index];
        if (!r.name.empty()) {
            return std::string(r.name.c_str());
        }
        return {};
    }
    
    [[nodiscard]] std::string get_region_name(data_addr_t addr) const override {
        const MemoryRegion* region = region_at(static_cast<ea_t>(addr));
        if (region && !region->name.empty()) {
            return std::string(region->name.c_str());
        }
        return {};
    }
    
    [[nodiscard]] ViewportData get_viewport() const override {
        return {
            static_cast<data_addr_t>(viewport_.start_ea),
            static_cast<data_addr_t>(viewport_.end_ea),
            viewport_.zoom
        };
    }
    
    // =========================================================================
    // Viewport Management
    // =========================================================================
    
    /// @brief Get current viewport (IDA version)
    [[nodiscard]] const Viewport& viewport() const noexcept { return viewport_; }
    
    /// @brief Set viewport to show entire database
    void reset_viewport();
    
    /// @brief Set viewport to specific range (IDA types)
    void set_viewport(ea_t start, ea_t end);
    
    /// @brief Zoom viewport by factor (IDA types)
    void zoom_ida(double factor, ea_t center);
    
    /// @brief Zoom viewport (interface version)
    void zoom(double factor, data_addr_t center) override {
        zoom_ida(factor, static_cast<ea_t>(center));
    }
    
    /// @brief Pan viewport (IDA types)
    void pan_ida(sval_t delta);
    
    /// @brief Pan viewport (interface version)
    void pan(data_sval_t delta) override {
        pan_ida(static_cast<sval_t>(delta));
    }
    
    // =========================================================================
    // Coordinate Transformation (Interface implementation)
    // =========================================================================
    
    [[nodiscard]] data_addr_t y_to_address(int y, int height) const override;
    [[nodiscard]] data_addr_t x_to_address(int x, int width) const override;
    [[nodiscard]] int address_to_y(data_addr_t addr, int height) const override;
    [[nodiscard]] int address_to_x(data_addr_t addr, int width) const override;
    [[nodiscard]] double entropy_at(data_addr_t addr) const override;
    
    // =========================================================================
    // IDA-specific coordinate methods
    // =========================================================================
    
    [[nodiscard]] ea_t y_to_address_ea(int y, int height) const;
    [[nodiscard]] ea_t x_to_address_ea(int x, int width) const;
    [[nodiscard]] int address_to_y_ea(ea_t addr, int height) const;
    [[nodiscard]] int address_to_x_ea(ea_t addr, int width) const;
    [[nodiscard]] double entropy_at_ea(ea_t addr) const;
    
    /// @brief Find entropy block containing address
    [[nodiscard]] const EntropyBlock* block_at(ea_t addr) const;
    
    /// @brief Find memory region containing address
    [[nodiscard]] const MemoryRegion* region_at(ea_t addr) const;
    
    // =========================================================================
    // Statistics
    // =========================================================================
    
    /// @brief Get minimum entropy value
    [[nodiscard]] double min_entropy() const noexcept { return min_entropy_; }
    
    /// @brief Get maximum entropy value
    [[nodiscard]] double max_entropy() const noexcept { return max_entropy_; }
    
    /// @brief Get average entropy value
    [[nodiscard]] double avg_entropy() const noexcept { return avg_entropy_; }
    
    /// @brief Get block size used for last calculation
    [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
    
private:
    // Entropy data
    std::vector<EntropyBlock> blocks_;
    std::vector<MemoryRegion> regions_;
    
    // Database range
    ea_t db_start_ = 0;
    ea_t db_end_ = 0;
    
    // Viewport
    Viewport viewport_;
    
    // Statistics
    double min_entropy_ = 0.0;
    double max_entropy_ = 0.0;
    double avg_entropy_ = 0.0;
    std::size_t block_size_ = DEFAULT_BLOCK_SIZE;
    
    // State
    std::atomic<bool> valid_{false};
    
    // Calculator instance
    EntropyCalculator calculator_;
    
    /// Compute statistics from blocks
    void compute_statistics();
};

// =============================================================================
// Inline Implementation
// =============================================================================

inline ea_t MinimapData::y_to_address_ea(int y, int height) const {
    if (height <= 0 || y < 0 || y >= height) {
        return BADADDR;
    }
    
    const double t = static_cast<double>(y) / static_cast<double>(height);
    const asize_t range = viewport_.range();
    const ea_t offset = static_cast<ea_t>(t * range);
    
    return viewport_.start_ea + offset;
}

inline data_addr_t MinimapData::y_to_address(int y, int height) const {
    ea_t result = y_to_address_ea(y, height);
    return (result == BADADDR) ? DATA_BADADDR : static_cast<data_addr_t>(result);
}

inline ea_t MinimapData::x_to_address_ea(int x, int width) const {
    if (width <= 0 || x < 0 || x >= width) {
        return BADADDR;
    }
    
    const double t = static_cast<double>(x) / static_cast<double>(width);
    const asize_t range = viewport_.range();
    const ea_t offset = static_cast<ea_t>(t * range);
    
    return viewport_.start_ea + offset;
}

inline data_addr_t MinimapData::x_to_address(int x, int width) const {
    ea_t result = x_to_address_ea(x, width);
    return (result == BADADDR) ? DATA_BADADDR : static_cast<data_addr_t>(result);
}

inline int MinimapData::address_to_y_ea(ea_t addr, int height) const {
    if (height <= 0 || addr < viewport_.start_ea || addr >= viewport_.end_ea) {
        return -1;
    }
    
    const asize_t range = viewport_.range();
    if (range == 0) return 0;
    
    const double t = static_cast<double>(addr - viewport_.start_ea) / static_cast<double>(range);
    return static_cast<int>(t * height);
}

inline int MinimapData::address_to_y(data_addr_t addr, int height) const {
    return address_to_y_ea(static_cast<ea_t>(addr), height);
}

inline int MinimapData::address_to_x_ea(ea_t addr, int width) const {
    if (width <= 0 || addr < viewport_.start_ea || addr >= viewport_.end_ea) {
        return -1;
    }
    
    const asize_t range = viewport_.range();
    if (range == 0) return 0;
    
    const double t = static_cast<double>(addr - viewport_.start_ea) / static_cast<double>(range);
    return static_cast<int>(t * width);
}

inline int MinimapData::address_to_x(data_addr_t addr, int width) const {
    return address_to_x_ea(static_cast<ea_t>(addr), width);
}

inline double MinimapData::entropy_at_ea(ea_t addr) const {
    const EntropyBlock* block = block_at(addr);
    return block ? block->entropy : -1.0;
}

inline double MinimapData::entropy_at(data_addr_t addr) const {
    return entropy_at_ea(static_cast<ea_t>(addr));
}

inline const EntropyBlock* MinimapData::block_at(ea_t addr) const {
    // Binary search since blocks are sorted by address
    auto it = std::lower_bound(
        blocks_.begin(), blocks_.end(), addr,
        [](const EntropyBlock& block, ea_t a) { return block.end_ea <= a; }
    );
    
    if (it != blocks_.end() && it->contains(addr)) {
        return &(*it);
    }
    return nullptr;
}

inline const MemoryRegion* MinimapData::region_at(ea_t addr) const {
    for (const auto& region : regions_) {
        if (region.contains(addr)) {
            return &region;
        }
    }
    return nullptr;
}

} // namespace synopsia
