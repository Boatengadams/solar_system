#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "../core/Vector3.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {

inline constexpr int TELEMETRY_SCHEMA_VERSION = 1;
inline constexpr double TELEMETRY_UNAVAILABLE = std::numeric_limits<double>::quiet_NaN();

enum class TelemetryStatus {
    OK,
    WARNING,
    COLLISION,
    INVALID_STATE,
    INTEGRATION_FAILURE,
    ROLLBACK,
    COMPLETED,
    ABORTED,
};

enum class TelemetryEventType {
    Collision,
    CloseApproach,
    InvalidState,
    IntegrationFailure,
    AdaptiveTimestepReduction,
    Rollback,
};

enum class TelemetrySeverity {
    Info,
    Warning,
    Error,
};

const char* telemetryStatusName(TelemetryStatus status);
const char* telemetryEventTypeName(TelemetryEventType type);
const char* telemetrySeverityName(TelemetrySeverity severity);

struct TelemetryConfig {
    bool enabled = false;
    double samplingIntervalSeconds = 60.0;
    std::string referenceBodyId;
    CloseApproachPolicy closeApproach;
};

struct TelemetrySample {
    double simulationTimeSeconds = 0.0;
    std::string epoch;
    std::string referenceFrame;
    std::string bodyId;
    std::string referenceBodyId;
    Vec3 positionM;
    Vec3 velocityMps;
    Vec3 accelerationMps2;
    double massKg = TELEMETRY_UNAVAILABLE;
    double kineticEnergyJ = TELEMETRY_UNAVAILABLE;
    double potentialEnergyJ = TELEMETRY_UNAVAILABLE;
    double totalMechanicalEnergyJ = TELEMETRY_UNAVAILABLE;
    double specificEnergyJPerKg = TELEMETRY_UNAVAILABLE;
    Vec3 angularMomentumM2PerS;
    double angularMomentumMagnitudeM2PerS = TELEMETRY_UNAVAILABLE;
    double eccentricity = TELEMETRY_UNAVAILABLE;
    double semiMajorAxisM = TELEMETRY_UNAVAILABLE;
    double periapsisM = TELEMETRY_UNAVAILABLE;
    double apoapsisM = TELEMETRY_UNAVAILABLE;
    double distanceToReferenceM = TELEMETRY_UNAVAILABLE;
    double relativeSpeedMps = TELEMETRY_UNAVAILABLE;
    double timestepSeconds = 0.0;
    double requestedTimestepSeconds = 0.0;
    Integrator integrator = Integrator::VelocityVerlet;
    TelemetryStatus status = TelemetryStatus::OK;
};

struct TelemetryEvent {
    TelemetryEventType type = TelemetryEventType::InvalidState;
    TelemetrySeverity severity = TelemetrySeverity::Warning;
    double simulationTimeSeconds = 0.0;
    std::string bodyId;
    std::string otherBodyId;
    std::string message;
};

struct TelemetrySessionMetadata {
    std::string sessionId;
    std::string scenarioId;
    std::string scenarioName;
    std::string epoch;
    std::string referenceFrame;
    std::string referenceBodyId;
    std::string ephemerisProvider;
    std::string ephemerisSource;
    std::string ephemerisOriginBodyId;
    std::string ephemerisUnits = "SI";
    double ephemerisEpochJulianDate = TELEMETRY_UNAVAILABLE;
    std::string integrator;
    double initialTimestepSeconds = 0.0;
    double samplingIntervalSeconds = 0.0;
    double simulationStartTimeSeconds = 0.0;
    double simulationEndTimeSeconds = 0.0;
    TelemetryStatus status = TelemetryStatus::OK;
};

struct TelemetrySession {
    TelemetrySessionMetadata metadata;
    std::vector<TelemetrySample> samples;
    std::vector<TelemetryEvent> events;
    bool active = false;
    double nextSampleTimeSeconds = 0.0;

    void clear();
    bool start(const TelemetrySessionMetadata& sessionMetadata, double intervalSeconds,
               const std::vector<Body>& bodies, double simulationTimeSeconds,
               const TelemetryConfig& config);
    bool observe(const std::vector<Body>& bodies, double simulationTimeSeconds,
                 double timestepSeconds, double requestedTimestepSeconds,
                 Integrator integrator, TelemetryStatus status,
                 const TelemetryConfig& config, bool force = false);
    void recordPhysicsEvents(double simulationTimeSeconds, double requestedTimestepSeconds,
                             const PhysicsStepResult& result, const std::vector<Body>& bodies);
    void stop(double simulationTimeSeconds, TelemetryStatus finalStatus = TelemetryStatus::COMPLETED);
};

} // namespace bag
