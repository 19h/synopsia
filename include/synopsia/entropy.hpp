/// @file entropy.hpp
/// @brief Shannon entropy calculation engine

#pragma once

#include "types.hpp"
#include <array>
#include <span>

namespace synopsia {

/// @class EntropyCalculator
/// @brief Calculates Shannon entropy for binary data blocks
///
/// Shannon entropy measures the amount of randomness/information in data.
/// - Low entropy (0-4): Repetitive patterns, zeros, simple data
/// - Medium entropy (4-7): Code, structured data
/// - High entropy (7-8): Encrypted, compressed, or random data
class EntropyCalculator {
public:
    /// @brief Calculate Shannon entropy for a data buffer
    /// @param data Pointer to data buffer
    /// @param size Size of data buffer in bytes
    /// @return Entropy value in bits (0.0 to 8.0)
    [[nodiscard]] static double calculate(const void* data, std::size_t size);
    
    /// @brief Calculate entropy for a span of bytes
    /// @param data Span of byte data
    /// @return Entropy value in bits (0.0 to 8.0)
    [[nodiscard]] static double calculate(std::span<const std::uint8_t> data);
    
    /// @brief Analyze entire database and compute entropy blocks
    /// @param block_size Size of each analysis block in bytes
    /// @return Vector of entropy blocks covering the database
    [[nodiscard]] std::vector<EntropyBlock> analyze_database(std::size_t block_size = DEFAULT_BLOCK_SIZE) const;
    
    /// @brief Analyze a specific address range
    /// @param start_ea Start address
    /// @param end_ea End address (exclusive)
    /// @param block_size Size of each analysis block
    /// @return Vector of entropy blocks covering the range
    [[nodiscard]] std::vector<EntropyBlock> analyze_range(
        ea_t start_ea,
        ea_t end_ea,
        std::size_t block_size = DEFAULT_BLOCK_SIZE
    ) const;
    
    /// @brief Analyze a single segment
    /// @param seg Segment to analyze
    /// @param block_size Size of each analysis block
    /// @return Vector of entropy blocks for the segment
    [[nodiscard]] std::vector<EntropyBlock> analyze_segment(
        const segment_t* seg,
        std::size_t block_size = DEFAULT_BLOCK_SIZE
    ) const;
    
    /// @brief Get all memory regions (segments) in the database
    /// @return Vector of memory region descriptors
    [[nodiscard]] std::vector<MemoryRegion> get_memory_regions() const;
    
    /// @brief Calculate entropy for data at a specific address
    /// @param ea Start address
    /// @param size Number of bytes to analyze
    /// @return Entropy value, or -1.0 if data cannot be read
    [[nodiscard]] double calculate_at_address(ea_t ea, std::size_t size) const;
    
private:
    /// Internal buffer for reading database bytes
    mutable std::vector<std::uint8_t> read_buffer_;
    
    /// @brief Read bytes from database into buffer
    /// @param ea Start address
    /// @param size Number of bytes to read
    /// @return Number of bytes actually read
    std::size_t read_bytes(ea_t ea, std::size_t size) const;
};

// =============================================================================
// Implementation Details (inline for performance)
// =============================================================================

inline double EntropyCalculator::calculate(const void* data, std::size_t size) {
    if (data == nullptr || size == 0) {
        return 0.0;
    }
    
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    
    // Count byte frequencies
    std::array<std::size_t, 256> frequency{};
    for (std::size_t i = 0; i < size; ++i) {
        ++frequency[bytes[i]];
    }
    
    // Calculate Shannon entropy: H = -sum(p * log2(p))
    double entropy = 0.0;
    
    for (std::size_t count : frequency) {
        if (count > 0) {
            // p = count / size
            // p * log2(p) = (count/size) * log2(count/size)
            //             = (count/size) * (log2(count) - log2(size))
            const double p = static_cast<double>(count) / static_cast<double>(size);
            entropy -= p * std::log2(p);
        }
    }
    
    return entropy;
}

inline double EntropyCalculator::calculate(std::span<const std::uint8_t> data) {
    return calculate(data.data(), data.size());
}

} // namespace synopsia
