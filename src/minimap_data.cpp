/// @file minimap_data.cpp
/// @brief Minimap data model implementation

#include <synopsia/minimap_data.hpp>

namespace synopsia {

MinimapData::MinimapData() {
    // Initialize with empty state
}

bool MinimapData::refresh(std::size_t block_size) {
    // Check if database is loaded
    if (!is_database_loaded()) {
        valid_.store(false);
        return false;
    }
    
    // Save block size
    block_size_ = block_size;
    
    // Get database range
    auto [db_min, db_max] = get_database_range();
    db_start_ = db_min;
    db_end_ = db_max;
    
    // Analyze entropy
    blocks_ = calculator_.analyze_database(block_size);
    
    // Get memory regions
    regions_ = calculator_.get_memory_regions();
    
    // Compute statistics
    compute_statistics();
    
    // Reset viewport to show entire database
    reset_viewport();
    
    valid_.store(true);
    return true;
}

void MinimapData::compute_statistics() {
    if (blocks_.empty()) {
        min_entropy_ = 0.0;
        max_entropy_ = 0.0;
        avg_entropy_ = 0.0;
        return;
    }
    
    min_entropy_ = MAX_ENTROPY;
    max_entropy_ = 0.0;
    double total = 0.0;
    
    for (const auto& block : blocks_) {
        min_entropy_ = std::min(min_entropy_, block.entropy);
        max_entropy_ = std::max(max_entropy_, block.entropy);
        total += block.entropy;
    }
    
    avg_entropy_ = total / static_cast<double>(blocks_.size());
}

void MinimapData::reset_viewport() {
    viewport_.reset(db_start_, db_end_);
}

void MinimapData::set_viewport(ea_t start, ea_t end) {
    if (start >= end) return;
    
    // Clamp to database bounds
    viewport_.start_ea = std::max(start, db_start_);
    viewport_.end_ea = std::min(end, db_end_);
    
    // Calculate zoom factor
    const asize_t db_range = db_end_ - db_start_;
    const asize_t vp_range = viewport_.range();
    
    if (db_range > 0) {
        viewport_.zoom = static_cast<double>(db_range) / static_cast<double>(vp_range);
    } else {
        viewport_.zoom = 1.0;
    }
}

void MinimapData::zoom_ida(double factor, ea_t center) {
    if (factor <= 0.0) return;
    
    // Get current range
    const asize_t old_range = viewport_.range();
    const asize_t new_range = static_cast<asize_t>(old_range / factor);
    
    // Minimum range check (at least one block)
    if (new_range < block_size_) return;
    
    // Maximum range check (entire database)
    const asize_t db_range = db_end_ - db_start_;
    const asize_t clamped_range = std::min(new_range, db_range);
    
    // Calculate new viewport centered on the given address
    const double center_ratio = static_cast<double>(center - viewport_.start_ea) / 
                                static_cast<double>(old_range);
    
    const asize_t offset_before = static_cast<asize_t>(center_ratio * clamped_range);
    
    ea_t new_start = (center >= offset_before) ? center - offset_before : db_start_;
    ea_t new_end = new_start + clamped_range;
    
    // Clamp to database bounds
    if (new_end > db_end_) {
        new_end = db_end_;
        new_start = (new_end >= clamped_range) ? new_end - clamped_range : db_start_;
    }
    if (new_start < db_start_) {
        new_start = db_start_;
        new_end = std::min(new_start + clamped_range, db_end_);
    }
    
    viewport_.start_ea = new_start;
    viewport_.end_ea = new_end;
    viewport_.zoom = static_cast<double>(db_range) / static_cast<double>(viewport_.range());
}

void MinimapData::pan_ida(sval_t delta) {
    const asize_t range = viewport_.range();
    
    ea_t new_start = viewport_.start_ea;
    ea_t new_end = viewport_.end_ea;
    
    if (delta > 0) {
        // Panning towards higher addresses
        const asize_t max_delta = db_end_ - viewport_.end_ea;
        const asize_t actual_delta = std::min(static_cast<asize_t>(delta), max_delta);
        new_start += actual_delta;
        new_end += actual_delta;
    } else if (delta < 0) {
        // Panning towards lower addresses
        const asize_t max_delta = viewport_.start_ea - db_start_;
        const asize_t actual_delta = std::min(static_cast<asize_t>(-delta), max_delta);
        new_start -= actual_delta;
        new_end -= actual_delta;
    }
    
    viewport_.start_ea = new_start;
    viewport_.end_ea = new_end;
}

} // namespace synopsia
