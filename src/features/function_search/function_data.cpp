/// @file function_data.cpp
/// @brief Function data implementation

#include <synopsia/features/function_search/function_data.hpp>
#include <funcs.hpp>
#include <name.hpp>
#include <lines.hpp>
#include <bytes.hpp>
#include <ua.hpp>
#include <hexrays.hpp>

namespace synopsia {
namespace features {
namespace function_search {

// Check if Hex-Rays is available (cached)
static bool g_hexrays_checked = false;
static bool g_hexrays_available = false;

static bool check_hexrays() {
    if (!g_hexrays_checked) {
        g_hexrays_checked = true;
        g_hexrays_available = init_hexrays_plugin();
    }
    return g_hexrays_available;
}

bool FunctionData::refresh() {
    functions_.clear();
    name_to_addr_.clear();

    if (!is_database_loaded()) {
        valid_ = false;
        return false;
    }

    // Count functions for reserve
    std::size_t count = 0;
    for (std::size_t i = 0; i < get_func_qty(); ++i) {
        ++count;
    }
    functions_.reserve(count);
    name_to_addr_.reserve(count);

    // Iterate over all functions
    for (std::size_t i = 0; i < get_func_qty(); ++i) {
        func_t* func = getn_func(i);
        if (!func) continue;

        FunctionEntry entry;
        entry.address = func->start_ea;

        // Get function name
        qstring name;
        if (get_func_name(&name, func->start_ea) > 0) {
            entry.name = name;
        } else {
            entry.name.sprnt("sub_%llX", static_cast<unsigned long long>(func->start_ea));
        }

        // Get demangled name
        qstring demangled;
        if (get_demangled_name(&demangled, func->start_ea, 0, 0) > 0) {
            entry.demangled_name = demangled;
        }

        // Build name lookup maps
        name_to_addr_[std::string(entry.name.c_str())] = static_cast<func_addr_t>(entry.address);
        if (!entry.demangled_name.empty()) {
            name_to_addr_[std::string(entry.demangled_name.c_str())] = static_cast<func_addr_t>(entry.address);
        }

        functions_.push_back(std::move(entry));
    }

    valid_ = true;
    return true;
}

bool FunctionData::has_decompiler() const {
    return check_hexrays();
}

func_addr_t FunctionData::find_function_by_name(const std::string& name) const {
    auto it = name_to_addr_.find(name);
    if (it != name_to_addr_.end()) {
        return it->second;
    }
    return FUNC_BADADDR;
}

func_addr_t FunctionData::find_function_at(func_addr_t address) const {
    func_t* func = get_func(static_cast<ea_t>(address));
    if (func) {
        return static_cast<func_addr_t>(func->start_ea);
    }
    return FUNC_BADADDR;
}

std::string FunctionData::get_decompilation(func_addr_t address) const {
    if (!check_hexrays()) {
        return "// Hex-Rays decompiler not available";
    }

    func_t* func = get_func(static_cast<ea_t>(address));
    if (!func) {
        return "// Function not found";
    }

    // Decompile the function
    hexrays_failure_t hf;
    cfuncptr_t cfunc = decompile(func, &hf, DECOMP_WARNINGS);

    if (!cfunc) {
        std::string error = "// Decompilation failed";
        if (!hf.desc().empty()) {
            error += ": ";
            error += hf.desc().c_str();
        }
        return error;
    }

    // Get the pseudocode as text
    const strvec_t& sv = cfunc->get_pseudocode();

    std::string result;
    result.reserve(sv.size() * 80);

    for (const auto& line : sv) {
        qstring clean_line;
        tag_remove(&clean_line, line.line);
        result += clean_line.c_str();
        result += "\n";
    }

    return result.empty() ? "// Empty decompilation" : result;
}

std::string FunctionData::get_disassembly(func_addr_t address) const {
    func_t* func = get_func(static_cast<ea_t>(address));
    if (!func) {
        return "// Function not found";
    }

    std::string result;
    result.reserve(8192);

    // Iterate over function instructions using func_item_iterator
    // This is more reliable than func_tail_iterator for getting actual instructions
    func_item_iterator_t fii;
    bool ok = fii.set(func);

    if (!ok) {
        // Fallback: try direct iteration from function start
        ea_t addr = func->start_ea;
        ea_t end = func->end_ea;

        while (addr < end && addr != BADADDR) {
            qstring line;
            // Use GENDSM_FORCE_CODE to ensure we get disassembly
            if (generate_disasm_line(&line, addr, GENDSM_FORCE_CODE)) {
                qstring clean_line;
                tag_remove(&clean_line, line);

                char addr_buf[32];
                qsnprintf(addr_buf, sizeof(addr_buf), "%08llX  ",
                          static_cast<unsigned long long>(addr));

                result += addr_buf;
                result += clean_line.c_str();
                result += "\n";
            }

            // Get instruction size to advance
            insn_t insn;
            int size = decode_insn(&insn, addr);
            if (size <= 0) {
                // Not a valid instruction, try next byte
                addr++;
            } else {
                addr += size;
            }
        }

        return result.empty() ? "// Could not decode instructions" : result;
    }

    // Use func_item_iterator for proper iteration
    do {
        ea_t addr = fii.current();

        qstring line;
        if (generate_disasm_line(&line, addr, GENDSM_FORCE_CODE)) {
            qstring clean_line;
            tag_remove(&clean_line, line);

            char addr_buf[32];
            qsnprintf(addr_buf, sizeof(addr_buf), "%08llX  ",
                      static_cast<unsigned long long>(addr));

            result += addr_buf;
            result += clean_line.c_str();
            result += "\n";
        }
    } while (fii.next_code());

    return result.empty() ? "// No code in function" : result;
}

} // namespace function_search
} // namespace features
} // namespace synopsia
