#include "ExperimentEvaluation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "EducationContent.hpp"
#include "../missions/Mission.hpp"

namespace bag {
namespace {

bool finite(double value) { return std::isfinite(value); }
bool positive(double value) { return finite(value) && value > 0.0; }
bool nonNegativeFinite(double value) { return finite(value) && value >= 0.0; }

ChallengeScoringRules analyticalRules() {
    ChallengeScoringRules rules;
    rules.fullCreditRelativeError = ExperimentEvaluationThresholds::analyticalFullCreditRelativeError;
    rules.passingRelativeError = ExperimentEvaluationThresholds::analyticalPassingRelativeError;
    return rules;
}

bool validBenchmarkMetric(const IntegratorBenchmarkMetrics& metric) {
    return metric.valid && nonNegativeFinite(metric.energyDrift) &&
        nonNegativeFinite(metric.angularMomentumDrift) &&
        nonNegativeFinite(metric.positionError) && nonNegativeFinite(metric.velocityError) &&
        nonNegativeFinite(metric.orbitalPeriodError) && nonNegativeFinite(metric.timestep) &&
        nonNegativeFinite(metric.elapsedSimulationTime);
}

const Experiment* definitionFor(const std::string& id) {
    for (int index = 0; index < experimentCount(); ++index) {
        const Experiment& experiment = experimentAt(index);
        if (experiment.id == id) return &experiment;
    }
    return nullptr;
}

ExperimentEvaluation base(const std::string& id, ExperimentEvaluationMode mode) {
    ExperimentEvaluation result;
    result.experimentId = id;
    result.mode = mode;
    if (const Experiment* definition = definitionFor(id)) result.experimentTitle = definition->title;
    return result;
}

ExperimentEvaluation invalid(const std::string& id, ExperimentEvaluationStatus status,
                             const std::string& feedback) {
    ExperimentEvaluation result = base(id, ExperimentEvaluationMode::AnalyticalReference);
    result.status = status;
    result.feedback = feedback;
    return result;
}

void complete(ExperimentEvaluation& result, double score, bool passed,
              const std::string& feedback, const std::string& explanation,
              const std::string& nextStep) {
    result.valid = true;
    result.status = ExperimentEvaluationStatus::Valid;
    result.score = std::max(0.0, std::min(100.0, score));
    result.passed = passed;
    result.grade = challengeGrade(result.score, result.passed);
    result.feedback = feedback;
    result.explanation = explanation;
    result.nextStep = nextStep;
}

ExperimentEvaluation analytical(const std::string& id, const ExperimentObservation& observation,
                                double expected, const ChallengeScoringRules& rules,
                                const std::string& units, const std::string& explanation,
                                const std::string& nextStep) {
    ExperimentEvaluation result = base(id, ExperimentEvaluationMode::AnalyticalReference);
    if (!finite(observation.measuredPrimaryValue) || !finite(expected)) {
        return invalid(id, ExperimentEvaluationStatus::InvalidInput, "A finite measured result is required.");
    }
    const double absoluteError = std::abs(observation.measuredPrimaryValue - expected);
    const double relative = absoluteError / std::max(std::abs(expected), 1.0e-12);
    result.metrics.measuredPrimaryValue = observation.measuredPrimaryValue;
    result.metrics.referencePrimaryValue = expected;
    result.metrics.absolutePrimaryError = absoluteError;
    result.metrics.primaryRelativeError = relative;
    const double score = scoreRelativeError(relative, rules);
    complete(result, score, relative <= rules.passingRelativeError,
             relative <= rules.passingRelativeError
                 ? "The measured " + units + " agrees with the analytical reference."
                 : "The measured " + units + " is outside the documented reference tolerance.",
             explanation, nextStep);
    return result;
}

} // namespace

ExperimentEvaluation evaluateExperiment(const std::string& experimentId,
                                         const ExperimentObservation& observation) {
    if (!definitionFor(experimentId)) return invalid(experimentId, ExperimentEvaluationStatus::Unsupported, "This experiment has no evaluator yet.");

    if (experimentId == "escape-velocity") {
        if (!positive(observation.radiusM) || observation.radiusM <= PhysicsEngine::MIN_PHYSICS_DISTANCE ||
            !positive(observation.centralMassKg) ||
            observation.centralMassKg != PhysicsEngine::SOLAR_MASS) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput,
                           "Escape-velocity evaluation currently requires a finite Sun-centered radius and solar mass.");
        }
        Body body;
        body.position = {observation.radiusM, 0.0, 0.0};
        const double expected = PhysicsEngine::escapeVelocity(body);
        return analytical(experimentId, observation, expected, analyticalRules(), "escape velocity in m/s",
                          "Escape velocity is the zero-specific-energy boundary in the Sun-centered Newtonian model.",
                          "Repeat just below and just above the reference and inspect the orbit classification.");
    }

    if (experimentId == "kepler-test") {
        if (!positive(observation.radiusM) || observation.radiusM <= PhysicsEngine::MIN_PHYSICS_DISTANCE ||
            !positive(observation.measuredPrimaryValue)) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput,
                           "Kepler evaluation requires a positive radius and measured period.");
        }
        Body body;
        body.position = {observation.radiusM, 0.0, 0.0};
        body.velocity = {0.0, PhysicsEngine::orbitalVelocity(body), 0.0};
        const OrbitalElements elements = PhysicsEngine::orbitalElements(body);
        if (!elements.valid || !positive(elements.period)) return invalid(experimentId, ExperimentEvaluationStatus::ScientificFailure, "The orbital reference could not be computed.");
        return analytical(experimentId, observation, elements.period, analyticalRules(), "orbital period in seconds",
                          "Kepler's period reference is computed from the existing circular-orbit orbital-elements implementation.",
                          "Repeat at another radius and compare the period ratio with the radius ratio.");
    }

    if (experimentId == "gravity-lab") {
        if (!positive(observation.radiusM) || observation.radiusM <= PhysicsEngine::MIN_PHYSICS_DISTANCE ||
            !positive(observation.centralMassKg) || !finite(observation.measuredPrimaryValue)) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput,
                           "Gravity evaluation requires a positive safe radius, central mass, and finite acceleration.");
        }
        Body central;
        central.mass = observation.centralMassKg;
        Body probe;
        probe.mass = 1.0;
        probe.position = {observation.radiusM, 0.0, 0.0};
        std::vector<Body> bodies = {central, probe};
        bool valid = false;
        const Vec3 acceleration = PhysicsEngine::acceleration(bodies, 1, {}, &valid);
        if (!valid || !finite(length(acceleration))) return invalid(experimentId, ExperimentEvaluationStatus::ScientificFailure, "The gravity reference could not be computed.");
        return analytical(experimentId, observation, length(acceleration), analyticalRules(), "acceleration in m/s^2",
                          "The reference acceleration is the existing N-body PhysicsEngine result for a unit probe.",
                          "Change the radius while keeping mass fixed and observe the inverse-distance behavior.");
    }

    if (experimentId == "orbit-energy") {
        ExperimentEvaluation result = base(experimentId, ExperimentEvaluationMode::BoundedBehavior);
        if (!finite(observation.measuredPrimaryValue)) return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "A finite specific-energy measurement is required.");
        result.metrics.measuredPrimaryValue = observation.measuredPrimaryValue;
        result.metrics.referencePrimaryValue = 0.0;
        result.metrics.absolutePrimaryError = std::abs(observation.measuredPrimaryValue);
        result.metrics.numericallyStable = observation.measuredPrimaryValue < 0.0;
        complete(result, result.metrics.numericallyStable ? 100.0 : 0.0, result.metrics.numericallyStable,
                 result.metrics.numericallyStable ? "Negative specific energy indicates a bound orbit." : "Zero or positive specific energy is not a bound orbit.",
                 "This is a bounded physical classification, not a proximity score: the zero-energy boundary separates bound and unbound states.",
                 "Vary the speed around the zero-energy boundary and compare the classification.");
        return result;
    }

    if (experimentId == "hohmann-lab") {
        if (!positive(observation.radiusM) || !positive(observation.targetRadiusM) ||
            observation.radiusM == observation.targetRadiusM || !positive(observation.centralMassKg) ||
            !finite(observation.measuredPrimaryValue) ||
            (finite(observation.centralBodyRadiusM) &&
             (observation.radiusM <= observation.centralBodyRadiusM || observation.targetRadiusM <= observation.centralBodyRadiusM))) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "Hohmann evaluation requires distinct finite circular radii outside the central body and a finite total delta-v.");
        }
        const HohmannTransfer transfer = PhysicsEngine::hohmannTransfer(observation.radiusM, observation.targetRadiusM, observation.centralMassKg);
        if (!transfer.valid) return invalid(experimentId, ExperimentEvaluationStatus::ScientificFailure, "The Hohmann reference could not be computed.");
        ExperimentEvaluation result = analytical(experimentId, observation, transfer.totalDeltaV, analyticalRules(), "total delta-v in m/s",
            "The reference is the existing two-burn Hohmann calculation for coplanar circular point-mass orbits.",
            "Compare the departure and arrival burn components, not only their sum.");
        result.metrics.referenceSecondaryValue = transfer.arrivalDeltaV;
        result.metrics.measuredSecondaryValue = transfer.departureDeltaV;
        return result;
    }

    if (experimentId == "assist-lab") {
        if (!positive(observation.centralMassKg) || !positive(observation.periapsisRadiusM) ||
            !positive(observation.incomingSpeedMps) || !finite(observation.measuredPrimaryValue)) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "Gravity-assist evaluation requires positive finite mass, periapsis, incoming speed, and measured turn angle.");
        }
        const GravityAssistResult reference = gravityAssistTurn(PhysicsEngine::G * observation.centralMassKg,
                                                                 observation.periapsisRadiusM, observation.incomingSpeedMps);
        if (!reference.valid) return invalid(experimentId, ExperimentEvaluationStatus::ScientificFailure, "The gravity-assist reference could not be computed.");
        return analytical(experimentId, observation, reference.turnAngleRadians, analyticalRules(), "turn angle in radians",
                          "The reference uses the existing patched-conic gravity-assist turn-angle calculation.",
                          "Vary periapsis and incoming relative speed separately to see how the turn changes.");
    }

    if (experimentId == "numerical-methods") {
        const ChallengeDefinition* definition = findChallenge("integrator-comparison");
        if (!definition || observation.integratorMetrics.empty()) return invalid(experimentId, ExperimentEvaluationStatus::InsufficientData, "Integrator evaluation requires benchmark metrics for at least one method.");
        if (!std::all_of(observation.integratorMetrics.begin(), observation.integratorMetrics.end(), validBenchmarkMetric)) {
            return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "Integrator benchmark metrics must be finite, non-negative, and physically tagged as valid.");
        }
        const ChallengeResult benchmark = evaluateIntegratorMetrics(*definition, observation.integratorMetrics, observation.integrator);
        if (!benchmark.valid) return invalid(experimentId, ExperimentEvaluationStatus::ScientificFailure, "The supplied integrator benchmark contains no valid selected result.");
        ExperimentEvaluation result = base(experimentId, ExperimentEvaluationMode::NumericalComparison);
        result.valid = true;
        result.status = ExperimentEvaluationStatus::Valid;
        result.passed = benchmark.passed;
        result.score = benchmark.score;
        result.grade = benchmark.grade;
        result.metrics.energyDrift = benchmark.metrics.energyDrift;
        result.metrics.angularMomentumDrift = benchmark.metrics.angularMomentumDrift;
        result.metrics.positionErrorM = benchmark.metrics.positionError;
        result.metrics.velocityErrorMps = benchmark.metrics.velocityError;
        result.metrics.orbitalPeriodError = benchmark.metrics.orbitalPeriodError;
        result.metrics.normalizedError = benchmark.metrics.normalizedNumericalError;
        result.metrics.numericallyStable = benchmark.metrics.numericallyStable;
        result.feedback = benchmark.feedback;
        result.explanation = "The score reuses the integrator challenge's weighted energy, position, velocity, and stability metrics against its smaller-step RK4 numerical reference.";
        result.nextStep = "Repeat with a different timestep; no integrator is universally best for every problem.";
        return result;
    }

    if (experimentId == "timestep-sensitivity") {
        if (!finite(observation.measuredPrimaryValue) || !finite(observation.measuredSecondaryValue) ||
            observation.measuredPrimaryValue < 0.0 || observation.measuredSecondaryValue < 0.0 ||
            observation.measuredPrimaryValue <= 0.0) return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "Timestep evaluation requires finite, non-negative coarse and fine errors with a positive coarse error.");
        const double improvement = (observation.measuredPrimaryValue - observation.measuredSecondaryValue) / observation.measuredPrimaryValue;
        const double fullCreditImprovement = ExperimentEvaluationThresholds::timestepFullCreditImprovement;
        const double passingImprovement = ExperimentEvaluationThresholds::timestepPassingImprovement;
        const double score = improvement >= fullCreditImprovement ? 100.0
            : improvement <= passingImprovement ? 0.0
            : 100.0 * (improvement - passingImprovement) / (fullCreditImprovement - passingImprovement);
        ExperimentEvaluation result = base(experimentId, ExperimentEvaluationMode::NumericalComparison);
        result.metrics.measuredPrimaryValue = observation.measuredPrimaryValue;
        result.metrics.measuredSecondaryValue = observation.measuredSecondaryValue;
        result.metrics.improvementRatio = improvement;
        complete(result, score, improvement >= passingImprovement,
                 improvement >= passingImprovement ? "The refined timestep reduced the measured endpoint error." : "The refined timestep did not improve error by the documented minimum.",
                 "This convergence check compares two numerical runs; it is not an exact physical truth.",
                 "Refine the timestep again and record the step count alongside the error.");
        return result;
    }

    if (experimentId == "conservation") {
        if (!finite(observation.energyDrift) || !finite(observation.angularMomentumDrift)) return invalid(experimentId, ExperimentEvaluationStatus::InsufficientData, "Conservation evaluation requires both energy and angular-momentum drift metrics.");
        if (observation.energyDrift < 0.0 || observation.angularMomentumDrift < 0.0) return invalid(experimentId, ExperimentEvaluationStatus::InvalidInput, "Conservation drift cannot be negative.");
        ChallengeScoringRules energyRules;
        energyRules.fullCreditRelativeError = ExperimentEvaluationThresholds::conservationFullCreditDrift;
        energyRules.passingRelativeError = ExperimentEvaluationThresholds::conservationPassingDrift;
        ChallengeScoringRules momentumRules = energyRules;
        const double energyScore = scoreRelativeError(observation.energyDrift, energyRules);
        const double momentumScore = scoreRelativeError(observation.angularMomentumDrift, momentumRules);
        const double score = std::min(energyScore, momentumScore);
        ExperimentEvaluation result = base(experimentId, ExperimentEvaluationMode::BoundedBehavior);
        result.metrics.energyDrift = observation.energyDrift;
        result.metrics.angularMomentumDrift = observation.angularMomentumDrift;
        result.metrics.normalizedError = std::max(observation.energyDrift, observation.angularMomentumDrift);
        result.metrics.numericallyStable = observation.energyDrift <= energyRules.passingRelativeError && observation.angularMomentumDrift <= momentumRules.passingRelativeError;
        complete(result, score, result.metrics.numericallyStable,
                 result.metrics.numericallyStable ? "Energy and angular momentum remain within the drift envelope." : "At least one conserved quantity exceeds the documented drift envelope.",
                 "Conservation drift measures numerical behavior in the isolated model; it does not prove universal physical accuracy.",
                 "Reduce the timestep or compare another integrator, then inspect which quantity dominates the drift.");
        return result;
    }

    if (experimentId == "prediction-reference") {
        ExperimentEvaluation result = base(experimentId, ExperimentEvaluationMode::NumericalComparison);
        if (!observation.comparisonAvailable || !finite(observation.comparisonPositionErrorM) ||
            !finite(observation.comparisonVelocityErrorMps) || observation.comparisonPositionErrorM < 0.0 ||
            observation.comparisonVelocityErrorMps < 0.0) {
            return invalid(experimentId, ExperimentEvaluationStatus::InsufficientData,
                           "Run a valid reference comparison before evaluating this activity.");
        }
        result.metrics.positionErrorM = observation.comparisonPositionErrorM;
        result.metrics.velocityErrorMps = observation.comparisonVelocityErrorMps;
        result.metrics.primaryRelativeError = observation.comparisonRelativePositionError;
        result.metrics.secondaryRelativeError = observation.comparisonRelativeVelocityError;
        result.metrics.normalizedError = observation.comparisonRelativePositionError;
        result.metrics.energyDrift = observation.comparisonRelativeEnergyDifference;
        result.metrics.numericallyStable = finite(observation.comparisonRelativePositionError) &&
            finite(observation.comparisonRelativeVelocityError);
        complete(result, result.metrics.numericallyStable ? 100.0 : 0.0, result.metrics.numericallyStable,
                 "The numerical prediction was compared with the selected reference state; inspect the raw errors before changing the model.",
                 "This activity measures agreement with a selected reference ephemeris. It does not establish that either trajectory is physical reality.",
                 "Repeat with another integrator or timestep and compare position, velocity, and energy differences.");
        return result;
    }

    return invalid(experimentId, ExperimentEvaluationStatus::Unsupported, "This experiment has no evaluator yet.");
}

const char* experimentEvaluationStatusName(ExperimentEvaluationStatus status) {
    switch (status) {
    case ExperimentEvaluationStatus::Valid: return "VALID";
    case ExperimentEvaluationStatus::InvalidInput: return "INVALID_INPUT";
    case ExperimentEvaluationStatus::InsufficientData: return "INSUFFICIENT_DATA";
    case ExperimentEvaluationStatus::ScientificFailure: return "SCIENTIFIC_FAILURE";
    case ExperimentEvaluationStatus::Unsupported: return "UNSUPPORTED";
    }
    return "UNKNOWN";
}

const char* experimentEvaluationModeName(ExperimentEvaluationMode mode) {
    switch (mode) {
    case ExperimentEvaluationMode::AnalyticalReference: return "analytical_reference";
    case ExperimentEvaluationMode::BoundedBehavior: return "bounded_behavior";
    case ExperimentEvaluationMode::NumericalComparison: return "numerical_comparison";
    }
    return "unknown";
}

bool experimentEvaluationIsPersistable(const ExperimentEvaluation& evaluation) {
    return evaluation.valid && evaluation.status == ExperimentEvaluationStatus::Valid &&
        finite(evaluation.score) && evaluation.score >= 0.0 && evaluation.score <= 100.0;
}

} // namespace bag
