/// @file feature_registry.cpp
/// @brief Feature registry implementation

#include <synopsia/core/feature_registry.hpp>

namespace synopsia {

bool FeatureRegistry::register_feature(std::unique_ptr<IFeature> feature) {
    if (!feature) {
        return false;
    }

    const std::string id = feature->id();

    // Check for duplicate
    if (id_map_.find(id) != id_map_.end()) {
        return false;
    }

    IFeature* raw_ptr = feature.get();
    features_.push_back(std::move(feature));
    id_map_[id] = raw_ptr;

    return true;
}

void FeatureRegistry::unregister_feature(const std::string& id) {
    auto it = id_map_.find(id);
    if (it == id_map_.end()) {
        return;
    }

    IFeature* feature = it->second;

    // Cleanup if initialized
    if (feature->is_initialized()) {
        feature->cleanup();
    }

    // Remove from vector
    features_.erase(
        std::remove_if(features_.begin(), features_.end(),
            [feature](const std::unique_ptr<IFeature>& f) {
                return f.get() == feature;
            }),
        features_.end()
    );

    // Remove from map
    id_map_.erase(it);
}

IFeature* FeatureRegistry::get_feature(const std::string& id) const {
    auto it = id_map_.find(id);
    return (it != id_map_.end()) ? it->second : nullptr;
}

bool FeatureRegistry::has_feature(const std::string& id) const {
    return id_map_.find(id) != id_map_.end();
}

std::size_t FeatureRegistry::initialize_all() {
    std::size_t count = 0;
    for (auto& feature : features_) {
        if (feature->initialize()) {
            ++count;
        }
    }
    return count;
}

void FeatureRegistry::cleanup_all() {
    for (auto& feature : features_) {
        if (feature->is_initialized()) {
            feature->cleanup();
        }
    }
}

void FeatureRegistry::broadcast_cursor_changed(ea_t addr) {
    for (auto& feature : features_) {
        if (feature->is_initialized()) {
            feature->on_cursor_changed(addr);
        }
    }
}

void FeatureRegistry::broadcast_database_closed() {
    for (auto& feature : features_) {
        if (feature->is_initialized()) {
            feature->on_database_closed();
        }
    }
}

void FeatureRegistry::broadcast_database_modified() {
    for (auto& feature : features_) {
        if (feature->is_initialized()) {
            feature->on_database_modified();
        }
    }
}

} // namespace synopsia
