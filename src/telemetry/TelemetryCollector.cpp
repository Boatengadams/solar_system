#include "TelemetryCollector.hpp"

#include <algorithm>
#include <cmath>

namespace bag {
namespace {

bool finiteVec(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

const Body* findBody(const std::vector<Body>& bodies, const std::string& id, std::size_t& index) {
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].id == id) { index = i; return &bodies[i]; }
    }
    return nullptr;
}

} // namespace

bool TelemetryCollector::collect(const std::vector<Body>& bodies, double simulationTimeSeconds,
                                 double timestepSeconds, double requestedTimestepSeconds,
                                 Integrator integrator, TelemetryStatus status,
                                 const TelemetryConfig& config, const std::string& epoch,
                                 const std::string& referenceFrame,
                                 std::vector<TelemetrySample>& output) {
    if (!config.enabled || !std::isfinite(simulationTimeSeconds) || !std::isfinite(timestepSeconds) ||
        !std::isfinite(requestedTimestepSeconds)) return false;
    std::size_t referenceIndex = 0;
    const Body* reference = findBody(bodies, config.referenceBodyId, referenceIndex);
    const double totalEnergy = PhysicsEngine::totalEnergy(bodies);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        if (!body.active) continue;
        TelemetrySample sample;
        sample.simulationTimeSeconds = simulationTimeSeconds;
        sample.epoch = epoch;
        sample.referenceFrame = referenceFrame;
        sample.bodyId = body.id;
        sample.referenceBodyId = config.referenceBodyId;
        sample.positionM = body.position;
        sample.velocityMps = body.velocity;
        bool accelerationValid = false;
        sample.accelerationMps2 = PhysicsEngine::acceleration(bodies, i, config.closeApproach, &accelerationValid);
        sample.massKg = body.mass;
        sample.kineticEnergyJ = 0.5 * body.mass * dot(body.velocity, body.velocity);
        sample.totalMechanicalEnergyJ = totalEnergy;
        sample.timestepSeconds = timestepSeconds;
        sample.requestedTimestepSeconds = requestedTimestepSeconds;
        sample.integrator = integrator;
        sample.status = status;
        if (!accelerationValid || !std::isfinite(totalEnergy) || !std::isfinite(body.mass) || body.mass < 0.0 || !finiteVec(body.position) || !finiteVec(body.velocity)) {
            sample.status = TelemetryStatus::INVALID_STATE;
        }
        if (reference && reference != &body && reference->active) {
            const Vec3 relativePosition = body.position - reference->position;
            const Vec3 relativeVelocity = body.velocity - reference->velocity;
            const double distance = length(relativePosition);
            sample.distanceToReferenceM = distance;
            sample.relativeSpeedMps = length(relativeVelocity);
            if (std::isfinite(distance) && distance > 0.0 && std::isfinite(reference->mass) && reference->mass >= 0.0) {
                sample.potentialEnergyJ = -PhysicsEngine::G * body.mass * reference->mass / distance;
                sample.specificEnergyJPerKg = 0.5 * dot(relativeVelocity, relativeVelocity) -
                    PhysicsEngine::G * (body.mass + reference->mass) / distance;
                sample.angularMomentumM2PerS = cross(relativePosition, relativeVelocity);
                sample.angularMomentumMagnitudeM2PerS = length(sample.angularMomentumM2PerS);
                Body relativeState;
                relativeState.position = relativePosition;
                relativeState.velocity = relativeVelocity;
                const OrbitalElements elements = PhysicsEngine::orbitalElements(relativeState, body.mass + reference->mass);
                if (elements.valid) {
                    sample.eccentricity = elements.eccentricity;
                    sample.semiMajorAxisM = elements.semiMajorAxis;
                    sample.periapsisM = elements.periapsis;
                    sample.apoapsisM = elements.apoapsis;
                }
            }
        }
        output.push_back(std::move(sample));
    }
    return true;
}

const char* telemetryStatusName(TelemetryStatus status) {
    switch (status) {
    case TelemetryStatus::OK: return "OK";
    case TelemetryStatus::WARNING: return "WARNING";
    case TelemetryStatus::COLLISION: return "COLLISION";
    case TelemetryStatus::INVALID_STATE: return "INVALID_STATE";
    case TelemetryStatus::INTEGRATION_FAILURE: return "INTEGRATION_FAILURE";
    case TelemetryStatus::ROLLBACK: return "ROLLBACK";
    case TelemetryStatus::COMPLETED: return "COMPLETED";
    case TelemetryStatus::ABORTED: return "ABORTED";
    }
    return "INVALID_STATE";
}

const char* telemetryEventTypeName(TelemetryEventType type) {
    switch (type) {
    case TelemetryEventType::Collision: return "COLLISION";
    case TelemetryEventType::CloseApproach: return "CLOSE_APPROACH";
    case TelemetryEventType::InvalidState: return "INVALID_STATE";
    case TelemetryEventType::IntegrationFailure: return "INTEGRATION_FAILURE";
    case TelemetryEventType::AdaptiveTimestepReduction: return "ADAPTIVE_TIMESTEP_REDUCTION";
    case TelemetryEventType::Rollback: return "ROLLBACK";
    }
    return "INVALID_STATE";
}

const char* telemetrySeverityName(TelemetrySeverity severity) {
    switch (severity) {
    case TelemetrySeverity::Info: return "INFO";
    case TelemetrySeverity::Warning: return "WARNING";
    case TelemetrySeverity::Error: return "ERROR";
    }
    return "WARNING";
}

} // namespace bag
