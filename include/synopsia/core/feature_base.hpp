/// @file feature_base.hpp
/// @brief Abstract base class for plugin features

#pragma once

#include <synopsia/common/types.hpp>

namespace synopsia {

/// @class IFeature
/// @brief Abstract interface for plugin features
///
/// Each feature implements this interface to integrate with the plugin.
/// Features are self-contained units with their own UI, data model, and actions.
class IFeature {
public:
    virtual ~IFeature() = default;

    // =========================================================================
    // Identity
    // =========================================================================

    /// @brief Get the unique identifier for this feature
    [[nodiscard]] virtual const char* id() const noexcept = 0;

    /// @brief Get the display name for this feature
    [[nodiscard]] virtual const char* name() const noexcept = 0;

    /// @brief Get the description of this feature
    [[nodiscard]] virtual const char* description() const noexcept = 0;

    /// @brief Get the hotkey for this feature (or nullptr if none)
    [[nodiscard]] virtual const char* hotkey() const noexcept = 0;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// @brief Initialize the feature
    /// @return true if initialization succeeded
    virtual bool initialize() = 0;

    /// @brief Clean up the feature
    virtual void cleanup() = 0;

    /// @brief Check if the feature is initialized
    [[nodiscard]] virtual bool is_initialized() const noexcept = 0;

    // =========================================================================
    // UI Management
    // =========================================================================

    /// @brief Show the feature's UI
    virtual void show() = 0;

    /// @brief Hide the feature's UI
    virtual void hide() = 0;

    /// @brief Toggle the feature's UI visibility
    virtual void toggle() = 0;

    /// @brief Check if the feature's UI is visible
    [[nodiscard]] virtual bool is_visible() const noexcept = 0;

    // =========================================================================
    // Event Handling
    // =========================================================================

    /// @brief Handle cursor position changes
    virtual void on_cursor_changed(ea_t addr) = 0;

    /// @brief Handle database closed event
    virtual void on_database_closed() = 0;

    /// @brief Handle database modifications
    virtual void on_database_modified() = 0;
};

/// @class FeatureBase
/// @brief Base implementation with common functionality
///
/// Provides default implementations for common feature operations.
class FeatureBase : public IFeature {
public:
    FeatureBase() = default;
    ~FeatureBase() override = default;

    // Non-copyable, non-movable
    FeatureBase(const FeatureBase&) = delete;
    FeatureBase& operator=(const FeatureBase&) = delete;
    FeatureBase(FeatureBase&&) = delete;
    FeatureBase& operator=(FeatureBase&&) = delete;

    // =========================================================================
    // Default Implementations
    // =========================================================================

    [[nodiscard]] bool is_initialized() const noexcept override {
        return initialized_;
    }

    [[nodiscard]] bool is_visible() const noexcept override {
        return visible_;
    }

    void toggle() override {
        if (visible_) {
            hide();
        } else {
            show();
        }
    }

    // Default no-op implementations for optional events
    void on_cursor_changed(ea_t) override {}
    void on_database_closed() override { hide(); }
    void on_database_modified() override {}

protected:
    bool initialized_ = false;
    bool visible_ = false;
    TWidget* widget_ = nullptr;
    void* content_ = nullptr;
};

} // namespace synopsia
