/// @file imgui_widget.cpp
/// @brief ImGui-based function search widget (GPU accelerated)

#include <synopsia/features/function_search/function_data.hpp>
#include <synopsia/imgui/qt_imgui_widget.hpp>

#include <imgui.h>
#include <TextEditor.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace synopsia {
namespace features {
namespace function_search {

// =============================================================================
// Function Search State
// =============================================================================

// Tab definitions
static constexpr const char* TAB_NAMES[] = {
    "Functions",
    "Strings",
    "Imports",
    "Exports"
};
static constexpr int TAB_COUNT = sizeof(TAB_NAMES) / sizeof(TAB_NAMES[0]);

class FunctionSearchState {
public:
    FunctionSearchState() {
        // Configure text editor for disassembly display
        editor_.SetLanguageDefinition(TextEditor::LanguageDefinition::C());
        editor_.SetReadOnly(true);
        editor_.SetShowWhitespaces(false);
        // Note: Line numbers shown by default
    }

    void refresh_functions() {
        data_.refresh();
    }

    void render() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 display_size = io.DisplaySize;

        // Handle Option+Tab / Shift+Option+Tab for tab navigation
        // On macOS, Option key maps to Alt in ImGui
        handle_tab_navigation(io);

        // Set window to fullscreen with no decorations
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(display_size);

        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("FullscreenWindow", nullptr, window_flags);

        // Render tab bar at top
        render_tab_bar();

        // Render content based on selected tab
        switch (current_tab_) {
            case 0: render_functions_tab(); break;
            case 1: render_strings_tab(); break;
            case 2: render_imports_tab(); break;
            case 3: render_exports_tab(); break;
            default: render_functions_tab(); break;
        }

        ImGui::End();
    }

private:
    void handle_tab_navigation(ImGuiIO& io) {
        // Check for Option+Tab (forward) or Shift+Option+Tab (backward)
        // Alt is the Option key on macOS
        bool alt_held = io.KeyAlt;
        bool shift_held = io.KeyShift;
        bool tab_pressed = ImGui::IsKeyPressed(ImGuiKey_Tab, false);

        if (alt_held && tab_pressed) {
            if (shift_held) {
                // Shift+Option+Tab: go backward with wrap
                current_tab_ = (current_tab_ - 1 + TAB_COUNT) % TAB_COUNT;
            } else {
                // Option+Tab: go forward with wrap
                current_tab_ = (current_tab_ + 1) % TAB_COUNT;
            }
        }
    }

    void render_tab_bar() {
        if (ImGui::BeginTabBar("##main-tabs", ImGuiTabBarFlags_None)) {
            for (int i = 0; i < TAB_COUNT; ++i) {
                ImGuiTabItemFlags flags = 0;
                if (i == current_tab_) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                }

                if (ImGui::BeginTabItem(TAB_NAMES[i], nullptr, flags)) {
                    // Update current tab if user clicked on it
                    if (ImGui::IsItemActive() || i == current_tab_) {
                        current_tab_ = i;
                    }
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    void render_functions_tab() {
        if (ImGui::BeginTable("Functions:", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Functions", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);

            // Left column: function list
            ImGui::TableNextColumn();
            render_function_list();

            // Right column: function details
            ImGui::TableNextColumn();
            render_function_details();

            ImGui::EndTable();
        }
    }

    void render_strings_tab() {
        ImGui::TextDisabled("Strings view - not yet implemented");
    }

    void render_imports_tab() {
        ImGui::TextDisabled("Imports view - not yet implemented");
    }

    void render_exports_tab() {
        ImGui::TextDisabled("Exports view - not yet implemented");
    }

private:
    void render_function_list() {
        // Filter input
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter-text", "<filter>", filter_buffer_, sizeof(filter_buffer_));
        ImGui::SetItemDefaultFocus();

        if (ImGui::BeginListBox("##functions-list-box", ImVec2(-1, -1))) {
            temporary_function_index_ = -1;

            std::string filter_lower;
            if (filter_buffer_[0] != '\0') {
                filter_lower = filter_buffer_;
                // Convert to lowercase
                for (char& c : filter_lower) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }

            std::size_t count = data_.function_count();
            for (std::size_t i = 0; i < count; ++i) {
                FunctionInfo func = data_.get_function(i);

                // Apply filter
                if (!filter_lower.empty()) {
                    std::string name_lower = func.name;
                    for (char& c : name_lower) {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                    if (name_lower.find(filter_lower) == std::string::npos) {
                        continue;
                    }
                }

                bool is_selected = (static_cast<int>(i) == current_function_index_);
                if (ImGui::Selectable(func.name.c_str(), is_selected)) {
                    if (is_selected) {
                        // Unselect
                        current_function_index_ = -1;
                    } else {
                        // Select
                        current_function_index_ = static_cast<int>(i);
                    }
                }

                if (ImGui::IsItemHovered()) {
                    temporary_function_index_ = static_cast<int>(i);
                }
            }

            ImGui::EndListBox();
        }
    }

    void render_function_details() {
        int best_index = (temporary_function_index_ >= 0) ? temporary_function_index_ : current_function_index_;

        if (best_index < 0 || static_cast<std::size_t>(best_index) >= data_.function_count()) {
            return;
        }

        FunctionInfo func = data_.get_function(static_cast<std::size_t>(best_index));

        // Function name
        ImGui::Text("Name        : %s", func.name.c_str());

        // Demangled name (if different)
        if (!func.demangled_name.empty() && func.demangled_name != func.name) {
            ImGui::Text("Demangled   : %s", func.demangled_name.c_str());
        }

        // Address
        ImGui::Text("Address     : %08llX", static_cast<unsigned long long>(func.address));

        ImGui::Separator();

        // Get disassembly
        std::string disasm = data_.get_disassembly(func.address);

        // Update editor if content changed
        std::size_t hash = std::hash<std::string>{}(disasm);
        if (hash != disasm_hash_) {
            disasm_hash_ = hash;
            editor_.SetText(disasm.empty() ? "// no disassembly available" : disasm);
        }

        // Render text editor
        editor_.Render("##disasm-text", ImVec2(-1, -1), false);
    }

    FunctionData data_;
    TextEditor editor_;
    char filter_buffer_[256] = {0};
    int current_function_index_ = -1;
    int temporary_function_index_ = -1;
    std::size_t disasm_hash_ = 0;
    int current_tab_ = 0;
};

// =============================================================================
// Global State and Bridge Functions
// =============================================================================

static std::unique_ptr<FunctionSearchState> g_state;

void init_function_search_state() {
    if (!g_state) {
        g_state = std::make_unique<FunctionSearchState>();
        g_state->refresh_functions();
    }
}

void cleanup_function_search_state() {
    g_state.reset();
}

void refresh_function_search_data() {
    if (g_state) {
        g_state->refresh_functions();
    }
}

void render_function_search() {
    if (g_state) {
        g_state->render();
    }
}

} // namespace function_search
} // namespace features
} // namespace synopsia
