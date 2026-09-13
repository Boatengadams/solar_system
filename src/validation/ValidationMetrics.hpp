#pragma once

#include <cmath>
#include <limits>
#include <vector>

namespace bag {

inline bool finite(double value) { return std::isfinite(value); }

inline double absoluteError(double actual, double expected) {
    if (!finite(actual) || !finite(expected)) return std::numeric_limits<double>::infinity();
    return std::abs(actual - expected);
}

inline double relativeError(double actual, double expected, double scale = 0.0) {
    if (!finite(actual) || !finite(expected)) return std::numeric_limits<double>::infinity();
    const double denominator = scale > 0.0 ? scale : std::max(std::abs(expected), 1.0);
    return std::abs(actual - expected) / denominator;
}

inline double maximumError(const std::vector<double>& errors) {
    double maximum = 0.0;
    for (double error : errors) if (!finite(error)) return std::numeric_limits<double>::infinity(); else maximum = std::max(maximum, std::abs(error));
    return maximum;
}

inline double rmsError(const std::vector<double>& errors) {
    if (errors.empty()) return 0.0;
    double sum = 0.0;
    for (double error : errors) {
        if (!finite(error)) return std::numeric_limits<double>::infinity();
        sum += error * error;
    }
    return std::sqrt(sum / static_cast<double>(errors.size()));
}

} // namespace bag
