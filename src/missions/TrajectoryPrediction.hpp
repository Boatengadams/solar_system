#pragma once

#include <string>
#include <vector>

#include "Mission.hpp"

namespace bag {

struct TrajectoryPoint {
    double timeSeconds = 0.0;
    Vec3 positionM;
    Vec3 velocityMps;
};

struct TrajectoryPredictionResult {
    bool success = false;
    std::vector<TrajectoryPoint> points;
    double finalTimeSeconds = 0.0;
    std::string error;
};

TrajectoryPredictionResult predictTrajectory(const Body& centralBody,
                                             const Spacecraft& spacecraft,
                                             double durationSeconds,
                                             double timestepSeconds,
                                             const std::vector<ManeuverNode>& maneuvers = {});

} // namespace bag
