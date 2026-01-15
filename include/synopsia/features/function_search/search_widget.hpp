/// @file search_widget.hpp
/// @brief Qt widget for function search with disassembly viewer

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

#ifdef SYNOPSIA_USE_QT

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>
#include <QFont>

#include "data_interface.hpp"

namespace synopsia {
namespace features {
namespace function_search {

/// Callback types
using NavigateCallback = std::function<void(func_addr_t address)>;

/// @class FunctionSearchWidget
/// @brief Qt widget for function search with disassembly viewer
///
/// Features:
/// - Function list with filtering
/// - Click to select function
/// - Hover to preview function
/// - Disassembly viewer for selected function
/// - Function name and address display
class FunctionSearchWidget : public QWidget {
    // Note: We avoid Q_OBJECT to prevent moc dependency
public:
    explicit FunctionSearchWidget(QWidget* parent = nullptr);
    ~FunctionSearchWidget() override = default;

    /// Set the data source
    void setDataSource(IFunctionDataSource* source);

    /// Get the data source
    [[nodiscard]] IFunctionDataSource* dataSource() const noexcept { return data_source_; }

    /// Refresh the function list
    void refresh();

    /// Navigate to function callback
    NavigateCallback onNavigate;

    /// Size hints
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

private:
    void setupUi();
    void populateList();
    void applyFilter(const QString& filter);
    void onSelectionChanged();
    void onItemHovered(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void showFunctionDetails(std::size_t index);

    IFunctionDataSource* data_source_ = nullptr;

    // UI elements
    QLineEdit* filter_edit_ = nullptr;
    QListWidget* function_list_ = nullptr;
    QLabel* name_label_ = nullptr;
    QLabel* demangled_label_ = nullptr;
    QLabel* address_label_ = nullptr;
    QTextEdit* disasm_view_ = nullptr;

    // State
    int current_index_ = -1;
    int hover_index_ = -1;

    // Map from list row to function index
    std::vector<std::size_t> row_to_index_;
};

} // namespace function_search
} // namespace features
} // namespace synopsia

#else // !SYNOPSIA_USE_QT

namespace synopsia {
namespace features {
namespace function_search {

class IFunctionDataSource;
using NavigateCallback = std::function<void(std::uint64_t)>;

class FunctionSearchWidget {
public:
    FunctionSearchWidget() = default;
    void setDataSource(IFunctionDataSource*) {}
    void refresh() {}
    NavigateCallback onNavigate;
};

} // namespace function_search
} // namespace features
} // namespace synopsia

#endif // SYNOPSIA_USE_QT
