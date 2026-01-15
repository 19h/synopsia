/// @file data_interface.hpp
/// @brief Abstract interface for function search data (Qt-compatible, no IDA dependencies)

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace synopsia {
namespace features {
namespace function_search {

/// Qt-compatible address type
using func_addr_t = std::uint64_t;
inline constexpr func_addr_t FUNC_BADADDR = static_cast<func_addr_t>(-1);

/// Function information for Qt widget
struct FunctionInfo {
    func_addr_t address;
    std::string name;
    std::string demangled_name;

    [[nodiscard]] bool has_demangled() const noexcept {
        return !demangled_name.empty() && demangled_name != name;
    }
};

/// @class IFunctionDataSource
/// @brief Abstract interface for function data source
///
/// This interface allows the Qt widget to access function data without
/// depending on IDA types.
class IFunctionDataSource {
public:
    virtual ~IFunctionDataSource() = default;

    /// Check if data is valid
    [[nodiscard]] virtual bool is_valid() const = 0;

    /// Get total number of functions
    [[nodiscard]] virtual std::size_t function_count() const = 0;

    /// Get function at index
    [[nodiscard]] virtual FunctionInfo get_function(std::size_t index) const = 0;

    /// Get disassembly for function at address
    [[nodiscard]] virtual std::string get_disassembly(func_addr_t address) const = 0;

    /// Get decompiled pseudocode for function at address (requires Hex-Rays)
    [[nodiscard]] virtual std::string get_decompilation(func_addr_t address) const = 0;

    /// Check if decompiler is available
    [[nodiscard]] virtual bool has_decompiler() const = 0;

    /// Find function by name (returns FUNC_BADADDR if not found)
    [[nodiscard]] virtual func_addr_t find_function_by_name(const std::string& name) const = 0;

    /// Find function containing address (returns FUNC_BADADDR if not in any function)
    [[nodiscard]] virtual func_addr_t find_function_at(func_addr_t address) const = 0;

    /// Refresh function list from database
    virtual bool refresh() = 0;
};

} // namespace function_search
} // namespace features
} // namespace synopsia
