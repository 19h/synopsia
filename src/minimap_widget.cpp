/// @file minimap_widget.cpp
/// @brief Qt-based minimap widget implementation
///
/// This file contains ONLY Qt code - no IDA headers are included here.
/// Data access is done through the IMinimapDataSource interface.

#include <synopsia/minimap_widget.hpp>

#ifdef SYNOPSIA_USE_QT

#include <QPainterPath>
#include <QFontMetrics>
#include <QFont>

namespace synopsia {

MinimapWidget::MinimapWidget(QWidget* parent)
    : QWidget(parent)
    , gradient_(ColorGradient::create_default())
{
    // Enable mouse tracking for hover effects
    setMouseTracking(true);
    
    // Set focus policy to accept keyboard input
    setFocusPolicy(Qt::StrongFocus);
    
    // Set minimum size
    setMinimumSize(QT_MIN_MINIMAP_WIDTH, 100);
    
    // Set background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(
        colors::Background.r,
        colors::Background.g,
        colors::Background.b
    ));
    setPalette(pal);
}

void MinimapWidget::setDataSource(IMinimapDataSource* source) {
    data_source_ = source;
    invalidateCache();
    update();
}

void MinimapWidget::refresh() {
    invalidateCache();
    update();
}

void MinimapWidget::invalidateCache() {
    cache_valid_ = false;
}

void MinimapWidget::setGradient(const ColorGradient& gradient) {
    gradient_ = gradient;
    invalidateCache();
    update();
}

void MinimapWidget::setVerticalLayout(bool vertical) {
    if (vertical_layout_ != vertical) {
        vertical_layout_ = vertical;
        invalidateCache();
        update();
    }
}

void MinimapWidget::setShowCursor(bool show) {
    if (show_cursor_ != show) {
        show_cursor_ = show;
        update();
    }
}

void MinimapWidget::setShowRegions(bool show) {
    if (show_regions_ != show) {
        show_regions_ = show;
        update();
    }
}

void MinimapWidget::setCurrentAddress(data_addr_t addr) {
    if (current_addr_ != addr) {
        current_addr_ = addr;
        update();
    }
}

void MinimapWidget::setVisibleRange(data_addr_t start, data_addr_t end) {
    if (visible_start_ != start || visible_end_ != end) {
        visible_start_ = start;
        visible_end_ = end;
        update();
    }
}

void MinimapWidget::setShowViewportFrame(bool show) {
    if (show_viewport_frame_ != show) {
        show_viewport_frame_ = show;
        update();
    }
}

QSize MinimapWidget::sizeHint() const {
    if (vertical_layout_) {
        return QSize(QT_DEFAULT_MINIMAP_WIDTH, 400);
    } else {
        return QSize(400, QT_DEFAULT_MINIMAP_WIDTH);
    }
}

QSize MinimapWidget::minimumSizeHint() const {
    if (vertical_layout_) {
        return QSize(QT_MIN_MINIMAP_WIDTH, 100);
    } else {
        return QSize(100, QT_MIN_MINIMAP_WIDTH);
    }
}

QRect MinimapWidget::contentRect() const {
    return rect().adjusted(QT_MINIMAP_MARGIN, QT_MINIMAP_MARGIN, 
                          -QT_MINIMAP_MARGIN, -QT_MINIMAP_MARGIN);
}

data_addr_t MinimapWidget::positionToAddress(const QPoint& pos) const {
    if (!data_source_ || !data_source_->is_valid()) {
        return DATA_BADADDR;
    }
    
    const QRect content = contentRect();
    
    if (vertical_layout_) {
        const int y = pos.y() - content.top();
        return data_source_->y_to_address(y, content.height());
    } else {
        const int x = pos.x() - content.left();
        return data_source_->x_to_address(x, content.width());
    }
}

int MinimapWidget::addressToPosition(data_addr_t addr) const {
    if (!data_source_ || !data_source_->is_valid() || addr == DATA_BADADDR) {
        return -1;
    }
    
    const QRect content = contentRect();
    
    if (vertical_layout_) {
        const int y = data_source_->address_to_y(addr, content.height());
        return (y >= 0) ? y + content.top() : -1;
    } else {
        const int x = data_source_->address_to_x(addr, content.width());
        return (x >= 0) ? x + content.left() : -1;
    }
}

void MinimapWidget::renderToCache() {
    const QRect content = contentRect();
    
    if (content.isEmpty()) {
        cache_valid_ = false;
        return;
    }
    
    // Create or resize cache image
    if (cache_image_.width() != content.width() || 
        cache_image_.height() != content.height()) {
        cache_image_ = QImage(content.size(), QImage::Format_RGB32);
    }
    
    // Fill with background
    cache_image_.fill(QColor(
        colors::Background.r,
        colors::Background.g,
        colors::Background.b
    ));
    
    if (!data_source_ || !data_source_->is_valid() || data_source_->block_count() == 0) {
        cache_valid_ = true;
        cached_width_ = content.width();
        cached_height_ = content.height();
        return;
    }
    
    const ViewportData viewport = data_source_->get_viewport();
    const std::size_t num_blocks = data_source_->block_count();
    const data_size_t vp_range = viewport.range();
    
    if (vp_range == 0) {
        cache_valid_ = true;
        cached_width_ = content.width();
        cached_height_ = content.height();
        return;
    }
    
    // Render each block
    for (std::size_t i = 0; i < num_blocks; ++i) {
        const EntropyBlockData block = data_source_->get_block(i);
        
        // Skip blocks completely outside viewport
        if (block.end_addr <= viewport.start_addr || block.start_addr >= viewport.end_addr) {
            continue;
        }
        
        // Clamp block addresses to viewport for proper rendering
        const data_addr_t clamped_start = std::max(block.start_addr, viewport.start_addr);
        const data_addr_t clamped_end = std::min(block.end_addr, viewport.end_addr);
        
        // Get color for this block's entropy
        const Color color = gradient_.sample_entropy(block.entropy);
        const QColor qcolor(color.r, color.g, color.b);
        
        if (vertical_layout_) {
            // Calculate Y positions using direct ratio (avoids -1 return from address_to_y)
            const double t1 = static_cast<double>(clamped_start - viewport.start_addr) / static_cast<double>(vp_range);
            const double t2 = static_cast<double>(clamped_end - viewport.start_addr) / static_cast<double>(vp_range);
            
            const int y1 = static_cast<int>(t1 * content.height());
            const int y2 = static_cast<int>(t2 * content.height());
            
            const int start_y = std::max(0, std::min(y1, y2));
            const int end_y = std::min(content.height(), std::max(y1, y2) + 1);
            
            // Fill horizontal line for each row
            for (int y = start_y; y < end_y; ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(cache_image_.scanLine(y));
                for (int x = 0; x < content.width(); ++x) {
                    line[x] = qcolor.rgb();
                }
            }
        } else {
            // Horizontal layout
            const double t1 = static_cast<double>(clamped_start - viewport.start_addr) / static_cast<double>(vp_range);
            const double t2 = static_cast<double>(clamped_end - viewport.start_addr) / static_cast<double>(vp_range);
            
            const int x1 = static_cast<int>(t1 * content.width());
            const int x2 = static_cast<int>(t2 * content.width());
            
            const int start_x = std::max(0, std::min(x1, x2));
            const int end_x = std::min(content.width(), std::max(x1, x2) + 1);
            
            // Fill vertical column for each column
            for (int y = 0; y < content.height(); ++y) {
                QRgb* line = reinterpret_cast<QRgb*>(cache_image_.scanLine(y));
                for (int x = start_x; x < end_x; ++x) {
                    line[x] = qcolor.rgb();
                }
            }
        }
    }
    
    cache_valid_ = true;
    cached_width_ = content.width();
    cached_height_ = content.height();
}

void MinimapWidget::drawContent(QPainter& painter) {
    const QRect content = contentRect();
    
    // Render to cache if needed
    if (!cache_valid_ || 
        cached_width_ != content.width() || 
        cached_height_ != content.height()) {
        renderToCache();
    }
    
    // Draw cached image
    if (cache_valid_ && !cache_image_.isNull()) {
        painter.drawImage(content.topLeft(), cache_image_);
    }
}

void MinimapWidget::drawRegions(QPainter& painter) {
    if (!data_source_ || !data_source_->is_valid() || !show_regions_) {
        return;
    }
    
    const QRect content = contentRect();
    const std::size_t num_regions = data_source_->region_count();
    
    // Set up pen for black, wider segment separators
    QPen pen(QColor(colors::RegionBorder.r, colors::RegionBorder.g, 
                    colors::RegionBorder.b, colors::RegionBorder.a));
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(2);
    
    // Set up small font for segment names
    QFont smallFont = painter.font();
    smallFont.setPixelSize(10);
    painter.setFont(smallFont);
    QFontMetrics fm(smallFont);
    
    // Colors for text and background
    QColor textColor(colors::RegionText.r, colors::RegionText.g,
                     colors::RegionText.b, colors::RegionText.a);
    QColor bgColor(colors::RegionTextBg.r, colors::RegionTextBg.g,
                   colors::RegionTextBg.b, colors::RegionTextBg.a);
    
    for (std::size_t i = 0; i < num_regions; ++i) {
        const RegionData region = data_source_->get_region(i);
        const std::string name = data_source_->get_region_name_at(i);
        
        if (vertical_layout_) {
            const int y = addressToPosition(region.start_addr);
            if (y >= 0 && y < content.bottom() - 14) {
                // Draw separator line
                painter.setPen(pen);
                painter.drawLine(content.left(), y, content.right(), y);
                
                // Draw segment name below the separator with background
                if (!name.empty()) {
                    QString qname = QString::fromStdString(name);
                    QRect textRect = fm.boundingRect(qname);
                    int textX = content.left() + 3;
                    int textY = y + 3;
                    
                    // Draw background rectangle
                    QRect bgRect(textX - 2, textY, textRect.width() + 4, textRect.height() + 2);
                    painter.fillRect(bgRect, bgColor);
                    
                    // Draw text
                    painter.setPen(textColor);
                    painter.drawText(textX, textY + fm.ascent(), qname);
                }
            }
        } else {
            const int x = addressToPosition(region.start_addr);
            if (x >= 0 && x < content.right() - 20) {
                // Draw separator line
                painter.setPen(pen);
                painter.drawLine(x, content.top(), x, content.bottom());
                
                // Draw segment name to the right of the separator with background
                if (!name.empty()) {
                    QString qname = QString::fromStdString(name);
                    QRect textRect = fm.boundingRect(qname);
                    int textX = x + 3;
                    int textY = content.top() + 3;
                    
                    // Draw background rectangle
                    QRect bgRect(textX - 2, textY, textRect.width() + 4, textRect.height() + 2);
                    painter.fillRect(bgRect, bgColor);
                    
                    // Draw text
                    painter.setPen(textColor);
                    painter.drawText(textX, textY + fm.ascent(), qname);
                }
            }
        }
    }
}

void MinimapWidget::drawCursor(QPainter& painter) {
    if (!show_cursor_ || current_addr_ == DATA_BADADDR) {
        return;
    }
    
    const int pos = addressToPosition(current_addr_);
    if (pos < 0) return;
    
    const QRect content = contentRect();
    
    QPen pen(QColor(colors::CursorLine.r, colors::CursorLine.g, 
                    colors::CursorLine.b, colors::CursorLine.a));
    pen.setWidth(QT_CURSOR_LINE_HEIGHT);
    painter.setPen(pen);
    
    if (vertical_layout_) {
        painter.drawLine(content.left(), pos, content.right(), pos);
    } else {
        painter.drawLine(pos, content.top(), pos, content.bottom());
    }
}

void MinimapWidget::drawHover(QPainter& painter) {
    if (!is_hovering_ || hover_addr_ == DATA_BADADDR) {
        return;
    }
    
    const int pos = addressToPosition(hover_addr_);
    if (pos < 0) return;
    
    const QRect content = contentRect();
    
    // Draw subtle highlight
    QColor highlight(colors::HoverHighlight.r, colors::HoverHighlight.g,
                     colors::HoverHighlight.b, colors::HoverHighlight.a);
    
    if (vertical_layout_) {
        painter.fillRect(content.left(), pos - 2, content.width(), 5, highlight);
    } else {
        painter.fillRect(pos - 2, content.top(), 5, content.height(), highlight);
    }
}

void MinimapWidget::drawViewportFrame(QPainter& painter) {
    if (!show_viewport_frame_ || !data_source_ || !data_source_->is_valid()) {
        return;
    }
    
    if (visible_start_ == DATA_BADADDR || visible_end_ == DATA_BADADDR) {
        return;
    }
    
    const QRect content = contentRect();
    
    // Calculate positions for the visible range
    const int start_pos = addressToPosition(visible_start_);
    const int end_pos = addressToPosition(visible_end_);
    
    if (start_pos < 0 && end_pos < 0) {
        return;
    }
    
    QRect frame_rect;
    if (vertical_layout_) {
        const int y1 = std::max(content.top(), start_pos);
        const int y2 = std::min(content.bottom(), end_pos);
        if (y2 > y1) {
            frame_rect = QRect(content.left(), y1, content.width(), y2 - y1);
        }
    } else {
        const int x1 = std::max(content.left(), start_pos);
        const int x2 = std::min(content.right(), end_pos);
        if (x2 > x1) {
            frame_rect = QRect(x1, content.top(), x2 - x1, content.height());
        }
    }
    
    if (frame_rect.isEmpty()) {
        return;
    }
    
    // Draw semi-transparent fill
    painter.fillRect(frame_rect, QColor(
        colors::ViewportFrame.r, colors::ViewportFrame.g,
        colors::ViewportFrame.b, colors::ViewportFrame.a
    ));
    
    // Draw border
    QPen pen(QColor(colors::ViewportFrameBorder.r, colors::ViewportFrameBorder.g,
                    colors::ViewportFrameBorder.b, colors::ViewportFrameBorder.a));
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawRect(frame_rect.adjusted(0, 0, -1, -1));
}

void MinimapWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // Draw background
    painter.fillRect(rect(), QColor(
        colors::Background.r,
        colors::Background.g,
        colors::Background.b
    ));
    
    // Draw content
    drawContent(painter);
    
    // Draw overlays (order matters for visibility)
    drawRegions(painter);       // Faint segment separators
    drawViewportFrame(painter); // IDA visible range frame
    drawHover(painter);         // Hover highlight
    drawCursor(painter);        // Current cursor position
    
    // Draw border
    painter.setPen(QColor(64, 64, 64));
    painter.drawRect(contentRect().adjusted(0, 0, -1, -1));
}

void MinimapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const data_addr_t addr = positionToAddress(event->pos());
        
        if (addr != DATA_BADADDR) {
            // Start drag for panning
            is_dragging_ = true;
            drag_start_ = event->pos();
            drag_start_addr_ = addr;
            
            // Also navigate on click
            if (onAddressClicked) {
                onAddressClicked(addr);
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void MinimapWidget::mouseMoveEvent(QMouseEvent* event) {
    const data_addr_t addr = positionToAddress(event->pos());
    
    if (is_dragging_) {
        // Continuously navigate to address while dragging
        if (addr != DATA_BADADDR && onAddressClicked) {
            onAddressClicked(addr);
        }
        // Update hover state during drag too
        hover_addr_ = addr;
        update();
    } else {
        // Update hover state
        hover_addr_ = addr;
        
        if (onAddressHovered && addr != DATA_BADADDR) {
            onAddressHovered(addr);
        }
        
        // Show tooltip with address, segment, and entropy info
        if (addr != DATA_BADADDR && data_source_) {
            const double entropy = data_source_->entropy_at(addr);
            const std::string segment_name = data_source_->get_region_name(addr);
            
            QString tooltip = QString("Address: 0x%1").arg(addr, 0, 16);
            
            if (!segment_name.empty()) {
                tooltip += QString("\nSegment: %1").arg(QString::fromStdString(segment_name));
            }
            
            if (entropy >= 0.0) {
                tooltip += QString("\nEntropy: %1").arg(entropy, 0, 'f', 2);
            }
            
            QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
        }
        
        update();
    }
    
    QWidget::mouseMoveEvent(event);
}

void MinimapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        is_dragging_ = false;
    }
    
    QWidget::mouseReleaseEvent(event);
}

void MinimapWidget::wheelEvent(QWheelEvent* event) {
    if (!data_source_) {
        QWidget::wheelEvent(event);
        return;
    }
    
    // Get wheel delta
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    
    // Calculate zoom factor
    const double factor = (delta > 0) ? 1.2 : (1.0 / 1.2);
    
    // Get address at cursor position for centered zoom
    const data_addr_t center_addr = positionToAddress(event->position().toPoint());
    
    if (center_addr != DATA_BADADDR) {
        data_source_->zoom(factor, center_addr);
        invalidateCache();
        update();
    }
    
    event->accept();
}

void MinimapWidget::resizeEvent(QResizeEvent* event) {
    invalidateCache();
    QWidget::resizeEvent(event);
}

void MinimapWidget::enterEvent(QEnterEvent* event) {
    is_hovering_ = true;
    QWidget::enterEvent(event);
}

void MinimapWidget::leaveEvent(QEvent* event) {
    is_hovering_ = false;
    hover_addr_ = DATA_BADADDR;
    update();
    QWidget::leaveEvent(event);
}

} // namespace synopsia

#endif // SYNOPSIA_USE_QT
