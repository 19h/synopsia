/// @file feature.hpp
/// @brief Function search feature implementation

#pragma once

#include <synopsia/core/feature_base.hpp>
#include <synopsia/common/types.hpp>
#include "function_data.hpp"
#include <memory>

namespace synopsia {
namespace features {

/// Feature constants
namespace function_search {
inline constexpr const char* FEATURE_ID = "function_search";
inline constexpr const char* FEATURE_NAME = "Function Search";
inline constexpr const char* FEATURE_DESCRIPTION = "Search and browse functions with disassembly viewer";
inline constexpr const char* FEATURE_HOTKEY = "Alt+F";
inline constexpr const char* ACTION_NAME = "synopsia:function_search";
inline constexpr const char* ACTION_LABEL = "Function Search";
inline constexpr const char* WIDGET_TITLE = "Function Search";
} // namespace function_search

/// @class FunctionSearchFeature
/// @brief Function search feature implementation
class FunctionSearchFeature : public FeatureBase {
public:
    FunctionSearchFeature();
    ~FunctionSearchFeature() override;

    // IFeature interface
    [[nodiscard]] const char* id() const noexcept override {
        return function_search::FEATURE_ID;
    }
    [[nodiscard]] const char* name() const noexcept override {
        return function_search::FEATURE_NAME;
    }
    [[nodiscard]] const char* description() const noexcept override {
        return function_search::FEATURE_DESCRIPTION;
    }
    [[nodiscard]] const char* hotkey() const noexcept override {
        return function_search::FEATURE_HOTKEY;
    }

    bool initialize() override;
    void cleanup() override;
    void show() override;
    void hide() override;

    void on_database_closed() override;

    // Feature-specific methods
    void refresh_data();
    void navigate_to(ea_t addr);

    // Singleton accessor
    [[nodiscard]] static FunctionSearchFeature* instance() noexcept { return instance_; }

private:
    bool create_widget();
    void destroy_widget();
    bool register_actions();
    void unregister_actions();

    std::unique_ptr<function_search::FunctionData> data_;
    static FunctionSearchFeature* instance_;
};

/// @class FunctionSearchAction
/// @brief Action handler for showing the function search
class FunctionSearchAction : public action_handler_t {
public:
    int idaapi activate(action_activation_ctx_t* ctx) override;
    action_state_t idaapi update(action_update_ctx_t* ctx) override;
};

} // namespace features
} // namespace synopsia
