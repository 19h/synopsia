/// @file color.cpp
/// @brief Color gradient implementation

#include <synopsia/color.hpp>

namespace synopsia {

ColorGradient::ColorGradient() {
    *this = create_default();
}

ColorGradient::ColorGradient(std::vector<Stop> stops)
    : stops_(std::move(stops))
{
    // Ensure stops are sorted by position
    std::sort(stops_.begin(), stops_.end(),
        [](const Stop& a, const Stop& b) {
            return a.position < b.position;
        });
}

ColorGradient ColorGradient::create_default() {
    using namespace colors;
    
    // Multi-stop gradient optimized for entropy visualization
    // The positions are chosen to emphasize different entropy ranges:
    // - 0.0-0.3: Low entropy (zeros, padding, simple repeating patterns)
    // - 0.3-0.6: Medium entropy (typical code, structured data)
    // - 0.6-0.9: High entropy (compressed, complex data)
    // - 0.9-1.0: Maximum entropy (encrypted, random)
    
    return ColorGradient({
        Stop(0.00, LowEntropy),       // Dark blue - zeros/padding
        Stop(0.25, MedLowEntropy),    // Cyan - simple data
        Stop(0.50, MedEntropy),       // Green - typical code
        Stop(0.70, MedHighEntropy),   // Yellow - mixed content
        Stop(0.85, HighEntropy),      // Orange - compressed
        Stop(1.00, MaxEntropy)        // Red - encrypted/random
    });
}

ColorGradient ColorGradient::create_simple(Color low, Color high) {
    return ColorGradient({
        Stop(0.0, low),
        Stop(1.0, high)
    });
}

ColorGradient ColorGradient::create_grayscale() {
    return ColorGradient({
        Stop(0.0, colors::Black),
        Stop(1.0, colors::White)
    });
}

ColorGradient ColorGradient::create_fire() {
    return ColorGradient({
        Stop(0.00, Color{0, 0, 0}),        // Black
        Stop(0.25, Color{128, 0, 0}),      // Dark red
        Stop(0.50, Color{255, 64, 0}),     // Orange-red
        Stop(0.75, Color{255, 192, 0}),    // Yellow-orange
        Stop(1.00, Color{255, 255, 224})   // Pale yellow (almost white)
    });
}

} // namespace synopsia
