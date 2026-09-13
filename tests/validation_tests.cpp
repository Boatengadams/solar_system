#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "astronomy/EphemerisProvider.hpp"
#include "physics/PhysicsEngine.hpp"
#include "validation/PredictionComparison.hpp"
#include "validation/ValidationRunner.hpp"

namespace {
using namespace bag;

class DeterministicProvider final : public EphemerisProvider {
public:
    bool unavailable = false;
    bool wrongFrame = false;
    bool wrongOrigin = false;
    bool wrongEpoch = false;
    bool nonFinite = false;

    EphemerisResult getState(const EphemerisRequest& request) override {
        if (unavailable) return {EphemerisStatus::PROVIDER_UNAVAILABLE, {}, "offline"};
        EphemerisState state;
        state.bodyId = request.bodyId;
        state.epoch = wrongEpoch ? Epoch::julianDate(request.epoch.value + 1.0) : request.epoch;
        state.frame = wrongFrame ? Frame{request.frame.type, request.frame.originBodyId, "ICRF"} :
            (wrongOrigin ? Frame::geocentric() : request.frame);
        state.positionM = {PhysicsEngine::AU, 0.0, 0.0};
        state.velocityMps = {0.0, 29784.6918, 0.0};
        if (nonFinite) state.positionM.x = std::numeric_limits<double>::quiet_NaN();
        state.source = "deterministic test reference";
        state.provider = name();
        state.units = "SI";
        state.status = EphemerisStatus::SUCCESS;
        return {EphemerisStatus::SUCCESS, state, {}};
    }

    const char* name() const override { return "DeterministicProvider"; }
};

PredictionComparisonRequest validRequest(EphemerisProvider& provider) {
    PredictionComparisonRequest request;
    request.bodyId = "earth";
    request.initialEpoch = Epoch::julianDate(2451545.0);
    request.finalEpoch = Epoch::julianDate(2451545.0 + 2.0 / 86400.0);
    request.sampleEpochs = {Epoch::julianDate(2451545.0 + 1.0 / 86400.0)};
    request.referenceProvider = &provider;
    request.requestedTimestepSeconds = 3600.0;
    request.centralMassKg = PhysicsEngine::SOLAR_MASS;
    return request;
}

void testPredictionComparison() {
    DeterministicProvider provider;
    const PredictionComparisonRequest request = validRequest(provider);
    const PredictionComparisonResult first = comparePredictionToReference(request);
    const PredictionComparisonResult second = comparePredictionToReference(request);
    assert(first.success() && second.success());
    assert(first.samples.size() == 3 && first.integrationSteps > 0);
    assert(first.samples[0].epoch.value < first.samples[1].epoch.value);
    assert(first.samples[1].epoch.value < first.samples[2].epoch.value);
    assert(first.positionErrorMagnitudeM == second.positionErrorMagnitudeM);
    assert(std::isfinite(first.positionErrorMagnitudeM));
    assert(std::isfinite(first.velocityErrorMagnitudeMps));
    assert(std::isfinite(first.referenceEnergyJPerKg));
    assert(std::isfinite(first.predictedEnergyJPerKg));

    PredictionComparisonRequest invalid = request;
    invalid.finalEpoch = invalid.initialEpoch;
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::InvalidDuration);
    invalid = request;
    invalid.requestedTimestepSeconds = 0.0;
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::InvalidTimestep);
    invalid = request;
    invalid.centralMassKg = 0.0;
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::InvalidInput);
    invalid = request;
    invalid.initialStateSource = PredictionInitialStateSource::Independent;
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::InvalidInput);
    invalid = request;
    invalid.integrator = static_cast<Integrator>(99);
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::InvalidInput);
    invalid = request;
    invalid.referenceProvider = nullptr;
    assert(comparePredictionToReference(invalid).status == PredictionComparisonStatus::ProviderUnavailable);

    provider.wrongFrame = true;
    assert(comparePredictionToReference(request).status == PredictionComparisonStatus::FrameMismatch);
    provider.wrongFrame = false;
    provider.wrongOrigin = true;
    assert(comparePredictionToReference(request).status == PredictionComparisonStatus::OriginMismatch);
    provider.wrongOrigin = false;
    provider.wrongEpoch = true;
    assert(comparePredictionToReference(request).status == PredictionComparisonStatus::EpochMismatch);
    provider.wrongEpoch = false;
    provider.nonFinite = true;
    assert(comparePredictionToReference(request).status == PredictionComparisonStatus::NonFiniteState);
    provider.nonFinite = false;
    provider.unavailable = true;
    assert(comparePredictionToReference(request).status == PredictionComparisonStatus::ProviderUnavailable);
}

void testPredictionMetrics() {
    EphemerisState reference{"earth", Epoch::julianDate(1.0), Frame::heliocentric(), {3.0, 0.0, 0.0}, {0.0, 4.0, 0.0}, "fixture", "fixture", "SI", EphemerisStatus::SUCCESS, {}};
    EphemerisState same = reference;
    const PredictionComparisonSample zero = comparePredictionStates(reference, same, PhysicsEngine::SOLAR_MASS);
    assert(zero.status == PredictionComparisonStatus::Success);
    assert(zero.positionErrorMagnitudeM == 0.0 && zero.velocityErrorMagnitudeMps == 0.0);
    assert(zero.relativePositionErrorDefined && zero.relativeVelocityErrorDefined && zero.energyDefined);
    same.positionM.x += 4.0;
    same.velocityMps.y += 3.0;
    const PredictionComparisonSample known = comparePredictionStates(reference, same, PhysicsEngine::SOLAR_MASS);
    assert(known.status == PredictionComparisonStatus::Success);
    assert(known.positionErrorMagnitudeM == 4.0 && known.velocityErrorMagnitudeMps == 3.0);
    assert(known.relativePositionError == 4.0 / 3.0 && known.relativeVelocityError == 3.0 / 4.0);
    reference.positionM = {};
    reference.velocityMps = {};
    const PredictionComparisonSample undefined = comparePredictionStates(reference, reference, PhysicsEngine::SOLAR_MASS);
    assert(undefined.status == PredictionComparisonStatus::Success);
    assert(!undefined.relativePositionErrorDefined && !undefined.relativeVelocityErrorDefined && !undefined.energyDefined);
    assert(std::isfinite(undefined.relativePositionError) && std::isfinite(undefined.relativeEnergyDifference));
    same = reference;
    same.epoch = Epoch::julianDate(2.0);
    assert(comparePredictionStates(reference, same, PhysicsEngine::SOLAR_MASS).status == PredictionComparisonStatus::EpochMismatch);
    same = reference;
    same.frame = Frame::geocentric();
    assert(comparePredictionStates(reference, same, PhysicsEngine::SOLAR_MASS).status == PredictionComparisonStatus::OriginMismatch);
}
}

int main() {
    testPredictionComparison();
    testPredictionMetrics();
    const bag::ValidationReport report = bag::runValidationSuite();
    std::cout << report.toCsv();
    assert(report.allPassed());
    return 0;
}
