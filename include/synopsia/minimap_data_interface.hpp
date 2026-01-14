/// @file minimap_data_interface.hpp
/// @brief Abstract interface for minimap data (Qt-compatible, no IDA dependencies)
///
/// This header defines an interface that can be used by Qt code without
/// including any IDA headers. The actual implementation in MinimapData
/// uses IDA types internally.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace synopsia {

// Qt-compatible address type (matches ea_t but without IDA headers)
using data_addr_t = std::uint64_t;
using data_size_t = std::uint64_t;
using data_sval_t = std::int64_t;

inline constexpr data_addr_t DATA_BADADDR = static_cast<data_addr_t>(-1);

/// Entropy block data for Qt (mirrors EntropyBlock without IDA types)
struct EntropyBlockData {
    data_addr_t start_addr;
    data_addr_t end_addr;
    double entropy;
    
    [[nodiscard]] constexpr data_size_t size() const noexcept {
        return end_addr - start_addr;
    }
    
    [[nodiscard]] constexpr bool contains(data_addr_t addr) const noexcept {
        return addr >= start_addr && addr < end_addr;
    }
};

/// Region data for Qt (mirrors MemoryRegion without IDA types)
struct RegionData {
    data_addr_t start_addr;
    data_addr_t end_addr;
    
    [[nodiscard]] constexpr data_size_t size() const noexcept {
        return end_addr - start_addr;
    }
};

/// Viewport data for Qt
struct ViewportData {
    data_addr_t start_addr;
    data_addr_t end_addr;
    double zoom;
    
    [[nodiscard]] constexpr data_size_t range() const noexcept {
        return end_addr - start_addr;
    }
};

/// @class IMinimapDataSource
/// @brief Abstract interface for minimap data source
///
/// This interface allows the Qt widget to access minimap data without
/// depending on IDA types. The actual MinimapData class implements this.
class IMinimapDataSource {
public:
    virtual ~IMinimapDataSource() = default;
    
    // Data validity
    [[nodiscard]] virtual bool is_valid() const = 0;
    
    // Blocks access
    [[nodiscard]] virtual std::size_t block_count() const = 0;
    [[nodiscard]] virtual EntropyBlockData get_block(std::size_t index) const = 0;
    
    // Regions access
    [[nodiscard]] virtual std::size_t region_count() const = 0;
    [[nodiscard]] virtual RegionData get_region(std::size_t index) const = 0;
    
    /// Get the name of a region by index
    [[nodiscard]] virtual std::string get_region_name_at(std::size_t index) const = 0;
    
    /// Get the name of the region containing the given address
    [[nodiscard]] virtual std::string get_region_name(data_addr_t addr) const = 0;
    
    // Viewport
    [[nodiscard]] virtual ViewportData get_viewport() const = 0;
    
    // Coordinate transformation
    [[nodiscard]] virtual data_addr_t y_to_address(int y, int height) const = 0;
    [[nodiscard]] virtual data_addr_t x_to_address(int x, int width) const = 0;
    [[nodiscard]] virtual int address_to_y(data_addr_t addr, int height) const = 0;
    [[nodiscard]] virtual int address_to_x(data_addr_t addr, int width) const = 0;
    
    // Entropy query
    [[nodiscard]] virtual double entropy_at(data_addr_t addr) const = 0;
    
    // Viewport control
    virtual void zoom(double factor, data_addr_t center) = 0;
    virtual void pan(data_sval_t delta) = 0;
};

} // namespace synopsia
