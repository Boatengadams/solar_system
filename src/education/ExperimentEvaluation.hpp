#pragma once

#include <limits>
#include <string>
#include <vector>

#include "EducationChallenges.hpp"
#include "../physics/IntegratorBenchmark.hpp"

namespace bag {

enum class ExperimentEvaluationStatus {
    Valid,
    InvalidInput,
    InsufficientData,
    ScientificFailure,
    Unsupported,
};

enum class ExperimentEvaluationMode {
    AnalyticalReference,
    BoundedBehavior,
    NumericalComparison,
};

struct ExperimentObservation {
    // SI values unless a field name states otherwise. NaN means not supplied.
    double radiusM = std::numeric_limits<double>::quiet_NaN();
    double targetRadiusM = std::numeric_limits<double>::quiet_NaN();
    double centralMassKg = PhysicsEngine::SOLAR_MASS;
    double centralBodyRadiusM = std::numeric_limits<double>::quiet_NaN();
    double measuredPrimaryValue = std::numeric_limits<double>::quiet_NaN();
    double measuredSecondaryValue = std::numeric_limits<double>::quiet_NaN();
    double periapsisRadiusM = std::numeric_limits<double>::quiet_NaN();
    double incomingSpeedMps = std::numeric_limits<double>::quiet_NaN();
    double energyDrift = std::numeric_limits<double>::quiet_NaN();
    double angularMomentumDrift = std::numeric_limits<double>::quiet_NaN();
    double positionErrorM = std::numeric_limits<double>::quiet_NaN();
    double velocityErrorMps = std::numeric_limits<double>::quiet_NaN();
    double orbitalPeriodError = std::numeric_limits<double>::quiet_NaN();
    double timestepSeconds = std::numeric_limits<double>::quiet_NaN();
    double durationSeconds = std::numeric_limits<double>::quiet_NaN();
    bool stable = false;
    Integrator integrator = Integrator::VelocityVerlet;
    std::vector<IntegratorBenchmarkMetrics> integratorMetrics;
};

struct ExperimentEvaluationMetrics {
    double measuredPrimaryValue = std::numeric_limits<double>::quiet_NaN();
    double measuredSecondaryValue = std::numeric_limits<double>::quiet_NaN();
    double referencePrimaryValue = std::numeric_limits<double>::quiet_NaN();
    double referenceSecondaryValue = std::numeric_limits<double>::quiet_NaN();
    double absolutePrimaryError = std::numeric_limits<double>::quiet_NaN();
    double absoluteSecondaryError = std::numeric_limits<double>::quiet_NaN();
    double primaryRelativeError = std::numeric_limits<double>::quiet_NaN();
    double secondaryRelativeError = std::numeric_limits<double>::quiet_NaN();
    double energyDrift = std::numeric_limits<double>::quiet_NaN();
    double angularMomentumDrift = std::numeric_limits<double>::quiet_NaN();
    double positionErrorM = std::numeric_limits<double>::quiet_NaN();
    double velocityErrorMps = std::numeric_limits<double>::quiet_NaN();
    double orbitalPeriodError = std::numeric_limits<double>::quiet_NaN();
    double normalizedError = std::numeric_limits<double>::quiet_NaN();
    double improvementRatio = std::numeric_limits<double>::quiet_NaN();
    bool numericallyStable = false;
};

struct ExperimentEvaluation {
    std::string experimentId;
    std::string experimentTitle;
    ExperimentEvaluationStatus status = ExperimentEvaluationStatus::Unsupported;
    ExperimentEvaluationMode mode = ExperimentEvaluationMode::AnalyticalReference;
    bool valid = false;
    bool passed = false;
    double score = 0.0;
    std::string grade = "INVALID";
    ExperimentEvaluationMetrics metrics;
    std::string feedback;
    std::string explanation;
    std::string nextStep;
};

ExperimentEvaluation evaluateExperiment(const std::string& experimentId,
                                         const ExperimentObservation& observation);
const char* experimentEvaluationStatusName(ExperimentEvaluationStatus status);
const char* experimentEvaluationModeName(ExperimentEvaluationMode mode);
bool experimentEvaluationIsPersistable(const ExperimentEvaluation& evaluation);

} // namespace bag
