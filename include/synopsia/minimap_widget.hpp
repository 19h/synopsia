/// @file minimap_widget.hpp
/// @brief Qt-based minimap widget for IDA integration

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

#ifdef SYNOPSIA_USE_QT

// ============================================================================
// Qt-only section - no IDA headers allowed here
// ============================================================================

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QToolTip>

// Include IDA-independent headers
#include "color.hpp"
#include "minimap_data_interface.hpp"

namespace synopsia {

// Visual configuration constants (Qt-local, no IDA dependency)
inline constexpr int QT_DEFAULT_MINIMAP_WIDTH = 180;  // Default width when docked
inline constexpr int QT_MIN_MINIMAP_WIDTH = 80;
inline constexpr int QT_CURSOR_LINE_HEIGHT = 2;
inline constexpr int QT_MINIMAP_MARGIN = 4;

/// Callback types (using interface types)
using QtAddressCallback = std::function<void(data_addr_t address)>;
using QtRefreshCallback = std::function<void()>;

/// @class MinimapWidget
/// @brief Qt widget for rendering the entropy minimap
///
/// This widget displays a color-coded visualization of entropy across the binary.
/// It supports:
/// - Click to navigate to address
/// - Scroll to zoom
/// - Drag to pan
/// - Hover to see entropy details
/// - Current cursor position indicator
class MinimapWidget : public QWidget {
    // Note: We avoid Q_OBJECT to prevent moc dependency
    // Use std::function callbacks instead of signals/slots
    
public:
    explicit MinimapWidget(QWidget* parent = nullptr);
    ~MinimapWidget() override = default;
    
    // =========================================================================
    // Data Management
    // =========================================================================
    
    /// @brief Set the minimap data source (uses abstract interface)
    void setDataSource(IMinimapDataSource* source);
    
    /// @brief Get the current data source
    [[nodiscard]] IMinimapDataSource* dataSource() const noexcept { return data_source_; }
    
    /// @brief Refresh the display from current data
    void refresh();
    
    /// @brief Invalidate the render cache
    void invalidateCache();
    
    // =========================================================================
    // Configuration
    // =========================================================================
    
    /// @brief Set the color gradient for entropy visualization
    void setGradient(const ColorGradient& gradient);
    
    /// @brief Get the current gradient
    [[nodiscard]] const ColorGradient& gradient() const noexcept { return gradient_; }
    
    /// @brief Set layout orientation
    /// @param vertical true for vertical bar, false for horizontal
    void setVerticalLayout(bool vertical);
    
    /// @brief Check if using vertical layout
    [[nodiscard]] bool isVerticalLayout() const noexcept { return vertical_layout_; }
    
    /// @brief Set whether to show the cursor position
    void setShowCursor(bool show);
    
    /// @brief Set whether to show region boundaries
    void setShowRegions(bool show);
    
    /// @brief Set the current cursor address to highlight
    void setCurrentAddress(data_addr_t addr);
    
    /// @brief Get the current highlighted address
    [[nodiscard]] data_addr_t currentAddress() const noexcept { return current_addr_; }
    
    /// @brief Set the visible range in IDA's view (for viewport frame)
    /// This draws a "frame" on the minimap showing what's currently visible
    /// in the IDA disassembly view, similar to VSCode's minimap
    void setVisibleRange(data_addr_t start, data_addr_t end);
    
    /// @brief Enable/disable the viewport frame
    void setShowViewportFrame(bool show);
    
    // =========================================================================
    // Callbacks (replaces Qt signals to avoid moc)
    // =========================================================================
    
    /// Callback when user clicks on an address
    QtAddressCallback onAddressClicked;
    
    /// Callback when user hovers over an address
    QtAddressCallback onAddressHovered;
    
    /// Callback when refresh is requested
    QtRefreshCallback onRefreshRequested;
    
    // =========================================================================
    // Size Hints
    // =========================================================================
    
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    
protected:
    // =========================================================================
    // Qt Event Handlers
    // =========================================================================
    
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    // =========================================================================
    // Rendering
    // =========================================================================
    
    /// Render entropy blocks to cache image
    void renderToCache();
    
    /// Draw the cached image and overlays
    void drawContent(QPainter& painter);
    
    /// Draw region boundaries
    void drawRegions(QPainter& painter);
    
    /// Draw current cursor position
    void drawCursor(QPainter& painter);
    
    /// Draw hover highlight
    void drawHover(QPainter& painter);
    
    /// Draw viewport frame (shows IDA's visible range)
    void drawViewportFrame(QPainter& painter);
    
    // =========================================================================
    // Coordinate Helpers
    // =========================================================================
    
    /// Convert widget position to address
    [[nodiscard]] data_addr_t positionToAddress(const QPoint& pos) const;
    
    /// Convert address to widget position
    [[nodiscard]] int addressToPosition(data_addr_t addr) const;
    
    /// Get the content rectangle (minus margins)
    [[nodiscard]] QRect contentRect() const;
    
    // =========================================================================
    // Member Data
    // =========================================================================
    
    IMinimapDataSource* data_source_ = nullptr;
    ColorGradient gradient_;
    
    // Display state
    bool vertical_layout_ = true;
    bool show_cursor_ = true;
    bool show_regions_ = true;
    bool show_viewport_frame_ = true;
    data_addr_t current_addr_ = DATA_BADADDR;
    data_addr_t visible_start_ = DATA_BADADDR;
    data_addr_t visible_end_ = DATA_BADADDR;
    
    // Interaction state
    bool is_hovering_ = false;
    data_addr_t hover_addr_ = DATA_BADADDR;
    bool is_dragging_ = false;
    QPoint drag_start_;
    data_addr_t drag_start_addr_ = 0;
    
    // Render cache
    QImage cache_image_;
    bool cache_valid_ = false;
    int cached_width_ = 0;
    int cached_height_ = 0;
};

} // namespace synopsia

#else // !SYNOPSIA_USE_QT

// ============================================================================
// Non-Qt stub - can include IDA headers safely
// ============================================================================

#include "types.hpp"

namespace synopsia {

/// Forward declaration
class IMinimapDataSource;

/// Stub MinimapWidget when Qt is not available
class MinimapWidget {
public:
    MinimapWidget() = default;
    
    void setDataSource(IMinimapDataSource*) {}
    void refresh() {}
    void setShowCursor(bool) {}
    void setShowRegions(bool) {}
    void setVerticalLayout(bool) {}
    void setCurrentAddress(ea_t) {}
    
    AddressCallback onAddressClicked;
    AddressCallback onAddressHovered;
    RefreshCallback onRefreshRequested;
};

} // namespace synopsia

#endif // SYNOPSIA_USE_QT
