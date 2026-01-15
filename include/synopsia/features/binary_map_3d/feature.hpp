/// @file feature.hpp
/// @brief 3D Binary Map feature implementation

#pragma once

#include <synopsia/core/feature_base.hpp>
#include <synopsia/common/types.hpp>
#include "map_data.hpp"
#include <memory>

namespace synopsia {
namespace features {

/// Feature constants
namespace binary_map_3d {
inline constexpr const char* FEATURE_ID = "binary_map_3d";
inline constexpr const char* FEATURE_NAME = "3D Binary Map";
inline constexpr const char* FEATURE_DESCRIPTION = "3D visualization with call depth and Hilbert curve layout";
inline constexpr const char* FEATURE_HOTKEY = "Alt+3";
inline constexpr const char* ACTION_NAME = "synopsia:binary_map_3d";
inline constexpr const char* ACTION_LABEL = "3D Binary Map";
inline constexpr const char* WIDGET_TITLE = "3D Binary Map";
} // namespace binary_map_3d

/// @class BinaryMap3DFeature
/// @brief 3D Binary Map feature implementation
class BinaryMap3DFeature : public FeatureBase {
public:
    BinaryMap3DFeature();
    ~BinaryMap3DFeature() override;

    // IFeature interface
    [[nodiscard]] const char* id() const noexcept override {
        return binary_map_3d::FEATURE_ID;
    }
    [[nodiscard]] const char* name() const noexcept override {
        return binary_map_3d::FEATURE_NAME;
    }
    [[nodiscard]] const char* description() const noexcept override {
        return binary_map_3d::FEATURE_DESCRIPTION;
    }
    [[nodiscard]] const char* hotkey() const noexcept override {
        return binary_map_3d::FEATURE_HOTKEY;
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
    [[nodiscard]] static BinaryMap3DFeature* instance() noexcept { return instance_; }

private:
    bool create_widget();
    void destroy_widget();
    bool register_actions();
    void unregister_actions();

    std::unique_ptr<binary_map_3d::BinaryMapData> data_;
    static BinaryMap3DFeature* instance_;
};

/// @class BinaryMap3DAction
/// @brief Action handler for showing the 3D binary map
class BinaryMap3DAction : public action_handler_t {
public:
    int idaapi activate(action_activation_ctx_t* ctx) override;
    action_state_t idaapi update(action_update_ctx_t* ctx) override;
};

} // namespace features
} // namespace synopsia
