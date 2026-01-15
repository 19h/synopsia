/// @file function_data.hpp
/// @brief Function data model for function search feature

#pragma once

#include "data_interface.hpp"
#include <synopsia/common/types.hpp>

namespace synopsia {
namespace features {
namespace function_search {

/// @class FunctionData
/// @brief Manages function list data from IDA database
class FunctionData : public IFunctionDataSource {
public:
    FunctionData() = default;
    ~FunctionData() override = default;

    // Non-copyable, non-movable
    FunctionData(const FunctionData&) = delete;
    FunctionData& operator=(const FunctionData&) = delete;
    FunctionData(FunctionData&&) = delete;
    FunctionData& operator=(FunctionData&&) = delete;

    // IFunctionDataSource interface
    [[nodiscard]] bool is_valid() const override { return valid_; }
    [[nodiscard]] std::size_t function_count() const override { return functions_.size(); }
    [[nodiscard]] FunctionInfo get_function(std::size_t index) const override;
    [[nodiscard]] std::string get_disassembly(func_addr_t address) const override;
    bool refresh() override;

private:
    struct FunctionEntry {
        ea_t address;
        qstring name;
        qstring demangled_name;
    };

    std::vector<FunctionEntry> functions_;
    bool valid_ = false;
};

// =============================================================================
// Inline Implementation
// =============================================================================

inline FunctionInfo FunctionData::get_function(std::size_t index) const {
    if (index >= functions_.size()) {
        return {FUNC_BADADDR, "", ""};
    }
    const auto& f = functions_[index];
    return {
        static_cast<func_addr_t>(f.address),
        std::string(f.name.c_str()),
        std::string(f.demangled_name.c_str())
    };
}

} // namespace function_search
} // namespace features
} // namespace synopsia
