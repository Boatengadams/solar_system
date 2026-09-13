#pragma once

#include <cstddef>
#include <string>

#include "TelemetryTypes.hpp"

namespace bag {

struct TelemetryAnalysis {
    std::size_t sampleCount = 0;
    double simulationDurationSeconds = 0.0;
    double minimumTimestepSeconds = TELEMETRY_UNAVAILABLE;
    double maximumTimestepSeconds = TELEMETRY_UNAVAILABLE;
    double meanTimestepSeconds = TELEMETRY_UNAVAILABLE;
    double minimumDistanceM = TELEMETRY_UNAVAILABLE;
    double maximumDistanceM = TELEMETRY_UNAVAILABLE;
    double minimumSpeedMps = TELEMETRY_UNAVAILABLE;
    double maximumSpeedMps = TELEMETRY_UNAVAILABLE;
    double initialEnergyJ = TELEMETRY_UNAVAILABLE;
    double finalEnergyJ = TELEMETRY_UNAVAILABLE;
    double relativeEnergyDrift = TELEMETRY_UNAVAILABLE;
    double initialAngularMomentumM2PerS = TELEMETRY_UNAVAILABLE;
    double finalAngularMomentumM2PerS = TELEMETRY_UNAVAILABLE;
    double relativeAngularMomentumDrift = TELEMETRY_UNAVAILABLE;
    double initialEccentricity = TELEMETRY_UNAVAILABLE;
    double finalEccentricity = TELEMETRY_UNAVAILABLE;
    double initialSemiMajorAxisM = TELEMETRY_UNAVAILABLE;
    double finalSemiMajorAxisM = TELEMETRY_UNAVAILABLE;
    double minimumPeriapsisM = TELEMETRY_UNAVAILABLE;
    double maximumApoapsisM = TELEMETRY_UNAVAILABLE;
    std::size_t eventCount = 0;
    std::size_t numericalFailureCount = 0;
};

TelemetryAnalysis analyzeTelemetry(const TelemetrySession& session, const std::string& bodyId = {});

struct TelemetryComparison {
    enum class Status {
        Compatible,
        InsufficientData,
        IncompatibleBody,
        IncompatibleFrame,
        IncompatibleOrigin,
        IncompatibleUnits,
        IncompatibleEpoch,
        IncompatibleReference,
        IncompatibleScenario,
        IncompatibleSchema,
        IncompatibleTime,
        InvalidMetadata,
        InvalidData,
    };

    bool valid = false;
    Status status = Status::InsufficientData;
    std::string message;
    std::string bodyId;
    double durationDifferenceSeconds = TELEMETRY_UNAVAILABLE;
    double finalPositionDifferenceM = TELEMETRY_UNAVAILABLE;
    double finalVelocityDifferenceMps = TELEMETRY_UNAVAILABLE;
    double energyDriftDifference = TELEMETRY_UNAVAILABLE;
    double angularMomentumDriftDifference = TELEMETRY_UNAVAILABLE;
    double timestepDifferenceSeconds = TELEMETRY_UNAVAILABLE;
    std::size_t firstSampleCount = 0;
    std::size_t secondSampleCount = 0;
};

const char* telemetryComparisonStatusName(TelemetryComparison::Status status);

TelemetryComparison compareTelemetry(const TelemetrySession& first, const TelemetrySession& second,
                                     const std::string& bodyId);

} // namespace bag
