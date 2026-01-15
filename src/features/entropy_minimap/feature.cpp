/// @file feature.cpp
/// @brief Entropy minimap feature implementation

#include <synopsia/features/entropy_minimap/feature.hpp>

#ifdef SYNOPSIA_USE_QT
// Forward declarations for bridge functions
extern "C" {
    void* synopsia_create_minimap_widget(void* parent_widget, void* data_source);
    void synopsia_add_minimap_to_layout(void* parent_widget, void* minimap_widget);
    void synopsia_set_address_callback(void* minimap_widget, void (*callback)(std::uint64_t));
    void synopsia_set_refresh_callback(void* minimap_widget, void (*callback)());
    void synopsia_refresh_widget(void* minimap_widget);
    void synopsia_set_current_address(void* minimap_widget, std::uint64_t addr);
    void synopsia_configure_widget(void* minimap_widget, bool show_cursor,
                                   bool show_regions, bool vertical_layout);
}
#endif

namespace synopsia {
namespace features {

// Static instance pointer
EntropyMinimapFeature* EntropyMinimapFeature::instance_ = nullptr;

// Static callback wrappers
#ifdef SYNOPSIA_USE_QT
static void address_click_callback(std::uint64_t addr) {
    if (auto* feature = EntropyMinimapFeature::instance()) {
        feature->navigate_to(static_cast<ea_t>(addr));
    }
}

static void refresh_callback() {
    if (auto* feature = EntropyMinimapFeature::instance()) {
        feature->refresh_data();
    }
}
#endif

EntropyMinimapFeature::EntropyMinimapFeature() {
    instance_ = this;
}

EntropyMinimapFeature::~EntropyMinimapFeature() {
    cleanup();
    instance_ = nullptr;
}

bool EntropyMinimapFeature::initialize() {
    config_.validate();

    if (!register_actions()) {
        return false;
    }

    data_ = std::make_unique<MinimapData>();
    initialized_ = true;

    msg("Synopsia [%s]: Feature initialized (hotkey: %s)\n",
        entropy_minimap::FEATURE_NAME, entropy_minimap::FEATURE_HOTKEY);

    return true;
}

void EntropyMinimapFeature::cleanup() {
    if (!initialized_) return;

    destroy_widget();
    unregister_actions();
    data_.reset();
    initialized_ = false;
}

bool EntropyMinimapFeature::register_actions() {
    static EntropyMinimapAction action_handler;

    const action_desc_t action_desc = ACTION_DESC_LITERAL(
        entropy_minimap::ACTION_NAME,
        entropy_minimap::ACTION_LABEL,
        &action_handler,
        entropy_minimap::FEATURE_HOTKEY,
        "Show entropy-based binary minimap",
        -1
    );

    if (!register_action(action_desc)) {
        msg("Synopsia [%s]: Failed to register action\n", entropy_minimap::FEATURE_NAME);
        return false;
    }

    attach_action_to_menu("View/", entropy_minimap::ACTION_NAME, SETMENU_APP);
    return true;
}

void EntropyMinimapFeature::unregister_actions() {
    detach_action_from_menu("View/", entropy_minimap::ACTION_NAME);
    unregister_action(entropy_minimap::ACTION_NAME);
}

void EntropyMinimapFeature::show() {
    if (visible_) return;

    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", entropy_minimap::FEATURE_NAME);
        return;
    }

    if (!create_widget()) {
        msg("Synopsia [%s]: Failed to create widget\n", entropy_minimap::FEATURE_NAME);
        return;
    }

    refresh_data();
    visible_ = true;
}

void EntropyMinimapFeature::hide() {
    if (!visible_) return;
    destroy_widget();
    visible_ = false;
}

bool EntropyMinimapFeature::create_widget() {
#ifdef SYNOPSIA_USE_QT
    widget_ = create_empty_widget(entropy_minimap::WIDGET_TITLE);
    if (!widget_) return false;

    content_ = synopsia_create_minimap_widget(widget_, data_.get());
    if (!content_) {
        close_widget(widget_, WCLS_DONT_SAVE_SIZE);
        widget_ = nullptr;
        return false;
    }

    synopsia_add_minimap_to_layout(widget_, content_);
    synopsia_set_address_callback(content_, address_click_callback);
    synopsia_set_refresh_callback(content_, refresh_callback);

    display_widget(widget_, WOPN_DP_RIGHT | WOPN_DP_SZHINT | WOPN_PERSIST);
    set_dock_pos(entropy_minimap::WIDGET_TITLE, nullptr, DP_RIGHT | DP_SZHINT);

    return true;
#else
    msg("Synopsia [%s]: Qt support not available\n", entropy_minimap::FEATURE_NAME);
    return false;
#endif
}

void EntropyMinimapFeature::destroy_widget() {
#ifdef SYNOPSIA_USE_QT
    if (widget_) {
        close_widget(widget_, WCLS_SAVE);
        widget_ = nullptr;
        content_ = nullptr;
    }
#endif
}

void EntropyMinimapFeature::refresh_data() {
    if (!data_) return;

    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", entropy_minimap::FEATURE_NAME);
        return;
    }

    msg("Synopsia [%s]: Analyzing entropy (block size: %zu bytes)...\n",
        entropy_minimap::FEATURE_NAME, config_.block_size);

    if (data_->refresh(config_.block_size)) {
        msg("Synopsia [%s]: Analysis complete (%zu blocks, avg entropy: %.2f)\n",
            entropy_minimap::FEATURE_NAME, data_->block_count(), data_->avg_entropy());

#ifdef SYNOPSIA_USE_QT
        if (content_) {
            synopsia_refresh_widget(content_);
        }
#endif
    } else {
        msg("Synopsia [%s]: Failed to analyze entropy\n", entropy_minimap::FEATURE_NAME);
    }
}

void EntropyMinimapFeature::set_config(const PluginConfig& config) {
    config_ = config;
    config_.validate();

    if (data_ && data_->is_valid() && data_->block_size() != config_.block_size) {
        refresh_data();
    }

#ifdef SYNOPSIA_USE_QT
    if (content_) {
        synopsia_configure_widget(content_, config_.show_cursor,
                                  config_.show_regions, config_.vertical_layout);
    }
#endif
}

void EntropyMinimapFeature::on_cursor_changed(ea_t addr) {
    if (addr == last_cursor_addr_) return;
    last_cursor_addr_ = addr;

#ifdef SYNOPSIA_USE_QT
    if (content_ && config_.show_cursor) {
        synopsia_set_current_address(content_, static_cast<std::uint64_t>(addr));
    }
#endif
}

void EntropyMinimapFeature::on_database_closed() {
    destroy_widget();
    if (data_) {
        data_->invalidate();
    }
    visible_ = false;
}

void EntropyMinimapFeature::on_database_modified() {
    if (data_) {
        data_->invalidate();
    }
    if (config_.auto_refresh && visible_) {
        refresh_data();
    }
}

void EntropyMinimapFeature::navigate_to(ea_t addr) {
    if (addr == BADADDR) return;
    jumpto(addr);
}

// Action handler implementation
int EntropyMinimapAction::activate(action_activation_ctx_t*) {
    if (auto* feature = EntropyMinimapFeature::instance()) {
        feature->toggle();
    }
    return 1;
}

action_state_t EntropyMinimapAction::update(action_update_ctx_t*) {
    return AST_ENABLE_ALWAYS;
}

} // namespace features
} // namespace synopsia
