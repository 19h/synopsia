/// @file entropy.cpp
/// @brief Shannon entropy calculation implementation

#include <synopsia/entropy.hpp>

namespace synopsia {

std::size_t EntropyCalculator::read_bytes(ea_t ea, std::size_t size) const {
    // Resize buffer if needed
    if (read_buffer_.size() < size) {
        read_buffer_.resize(size);
    }
    
    // Read bytes from IDA database
    // get_bytes returns the number of bytes read, or -1 on error
    ssize_t bytes_read = get_bytes(read_buffer_.data(), size, ea);
    
    if (bytes_read < 0) {
        return 0;
    }
    
    return static_cast<std::size_t>(bytes_read);
}

double EntropyCalculator::calculate_at_address(ea_t ea, std::size_t size) const {
    std::size_t bytes_read = read_bytes(ea, size);
    
    if (bytes_read == 0) {
        return -1.0;
    }
    
    return calculate(read_buffer_.data(), bytes_read);
}

std::vector<MemoryRegion> EntropyCalculator::get_memory_regions() const {
    std::vector<MemoryRegion> regions;
    
    // Iterate over all segments
    for (int i = 0; i < get_segm_qty(); ++i) {
        segment_t* seg = getnseg(i);
        if (!seg) continue;
        
        MemoryRegion region;
        region.start_ea = seg->start_ea;
        region.end_ea = seg->end_ea;
        
        // Get segment name
        qstring name;
        if (get_segm_name(&name, seg) > 0) {
            region.name = name;
        } else {
            region.name.sprnt("seg_%d", i);
        }
        
        // Check permissions and type
        region.readable = (seg->perm & SEGPERM_READ) != 0;
        
        // Check if segment has initialized data
        // SEG_DATA, SEG_CODE typically have initialized data
        // SEG_BSS is uninitialized
        region.initialized = (seg->type == SEG_CODE || seg->type == SEG_DATA);
        
        regions.push_back(std::move(region));
    }
    
    return regions;
}

std::vector<EntropyBlock> EntropyCalculator::analyze_segment(
    const segment_t* seg,
    std::size_t block_size
) const {
    std::vector<EntropyBlock> blocks;
    
    if (!seg || block_size == 0) {
        return blocks;
    }
    
    const ea_t start_ea = seg->start_ea;
    const ea_t end_ea = seg->end_ea;
    
    // Reserve approximate capacity
    const asize_t seg_size = end_ea - start_ea;
    const std::size_t num_blocks = (seg_size + block_size - 1) / block_size;
    blocks.reserve(num_blocks);
    
    // Iterate through the segment in block-sized chunks
    for (ea_t ea = start_ea; ea < end_ea; ) {
        // Calculate actual block size (may be smaller at end)
        const std::size_t remaining = end_ea - ea;
        const std::size_t actual_size = std::min(block_size, remaining);
        
        EntropyBlock block;
        block.start_ea = ea;
        block.end_ea = ea + actual_size;
        
        // Read and calculate entropy
        const std::size_t bytes_read = read_bytes(ea, actual_size);
        
        if (bytes_read > 0) {
            block.entropy = calculate(read_buffer_.data(), bytes_read);
        } else {
            // If we can't read, assume zero entropy (padding/uninitialized)
            block.entropy = 0.0;
        }
        
        blocks.push_back(block);
        ea += actual_size;
    }
    
    return blocks;
}

std::vector<EntropyBlock> EntropyCalculator::analyze_range(
    ea_t start_ea,
    ea_t end_ea,
    std::size_t block_size
) const {
    std::vector<EntropyBlock> blocks;
    
    if (start_ea >= end_ea || block_size == 0) {
        return blocks;
    }
    
    // Reserve approximate capacity
    const asize_t range_size = end_ea - start_ea;
    const std::size_t num_blocks = (range_size + block_size - 1) / block_size;
    blocks.reserve(num_blocks);
    
    // Iterate through the range in block-sized chunks
    for (ea_t ea = start_ea; ea < end_ea; ) {
        const std::size_t remaining = end_ea - ea;
        const std::size_t actual_size = std::min(block_size, remaining);
        
        EntropyBlock block;
        block.start_ea = ea;
        block.end_ea = ea + actual_size;
        
        const std::size_t bytes_read = read_bytes(ea, actual_size);
        
        if (bytes_read > 0) {
            block.entropy = calculate(read_buffer_.data(), bytes_read);
        } else {
            block.entropy = 0.0;
        }
        
        blocks.push_back(block);
        ea += actual_size;
    }
    
    return blocks;
}

std::vector<EntropyBlock> EntropyCalculator::analyze_database(std::size_t block_size) const {
    std::vector<EntropyBlock> all_blocks;
    
    // Analyze each segment
    for (int i = 0; i < get_segm_qty(); ++i) {
        segment_t* seg = getnseg(i);
        if (!seg) continue;
        
        // Skip non-readable segments
        if ((seg->perm & SEGPERM_READ) == 0) {
            continue;
        }
        
        auto seg_blocks = analyze_segment(seg, block_size);
        all_blocks.insert(all_blocks.end(), seg_blocks.begin(), seg_blocks.end());
    }
    
    // Sort by address (should already be sorted, but ensure it)
    std::sort(all_blocks.begin(), all_blocks.end(),
        [](const EntropyBlock& a, const EntropyBlock& b) {
            return a.start_ea < b.start_ea;
        });
    
    return all_blocks;
}

} // namespace synopsia
