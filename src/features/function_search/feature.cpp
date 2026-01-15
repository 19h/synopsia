/// @file feature.cpp
/// @brief Function search feature implementation (ImGui/GPU accelerated)

#include <synopsia/features/function_search/feature.hpp>

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
namespace function_search {
    void init_function_search_state();
    void cleanup_function_search_state();
    void refresh_function_search_data();
    void render_function_search();
}

// Static instance pointer
FunctionSearchFeature* FunctionSearchFeature::instance_ = nullptr;

// Render callback thunk
static void render_callback(void*) {
    function_search::render_function_search();
}

FunctionSearchFeature::FunctionSearchFeature() {
    instance_ = this;
}

FunctionSearchFeature::~FunctionSearchFeature() {
    cleanup();
    instance_ = nullptr;
}

bool FunctionSearchFeature::initialize() {
    if (!register_actions()) {
        return false;
    }

    data_ = std::make_unique<function_search::FunctionData>();
    initialized_ = true;

    msg("Synopsia [%s]: Feature initialized (hotkey: %s)\n",
        function_search::FEATURE_NAME, function_search::FEATURE_HOTKEY);

    return true;
}

void FunctionSearchFeature::cleanup() {
    if (!initialized_) return;

    destroy_widget();
    unregister_actions();
    data_.reset();
    function_search::cleanup_function_search_state();
    initialized_ = false;
}

bool FunctionSearchFeature::register_actions() {
    static FunctionSearchAction action_handler;

    const action_desc_t action_desc = ACTION_DESC_LITERAL(
        function_search::ACTION_NAME,
        function_search::ACTION_LABEL,
        &action_handler,
        function_search::FEATURE_HOTKEY,
        "Search and browse functions with disassembly viewer (ImGui/GPU)",
        -1
    );

    if (!register_action(action_desc)) {
        msg("Synopsia [%s]: Failed to register action\n", function_search::FEATURE_NAME);
        return false;
    }

    attach_action_to_menu("View/", function_search::ACTION_NAME, SETMENU_APP);
    return true;
}

void FunctionSearchFeature::unregister_actions() {
    detach_action_from_menu("View/", function_search::ACTION_NAME);
    unregister_action(function_search::ACTION_NAME);
}

void FunctionSearchFeature::show() {
    if (visible_) return;

    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", function_search::FEATURE_NAME);
        return;
    }

    if (!create_widget()) {
        msg("Synopsia [%s]: Failed to create widget\n", function_search::FEATURE_NAME);
        return;
    }

    refresh_data();
    visible_ = true;
}

void FunctionSearchFeature::hide() {
    if (!visible_) return;
    destroy_widget();
    visible_ = false;
}

bool FunctionSearchFeature::create_widget() {
#ifdef SYNOPSIA_USE_QT
    // Initialize ImGui state
    function_search::init_function_search_state();

    // Create IDA widget container
    widget_ = create_empty_widget(function_search::WIDGET_TITLE);
    if (!widget_) {
        function_search::cleanup_function_search_state();
        return false;
    }

    // Create ImGui OpenGL widget
    content_ = synopsia_imgui_create_widget(
        "synopsia_function_search",
        render_callback,
        nullptr
    );

    if (!content_) {
        close_widget(widget_, WCLS_DONT_SAVE_SIZE);
        widget_ = nullptr;
        function_search::cleanup_function_search_state();
        return false;
    }

    // Add ImGui widget to IDA widget layout
    synopsia_add_widget_to_layout(widget_, content_);

    // Display as a tabbed window
    display_widget(widget_, WOPN_DP_TAB | WOPN_PERSIST);

    return true;
#else
    msg("Synopsia [%s]: Qt support not available\n", function_search::FEATURE_NAME);
    return false;
#endif
}

void FunctionSearchFeature::destroy_widget() {
#ifdef SYNOPSIA_USE_QT
    if (content_) {
        synopsia_imgui_destroy_widget(content_);
        content_ = nullptr;
    }
    if (widget_) {
        close_widget(widget_, WCLS_SAVE);
        widget_ = nullptr;
    }
    function_search::cleanup_function_search_state();
#endif
}

void FunctionSearchFeature::refresh_data() {
    if (!is_database_loaded()) {
        msg("Synopsia [%s]: No database loaded\n", function_search::FEATURE_NAME);
        return;
    }

    // Refresh the ImGui state
    function_search::refresh_function_search_data();

    // Also refresh our local data for function count
    if (data_ && data_->refresh()) {
        msg("Synopsia [%s]: Loaded %zu functions\n",
            function_search::FEATURE_NAME, data_->function_count());
    }
}

void FunctionSearchFeature::on_database_closed() {
    destroy_widget();
    visible_ = false;
}

void FunctionSearchFeature::navigate_to(ea_t addr) {
    if (addr == BADADDR) return;
    jumpto(addr);
}

// Action handler implementation
int FunctionSearchAction::activate(action_activation_ctx_t*) {
    if (auto* feature = FunctionSearchFeature::instance()) {
        feature->toggle();
    }
    return 1;
}

action_state_t FunctionSearchAction::update(action_update_ctx_t*) {
    return AST_ENABLE_ALWAYS;
}

} // namespace features
} // namespace synopsia
