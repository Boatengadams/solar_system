#include "PredictionComparison.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bag {
namespace {

bool finite(double value) { return std::isfinite(value); }

bool sameFrame(const Frame& left, const Frame& right) {
    return left.type == right.type && left.originBodyId == right.originBodyId && left.orientation == right.orientation;
}

PredictionComparisonStatus frameStatus(const Frame& left, const Frame& right) {
    if (left.originBodyId != right.originBodyId) return PredictionComparisonStatus::OriginMismatch;
    return PredictionComparisonStatus::FrameMismatch;
}

bool validEpochRange(const Epoch& initial, const Epoch& final) {
    return initial.valid() && final.valid() && initial.type == final.type && final.value > initial.value;
}

bool validIntegrator(Integrator integrator) {
    switch (integrator) {
    case Integrator::Euler:
    case Integrator::SemiImplicitEuler:
    case Integrator::VelocityVerlet:
    case Integrator::RK4:
        return true;
    }
    return false;
}

PredictionComparisonResult failure(const PredictionComparisonRequest& request,
                                   PredictionComparisonStatus status, const std::string& explanation) {
    PredictionComparisonResult result;
    result.status = status;
    result.bodyId = request.bodyId;
    result.referenceProvider = request.referenceProvider ? request.referenceProvider->name() : std::string{};
    result.referenceSource = request.referenceSource.empty() && request.referenceProvider
        ? request.referenceProvider->name() : request.referenceSource;
    result.frame = request.frame;
    result.initialStateSource = request.initialStateSource;
    result.initialEpoch = request.initialEpoch;
    result.finalEpoch = request.finalEpoch;
    result.integrator = request.integrator;
    result.requestedTimestepSeconds = request.requestedTimestepSeconds;
    result.explanation = explanation;
    return result;
}

PredictionComparisonStatus providerFailure(EphemerisStatus status) {
    switch (status) {
    case EphemerisStatus::PROVIDER_UNAVAILABLE:
    case EphemerisStatus::NETWORK_ERROR:
    case EphemerisStatus::TIMEOUT:
        return PredictionComparisonStatus::ProviderUnavailable;
    default:
        return PredictionComparisonStatus::ReferenceUnavailable;
    }
}

bool stateMatches(const EphemerisState& state, const std::string& bodyId, const Epoch& epoch, const Frame& frame) {
    return state.valid() && state.bodyId == bodyId && state.epoch.type == epoch.type &&
        state.epoch.value == epoch.value && sameFrame(state.frame, frame);
}

EphemerisState makePredictedState(const PredictionComparisonRequest& request, const Epoch& epoch,
                                  const std::vector<Body>& bodies) {
    EphemerisState state;
    state.bodyId = request.bodyId;
    state.epoch = epoch;
    state.frame = request.frame;
    state.positionM = bodies[1].position - bodies[0].position;
    state.velocityMps = bodies[1].velocity - bodies[0].velocity;
    state.source = "BAGSOLAR numerical propagation";
    state.provider = "BAGSOLAR";
    state.units = "SI";
    state.status = EphemerisStatus::SUCCESS;
    return state;
}

void populateMetrics(PredictionComparisonSample& sample, double centralMassKg) {
    sample.positionErrorM = sample.predictedState.positionM - sample.referenceState.positionM;
    sample.positionErrorMagnitudeM = length(sample.positionErrorM);
    sample.velocityErrorMps = sample.predictedState.velocityMps - sample.referenceState.velocityMps;
    sample.velocityErrorMagnitudeMps = length(sample.velocityErrorMps);
    const double referencePositionMagnitude = length(sample.referenceState.positionM);
    const double referenceVelocityMagnitude = length(sample.referenceState.velocityMps);
    sample.relativePositionError = 0.0;
    sample.relativeVelocityError = 0.0;
    sample.relativePositionErrorDefined = false;
    sample.relativeVelocityErrorDefined = false;
    sample.absoluteEnergyDifferenceJPerKg = 0.0;
    sample.relativeEnergyDifference = 0.0;
    sample.energyDefined = false;
    if (finite(referencePositionMagnitude) && referencePositionMagnitude > 0.0) {
        sample.relativePositionError = sample.positionErrorMagnitudeM / referencePositionMagnitude;
        sample.relativePositionErrorDefined = finite(sample.relativePositionError);
    }
    if (finite(referenceVelocityMagnitude) && referenceVelocityMagnitude > 0.0) {
        sample.relativeVelocityError = sample.velocityErrorMagnitudeMps / referenceVelocityMagnitude;
        sample.relativeVelocityErrorDefined = finite(sample.relativeVelocityError);
    }

    Body referenceBody;
    referenceBody.position = sample.referenceState.positionM;
    referenceBody.velocity = sample.referenceState.velocityMps;
    Body predictedBody;
    predictedBody.position = sample.predictedState.positionM;
    predictedBody.velocity = sample.predictedState.velocityMps;
    if (!finite(referencePositionMagnitude) || referencePositionMagnitude <= 0.0 ||
        !finite(length(sample.predictedState.positionM)) || length(sample.predictedState.positionM) <= 0.0) {
        sample.referenceEnergyJPerKg = 0.0;
        sample.predictedEnergyJPerKg = 0.0;
        return;
    }
    sample.referenceEnergyJPerKg = PhysicsEngine::specificEnergy(referenceBody, centralMassKg);
    sample.predictedEnergyJPerKg = PhysicsEngine::specificEnergy(predictedBody, centralMassKg);
    if (!finite(sample.referenceEnergyJPerKg) || !finite(sample.predictedEnergyJPerKg)) {
        sample.referenceEnergyJPerKg = 0.0;
        sample.predictedEnergyJPerKg = 0.0;
        return;
    }
    sample.absoluteEnergyDifferenceJPerKg = std::abs(sample.predictedEnergyJPerKg - sample.referenceEnergyJPerKg);
    const double energyScale = std::abs(sample.referenceEnergyJPerKg);
    sample.energyDefined = finite(sample.absoluteEnergyDifferenceJPerKg) && energyScale > 0.0 && finite(energyScale);
    if (sample.energyDefined) sample.relativeEnergyDifference = sample.absoluteEnergyDifferenceJPerKg / energyScale;
}

} // namespace

const char* predictionComparisonStatusName(PredictionComparisonStatus status) {
    switch (status) {
    case PredictionComparisonStatus::Success: return "SUCCESS";
    case PredictionComparisonStatus::InvalidInput: return "INVALID_INPUT";
    case PredictionComparisonStatus::InvalidEpoch: return "INVALID_EPOCH";
    case PredictionComparisonStatus::InvalidDuration: return "INVALID_DURATION";
    case PredictionComparisonStatus::InvalidTimestep: return "INVALID_TIMESTEP";
    case PredictionComparisonStatus::EpochMismatch: return "EPOCH_MISMATCH";
    case PredictionComparisonStatus::FrameMismatch: return "FRAME_MISMATCH";
    case PredictionComparisonStatus::OriginMismatch: return "ORIGIN_MISMATCH";
    case PredictionComparisonStatus::UnitMismatch: return "UNIT_MISMATCH";
    case PredictionComparisonStatus::ProviderUnavailable: return "PROVIDER_UNAVAILABLE";
    case PredictionComparisonStatus::ReferenceUnavailable: return "REFERENCE_UNAVAILABLE";
    case PredictionComparisonStatus::PropagationFailed: return "PROPAGATION_FAILED";
    case PredictionComparisonStatus::NonFiniteState: return "NON_FINITE_STATE";
    case PredictionComparisonStatus::InsufficientData: return "INSUFFICIENT_DATA";
    }
    return "UNKNOWN";
}

const char* predictionInitialStateSourceName(PredictionInitialStateSource source) {
    return source == PredictionInitialStateSource::Reference ? "reference" : "independent";
}

PredictionComparisonResult comparePredictionToReference(const PredictionComparisonRequest& request) {
    if (request.bodyId.empty() || !request.frame.valid()) return failure(request, PredictionComparisonStatus::InvalidInput, "Body and frame are required.");
    if (request.initialStateSource != PredictionInitialStateSource::Reference)
        return failure(request, PredictionComparisonStatus::InvalidInput, "Prediction vs Reference requires initialization from the provider state at the initial epoch.");
    if (!validIntegrator(request.integrator)) return failure(request, PredictionComparisonStatus::InvalidInput, "An explicitly supported integrator is required.");
    if (!request.initialEpoch.valid() || !request.finalEpoch.valid() || request.initialEpoch.type != EpochType::JulianDate || request.finalEpoch.type != EpochType::JulianDate)
        return failure(request, PredictionComparisonStatus::InvalidEpoch, "Initial and final epochs must be finite Julian dates.");
    if (!validEpochRange(request.initialEpoch, request.finalEpoch)) return failure(request, PredictionComparisonStatus::InvalidDuration, "Final epoch must be later than initial epoch.");
    const double derivedDuration = (request.finalEpoch.value - request.initialEpoch.value) * PhysicsEngine::DAY;
    if (!finite(derivedDuration) || derivedDuration <= 0.0) return failure(request, PredictionComparisonStatus::InvalidDuration, "Epoch-derived propagation duration must be finite and positive.");
    if (finite(request.durationSeconds) && (request.durationSeconds <= 0.0 || std::abs(request.durationSeconds - derivedDuration) > 1.0e-6))
        return failure(request, PredictionComparisonStatus::InvalidDuration, "Supplied duration does not match the Julian-date interval.");
    if (!finite(request.requestedTimestepSeconds) || request.requestedTimestepSeconds <= 0.0)
        return failure(request, PredictionComparisonStatus::InvalidTimestep, "Requested timestep must be finite and positive.");
    if (!finite(request.centralMassKg) || request.centralMassKg <= 0.0 || !finite(request.targetMassKg) || request.targetMassKg < 0.0)
        return failure(request, PredictionComparisonStatus::InvalidInput, "Propagation masses must be finite and physically non-negative.");
    if (!request.referenceProvider) return failure(request, PredictionComparisonStatus::ProviderUnavailable, "No reference ephemeris provider was supplied.");

    PredictionComparisonResult result = failure(request, PredictionComparisonStatus::ReferenceUnavailable, "Reference state was unavailable.");
    result.durationSeconds = derivedDuration;
    result.referenceProvider = request.referenceProvider->name();
    EphemerisState initialState;
    if (request.initialStateSource == PredictionInitialStateSource::Reference) {
        const EphemerisResult initial = request.referenceProvider->getState({request.bodyId, request.initialEpoch, request.frame});
        if (!initial) {
            if (initial.status == EphemerisStatus::SUCCESS) {
                if (initial.state.epoch.type != request.initialEpoch.type || initial.state.epoch.value != request.initialEpoch.value)
                    result.status = PredictionComparisonStatus::EpochMismatch;
                else if (initial.state.units != "SI") result.status = PredictionComparisonStatus::UnitMismatch;
                else if (!sameFrame(initial.state.frame, request.frame)) result.status = frameStatus(initial.state.frame, request.frame);
                else result.status = PredictionComparisonStatus::NonFiniteState;
            } else result.status = providerFailure(initial.status);
            result.explanation = initial.message.empty() ? "The reference provider did not return the initial state." : initial.message;
            return result;
        }
        initialState = initial.state;
    }
    if (!stateMatches(initialState, request.bodyId, request.initialEpoch, request.frame)) {
        if (!initialState.epoch.valid() || initialState.epoch.type != request.initialEpoch.type ||
            initialState.epoch.value != request.initialEpoch.value) result.status = PredictionComparisonStatus::EpochMismatch;
        else if (initialState.units != "SI") result.status = PredictionComparisonStatus::UnitMismatch;
        else result.status = !sameFrame(initialState.frame, request.frame)
            ? frameStatus(initialState.frame, request.frame) : PredictionComparisonStatus::NonFiniteState;
        result.explanation = "The initial state is incompatible, non-finite, or does not match the requested body/frame/origin.";
        return result;
    }
    result.initialEpoch = request.initialEpoch;

    std::vector<Epoch> epochs = request.sampleEpochs;
    epochs.push_back(request.finalEpoch);
    epochs.push_back(request.initialEpoch);
    for (const Epoch& epoch : epochs) {
        if (!epoch.valid() || epoch.type != EpochType::JulianDate || epoch.value < request.initialEpoch.value || epoch.value > request.finalEpoch.value)
            return failure(request, PredictionComparisonStatus::InvalidEpoch, "Sample epochs must be finite Julian dates inside the propagation interval.");
    }
    std::sort(epochs.begin(), epochs.end(), [](const Epoch& left, const Epoch& right) { return left.value < right.value; });
    epochs.erase(std::unique(epochs.begin(), epochs.end(), [](const Epoch& left, const Epoch& right) { return left.value == right.value; }), epochs.end());

    std::vector<EphemerisState> references;
    references.reserve(epochs.size());
    for (const Epoch& epoch : epochs) {
        if (epoch.value == request.initialEpoch.value && request.initialStateSource == PredictionInitialStateSource::Reference) {
            references.push_back(initialState);
            continue;
        }
        const EphemerisResult reference = request.referenceProvider->getState({request.bodyId, epoch, request.frame});
        if (!reference) {
            if (reference.status == EphemerisStatus::SUCCESS) {
                if (reference.state.epoch.type != epoch.type || reference.state.epoch.value != epoch.value)
                    result.status = PredictionComparisonStatus::EpochMismatch;
                else if (reference.state.units != "SI") result.status = PredictionComparisonStatus::UnitMismatch;
                else if (!sameFrame(reference.state.frame, request.frame)) result.status = frameStatus(reference.state.frame, request.frame);
                else result.status = PredictionComparisonStatus::NonFiniteState;
            } else result.status = providerFailure(reference.status);
            result.explanation = reference.message.empty() ? "The reference provider did not return a comparison state." : reference.message;
            result.samples.clear();
            return result;
        }
        if (!stateMatches(reference.state, request.bodyId, epoch, request.frame)) {
            if (reference.state.epoch.value != epoch.value || reference.state.epoch.type != epoch.type)
                result.status = PredictionComparisonStatus::EpochMismatch;
            else if (reference.state.units != "SI") result.status = PredictionComparisonStatus::UnitMismatch;
            else result.status = !sameFrame(reference.state.frame, request.frame)
                ? frameStatus(reference.state.frame, request.frame) : PredictionComparisonStatus::NonFiniteState;
            result.explanation = "A reference comparison state is non-finite or frame/origin incompatible.";
            result.samples.clear();
            return result;
        }
        references.push_back(reference.state);
    }

    Body central;
    central.id = request.frame.originBodyId;
    central.mass = request.centralMassKg;
    central.position = {};
    central.velocity = {};
    central.radius = 1.0;
    central.realRadius = 1.0;
    Body target;
    target.id = request.bodyId;
    target.mass = request.targetMassKg;
    target.position = initialState.positionM;
    target.velocity = initialState.velocityMps;
    target.radius = 1.0;
    target.realRadius = 1.0;
    std::vector<Body> bodies{central, target};
    PhysicsEngine physics;
    double currentEpoch = request.initialEpoch.value;
    double lastStep = std::numeric_limits<double>::quiet_NaN();
    result.samples.reserve(epochs.size());
    for (std::size_t index = 0; index < epochs.size(); ++index) {
        const double segmentSeconds = (epochs[index].value - currentEpoch) * PhysicsEngine::DAY;
        double remaining = segmentSeconds;
        while (remaining > 1.0e-9) {
            const double step = std::min(request.requestedTimestepSeconds, remaining);
            const PhysicsStepResult integrated = physics.integrate(bodies, step, request.integrator, request.closeApproach);
            if (!integrated.success) {
                result.status = PredictionComparisonStatus::PropagationFailed;
                result.explanation = integrated.error.empty() ? "PhysicsEngine propagation failed." : integrated.error;
                result.samples.clear();
                return result;
            }
            remaining -= integrated.timestepUsed;
            lastStep = integrated.timestepUsed;
            ++result.integrationSteps;
        }
        currentEpoch = epochs[index].value;
        const EphemerisState predicted = makePredictedState(request, epochs[index], bodies);
        if (!predicted.valid()) {
            result.status = PredictionComparisonStatus::NonFiniteState;
            result.explanation = "PhysicsEngine produced a non-finite predicted state.";
            result.samples.clear();
            return result;
        }
        PredictionComparisonSample sample;
        sample.epoch = epochs[index];
        sample.referenceState = references[index];
        sample.predictedState = predicted;
        populateMetrics(sample, request.centralMassKg);
        if (!finite(sample.positionErrorMagnitudeM) || !finite(sample.velocityErrorMagnitudeMps)) {
            result.status = PredictionComparisonStatus::NonFiniteState;
            result.explanation = "Comparison metrics were non-finite.";
            result.samples.clear();
            return result;
        }
        sample.status = PredictionComparisonStatus::Success;
        result.samples.push_back(sample);
    }
    result.status = PredictionComparisonStatus::Success;
    result.initialReferenceState = result.samples.front().referenceState;
    result.finalReferenceState = result.samples.back().referenceState;
    result.finalPredictedState = result.samples.back().predictedState;
    result.positionErrorM = result.samples.back().positionErrorM;
    result.positionErrorMagnitudeM = result.samples.back().positionErrorMagnitudeM;
    result.velocityErrorMps = result.samples.back().velocityErrorMps;
    result.velocityErrorMagnitudeMps = result.samples.back().velocityErrorMagnitudeMps;
    result.relativePositionError = result.samples.back().relativePositionError;
    result.relativeVelocityError = result.samples.back().relativeVelocityError;
    result.relativePositionErrorDefined = result.samples.back().relativePositionErrorDefined;
    result.relativeVelocityErrorDefined = result.samples.back().relativeVelocityErrorDefined;
    result.referenceEnergyJPerKg = result.samples.back().referenceEnergyJPerKg;
    result.predictedEnergyJPerKg = result.samples.back().predictedEnergyJPerKg;
    result.absoluteEnergyDifferenceJPerKg = result.samples.back().absoluteEnergyDifferenceJPerKg;
    result.relativeEnergyDifference = result.samples.back().relativeEnergyDifference;
    result.energyDefined = result.samples.back().energyDefined;
    result.actualTimestepSeconds = lastStep;
    result.explanation = request.initialStateSource == PredictionInitialStateSource::Reference
        ? "The prediction starts from the same reference state; differences reflect the selected numerical model and timestep relative to the selected reference ephemeris."
        : "The prediction starts from an independent state; differences include initial-condition disagreement and numerical propagation error."
        ;
    return result;
}

PredictionComparisonSample comparePredictionStates(const EphemerisState& referenceState,
                                                   const EphemerisState& predictedState,
                                                   double centralMassKg) {
    PredictionComparisonSample sample;
    sample.epoch = referenceState.epoch;
    sample.referenceState = referenceState;
    sample.predictedState = predictedState;
    if (!referenceState.valid() || !predictedState.valid()) {
        sample.status = (referenceState.units != "SI" || predictedState.units != "SI")
            ? PredictionComparisonStatus::UnitMismatch : PredictionComparisonStatus::NonFiniteState;
        return sample;
    }
    if (referenceState.bodyId != predictedState.bodyId) {
        sample.status = PredictionComparisonStatus::InvalidInput;
        return sample;
    }
    if (referenceState.epoch.type != predictedState.epoch.type || referenceState.epoch.value != predictedState.epoch.value) {
        sample.status = PredictionComparisonStatus::EpochMismatch;
        return sample;
    }
    if (!sameFrame(referenceState.frame, predictedState.frame)) {
        sample.status = frameStatus(referenceState.frame, predictedState.frame);
        return sample;
    }
    if (!finite(centralMassKg) || centralMassKg <= 0.0) return sample;
    populateMetrics(sample, centralMassKg);
    if (!finite(sample.positionErrorMagnitudeM) || !finite(sample.velocityErrorMagnitudeMps)) return sample;
    sample.status = PredictionComparisonStatus::Success;
    return sample;
}

} // namespace bag
