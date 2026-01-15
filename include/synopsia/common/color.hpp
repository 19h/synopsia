/// @file color.hpp
/// @brief Color gradient utilities for entropy visualization

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

namespace synopsia {

/// Shannon entropy maximum value (8 bits per byte = 8.0)
inline constexpr double MAX_ENTROPY_VALUE = 8.0;

/// @struct Color
/// @brief RGBA color representation
struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
    
    constexpr Color() noexcept : r(0), g(0), b(0), a(255) {}
    
    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255) noexcept
        : r(red), g(green), b(blue), a(alpha) {}
    
    /// Create from 32-bit ARGB value
    [[nodiscard]] static constexpr Color from_argb(std::uint32_t argb) noexcept {
        return Color(
            static_cast<std::uint8_t>((argb >> 16) & 0xFF),
            static_cast<std::uint8_t>((argb >> 8) & 0xFF),
            static_cast<std::uint8_t>(argb & 0xFF),
            static_cast<std::uint8_t>((argb >> 24) & 0xFF)
        );
    }
    
    /// Convert to 32-bit ARGB value
    [[nodiscard]] constexpr std::uint32_t to_argb() const noexcept {
        return (static_cast<std::uint32_t>(a) << 24) |
               (static_cast<std::uint32_t>(r) << 16) |
               (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }
    
    /// Convert to 32-bit RGBA value
    [[nodiscard]] constexpr std::uint32_t to_rgba() const noexcept {
        return (static_cast<std::uint32_t>(r) << 24) |
               (static_cast<std::uint32_t>(g) << 16) |
               (static_cast<std::uint32_t>(b) << 8) |
               static_cast<std::uint32_t>(a);
    }
};

/// @class ColorGradient
/// @brief Multi-stop color gradient for entropy visualization
///
/// Default gradient:
/// - 0.0 (entropy 0): Dark blue  - zeros, padding
/// - 0.3 (entropy ~2.4): Cyan    - simple data
/// - 0.5 (entropy 4): Green      - typical code
/// - 0.7 (entropy ~5.6): Yellow  - mixed content
/// - 0.85 (entropy ~6.8): Orange - compressed/encrypted
/// - 1.0 (entropy 8): Red        - maximum entropy
class ColorGradient {
public:
    /// Color stop definition
    struct Stop {
        double position;    ///< Position in gradient (0.0 to 1.0)
        Color color;        ///< Color at this position
        
        constexpr Stop(double pos, Color col) noexcept : position(pos), color(col) {}
    };
    
    /// Default constructor creates the standard entropy gradient
    ColorGradient();
    
    /// Create gradient with custom stops
    explicit ColorGradient(std::vector<Stop> stops);
    
    /// @brief Sample the gradient at a given position
    /// @param t Position in gradient (0.0 to 1.0, will be clamped)
    /// @return Interpolated color
    [[nodiscard]] Color sample(double t) const;
    
    /// @brief Sample the gradient using entropy value directly
    /// @param entropy Entropy value (0.0 to 8.0)
    /// @return Interpolated color based on normalized entropy
    [[nodiscard]] Color sample_entropy(double entropy) const;
    
    /// Get the current gradient stops
    [[nodiscard]] const std::vector<Stop>& stops() const noexcept { return stops_; }
    
    /// @brief Create the default entropy gradient
    /// @return Standard entropy visualization gradient
    [[nodiscard]] static ColorGradient create_default();
    
    /// @brief Create a simple two-color gradient
    /// @param low Color for low values (t=0)
    /// @param high Color for high values (t=1)
    /// @return Two-stop gradient
    [[nodiscard]] static ColorGradient create_simple(Color low, Color high);
    
    /// @brief Create a grayscale gradient
    /// @return Gradient from black to white
    [[nodiscard]] static ColorGradient create_grayscale();
    
    /// @brief Create a "fire" gradient (black -> red -> yellow -> white)
    /// @return Fire-style gradient
    [[nodiscard]] static ColorGradient create_fire();
    
private:
    std::vector<Stop> stops_;
    
    /// Linear interpolation between two colors
    [[nodiscard]] static Color lerp(const Color& a, const Color& b, double t) noexcept;
};

// =============================================================================
// Predefined Colors
// =============================================================================

namespace colors {

// Basic colors
inline constexpr Color Black{0, 0, 0};
inline constexpr Color White{255, 255, 255};
inline constexpr Color Red{255, 0, 0};
inline constexpr Color Green{0, 255, 0};
inline constexpr Color Blue{0, 0, 255};
inline constexpr Color Yellow{255, 255, 0};
inline constexpr Color Cyan{0, 255, 255};
inline constexpr Color Magenta{255, 0, 255};

// Entropy gradient colors
inline constexpr Color LowEntropy{16, 32, 128};      ///< Dark blue for zeros/padding
inline constexpr Color MedLowEntropy{32, 128, 192};  ///< Cyan for simple data
inline constexpr Color MedEntropy{32, 192, 64};      ///< Green for typical code
inline constexpr Color MedHighEntropy{224, 192, 32}; ///< Yellow for mixed content
inline constexpr Color HighEntropy{224, 96, 16};     ///< Orange for compressed
inline constexpr Color MaxEntropy{192, 16, 16};      ///< Red for encrypted/random

// UI colors
inline constexpr Color Background{32, 32, 32};
inline constexpr Color CursorLine{255, 255, 255, 200};
inline constexpr Color RegionBorder{0, 0, 0, 255};             ///< Black segment separators
inline constexpr Color RegionText{220, 220, 220, 255};         ///< Segment name text color (brighter)
inline constexpr Color RegionTextBg{0, 0, 0, 180};             ///< Semi-transparent background for segment text
inline constexpr Color HoverHighlight{255, 255, 255, 64};

} // namespace colors

// =============================================================================
// Inline Implementation
// =============================================================================

inline Color ColorGradient::lerp(const Color& a, const Color& b, double t) noexcept {
    // Clamp t to [0, 1]
    t = (t < 0.0) ? 0.0 : (t > 1.0) ? 1.0 : t;
    
    return Color(
        static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<std::uint8_t>(a.b + (b.b - a.b) * t),
        static_cast<std::uint8_t>(a.a + (b.a - a.a) * t)
    );
}

inline Color ColorGradient::sample(double t) const {
    if (stops_.empty()) {
        return Color{};
    }
    
    // Clamp t to [0, 1]
    t = (t < 0.0) ? 0.0 : (t > 1.0) ? 1.0 : t;
    
    // Find surrounding stops
    const Stop* prev = &stops_.front();
    const Stop* next = &stops_.back();
    
    for (std::size_t i = 0; i < stops_.size() - 1; ++i) {
        if (t >= stops_[i].position && t <= stops_[i + 1].position) {
            prev = &stops_[i];
            next = &stops_[i + 1];
            break;
        }
    }
    
    // Handle edge cases
    if (t <= prev->position) return prev->color;
    if (t >= next->position) return next->color;
    
    // Interpolate
    const double range = next->position - prev->position;
    const double local_t = (range > 0.0) ? (t - prev->position) / range : 0.0;
    
    return lerp(prev->color, next->color, local_t);
}

inline Color ColorGradient::sample_entropy(double entropy) const {
    return sample(entropy / MAX_ENTROPY_VALUE);
}

} // namespace synopsia
