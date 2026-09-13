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

bool finiteVec(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool nearlyEqual(double left, double right) {
    return std::isfinite(left) && std::isfinite(right) && std::abs(left - right) <= 1e-12;
}

const char* statusMessage(TelemetryComparison::Status status) {
    switch (status) {
    case TelemetryComparison::Status::Compatible: return "telemetry datasets are compatible";
    case TelemetryComparison::Status::InsufficientData: return "telemetry datasets do not contain enough samples";
    case TelemetryComparison::Status::IncompatibleBody: return "telemetry body identities differ";
    case TelemetryComparison::Status::IncompatibleFrame: return "telemetry reference frames differ or are unavailable";
    case TelemetryComparison::Status::IncompatibleOrigin: return "telemetry reference origins differ or are unavailable";
    case TelemetryComparison::Status::IncompatibleUnits: return "telemetry units differ or are unavailable";
    case TelemetryComparison::Status::IncompatibleEpoch: return "telemetry epoch/reference context differs or is unavailable";
    case TelemetryComparison::Status::IncompatibleReference: return "telemetry ephemeris reference provenance differs or is unavailable";
    case TelemetryComparison::Status::IncompatibleScenario: return "telemetry scenario identities differ or are unavailable";
    case TelemetryComparison::Status::IncompatibleSchema: return "telemetry schema versions differ";
    case TelemetryComparison::Status::IncompatibleTime: return "telemetry samples do not end at the same simulation time";
    case TelemetryComparison::Status::InvalidMetadata: return "telemetry metadata is invalid or incomplete";
    case TelemetryComparison::Status::InvalidData: return "telemetry contains non-finite or unordered samples";
    }
    return "telemetry comparison failed";
}

TelemetryComparison failComparison(TelemetryComparison result, TelemetryComparison::Status status,
                                   const std::string& detail = {}) {
    result.status = status;
    result.message = detail.empty() ? statusMessage(status) : detail;
    return result;
}

bool hasBodySamples(const TelemetrySession& session, const std::string& bodyId) {
    return std::any_of(session.samples.begin(), session.samples.end(), [&](const TelemetrySample& sample) {
        return sample.bodyId == bodyId;
    });
}

bool validateBodySamples(const TelemetrySession& session, const std::string& bodyId,
                         std::string& error) {
    double previousTime = 0.0;
    bool hasPreviousTime = false;
    for (const TelemetrySample& sample : session.samples) {
        if (sample.bodyId != bodyId) continue;
        if (!std::isfinite(sample.simulationTimeSeconds) || !finiteVec(sample.positionM) ||
            !finiteVec(sample.velocityMps) || !std::isfinite(sample.timestepSeconds) ||
            !std::isfinite(sample.requestedTimestepSeconds)) {
            error = "selected telemetry samples contain non-finite required values";
            return false;
        }
        if (hasPreviousTime && sample.simulationTimeSeconds < previousTime) {
            error = "selected telemetry sample times are not monotonic";
            return false;
        }
        previousTime = sample.simulationTimeSeconds;
        hasPreviousTime = true;
        if (sample.referenceFrame.empty() || sample.referenceBodyId.empty()) {
            error = "selected telemetry samples are missing frame or origin metadata";
            return false;
        }
        if (sample.referenceFrame != session.metadata.referenceFrame ||
            sample.referenceBodyId != session.metadata.referenceBodyId ||
            sample.epoch != session.metadata.epoch) {
            error = "sample metadata does not match session metadata";
            return false;
        }
    }
    return true;
}

} // namespace

const char* telemetryComparisonStatusName(TelemetryComparison::Status status) {
    switch (status) {
    case TelemetryComparison::Status::Compatible: return "COMPATIBLE";
    case TelemetryComparison::Status::InsufficientData: return "INSUFFICIENT_DATA";
    case TelemetryComparison::Status::IncompatibleBody: return "INCOMPATIBLE_BODY";
    case TelemetryComparison::Status::IncompatibleFrame: return "INCOMPATIBLE_FRAME";
    case TelemetryComparison::Status::IncompatibleOrigin: return "INCOMPATIBLE_ORIGIN";
    case TelemetryComparison::Status::IncompatibleUnits: return "INCOMPATIBLE_UNITS";
    case TelemetryComparison::Status::IncompatibleEpoch: return "INCOMPATIBLE_EPOCH";
    case TelemetryComparison::Status::IncompatibleReference: return "INCOMPATIBLE_REFERENCE";
    case TelemetryComparison::Status::IncompatibleScenario: return "INCOMPATIBLE_SCENARIO";
    case TelemetryComparison::Status::IncompatibleSchema: return "INCOMPATIBLE_SCHEMA";
    case TelemetryComparison::Status::IncompatibleTime: return "INCOMPATIBLE_TIME";
    case TelemetryComparison::Status::InvalidMetadata: return "INVALID_METADATA";
    case TelemetryComparison::Status::InvalidData: return "INVALID_DATA";
    }
    return "UNKNOWN";
}

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
    if (bodyId.empty()) return failComparison(result, TelemetryComparison::Status::InvalidMetadata, "a body ID is required for telemetry comparison");
    if (first.samples.empty() || second.samples.empty()) return failComparison(result, TelemetryComparison::Status::InsufficientData);
    if (!hasBodySamples(first, bodyId) || !hasBodySamples(second, bodyId)) {
        const bool firstHasOtherBody = !first.samples.empty() && !hasBodySamples(first, bodyId);
        const bool secondHasOtherBody = !second.samples.empty() && !hasBodySamples(second, bodyId);
        if (firstHasOtherBody && secondHasOtherBody) return failComparison(result, TelemetryComparison::Status::IncompatibleBody);
        return failComparison(result, TelemetryComparison::Status::InsufficientData);
    }
    if (first.metadata.schemaVersion != TELEMETRY_SCHEMA_VERSION || second.metadata.schemaVersion != TELEMETRY_SCHEMA_VERSION ||
        first.metadata.schemaVersion != second.metadata.schemaVersion) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleSchema);
    }
    if (first.metadata.referenceFrame.empty() || second.metadata.referenceFrame.empty()) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleFrame);
    }
    if (first.metadata.referenceFrame != second.metadata.referenceFrame) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleFrame);
    }
    if (first.metadata.referenceBodyId.empty() || second.metadata.referenceBodyId.empty()) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleOrigin);
    }
    if (first.metadata.referenceBodyId != second.metadata.referenceBodyId) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleOrigin);
    }
    if (first.metadata.ephemerisOriginBodyId.empty() != second.metadata.ephemerisOriginBodyId.empty() ||
        first.metadata.ephemerisOriginBodyId != second.metadata.ephemerisOriginBodyId) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleOrigin);
    }
    if (first.metadata.ephemerisUnits.empty() || second.metadata.ephemerisUnits.empty() ||
        first.metadata.ephemerisUnits != second.metadata.ephemerisUnits) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleUnits);
    }
    if (first.metadata.epoch.empty() || second.metadata.epoch.empty() || first.metadata.epoch != second.metadata.epoch) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleEpoch);
    }
    const bool firstHasJulianEpoch = std::isfinite(first.metadata.ephemerisEpochJulianDate);
    const bool secondHasJulianEpoch = std::isfinite(second.metadata.ephemerisEpochJulianDate);
    if (firstHasJulianEpoch != secondHasJulianEpoch || (firstHasJulianEpoch && !nearlyEqual(first.metadata.ephemerisEpochJulianDate, second.metadata.ephemerisEpochJulianDate))) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleEpoch);
    }
    if (first.metadata.ephemerisProvider.empty() != second.metadata.ephemerisProvider.empty() ||
        first.metadata.ephemerisProvider != second.metadata.ephemerisProvider ||
        first.metadata.ephemerisSource.empty() != second.metadata.ephemerisSource.empty() ||
        first.metadata.ephemerisSource != second.metadata.ephemerisSource) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleReference);
    }
    if (first.metadata.scenarioId.empty() || second.metadata.scenarioId.empty() || first.metadata.scenarioId != second.metadata.scenarioId) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleScenario);
    }
    if (!std::isfinite(first.metadata.simulationStartTimeSeconds) || !std::isfinite(second.metadata.simulationStartTimeSeconds) ||
        !std::isfinite(first.metadata.simulationEndTimeSeconds) || !std::isfinite(second.metadata.simulationEndTimeSeconds) ||
        first.metadata.simulationEndTimeSeconds < first.metadata.simulationStartTimeSeconds ||
        second.metadata.simulationEndTimeSeconds < second.metadata.simulationStartTimeSeconds ||
        !nearlyEqual(first.metadata.simulationStartTimeSeconds, second.metadata.simulationStartTimeSeconds) ||
        !nearlyEqual(first.metadata.simulationEndTimeSeconds, second.metadata.simulationEndTimeSeconds)) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleTime);
    }
    std::string validationError;
    if (!validateBodySamples(first, bodyId, validationError) || !validateBodySamples(second, bodyId, validationError)) {
        return failComparison(result, TelemetryComparison::Status::InvalidData, validationError);
    }
    const TelemetrySample* firstSample = lastBodySample(first, bodyId);
    const TelemetrySample* secondSample = lastBodySample(second, bodyId);
    if (!firstSample || !secondSample) return failComparison(result, TelemetryComparison::Status::InsufficientData);
    if (!nearlyEqual(firstSample->simulationTimeSeconds, secondSample->simulationTimeSeconds)) {
        return failComparison(result, TelemetryComparison::Status::IncompatibleTime,
                              "telemetry final samples do not occur at the same simulation time");
    }
    if (firstSample->referenceFrame != secondSample->referenceFrame || firstSample->referenceBodyId != secondSample->referenceBodyId) {
        return failComparison(result, TelemetryComparison::Status::InvalidMetadata, "sample metadata does not match session metadata");
    }
    const TelemetryAnalysis firstAnalysis = analyzeTelemetry(first, bodyId);
    const TelemetryAnalysis secondAnalysis = analyzeTelemetry(second, bodyId);
    result.valid = true;
    result.status = TelemetryComparison::Status::Compatible;
    result.message = statusMessage(result.status);
    result.durationDifferenceSeconds = first.metadata.simulationEndTimeSeconds - second.metadata.simulationEndTimeSeconds;
    result.finalPositionDifferenceM = length(firstSample->positionM - secondSample->positionM);
    result.finalVelocityDifferenceMps = length(firstSample->velocityMps - secondSample->velocityMps);
    result.energyDriftDifference = firstAnalysis.relativeEnergyDrift - secondAnalysis.relativeEnergyDrift;
    result.angularMomentumDriftDifference = firstAnalysis.relativeAngularMomentumDrift - secondAnalysis.relativeAngularMomentumDrift;
    result.timestepDifferenceSeconds = firstSample->timestepSeconds - secondSample->timestepSeconds;
    return result;
}

} // namespace bag
