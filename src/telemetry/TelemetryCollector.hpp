#pragma once

#include <vector>

#include "TelemetryTypes.hpp"

namespace bag {

class TelemetryCollector {
public:
    static bool collect(const std::vector<Body>& bodies, double simulationTimeSeconds,
                        double timestepSeconds, double requestedTimestepSeconds,
                        Integrator integrator, TelemetryStatus status,
                        const TelemetryConfig& config, const std::string& epoch,
                        const std::string& referenceFrame,
                        std::vector<TelemetrySample>& output);
};

} // namespace bag
