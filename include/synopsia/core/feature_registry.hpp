/// @file feature_registry.hpp
/// @brief Central registry for plugin features

#pragma once

#include "feature_base.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace synopsia {

/// @class FeatureRegistry
/// @brief Central registry for managing plugin features
///
/// The registry owns all features and handles their lifecycle.
/// It also provides lookup by ID and iteration over features.
class FeatureRegistry {
public:
    FeatureRegistry() = default;
    ~FeatureRegistry() = default;

    // Non-copyable, non-movable
    FeatureRegistry(const FeatureRegistry&) = delete;
    FeatureRegistry& operator=(const FeatureRegistry&) = delete;
    FeatureRegistry(FeatureRegistry&&) = delete;
    FeatureRegistry& operator=(FeatureRegistry&&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    /// @brief Register a feature with the registry
    /// @param feature The feature to register (ownership transferred)
    /// @return true if registration succeeded
    bool register_feature(std::unique_ptr<IFeature> feature);

    /// @brief Unregister a feature by ID
    /// @param id The feature ID to unregister
    void unregister_feature(const std::string& id);

    // =========================================================================
    // Lookup
    // =========================================================================

    /// @brief Get a feature by ID
    /// @param id The feature ID
    /// @return Pointer to the feature, or nullptr if not found
    [[nodiscard]] IFeature* get_feature(const std::string& id) const;

    /// @brief Check if a feature is registered
    [[nodiscard]] bool has_feature(const std::string& id) const;

    /// @brief Get the number of registered features
    [[nodiscard]] std::size_t count() const noexcept { return features_.size(); }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// @brief Initialize all registered features
    /// @return Number of successfully initialized features
    std::size_t initialize_all();

    /// @brief Cleanup all registered features
    void cleanup_all();

    // =========================================================================
    // Event Broadcasting
    // =========================================================================

    /// @brief Broadcast cursor change to all features
    void broadcast_cursor_changed(ea_t addr);

    /// @brief Broadcast database closed to all features
    void broadcast_database_closed();

    /// @brief Broadcast database modified to all features
    void broadcast_database_modified();

    // =========================================================================
    // Iteration
    // =========================================================================

    /// @brief Iterate over all features
    template<typename Func>
    void for_each(Func&& func) const {
        for (const auto& feature : features_) {
            func(feature.get());
        }
    }

private:
    std::vector<std::unique_ptr<IFeature>> features_;
    std::unordered_map<std::string, IFeature*> id_map_;
};

} // namespace synopsia
