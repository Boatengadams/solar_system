#include "TelemetryAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bag {
namespace {

bool available(double value) { return std::isfinite(value); }

template <typename Function>
void forBodySamples(const TelemetrySession& session, const std::string& bodyId, Function function) {
    for (const TelemetrySample& sample : session.samples) {
        if (bodyId.empty() || sample.bodyId == bodyId) function(sample);
    }
}

const TelemetrySample* lastBodySample(const TelemetrySession& session, const std::string& bodyId) {
    for (auto iterator = session.samples.rbegin(); iterator != session.samples.rend(); ++iterator) {
        if (iterator->bodyId == bodyId) return &*iterator;
    }
    return nullptr;
}

} // namespace

TelemetryAnalysis analyzeTelemetry(const TelemetrySession& session, const std::string& bodyId) {
    TelemetryAnalysis result;
    result.sampleCount = 0;
    double timestepSum = 0.0;
    const TelemetrySample* first = nullptr;
    const TelemetrySample* last = nullptr;
    forBodySamples(session, bodyId, [&](const TelemetrySample& sample) {
        ++result.sampleCount;
        if (!first) first = &sample;
        last = &sample;
        if (available(sample.timestepSeconds)) {
            result.minimumTimestepSeconds = std::isfinite(result.minimumTimestepSeconds) ? std::min(result.minimumTimestepSeconds, sample.timestepSeconds) : sample.timestepSeconds;
            result.maximumTimestepSeconds = std::isfinite(result.maximumTimestepSeconds) ? std::max(result.maximumTimestepSeconds, sample.timestepSeconds) : sample.timestepSeconds;
            timestepSum += sample.timestepSeconds;
        }
        if (available(sample.distanceToReferenceM)) {
            result.minimumDistanceM = std::isfinite(result.minimumDistanceM) ? std::min(result.minimumDistanceM, sample.distanceToReferenceM) : sample.distanceToReferenceM;
            result.maximumDistanceM = std::isfinite(result.maximumDistanceM) ? std::max(result.maximumDistanceM, sample.distanceToReferenceM) : sample.distanceToReferenceM;
        }
        const double speed = length(sample.velocityMps);
        if (available(speed)) {
            result.minimumSpeedMps = std::isfinite(result.minimumSpeedMps) ? std::min(result.minimumSpeedMps, speed) : speed;
            result.maximumSpeedMps = std::isfinite(result.maximumSpeedMps) ? std::max(result.maximumSpeedMps, speed) : speed;
        }
        if (available(sample.periapsisM)) result.minimumPeriapsisM = std::isfinite(result.minimumPeriapsisM) ? std::min(result.minimumPeriapsisM, sample.periapsisM) : sample.periapsisM;
        if (available(sample.apoapsisM)) result.maximumApoapsisM = std::isfinite(result.maximumApoapsisM) ? std::max(result.maximumApoapsisM, sample.apoapsisM) : sample.apoapsisM;
    });
    result.simulationDurationSeconds = session.metadata.simulationEndTimeSeconds - session.metadata.simulationStartTimeSeconds;
    if (result.sampleCount > 0) {
        result.meanTimestepSeconds = timestepSum / static_cast<double>(result.sampleCount);
        result.initialEnergyJ = first->totalMechanicalEnergyJ;
        result.finalEnergyJ = last->totalMechanicalEnergyJ;
        result.initialAngularMomentumM2PerS = first->angularMomentumMagnitudeM2PerS;
        result.finalAngularMomentumM2PerS = last->angularMomentumMagnitudeM2PerS;
        result.initialEccentricity = first->eccentricity;
        result.finalEccentricity = last->eccentricity;
        result.initialSemiMajorAxisM = first->semiMajorAxisM;
        result.finalSemiMajorAxisM = last->semiMajorAxisM;
        if (available(result.initialEnergyJ) && available(result.finalEnergyJ)) result.relativeEnergyDrift = std::abs(result.finalEnergyJ - result.initialEnergyJ) / std::max(std::abs(result.initialEnergyJ), 1.0);
        if (available(result.initialAngularMomentumM2PerS) && available(result.finalAngularMomentumM2PerS)) result.relativeAngularMomentumDrift = std::abs(result.finalAngularMomentumM2PerS - result.initialAngularMomentumM2PerS) / std::max(std::abs(result.initialAngularMomentumM2PerS), 1.0);
    }
    result.eventCount = session.events.size();
    for (const TelemetrySample& sample : session.samples) {
        if (sample.status == TelemetryStatus::INVALID_STATE || sample.status == TelemetryStatus::INTEGRATION_FAILURE || sample.status == TelemetryStatus::ROLLBACK) ++result.numericalFailureCount;
    }
    return result;
}

TelemetryComparison compareTelemetry(const TelemetrySession& first, const TelemetrySession& second,
                                     const std::string& bodyId) {
    TelemetryComparison result;
    result.bodyId = bodyId;
    result.firstSampleCount = first.samples.size();
    result.secondSampleCount = second.samples.size();
    const TelemetrySample* firstSample = lastBodySample(first, bodyId);
    const TelemetrySample* secondSample = lastBodySample(second, bodyId);
    if (!firstSample || !secondSample) return result;
    const TelemetryAnalysis firstAnalysis = analyzeTelemetry(first, bodyId);
    const TelemetryAnalysis secondAnalysis = analyzeTelemetry(second, bodyId);
    result.valid = true;
    result.durationDifferenceSeconds = first.metadata.simulationEndTimeSeconds - second.metadata.simulationEndTimeSeconds;
    result.finalPositionDifferenceM = length(firstSample->positionM - secondSample->positionM);
    result.finalVelocityDifferenceMps = length(firstSample->velocityMps - secondSample->velocityMps);
    result.energyDriftDifference = firstAnalysis.relativeEnergyDrift - secondAnalysis.relativeEnergyDrift;
    result.angularMomentumDriftDifference = firstAnalysis.relativeAngularMomentumDrift - secondAnalysis.relativeAngularMomentumDrift;
    result.timestepDifferenceSeconds = firstSample->timestepSeconds - secondSample->timestepSeconds;
    return result;
}

} // namespace bag
