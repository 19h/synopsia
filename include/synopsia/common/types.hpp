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
inline constexpr const char* PLUGIN_COMMENT = "Multi-feature binary analysis toolkit";
inline constexpr const char* PLUGIN_HELP = "Visual binary analysis with multiple analysis features";

// =============================================================================
// Callback Types
// =============================================================================

/// Callback when user clicks on an address
using AddressCallback = std::function<void(ea_t address)>;

/// Callback for refresh requests
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

} // namespace synopsia
