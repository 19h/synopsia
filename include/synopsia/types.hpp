/// @file types.hpp
/// @brief Core types, constants, and forward declarations for Synopsia plugin
/// @note This file is kept for backward compatibility with existing code.
///       New code should include synopsia/common/types.hpp directly.

#pragma once

#include <synopsia/common/types.hpp>

#include <memory>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <algorithm>
#include <cmath>

namespace synopsia {

// =============================================================================
// Entropy Minimap Constants (feature-specific)
// =============================================================================

inline constexpr const char* DEFAULT_HOTKEY = "Alt+E";

inline constexpr const char* ACTION_NAME = "synopsia:show_minimap";
inline constexpr const char* ACTION_LABEL = "Show JS Minimap";
inline constexpr const char* WIDGET_TITLE = "JS Minimap";

// =============================================================================
// JS Divergence Configuration
// =============================================================================

/// Default block size for JS divergence calculation (bytes)
inline constexpr std::size_t DEFAULT_BLOCK_SIZE = 256;

/// Minimum block size allowed
inline constexpr std::size_t MIN_BLOCK_SIZE = 16;

/// Maximum block size allowed
inline constexpr std::size_t MAX_BLOCK_SIZE = 4096;

/// Maximum JS divergence value (scaled to 8.0 for visualization compatibility)
inline constexpr double MAX_ENTROPY = 8.0;

/// Threshold for "high randomness" (close to uniform distribution)
inline constexpr double HIGH_ENTROPY_THRESHOLD = 7.0;

/// Threshold for "low randomness" (structured/repetitive data)
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
// Utility Functions (entropy-specific)
// =============================================================================

/// Format entropy value for display
[[nodiscard]] inline qstring format_entropy(double entropy) {
    qstring result;
    result.sprnt("%.2f", entropy);
    return result;
}

} // namespace synopsia
