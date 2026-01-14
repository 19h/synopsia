/// @file widget_bridge.cpp
/// @brief Bridge between IDA plugin and Qt widget
///
/// This file handles the Qt widget creation WITHOUT including IDA headers.
/// It provides C-linkage functions that can be called from the IDA plugin code.

#ifdef SYNOPSIA_USE_QT

#include <synopsia/minimap_widget.hpp>
#include <QVBoxLayout>
#include <QWidget>

// Use extern "C" to avoid name mangling and allow calling from IDA-side code
extern "C" {

/// Create a MinimapWidget and set it up with a parent
void* synopsia_create_minimap_widget(void* parent_widget, void* data_source) {
    QWidget* parent = reinterpret_cast<QWidget*>(parent_widget);
    synopsia::IMinimapDataSource* source = 
        reinterpret_cast<synopsia::IMinimapDataSource*>(data_source);
    
    synopsia::MinimapWidget* widget = new synopsia::MinimapWidget(parent);
    widget->setDataSource(source);
    
    return widget;
}

/// Add the widget to the parent's layout
void synopsia_add_widget_to_layout(void* parent_widget, void* minimap_widget) {
    QWidget* parent = reinterpret_cast<QWidget*>(parent_widget);
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    
    QVBoxLayout* layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);
}

/// Set the address click callback
void synopsia_set_address_callback(void* minimap_widget, void (*callback)(std::uint64_t)) {
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    
    widget->onAddressClicked = [callback](synopsia::data_addr_t addr) {
        if (callback) {
            callback(addr);
        }
    };
}

/// Set the refresh callback
void synopsia_set_refresh_callback(void* minimap_widget, void (*callback)()) {
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    
    widget->onRefreshRequested = [callback]() {
        if (callback) {
            callback();
        }
    };
}

/// Refresh the widget
void synopsia_refresh_widget(void* minimap_widget) {
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    widget->refresh();
}

/// Set current address
void synopsia_set_current_address(void* minimap_widget, std::uint64_t addr) {
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    widget->setCurrentAddress(addr);
}

/// Configure widget display options
void synopsia_configure_widget(void* minimap_widget, bool show_cursor, 
                               bool show_regions, bool vertical_layout) {
    synopsia::MinimapWidget* widget = 
        reinterpret_cast<synopsia::MinimapWidget*>(minimap_widget);
    widget->setShowCursor(show_cursor);
    widget->setShowRegions(show_regions);
    widget->setVerticalLayout(vertical_layout);
}

} // extern "C"

#endif // SYNOPSIA_USE_QT
