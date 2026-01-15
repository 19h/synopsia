/// @file feature.cpp
/// @brief 3D Binary Map feature implementation

#include <synopsia/features/binary_map_3d/feature.hpp>

// Bridge functions for ImGui widget
extern "C" {
    void* synopsia_imgui_create_widget(
        const char* ini_prefix,
        void (*render_callback)(void* user_data),
        void* user_data
    );
    void synopsia_imgui_destroy_widget(void* widget);
    void synopsia_add_widget_to_layout(void* parent, void* child);
}

namespace synopsia {
namespace features {

// Forward declarations for imgui_widget.cpp functions
namespace binary_map_3d {
    void init_binary_map_3d_state();
    void cleanup_binary_map_3d_state();
    void refresh_binary_map_3d_data();
    void render_binary_map_3d();
}

// Static instance pointer
BinaryMap3DFeature* BinaryMap3DFeature::instance_ = nullptr;

// Render callback thunk
static void render_callback(void*) {
    binary_map_3d::render_binary_map_3d();
}

BinaryMap3DFeature::BinaryMap3DFeature() {
    instance_ = this;
}

BinaryMap3DFeature::~BinaryMap3DFeature() {
    cleanup();
    instance_ = nullptr;
}

bool BinaryMap3DFeature::initialize() {
    if (!register_actions()) {
        return false;
    }

    data_ = std::make_unique<binary_map_3d::BinaryMapData>();
    initialized_ = true;

    msg("Synopsia [%s]: Feature initialized (hotkey: %s)\n",
        binary_map_3d::FEATURE_NAME, binary_map_3d::FEATURE_HOTKEY);

    return true;
}

void BinaryMap3DFeature::cleanup() {
    if (!initialized_) return;

    destroy_widget();
    unregister_actions();
    data_.reset();
    binary_map_3d::cleanup_binary_map_3d_state();
    initialized_ = false;
}

bool BinaryMap3DFeature::register_actions() {
    static BinaryMap3DAction action_handler;

    const action_desc_t action_desc = ACTION_DESC_LITERAL(
        binary_map_3d::ACTION_NAME,
        binary_map_3d::ACTION_LABEL,
        &action_handler,
        binary_map_3d::FEATURE_HOTKEY,
        "3D visualization with call depth and Hilbert curve layout",
        -1
    );

    if (!register_action(action_desc)) {
        msg("Synopsia [%s]: Failed to register action\n", binary_map_3d::FEATURE_NAME);
        return false;
    }

    attach_action_to_menu("View/", binary_map_3d::ACTION_NAME, SETMENU_APP);
    return true;
}

void BinaryMap3DFeature::unregister_actions() {
    detach_action_from_menu("View/", binary_map_3d::ACTION_NAME);
    unregister_action(binary_map_3d::ACTION_NAME);
}

void BinaryMap3DFeature::show() {
    if (visible_) return;

    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", binary_map_3d::FEATURE_NAME);
        return;
    }

    if (!create_widget()) {
        msg("Synopsia [%s]: Failed to create widget\n", binary_map_3d::FEATURE_NAME);
        return;
    }

    refresh_data();
    visible_ = true;
}

void BinaryMap3DFeature::hide() {
    if (!visible_) return;
    destroy_widget();
    visible_ = false;
}

bool BinaryMap3DFeature::create_widget() {
#ifdef SYNOPSIA_USE_QT
    // Initialize ImGui state
    binary_map_3d::init_binary_map_3d_state();

    // Create IDA widget container
    widget_ = create_empty_widget(binary_map_3d::WIDGET_TITLE);
    if (!widget_) {
        binary_map_3d::cleanup_binary_map_3d_state();
        return false;
    }

    // Create ImGui OpenGL widget
    content_ = synopsia_imgui_create_widget(
        "synopsia_binary_map_3d",
        render_callback,
        nullptr
    );

    if (!content_) {
        close_widget(widget_, WCLS_DONT_SAVE_SIZE);
        widget_ = nullptr;
        binary_map_3d::cleanup_binary_map_3d_state();
        return false;
    }

    // Add ImGui widget to IDA widget layout
    synopsia_add_widget_to_layout(widget_, content_);

    // Display as a tabbed window
    display_widget(widget_, WOPN_DP_TAB | WOPN_PERSIST);

    return true;
#else
    msg("Synopsia [%s]: Qt support not available\n", binary_map_3d::FEATURE_NAME);
    return false;
#endif
}

void BinaryMap3DFeature::destroy_widget() {
#ifdef SYNOPSIA_USE_QT
    if (content_) {
        synopsia_imgui_destroy_widget(content_);
        content_ = nullptr;
    }
    if (widget_) {
        close_widget(widget_, WCLS_SAVE);
        widget_ = nullptr;
    }
    binary_map_3d::cleanup_binary_map_3d_state();
#endif
}

void BinaryMap3DFeature::refresh_data() {
    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", binary_map_3d::FEATURE_NAME);
        return;
    }

    // Refresh the ImGui state
    binary_map_3d::refresh_binary_map_3d_data();

    // Also refresh our local data for info
    if (data_ && data_->refresh()) {
        msg("Synopsia [%s]: Loaded %zu functions, %zu edges\n",
            binary_map_3d::FEATURE_NAME, data_->nodes().size(), data_->edges().size());
    }
}

void BinaryMap3DFeature::on_database_closed() {
    destroy_widget();
    visible_ = false;
}

void BinaryMap3DFeature::navigate_to(ea_t addr) {
    if (addr == BADADDR) return;
    jumpto(addr);
}

// Action handler implementation
int BinaryMap3DAction::activate(action_activation_ctx_t*) {
    if (auto* feature = BinaryMap3DFeature::instance()) {
        feature->toggle();
    }
    return 1;
}

action_state_t BinaryMap3DAction::update(action_update_ctx_t*) {
    return AST_ENABLE_ALWAYS;
}

} // namespace features
} // namespace synopsia
