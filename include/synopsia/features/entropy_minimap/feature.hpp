/// @file feature.hpp
/// @brief Entropy minimap feature implementation

#pragma once

#include <synopsia/core/feature_base.hpp>
#include <synopsia/types.hpp>
#include <synopsia/minimap_data.hpp>
#include <memory>

namespace synopsia {
namespace features {

/// Feature constants
namespace entropy_minimap {
inline constexpr const char* FEATURE_ID = "entropy_minimap";
inline constexpr const char* FEATURE_NAME = "Entropy Minimap";
inline constexpr const char* FEATURE_DESCRIPTION = "Visual JS divergence analysis with click-to-navigate";
inline constexpr const char* FEATURE_HOTKEY = "Alt+E";
inline constexpr const char* ACTION_NAME = "synopsia:entropy_minimap";
inline constexpr const char* ACTION_LABEL = "Show JS Minimap";
inline constexpr const char* WIDGET_TITLE = "JS Minimap";
} // namespace entropy_minimap

/// @class EntropyMinimapFeature
/// @brief Entropy minimap feature implementation
class EntropyMinimapFeature : public FeatureBase {
public:
    EntropyMinimapFeature();
    ~EntropyMinimapFeature() override;

    // IFeature interface
    [[nodiscard]] const char* id() const noexcept override {
        return entropy_minimap::FEATURE_ID;
    }
    [[nodiscard]] const char* name() const noexcept override {
        return entropy_minimap::FEATURE_NAME;
    }
    [[nodiscard]] const char* description() const noexcept override {
        return entropy_minimap::FEATURE_DESCRIPTION;
    }
    [[nodiscard]] const char* hotkey() const noexcept override {
        return entropy_minimap::FEATURE_HOTKEY;
    }

    bool initialize() override;
    void cleanup() override;
    void show() override;
    void hide() override;

    void on_cursor_changed(ea_t addr) override;
    void on_database_closed() override;
    void on_database_modified() override;

    // Feature-specific methods
    void refresh_data();
    void navigate_to(ea_t addr);
    [[nodiscard]] const PluginConfig& config() const noexcept { return config_; }
    void set_config(const PluginConfig& config);

    // Singleton accessor
    [[nodiscard]] static EntropyMinimapFeature* instance() noexcept { return instance_; }

private:
    bool create_widget();
    void destroy_widget();
    bool register_actions();
    void unregister_actions();

    std::unique_ptr<MinimapData> data_;
    PluginConfig config_;
    ea_t last_cursor_addr_ = BADADDR;

    static EntropyMinimapFeature* instance_;
};

/// @class EntropyMinimapAction
/// @brief Action handler for showing the entropy minimap
class EntropyMinimapAction : public action_handler_t {
public:
    int idaapi activate(action_activation_ctx_t* ctx) override;
    action_state_t idaapi update(action_update_ctx_t* ctx) override;
};

} // namespace features
} // namespace synopsia
