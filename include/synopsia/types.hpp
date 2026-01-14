/// @file types.hpp
/// @brief Core types, constants, and forward declarations for Synopsia plugin

#pragma once

#include <pro.h>
#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <segment.hpp>
#include <bytes.hpp>

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <cmath>

namespace synopsia {

// =============================================================================
// Plugin Constants
// =============================================================================

inline constexpr const char* PLUGIN_NAME = "Synopsia";
inline constexpr const char* PLUGIN_VERSION = "1.0.0";
inline constexpr const char* PLUGIN_COMMENT = "Entropy-based binary minimap";
inline constexpr const char* PLUGIN_HELP = "Visual entropy analysis with click-to-navigate";
inline constexpr const char* DEFAULT_HOTKEY = "Alt+E";

inline constexpr const char* ACTION_NAME = "synopsia:show_minimap";
inline constexpr const char* ACTION_LABEL = "Show Entropy Minimap";
inline constexpr const char* WIDGET_TITLE = "Entropy Minimap";

// =============================================================================
// Entropy Configuration
// =============================================================================

/// Default block size for entropy calculation (bytes)
inline constexpr std::size_t DEFAULT_BLOCK_SIZE = 256;

/// Minimum block size allowed
inline constexpr std::size_t MIN_BLOCK_SIZE = 16;

/// Maximum block size allowed
inline constexpr std::size_t MAX_BLOCK_SIZE = 4096;

/// Shannon entropy maximum value (8 bits per byte = 8.0)
inline constexpr double MAX_ENTROPY = 8.0;

/// Entropy threshold for "high entropy" (typically encrypted/compressed)
inline constexpr double HIGH_ENTROPY_THRESHOLD = 7.0;

/// Entropy threshold for "low entropy" (typically code/data)
inline constexpr double LOW_ENTROPY_THRESHOLD = 4.0;

// =============================================================================
// Visual Configuration
// =============================================================================

/// Default minimap width in pixels
inline constexpr int DEFAULT_MINIMAP_WIDTH = 120;

/// Minimum minimap width
inline constexpr int MIN_MINIMAP_WIDTH = 60;

/// Maximum minimap width  
inline constexpr int MAX_MINIMAP_WIDTH = 400;

/// Height of the cursor indicator line
inline constexpr int CURSOR_LINE_HEIGHT = 2;

/// Margin around the minimap content
inline constexpr int MINIMAP_MARGIN = 4;

// =============================================================================
// Forward Declarations
// =============================================================================

class EntropyCalculator;
class MinimapData;
class MinimapWidget;
class SynopsiaPlugin;

// =============================================================================
// Core Data Types
// =============================================================================

/// Represents a block of data with its calculated entropy
struct EntropyBlock {
    ea_t start_ea;              ///< Start address in the database
    ea_t end_ea;                ///< End address (exclusive)
    double entropy;             ///< Shannon entropy (0.0 to 8.0)
    
    /// Size of the block in bytes
    [[nodiscard]] constexpr asize_t size() const noexcept {
        return end_ea - start_ea;
    }
    
    /// Check if an address falls within this block
    [[nodiscard]] constexpr bool contains(ea_t addr) const noexcept {
        return addr >= start_ea && addr < end_ea;
    }
    
    /// Normalized entropy (0.0 to 1.0)
    [[nodiscard]] constexpr double normalized() const noexcept {
        return entropy / MAX_ENTROPY;
    }
};

/// Represents a contiguous memory region (segment or section)
struct MemoryRegion {
    ea_t start_ea;              ///< Region start address
    ea_t end_ea;                ///< Region end address (exclusive)
    qstring name;               ///< Region name (segment name)
    bool readable;              ///< Can read from this region
    bool initialized;           ///< Contains initialized data
    
    [[nodiscard]] constexpr asize_t size() const noexcept {
        return end_ea - start_ea;
    }
    
    [[nodiscard]] constexpr bool contains(ea_t addr) const noexcept {
        return addr >= start_ea && addr < end_ea;
    }
};

/// Viewport configuration for pan/zoom
struct Viewport {
    ea_t start_ea;              ///< Visible range start
    ea_t end_ea;                ///< Visible range end
    double zoom;                ///< Zoom factor (1.0 = fit to view)
    
    Viewport() : start_ea(0), end_ea(0), zoom(1.0) {}
    
    [[nodiscard]] constexpr asize_t range() const noexcept {
        return end_ea - start_ea;
    }
    
    /// Reset to show entire database
    void reset(ea_t db_start, ea_t db_end) {
        start_ea = db_start;
        end_ea = db_end;
        zoom = 1.0;
    }
};

/// Plugin configuration options
struct PluginConfig {
    std::size_t block_size = DEFAULT_BLOCK_SIZE;
    int minimap_width = DEFAULT_MINIMAP_WIDTH;
    bool show_cursor = true;
    bool show_regions = true;
    bool auto_refresh = true;
    bool vertical_layout = true;  ///< true = vertical bar, false = horizontal
    
    /// Validate and clamp configuration values
    void validate() {
        block_size = std::clamp(block_size, MIN_BLOCK_SIZE, MAX_BLOCK_SIZE);
        minimap_width = std::clamp(minimap_width, MIN_MINIMAP_WIDTH, MAX_MINIMAP_WIDTH);
    }
};

// =============================================================================
// Callback Types
// =============================================================================

/// Callback when user clicks on an address in the minimap
using AddressCallback = std::function<void(ea_t address)>;

/// Callback for minimap refresh requests
using RefreshCallback = std::function<void()>;

// =============================================================================
// Utility Functions
// =============================================================================

/// Get pointer size for current database
[[nodiscard]] inline std::uint32_t get_ptr_size() noexcept {
    return static_cast<std::uint32_t>(inf_is_64bit() ? 8 : 4);
}

/// Check if database is loaded
[[nodiscard]] inline bool is_database_loaded() noexcept {
    return inf_get_min_ea() != BADADDR && inf_get_max_ea() != BADADDR;
}

/// Get database address range
[[nodiscard]] inline std::pair<ea_t, ea_t> get_database_range() noexcept {
    return {inf_get_min_ea(), inf_get_max_ea()};
}

/// Format address for display
[[nodiscard]] inline qstring format_address(ea_t addr) {
    qstring result;
    result.sprnt("%llX", static_cast<unsigned long long>(addr));
    return result;
}

/// Format entropy value for display
[[nodiscard]] inline qstring format_entropy(double entropy) {
    qstring result;
    result.sprnt("%.2f", entropy);
    return result;
}

} // namespace synopsia
