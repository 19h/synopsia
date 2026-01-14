/// @file plugin.hpp
/// @brief Main plugin class and IDA integration

#pragma once

#include "types.hpp"
#include "minimap_data.hpp"
// Note: minimap_widget.hpp is NOT included here to avoid Qt/IDA header conflicts
// Widget operations are done through bridge functions in plugin.cpp

namespace synopsia {

/// @class SynopsiaPlugin
/// @brief Main IDA plugin class for the entropy minimap
///
/// This class handles:
/// - Plugin lifecycle (init, run, term)
/// - IDA event handling (database changes, cursor movement)
/// - UI creation and management
/// - Action registration
class SynopsiaPlugin : public plugmod_t, public event_listener_t {
public:
    SynopsiaPlugin();
    ~SynopsiaPlugin() override;
    
    // Non-copyable, non-movable
    SynopsiaPlugin(const SynopsiaPlugin&) = delete;
    SynopsiaPlugin& operator=(const SynopsiaPlugin&) = delete;
    SynopsiaPlugin(SynopsiaPlugin&&) = delete;
    SynopsiaPlugin& operator=(SynopsiaPlugin&&) = delete;
    
    // =========================================================================
    // plugmod_t Interface
    // =========================================================================
    
    /// Called when plugin is executed (e.g., from menu or hotkey)
    bool idaapi run(size_t arg) override;
    
    // =========================================================================
    // event_listener_t Interface
    // =========================================================================
    
    /// Handle IDA events (database changes, UI events, etc.)
    ssize_t idaapi on_event(ssize_t code, va_list va) override;
    
    // =========================================================================
    // UI Management
    // =========================================================================
    
    /// Show the minimap widget
    void show_minimap();
    
    /// Hide the minimap widget
    void hide_minimap();
    
    /// Toggle minimap visibility
    void toggle_minimap();
    
    /// Check if minimap is visible
    [[nodiscard]] bool is_minimap_visible() const noexcept { return widget_visible_; }
    
    /// Refresh minimap data
    void refresh_data();
    
    /// Navigate to address (called from widget via bridge)
    void navigate_to(ea_t addr);
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// Get current plugin configuration
    [[nodiscard]] const PluginConfig& config() const noexcept { return config_; }
    
    /// Set plugin configuration
    void set_config(const PluginConfig& config);
    
    // =========================================================================
    // Static Instance
    // =========================================================================
    
    /// Get the singleton plugin instance (may be null if not loaded)
    [[nodiscard]] static SynopsiaPlugin* instance() noexcept { return instance_; }
    
private:
    // =========================================================================
    // Internal Helpers
    // =========================================================================
    
    /// Initialize the plugin (called from constructor)
    bool initialize();
    
    /// Cleanup resources (called from destructor)
    void cleanup();
    
    /// Create the minimap widget
    bool create_widget();
    
    /// Destroy the minimap widget
    void destroy_widget();
    
    /// Register plugin actions
    bool register_actions();
    
    /// Unregister plugin actions
    void unregister_actions();
    
    /// Handle cursor position changes
    void on_cursor_changed(ea_t addr);
    
    /// Handle database modifications
    void on_database_modified();
    
    // =========================================================================
    // Member Data
    // =========================================================================
    
    // Configuration
    PluginConfig config_;
    
    // Data model
    std::unique_ptr<MinimapData> data_;
    
    // UI state
    TWidget* widget_ = nullptr;
    void* content_ = nullptr;  // Opaque pointer to MinimapWidget (used via bridge)
    bool widget_visible_ = false;
    bool initialized_ = false;
    
    // Current state
    ea_t last_cursor_addr_ = BADADDR;
    
    // Singleton instance
    static SynopsiaPlugin* instance_;
};

/// @class ShowMinimapAction
/// @brief Action handler for showing the entropy minimap
class ShowMinimapAction : public action_handler_t {
public:
    int idaapi activate(action_activation_ctx_t* ctx) override;
    action_state_t idaapi update(action_update_ctx_t* ctx) override;
};

// =============================================================================
// Plugin Entry Points
// =============================================================================

/// Plugin initialization
plugmod_t* idaapi plugin_init();

} // namespace synopsia

// =============================================================================
// Plugin Export
// =============================================================================

extern plugin_t PLUGIN;
