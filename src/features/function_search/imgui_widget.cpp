/// @file imgui_widget.cpp
/// @brief ImGui-based function search widget (GPU accelerated)

#include <synopsia/features/function_search/function_data.hpp>
#include <synopsia/imgui/qt_imgui_widget.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <cctype>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace synopsia {
namespace features {
namespace function_search {

// =============================================================================
// Navigation History
// =============================================================================

struct NavigationHistory {
    std::vector<func_addr_t> history;
    int current_index = -1;

    void navigate_to(func_addr_t addr) {
        if (addr == FUNC_BADADDR) return;

        // If we're not at the end, truncate forward history
        if (current_index >= 0 && current_index < static_cast<int>(history.size()) - 1) {
            history.resize(current_index + 1);
        }

        // Don't add duplicate consecutive entries
        if (!history.empty() && history.back() == addr) {
            return;
        }

        history.push_back(addr);
        current_index = static_cast<int>(history.size()) - 1;

        // Limit history size
        if (history.size() > 100) {
            history.erase(history.begin());
            --current_index;
        }
    }

    func_addr_t go_back() {
        if (current_index > 0) {
            --current_index;
            return history[current_index];
        }
        return FUNC_BADADDR;
    }

    func_addr_t go_forward() {
        if (current_index < static_cast<int>(history.size()) - 1) {
            ++current_index;
            return history[current_index];
        }
        return FUNC_BADADDR;
    }

    bool can_go_back() const { return current_index > 0; }
    bool can_go_forward() const { return current_index < static_cast<int>(history.size()) - 1; }
};

// =============================================================================
// Function Search State
// =============================================================================

// Detail view tab definitions
static constexpr const char* DETAIL_TAB_NAMES[] = {
    "Disassembly",
    "Decompilation"
};
static constexpr int DETAIL_TAB_COUNT = 2;

class FunctionSearchState {
public:
    FunctionSearchState() = default;

    void refresh_functions() {
        data_.refresh();
    }

    // Called from Qt when mouse back/forward buttons are pressed
    void navigate_back() {
        func_addr_t addr = nav_history_.go_back();
        if (addr != FUNC_BADADDR) {
            select_function_by_address(addr);
        }
    }

    void navigate_forward() {
        func_addr_t addr = nav_history_.go_forward();
        if (addr != FUNC_BADADDR) {
            select_function_by_address(addr);
        }
    }

    void render() {
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 display_size = io.DisplaySize;

        // Handle keyboard shortcuts
        handle_keyboard_shortcuts(io);

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

        // Two-column layout: function list | details
        if (ImGui::BeginTable("##main-layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Functions", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);

            // Left column: function list
            ImGui::TableNextColumn();
            render_function_list();

            // Right column: function details with tabs
            ImGui::TableNextColumn();
            render_function_details();

            ImGui::EndTable();
        }

        ImGui::End();
    }

private:
    void handle_keyboard_shortcuts(ImGuiIO& io) {
        bool alt_held = io.KeyAlt;
        bool shift_held = io.KeyShift;
        bool tab_pressed = ImGui::IsKeyPressed(ImGuiKey_Tab, false);

        // Option+Tab / Shift+Option+Tab: switch between Disassembly/Decompilation
        if (alt_held && tab_pressed) {
            if (shift_held) {
                detail_tab_ = (detail_tab_ - 1 + DETAIL_TAB_COUNT) % DETAIL_TAB_COUNT;
            } else {
                detail_tab_ = (detail_tab_ + 1) % DETAIL_TAB_COUNT;
            }
            tab_changed_programmatically_ = true;
        }

        // Cmd+[ / Cmd+] for back/forward (macOS style)
        if (io.KeySuper) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) {
                navigate_back();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) {
                navigate_forward();
            }
        }
    }

    void select_function_by_address(func_addr_t addr) {
        // Find function index by address
        std::size_t count = data_.function_count();
        for (std::size_t i = 0; i < count; ++i) {
            FunctionInfo func = data_.get_function(i);
            if (func.address == addr) {
                current_function_index_ = static_cast<int>(i);
                return;
            }
        }
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
            ImGui::TextDisabled("Select a function to view details");
            return;
        }

        FunctionInfo func = data_.get_function(static_cast<std::size_t>(best_index));

        // Track navigation when function changes via click (not hover)
        if (temporary_function_index_ < 0 && func.address != last_selected_addr_) {
            last_selected_addr_ = func.address;
            nav_history_.navigate_to(func.address);
        }

        // Function header info
        ImGui::Text("Name        : %s", func.name.c_str());
        if (!func.demangled_name.empty() && func.demangled_name != func.name) {
            ImGui::Text("Demangled   : %s", func.demangled_name.c_str());
        }
        ImGui::Text("Address     : %08llX", static_cast<unsigned long long>(func.address));

        // Navigation buttons
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        ImGui::BeginDisabled(!nav_history_.can_go_back());
        if (ImGui::SmallButton("<")) {
            navigate_back();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!nav_history_.can_go_forward());
        if (ImGui::SmallButton(">")) {
            navigate_forward();
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        // Tab bar for Disassembly / Decompilation
        if (ImGui::BeginTabBar("##detail-tabs", ImGuiTabBarFlags_None)) {
            for (int i = 0; i < DETAIL_TAB_COUNT; ++i) {
                ImGuiTabItemFlags flags = 0;
                // Only force selection when changed via keyboard shortcut
                if (tab_changed_programmatically_ && i == detail_tab_) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                }

                if (ImGui::BeginTabItem(DETAIL_TAB_NAMES[i], nullptr, flags)) {
                    // Track tab selection from click
                    detail_tab_ = i;
                    ImGui::EndTabItem();
                }
            }
            // Clear the flag after processing
            tab_changed_programmatically_ = false;
            ImGui::EndTabBar();
        }

        // Invalidate cache if function changed
        if (func.address != cached_addr_) {
            cached_addr_ = func.address;
            cached_disasm_.clear();
            cached_decomp_.clear();
        }

        // Render content based on selected tab (lazy loading)
        if (detail_tab_ == 0) {
            // Fetch disassembly only when needed
            if (cached_disasm_.empty()) {
                cached_disasm_ = data_.get_disassembly(func.address);
                if (cached_disasm_.empty()) {
                    cached_disasm_ = "; no disassembly available";
                }
            }
            render_disassembly_view();
        } else {
            // Fetch decompilation only when tab is active
            if (cached_decomp_.empty()) {
                cached_decomp_ = data_.get_decompilation(func.address);
                if (cached_decomp_.empty()) {
                    cached_decomp_ = "// decompilation not available";
                }
            }
            render_decompilation_view();
        }
    }

    void render_decompilation_view() {
        ImVec2 avail = ImGui::GetContentRegionAvail();

        if (ImGui::BeginChild("##decomp-scroll", avail, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            // Parse and render decompiled code with C-like highlighting
            const char* text = cached_decomp_.c_str();
            const char* end = text + cached_decomp_.size();
            const char* line_start = text;

            while (line_start < end) {
                const char* line_end = line_start;
                while (line_end < end && *line_end != '\n') ++line_end;

                render_decomp_line(line_start, line_end);
                line_start = (line_end < end) ? line_end + 1 : end;
            }
        }
        ImGui::EndChild();
    }

    void render_decomp_line(const char* start, const char* end) {
        if (start >= end) {
            ImGui::NewLine();
            return;
        }

        // C-like syntax highlighting colors
        const ImVec4 keyword_color(0.8f, 0.4f, 0.8f, 1.0f);   // Purple for keywords
        const ImVec4 type_color(0.4f, 0.7f, 1.0f, 1.0f);      // Blue for types
        const ImVec4 string_color(0.9f, 0.6f, 0.4f, 1.0f);    // Orange for strings
        const ImVec4 number_color(0.6f, 0.9f, 0.6f, 1.0f);    // Green for numbers
        const ImVec4 comment_color(0.5f, 0.5f, 0.5f, 1.0f);   // Gray for comments
        const ImVec4 func_color(0.9f, 0.9f, 0.5f, 1.0f);      // Yellow for function calls
        const ImVec4 default_color(0.9f, 0.9f, 0.9f, 1.0f);

        static const char* keywords[] = {
            "if", "else", "while", "for", "do", "switch", "case", "default",
            "break", "continue", "return", "goto", "sizeof", "typedef", "struct",
            "union", "enum", "const", "static", "extern", "register", "volatile"
        };
        static const char* types[] = {
            "void", "char", "short", "int", "long", "float", "double", "signed",
            "unsigned", "bool", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t", "BOOL",
            "DWORD", "QWORD", "BYTE", "WORD", "__int64", "_BOOL"
        };

        const char* ptr = start;

        while (ptr < end) {
            // Skip whitespace
            if (std::isspace(*ptr)) {
                ImGui::TextUnformatted(" ");
                ImGui::SameLine(0, 0);
                ++ptr;
                continue;
            }

            // Check for // comment
            if (ptr + 1 < end && ptr[0] == '/' && ptr[1] == '/') {
                ImGui::TextColored(comment_color, "%.*s", static_cast<int>(end - ptr), ptr);
                break;
            }

            // Check for string literal
            if (*ptr == '"') {
                const char* str_end = ptr + 1;
                while (str_end < end && *str_end != '"') {
                    if (*str_end == '\\' && str_end + 1 < end) ++str_end;
                    ++str_end;
                }
                if (str_end < end) ++str_end;
                ImGui::TextColored(string_color, "%.*s", static_cast<int>(str_end - ptr), ptr);
                ImGui::SameLine(0, 0);
                ptr = str_end;
                continue;
            }

            // Check for number
            if (std::isdigit(*ptr) || (*ptr == '0' && ptr + 1 < end && (ptr[1] == 'x' || ptr[1] == 'X'))) {
                const char* num_end = ptr;
                if (*num_end == '0' && num_end + 1 < end && (num_end[1] == 'x' || num_end[1] == 'X')) {
                    num_end += 2;
                    while (num_end < end && std::isxdigit(*num_end)) ++num_end;
                } else {
                    while (num_end < end && (std::isdigit(*num_end) || *num_end == '.')) ++num_end;
                }
                while (num_end < end && (*num_end == 'u' || *num_end == 'U' || *num_end == 'l' || *num_end == 'L')) ++num_end;
                ImGui::TextColored(number_color, "%.*s", static_cast<int>(num_end - ptr), ptr);
                ImGui::SameLine(0, 0);
                ptr = num_end;
                continue;
            }

            // Check for identifier/keyword
            if (std::isalpha(*ptr) || *ptr == '_') {
                const char* id_end = ptr;
                while (id_end < end && (std::isalnum(*id_end) || *id_end == '_')) ++id_end;
                std::string token(ptr, id_end);

                ImVec4 color = default_color;

                // Check if it's a keyword
                for (const char* kw : keywords) {
                    if (token == kw) { color = keyword_color; break; }
                }
                // Check if it's a type
                if (color.x == default_color.x) {
                    for (const char* t : types) {
                        if (token == t) { color = type_color; break; }
                    }
                }
                // Check if it's a function call (followed by '(')
                if (color.x == default_color.x && id_end < end && *id_end == '(') {
                    color = func_color;
                    // Make function calls clickable
                    if (render_clickable_function(token, color)) {
                        ptr = id_end;
                        continue;
                    }
                }

                ImGui::TextColored(color, "%.*s", static_cast<int>(id_end - ptr), ptr);
                ImGui::SameLine(0, 0);
                ptr = id_end;
                continue;
            }

            // Other characters
            ImGui::TextUnformatted(ptr, ptr + 1);
            ImGui::SameLine(0, 0);
            ++ptr;
        }

        ImGui::NewLine();
    }

    // Returns true if rendered as clickable, false otherwise
    bool render_clickable_function(const std::string& name, const ImVec4& color) {
        // Check if this is a known function
        func_addr_t addr = data_.find_function_by_name(name);
        if (addr == FUNC_BADADDR) {
            return false;
        }

        // Render as a clickable button-like text
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

        if (ImGui::SmallButton(name.c_str())) {
            // Navigate to this function
            nav_history_.navigate_to(addr);
            select_function_by_address(addr);
        }

        ImGui::PopStyleColor(4);
        ImGui::SameLine(0, 0);
        return true;
    }

    void render_disassembly_view() {
        // Get available space
        ImVec2 avail = ImGui::GetContentRegionAvail();

        // Create a child window for scrolling
        if (ImGui::BeginChild("##disasm-scroll", avail, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            // Use a monospace-friendly color scheme
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

            // Parse and render each line
            const char* text = cached_disasm_.c_str();
            const char* end = text + cached_disasm_.size();
            const char* line_start = text;

            while (line_start < end) {
                // Find end of line
                const char* line_end = line_start;
                while (line_end < end && *line_end != '\n') {
                    ++line_end;
                }

                // Render the line with syntax highlighting
                render_disasm_line(line_start, line_end);

                // Move to next line
                line_start = (line_end < end) ? line_end + 1 : end;
            }

            ImGui::PopStyleColor();
        }
        ImGui::EndChild();
    }

    void render_disasm_line(const char* start, const char* end) {
        if (start >= end) {
            ImGui::TextUnformatted("");
            return;
        }

        // Colors for different parts
        const ImVec4 addr_color(0.6f, 0.6f, 0.6f, 1.0f);      // Gray for address
        const ImVec4 mnemonic_color(0.4f, 0.7f, 1.0f, 1.0f);  // Blue for mnemonic
        const ImVec4 reg_color(0.9f, 0.7f, 0.4f, 1.0f);       // Orange for registers
        const ImVec4 num_color(0.6f, 0.9f, 0.6f, 1.0f);       // Green for numbers
        const ImVec4 comment_color(0.5f, 0.5f, 0.5f, 1.0f);   // Dark gray for comments
        const ImVec4 default_color(0.9f, 0.9f, 0.9f, 1.0f);   // White default

        const char* ptr = start;

        // Parse address (first 8+ hex chars followed by spaces)
        const char* addr_end = ptr;
        while (addr_end < end && (std::isxdigit(*addr_end) || *addr_end == ' ')) {
            ++addr_end;
        }

        if (addr_end > ptr) {
            ImGui::TextColored(addr_color, "%.*s", static_cast<int>(addr_end - ptr), ptr);
            ImGui::SameLine(0, 0);
            ptr = addr_end;
        }

        // Parse mnemonic (next word)
        while (ptr < end && *ptr == ' ') ++ptr;
        const char* mnem_start = ptr;
        while (ptr < end && *ptr != ' ' && *ptr != '\t') ++ptr;

        if (ptr > mnem_start) {
            ImGui::TextColored(mnemonic_color, "%.*s", static_cast<int>(ptr - mnem_start), mnem_start);
            ImGui::SameLine(0, 0);
        }

        // Rest of the line - simple coloring for operands
        if (ptr < end) {
            // Check for comment (;)
            const char* comment = ptr;
            while (comment < end && *comment != ';') ++comment;

            if (comment > ptr) {
                // Render operands before comment
                render_operands(ptr, comment, reg_color, num_color, default_color);
            }

            if (comment < end) {
                // Render comment
                ImGui::TextColored(comment_color, "%.*s", static_cast<int>(end - comment), comment);
            }
        }

        // Newline (ImGui::Text already does this, but we need it for empty lines)
        ImGui::NewLine();
    }

    void render_operands(const char* start, const char* end,
                        const ImVec4& reg_color, const ImVec4& num_color,
                        const ImVec4& default_color) {
        const char* ptr = start;

        while (ptr < end) {
            // Skip whitespace, render as-is
            if (*ptr == ' ' || *ptr == '\t') {
                ImGui::TextUnformatted(" ");
                ImGui::SameLine(0, 0);
                ++ptr;
                continue;
            }

            // Find token end
            const char* tok_start = ptr;
            bool is_hex = false;
            bool is_reg = false;

            // Check for hex number (0x... or ends with 'h')
            if (std::isxdigit(*ptr)) {
                while (ptr < end && (std::isxdigit(*ptr) || *ptr == 'x' || *ptr == 'X')) {
                    ++ptr;
                }
                if (ptr < end && (*ptr == 'h' || *ptr == 'H')) ++ptr;
                is_hex = true;
            }
            // Check for register-like tokens (short alphanumeric)
            else if (std::isalpha(*ptr)) {
                while (ptr < end && (std::isalnum(*ptr) || *ptr == '_')) {
                    ++ptr;
                }
                // Common x86 registers
                std::string tok(tok_start, ptr);
                if (tok.size() <= 4 ||
                    tok.find("xmm") == 0 || tok.find("ymm") == 0 || tok.find("zmm") == 0 ||
                    tok == "rax" || tok == "rbx" || tok == "rcx" || tok == "rdx" ||
                    tok == "rsi" || tok == "rdi" || tok == "rbp" || tok == "rsp" ||
                    tok == "eax" || tok == "ebx" || tok == "ecx" || tok == "edx" ||
                    tok == "r8" || tok == "r9" || tok == "r10" || tok == "r11" ||
                    tok == "r12" || tok == "r13" || tok == "r14" || tok == "r15") {
                    is_reg = true;
                }
            }
            // Other characters (punctuation, brackets, etc.)
            else {
                ++ptr;
            }

            // Render token
            if (ptr > tok_start) {
                ImVec4 color = default_color;
                if (is_hex) color = num_color;
                else if (is_reg) color = reg_color;

                ImGui::TextColored(color, "%.*s", static_cast<int>(ptr - tok_start), tok_start);
                ImGui::SameLine(0, 0);
            }
        }
    }

    FunctionData data_;
    char filter_buffer_[256] = {0};
    int current_function_index_ = -1;
    int temporary_function_index_ = -1;
    int detail_tab_ = 0;  // 0 = Disassembly, 1 = Decompilation
    bool tab_changed_programmatically_ = false;  // Flag for keyboard-triggered tab changes

    // Navigation history
    NavigationHistory nav_history_;
    func_addr_t last_selected_addr_ = FUNC_BADADDR;

    // Cached content
    func_addr_t cached_addr_ = FUNC_BADADDR;
    std::string cached_disasm_;
    std::string cached_decomp_;
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

void navigate_back() {
    if (g_state) {
        g_state->navigate_back();
    }
}

void navigate_forward() {
    if (g_state) {
        g_state->navigate_forward();
    }
}

} // namespace function_search
} // namespace features
} // namespace synopsia

// =============================================================================
// C Linkage Bridge for Mouse Back/Forward Buttons
// =============================================================================

extern "C" {

void synopsia_function_search_navigate_back() {
    synopsia::features::function_search::navigate_back();
}

void synopsia_function_search_navigate_forward() {
    synopsia::features::function_search::navigate_forward();
}

} // extern "C"
