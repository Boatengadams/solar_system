#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "../physics/PhysicsEngine.hpp"

namespace bag {

enum class ValidationStatus {
    Pass,
    ExpectedFailure,
    NumericalFailure,
    InvalidInput,
};

const char* validationStatusName(ValidationStatus status);

struct ValidationTolerances {
    // These tolerances are tied to the one-year, two-body experiments used by
    // Phase 4. They are deliberately loose for low-order methods and tighter
    // for higher-order methods, while remaining far below qualitative failure.
    double circularRadiusRelative = 0.20;
    double circularEnergyRelative = 0.25;
    double circularAngularMomentumRelative = 0.25;
    double ellipticalElementRelative = 0.05;
    double hyperbolicEnergyRelative = 1.0e-12;
    // A six-hour Euler step produces a few-percent phase-period error over
    // one orbit; this threshold remains a bounded-orbit validation envelope.
    double periodRelative = 0.05;
    double convergenceImprovement = 0.05;
};

struct ValidationMetrics {
    double absoluteError = 0.0;
    double relativeError = 0.0;
    double maximumError = 0.0;
    double rmsError = 0.0;
    double energyDrift = 0.0;
    double angularMomentumDrift = 0.0;
    double positionError = 0.0;
    double velocityError = 0.0;
    double orbitalPeriodError = 0.0;
    double analyticalPeriod = 0.0;
    double measuredPeriod = 0.0;
};

struct ValidationResult {
    std::string caseName;
    std::string initialStateId;
    Integrator integrator = Integrator::VelocityVerlet;
    ValidationStatus status = ValidationStatus::InvalidInput;
    bool passed = false;
    double timestep = 0.0;
    double duration = 0.0;
    std::size_t integrationSteps = 0;
    double tolerance = 0.0;
    ValidationMetrics metrics;
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationResult> results;

    bool allPassed() const;
    std::string toCsv() const;
};

} // namespace bag
