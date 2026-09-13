#pragma once

#include <cstddef>
#include <vector>

#include "PhysicsEngine.hpp"

namespace bag {

struct IntegratorBenchmarkMetrics {
    Integrator integrator = Integrator::VelocityVerlet;
    bool valid = false;
    double timestep = 0.0;
    std::size_t integrationSteps = 0;
    double energyDrift = 0.0;
    double positionError = 0.0;
    double velocityError = 0.0;
};

struct IntegratorBenchmarkConfig {
    double timestep = 3600.0;
    double duration = 86400.0;
    std::size_t trackedBody = 1;
    double referenceTimestep = 0.0;
    CloseApproachPolicy closeApproach;
};

std::vector<IntegratorBenchmarkMetrics> compareIntegrators(
    const std::vector<Body>& initialBodies, const IntegratorBenchmarkConfig& config);

} // namespace bag
