/// @file entropy.hpp
/// @brief Jensen-Shannon divergence calculation engine

#pragma once

#include "types.hpp"
#include <array>
#include <span>

namespace synopsia {

/// @class EntropyCalculator
/// @brief Calculates Jensen-Shannon divergence for binary data blocks
///
/// JS divergence measures how different the byte distribution is from uniform.
/// We compare the observed distribution P against uniform distribution Q (1/256).
/// Result is scaled to 0-8 range for compatibility with entropy visualization:
/// - Low value (0-4): Repetitive patterns, zeros, simple data (high JS divergence)
/// - Medium value (4-7): Code, structured data
/// - High value (7-8): Random/uniform data (low JS divergence from uniform)
class EntropyCalculator {
public:
    /// @brief Calculate JS divergence for a data buffer (scaled to 0-8)
    /// @param data Pointer to data buffer
    /// @param size Size of data buffer in bytes
    /// @return Scaled JS divergence value (0.0 to 8.0), where 8 = uniform/random
    [[nodiscard]] static double calculate(const void* data, std::size_t size);
    
    /// @brief Calculate JS divergence for a span of bytes
    /// @param data Span of byte data
    /// @return Scaled JS divergence value (0.0 to 8.0)
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
    
    // Calculate Jensen-Shannon divergence between observed distribution P
    // and uniform distribution Q (1/256 for each byte value)
    //
    // JS(P || Q) = 0.5 * KL(P || M) + 0.5 * KL(Q || M)
    // where M = 0.5 * (P + Q)
    //
    // KL(P || Q) = sum(P(x) * log2(P(x) / Q(x)))
    
    constexpr double uniform_prob = 1.0 / 256.0;  // Q(x) = 1/256 for all x
    const double size_d = static_cast<double>(size);
    
    double kl_p_m = 0.0;  // KL(P || M)
    double kl_q_m = 0.0;  // KL(Q || M)
    
    for (std::size_t i = 0; i < 256; ++i) {
        const double p = static_cast<double>(frequency[i]) / size_d;  // P(x)
        const double q = uniform_prob;                                  // Q(x)
        const double m = 0.5 * (p + q);                                // M(x)
        
        // KL(P || M): sum over x where P(x) > 0
        if (p > 0.0 && m > 0.0) {
            kl_p_m += p * std::log2(p / m);
        }
        
        // KL(Q || M): Q is always > 0 (uniform), so always contribute
        if (m > 0.0) {
            kl_q_m += q * std::log2(q / m);
        }
    }
    
    // JS divergence (bounded 0 to 1)
    const double js_divergence = 0.5 * kl_p_m + 0.5 * kl_q_m;
    
    // Scale to 0-8 range, inverted so that:
    // - High value (8) = uniform/random (low JS divergence from uniform)
    // - Low value (0) = structured/repetitive (high JS divergence from uniform)
    return (1.0 - js_divergence) * 8.0;
}

inline double EntropyCalculator::calculate(std::span<const std::uint8_t> data) {
    return calculate(data.data(), data.size());
}

} // namespace synopsia
