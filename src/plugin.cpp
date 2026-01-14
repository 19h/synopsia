/// @file plugin.cpp
/// @brief Main IDA plugin entry point and implementation
///
/// This file contains IDA plugin code ONLY - no Qt headers are included here.
/// Qt widget operations are handled through the widget_bridge functions.

#include <synopsia/plugin.hpp>

#ifdef SYNOPSIA_USE_QT
// Forward declarations for bridge functions (defined in widget_bridge.cpp)
extern "C" {
    void* synopsia_create_minimap_widget(void* parent_widget, void* data_source);
    void synopsia_add_widget_to_layout(void* parent_widget, void* minimap_widget);
    void synopsia_set_address_callback(void* minimap_widget, void (*callback)(std::uint64_t));
    void synopsia_set_refresh_callback(void* minimap_widget, void (*callback)());
    void synopsia_refresh_widget(void* minimap_widget);
    void synopsia_set_current_address(void* minimap_widget, std::uint64_t addr);
    void synopsia_configure_widget(void* minimap_widget, bool show_cursor, 
                                   bool show_regions, bool vertical_layout);
    void synopsia_set_visible_range(void* minimap_widget, std::uint64_t start, std::uint64_t end);
}
#endif

namespace synopsia {

// Static instance pointer
SynopsiaPlugin* SynopsiaPlugin::instance_ = nullptr;

// Static callback wrappers for bridge functions
#ifdef SYNOPSIA_USE_QT
static void address_click_callback(std::uint64_t addr) {
    if (SynopsiaPlugin* plugin = SynopsiaPlugin::instance()) {
        plugin->navigate_to(static_cast<ea_t>(addr));
    }
}

static void refresh_callback() {
    if (SynopsiaPlugin* plugin = SynopsiaPlugin::instance()) {
        plugin->refresh_data();
    }
}
#endif

// =============================================================================
// SynopsiaPlugin Implementation
// =============================================================================

SynopsiaPlugin::SynopsiaPlugin() {
    instance_ = this;
    
    if (!initialize()) {
        msg("Synopsia: Failed to initialize plugin\n");
    }
}

SynopsiaPlugin::~SynopsiaPlugin() {
    cleanup();
    instance_ = nullptr;
}

bool SynopsiaPlugin::initialize() {
    // Validate configuration
    config_.validate();
    
    // Register actions
    if (!register_actions()) {
        return false;
    }
    
    // Hook UI events
    hook_event_listener(HT_UI, this);
    hook_event_listener(HT_VIEW, this);
    hook_event_listener(HT_IDB, this);
    
    // Create data model
    data_ = std::make_unique<MinimapData>();
    
    initialized_ = true;
    
    msg("Synopsia %s: Plugin initialized (hotkey: %s)\n", 
        PLUGIN_VERSION, DEFAULT_HOTKEY);
    
    return true;
}

void SynopsiaPlugin::cleanup() {
    if (!initialized_) return;
    
    // Destroy widget first
    destroy_widget();
    
    // Unhook events
    unhook_event_listener(HT_UI, this);
    unhook_event_listener(HT_VIEW, this);
    unhook_event_listener(HT_IDB, this);
    
    // Unregister actions
    unregister_actions();
    
    // Clear data
    data_.reset();
    
    initialized_ = false;
}

bool SynopsiaPlugin::register_actions() {
    // Create action handler
    static ShowMinimapAction action_handler;
    
    // Register show minimap action
    const action_desc_t action_desc = ACTION_DESC_LITERAL(
        ACTION_NAME,
        ACTION_LABEL,
        &action_handler,
        DEFAULT_HOTKEY,
        "Show entropy-based binary minimap",
        -1
    );
    
    if (!register_action(action_desc)) {
        msg("Synopsia: Failed to register action\n");
        return false;
    }
    
    // Attach to View menu
    attach_action_to_menu("View/", ACTION_NAME, SETMENU_APP);
    
    return true;
}

void SynopsiaPlugin::unregister_actions() {
    detach_action_from_menu("View/", ACTION_NAME);
    unregister_action(ACTION_NAME);
}

bool SynopsiaPlugin::run(size_t arg) {
    // Toggle minimap visibility
    toggle_minimap();
    return true;
}

ssize_t SynopsiaPlugin::on_event(ssize_t code, va_list va) {
    // This callback handles multiple event types (HT_UI, HT_VIEW, HT_IDB)
    
    // UI events (ui_notification_t)
    if (code == ui_database_closed) {
        destroy_widget();
        if (data_) {
            data_->invalidate();
        }
        return 0;
    }
    
    // View events (view_notification_t) 
    if (code == view_curpos) {
        TWidget* view = va_arg(va, TWidget*);
        (void)view;
        ea_t addr = get_screen_ea();
        on_cursor_changed(addr);
        return 0;
    }
    
    return 0;
}

void SynopsiaPlugin::show_minimap() {
    if (widget_visible_) return;
    
    // Check if database is loaded before creating widget
    if (!is_database_loaded()) {
        msg("Synopsia: No database loaded. Please open a file first.\n");
        return;
    }
    
    if (!create_widget()) {
        msg("Synopsia: Failed to create minimap widget\n");
        return;
    }
    
    // Refresh data
    refresh_data();
    
    widget_visible_ = true;
}

void SynopsiaPlugin::hide_minimap() {
    if (!widget_visible_) return;
    
    destroy_widget();
    widget_visible_ = false;
}

void SynopsiaPlugin::toggle_minimap() {
    if (widget_visible_) {
        hide_minimap();
    } else {
        show_minimap();
    }
}

bool SynopsiaPlugin::create_widget() {
#ifdef SYNOPSIA_USE_QT
    // Create IDA widget
    widget_ = create_empty_widget(WIDGET_TITLE);
    if (!widget_) {
        return false;
    }
    
    // Create minimap widget via bridge
    content_ = synopsia_create_minimap_widget(widget_, data_.get());
    if (!content_) {
        close_widget(widget_, WCLS_DONT_SAVE_SIZE);
        widget_ = nullptr;
        return false;
    }
    
    // Add to layout
    synopsia_add_widget_to_layout(widget_, content_);
    
    // Set up callbacks
    synopsia_set_address_callback(content_, address_click_callback);
    synopsia_set_refresh_callback(content_, refresh_callback);
    
    // Display the widget docked to the right side
    // Use SZHINT to respect the widget's sizeHint() for initial sizing
    display_widget(widget_, WOPN_DP_RIGHT | WOPN_DP_SZHINT | WOPN_PERSIST);
    
    // Explicitly set dock position to the right of the main IDA view
    set_dock_pos(WIDGET_TITLE, "IDA View-A", DP_RIGHT | DP_SZHINT);
    
    return true;
#else
    msg("Synopsia: Qt support not available\n");
    return false;
#endif
}

void SynopsiaPlugin::destroy_widget() {
#ifdef SYNOPSIA_USE_QT
    if (widget_) {
        close_widget(widget_, WCLS_SAVE);
        widget_ = nullptr;
        content_ = nullptr;
    }
#endif
}

void SynopsiaPlugin::refresh_data() {
    if (!data_) return;
    
    // Check if database is loaded
    if (!is_database_loaded()) {
        msg("Synopsia: No database loaded\n");
        return;
    }
    
    msg("Synopsia: Analyzing entropy (block size: %zu bytes)...\n", 
        config_.block_size);
    
    // Refresh entropy data
    if (data_->refresh(config_.block_size)) {
        msg("Synopsia: Analysis complete (%zu blocks, avg entropy: %.2f)\n",
            data_->block_count(), data_->avg_entropy());
        
        // Update widget
#ifdef SYNOPSIA_USE_QT
        if (content_) {
            synopsia_refresh_widget(content_);
        }
#endif
    } else {
        msg("Synopsia: Failed to analyze entropy\n");
    }
}

void SynopsiaPlugin::set_config(const PluginConfig& config) {
    config_ = config;
    config_.validate();
    
    // Re-analyze if block size changed
    if (data_ && data_->is_valid() && 
        data_->block_size() != config_.block_size) {
        refresh_data();
    }
    
    // Update widget settings
#ifdef SYNOPSIA_USE_QT
    if (content_) {
        synopsia_configure_widget(content_, config_.show_cursor, 
                                  config_.show_regions, config_.vertical_layout);
    }
#endif
}

void SynopsiaPlugin::on_cursor_changed(ea_t addr) {
    if (addr == last_cursor_addr_) return;
    last_cursor_addr_ = addr;
    
#ifdef SYNOPSIA_USE_QT
    if (content_) {
        // Update widget cursor position
        if (config_.show_cursor) {
            synopsia_set_current_address(content_, static_cast<std::uint64_t>(addr));
        }
        
        // Update visible range for viewport frame
        // Estimate visible range as ~2KB around cursor (typical view shows ~50-100 lines)
        // This could be improved by querying the actual view state
        constexpr ea_t VISIBLE_RANGE_ESTIMATE = 0x800; // 2KB
        ea_t vis_start = (addr > VISIBLE_RANGE_ESTIMATE) ? addr - VISIBLE_RANGE_ESTIMATE : 0;
        ea_t vis_end = addr + VISIBLE_RANGE_ESTIMATE;
        
        // Clamp to database range
        if (data_ && data_->is_valid()) {
            auto [db_start, db_end] = data_->address_range();
            vis_start = std::max(vis_start, db_start);
            vis_end = std::min(vis_end, db_end);
        }
        
        synopsia_set_visible_range(content_, 
            static_cast<std::uint64_t>(vis_start),
            static_cast<std::uint64_t>(vis_end));
    }
#endif
}

void SynopsiaPlugin::on_database_modified() {
    // Invalidate cached data
    if (data_) {
        data_->invalidate();
    }
    
    // Refresh if auto-refresh is enabled and widget is visible
    if (config_.auto_refresh && widget_visible_) {
        refresh_data();
    }
}

void SynopsiaPlugin::navigate_to(ea_t addr) {
    if (addr == BADADDR) return;
    
    // Jump to address in IDA
    jumpto(addr);
}

// =============================================================================
// ShowMinimapAction Implementation
// =============================================================================

int ShowMinimapAction::activate(action_activation_ctx_t* ctx) {
    if (SynopsiaPlugin* plugin = SynopsiaPlugin::instance()) {
        plugin->toggle_minimap();
    }
    return 1;
}

action_state_t ShowMinimapAction::update(action_update_ctx_t* ctx) {
    // Action is always available when plugin is loaded
    return AST_ENABLE_ALWAYS;
}

// =============================================================================
// Plugin Entry Points
// =============================================================================

plugmod_t* idaapi plugin_init() {
    return new SynopsiaPlugin();
}

} // namespace synopsia

// =============================================================================
// Plugin Export
// =============================================================================

plugin_t PLUGIN = {
    IDP_INTERFACE_VERSION,
    PLUGIN_MULTI,                               // Plugin flags
    synopsia::plugin_init,                      // Initialize
    nullptr,                                    // Terminate (handled by destructor)
    nullptr,                                    // Run (handled by plugmod_t::run)
    synopsia::PLUGIN_COMMENT,                   // Comment
    synopsia::PLUGIN_HELP,                      // Help
    synopsia::PLUGIN_NAME,                      // Wanted name
    synopsia::DEFAULT_HOTKEY                    // Wanted hotkey
};
