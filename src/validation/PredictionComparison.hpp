#pragma once

#include <limits>
#include <string>
#include <vector>

#include "../astronomy/EphemerisProvider.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {

enum class PredictionComparisonStatus {
    Success,
    InvalidInput,
    InvalidEpoch,
    InvalidDuration,
    InvalidTimestep,
    EpochMismatch,
    FrameMismatch,
    OriginMismatch,
    UnitMismatch,
    ProviderUnavailable,
    ReferenceUnavailable,
    PropagationFailed,
    NonFiniteState,
    InsufficientData,
};

enum class PredictionInitialStateSource {
    Reference,
    Independent,
};

const char* predictionComparisonStatusName(PredictionComparisonStatus status);
const char* predictionInitialStateSourceName(PredictionInitialStateSource source);

struct PredictionComparisonRequest {
    std::string bodyId;
    Epoch initialEpoch;
    Epoch finalEpoch;
    double durationSeconds = std::numeric_limits<double>::quiet_NaN();
    Frame frame = Frame::heliocentric();
    EphemerisProvider* referenceProvider = nullptr; // non-owning; provider-specific behavior stays at the boundary
    std::string referenceSource;
    PredictionInitialStateSource initialStateSource = PredictionInitialStateSource::Reference;
    EphemerisState independentInitialState;
    Integrator integrator = Integrator::VelocityVerlet;
    double requestedTimestepSeconds = std::numeric_limits<double>::quiet_NaN();
    double centralMassKg = PhysicsEngine::SOLAR_MASS;
    double targetMassKg = 0.0;
    CloseApproachPolicy closeApproach;
    std::vector<Epoch> sampleEpochs;
};

struct PredictionComparisonSample {
    PredictionComparisonStatus status = PredictionComparisonStatus::InvalidInput;
    Epoch epoch;
    EphemerisState referenceState;
    EphemerisState predictedState;
    Vec3 positionErrorM;
    double positionErrorMagnitudeM = std::numeric_limits<double>::quiet_NaN();
    Vec3 velocityErrorMps;
    double velocityErrorMagnitudeMps = std::numeric_limits<double>::quiet_NaN();
    double relativePositionError = std::numeric_limits<double>::quiet_NaN();
    double relativeVelocityError = std::numeric_limits<double>::quiet_NaN();
    bool relativePositionErrorDefined = false;
    bool relativeVelocityErrorDefined = false;
    double referenceEnergyJPerKg = std::numeric_limits<double>::quiet_NaN();
    double predictedEnergyJPerKg = std::numeric_limits<double>::quiet_NaN();
    double absoluteEnergyDifferenceJPerKg = std::numeric_limits<double>::quiet_NaN();
    double relativeEnergyDifference = std::numeric_limits<double>::quiet_NaN();
    bool energyDefined = false;
};

struct PredictionComparisonResult {
    PredictionComparisonStatus status = PredictionComparisonStatus::InvalidInput;
    std::string bodyId;
    std::string referenceProvider;
    std::string referenceSource;
    Frame frame = Frame::heliocentric();
    PredictionInitialStateSource initialStateSource = PredictionInitialStateSource::Reference;
    Epoch initialEpoch;
    Epoch finalEpoch;
    double durationSeconds = std::numeric_limits<double>::quiet_NaN();
    Integrator integrator = Integrator::VelocityVerlet;
    double requestedTimestepSeconds = std::numeric_limits<double>::quiet_NaN();
    double actualTimestepSeconds = std::numeric_limits<double>::quiet_NaN();
    std::size_t integrationSteps = 0;
    EphemerisState initialReferenceState;
    EphemerisState finalReferenceState;
    EphemerisState finalPredictedState;
    Vec3 positionErrorM;
    double positionErrorMagnitudeM = std::numeric_limits<double>::quiet_NaN();
    Vec3 velocityErrorMps;
    double velocityErrorMagnitudeMps = std::numeric_limits<double>::quiet_NaN();
    double relativePositionError = std::numeric_limits<double>::quiet_NaN();
    double relativeVelocityError = std::numeric_limits<double>::quiet_NaN();
    bool relativePositionErrorDefined = false;
    bool relativeVelocityErrorDefined = false;
    double referenceEnergyJPerKg = std::numeric_limits<double>::quiet_NaN();
    double predictedEnergyJPerKg = std::numeric_limits<double>::quiet_NaN();
    double absoluteEnergyDifferenceJPerKg = std::numeric_limits<double>::quiet_NaN();
    double relativeEnergyDifference = std::numeric_limits<double>::quiet_NaN();
    bool energyDefined = false;
    std::vector<PredictionComparisonSample> samples;
    std::string explanation;

    bool success() const { return status == PredictionComparisonStatus::Success; }
};

PredictionComparisonResult comparePredictionToReference(const PredictionComparisonRequest& request);
PredictionComparisonSample comparePredictionStates(const EphemerisState& referenceState,
                                                   const EphemerisState& predictedState,
                                                   double centralMassKg);

} // namespace bag
