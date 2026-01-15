/// @file plugin.cpp
/// @brief Main IDA plugin entry point with feature registry

#include <synopsia/core/feature_registry.hpp>
#include <synopsia/common/types.hpp>
#include <synopsia/features/entropy_minimap/feature.hpp>
#include <synopsia/features/function_search/feature.hpp>

namespace synopsia {

/// @class SynopsiaPlugin
/// @brief Main plugin class managing all features
class SynopsiaPlugin : public plugmod_t, public event_listener_t {
public:
    SynopsiaPlugin();
    ~SynopsiaPlugin() override;

    // plugmod_t interface
    bool idaapi run(size_t arg) override;

    // event_listener_t interface
    ssize_t idaapi on_event(ssize_t code, va_list va) override;

    // Singleton accessor
    [[nodiscard]] static SynopsiaPlugin* instance() noexcept { return instance_; }

private:
    bool initialize();
    void cleanup();

    FeatureRegistry registry_;
    bool initialized_ = false;

    static SynopsiaPlugin* instance_;
};

// Static instance pointer
SynopsiaPlugin* SynopsiaPlugin::instance_ = nullptr;

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
    // Hook events
    hook_event_listener(HT_UI, this);
    hook_event_listener(HT_VIEW, this);
    hook_event_listener(HT_IDB, this);

    // Register features
    registry_.register_feature(std::make_unique<features::EntropyMinimapFeature>());
    registry_.register_feature(std::make_unique<features::FunctionSearchFeature>());

    // Initialize all features
    std::size_t count = registry_.initialize_all();
    msg("Synopsia %s: Plugin initialized with %zu features\n", PLUGIN_VERSION, count);

    initialized_ = true;
    return true;
}

void SynopsiaPlugin::cleanup() {
    if (!initialized_) return;

    // Cleanup all features
    registry_.cleanup_all();

    // Unhook events
    unhook_event_listener(HT_UI, this);
    unhook_event_listener(HT_VIEW, this);
    unhook_event_listener(HT_IDB, this);

    initialized_ = false;
}

bool SynopsiaPlugin::run(size_t arg) {
    // Run cycles through features, showing the next one
    // For simplicity, just show/toggle based on arg or show first feature
    if (arg < registry_.count()) {
        std::size_t idx = 0;
        registry_.for_each([&idx, arg](IFeature* feature) {
            if (idx == arg) {
                feature->toggle();
            }
            ++idx;
        });
    } else {
        // Default: toggle first feature (entropy minimap)
        registry_.for_each([](IFeature* feature) {
            feature->toggle();
            return; // Only toggle first
        });
    }
    return true;
}

ssize_t SynopsiaPlugin::on_event(ssize_t code, va_list va) {
    // UI events
    if (code == ui_database_closed) {
        registry_.broadcast_database_closed();
        return 0;
    }

    // View events
    if (code == view_curpos) {
        TWidget* view = va_arg(va, TWidget*);
        (void)view;
        ea_t addr = get_screen_ea();
        registry_.broadcast_cursor_changed(addr);
        return 0;
    }

    return 0;
}

// Plugin entry point
plugmod_t* idaapi plugin_init() {
    return new SynopsiaPlugin();
}

} // namespace synopsia

// Plugin export
plugin_t PLUGIN = {
    IDP_INTERFACE_VERSION,
    PLUGIN_MULTI,
    synopsia::plugin_init,
    nullptr,
    nullptr,
    synopsia::PLUGIN_COMMENT,
    synopsia::PLUGIN_HELP,
    synopsia::PLUGIN_NAME,
    ""  // No default hotkey - features have their own
};
