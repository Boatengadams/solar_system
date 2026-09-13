#include "TelemetryTypes.hpp"

#include <algorithm>
#include <cmath>

#include "TelemetryCollector.hpp"

namespace bag {

void TelemetrySession::clear() {
    metadata = {};
    samples.clear();
    events.clear();
    active = false;
    nextSampleTimeSeconds = 0.0;
}

bool TelemetrySession::start(const TelemetrySessionMetadata& sessionMetadata, double intervalSeconds,
                             const std::vector<Body>& bodies, double simulationTimeSeconds,
                             const TelemetryConfig& config) {
    if (intervalSeconds <= 0.0 || !std::isfinite(intervalSeconds) || !std::isfinite(simulationTimeSeconds)) return false;
    clear();
    metadata = sessionMetadata;
    metadata.samplingIntervalSeconds = intervalSeconds;
    metadata.simulationStartTimeSeconds = simulationTimeSeconds;
    metadata.simulationEndTimeSeconds = simulationTimeSeconds;
    active = true;
    nextSampleTimeSeconds = simulationTimeSeconds + intervalSeconds;
    TelemetryConfig initialConfig = config;
    initialConfig.enabled = true;
    Integrator integrator = Integrator::VelocityVerlet;
    parseIntegrator(metadata.integrator, integrator);
    return TelemetryCollector::collect(bodies, simulationTimeSeconds, metadata.initialTimestepSeconds,
                                       metadata.initialTimestepSeconds, integrator,
                                       TelemetryStatus::OK, initialConfig, metadata.epoch,
                                       metadata.referenceFrame, samples);
}

bool TelemetrySession::observe(const std::vector<Body>& bodies, double simulationTimeSeconds,
                               double timestepSeconds, double requestedTimestepSeconds,
                               Integrator integrator, TelemetryStatus status,
                               const TelemetryConfig& config, bool force) {
    if (!active || (!force && simulationTimeSeconds < nextSampleTimeSeconds)) return true;
    if (force && !samples.empty() && samples.back().simulationTimeSeconds == simulationTimeSeconds) return true;
    const bool collected = TelemetryCollector::collect(bodies, simulationTimeSeconds, timestepSeconds,
                                                       requestedTimestepSeconds, integrator, status,
                                                       config, metadata.epoch, metadata.referenceFrame, samples);
    if (!collected) return false;
    while (nextSampleTimeSeconds <= simulationTimeSeconds) nextSampleTimeSeconds += metadata.samplingIntervalSeconds;
    return true;
}

void TelemetrySession::recordPhysicsEvents(double simulationTimeSeconds, double requestedTimestepSeconds,
                                            const PhysicsStepResult& result, const std::vector<Body>& bodies) {
    if (!active) return;
    if (!result.success) events.push_back({TelemetryEventType::IntegrationFailure, TelemetrySeverity::Error, simulationTimeSeconds, {}, {}, result.error});
    if (result.interactions.closeApproach.triggered) {
        const std::size_t a = result.interactions.closeApproach.bodyA;
        const std::size_t b = result.interactions.closeApproach.bodyB;
        events.push_back({TelemetryEventType::CloseApproach, result.interactions.closeApproach.unstable ? TelemetrySeverity::Error : TelemetrySeverity::Warning,
                          simulationTimeSeconds, a < bodies.size() ? bodies[a].id : std::string{}, b < bodies.size() ? bodies[b].id : std::string{},
                          result.interactions.closeApproach.unstable ? "close approach is numerically unstable" : "close approach detected"});
    }
    for (const CollisionInfo& collision : result.interactions.collisions) {
        events.push_back({TelemetryEventType::Collision, TelemetrySeverity::Warning, simulationTimeSeconds,
                          collision.bodyA < bodies.size() ? bodies[collision.bodyA].id : std::string{},
                          collision.bodyB < bodies.size() ? bodies[collision.bodyB].id : std::string{}, "body radii overlap"});
    }
    if (result.success && result.suggestedTimestep > 0.0 && result.suggestedTimestep < requestedTimestepSeconds) {
        events.push_back({TelemetryEventType::AdaptiveTimestepReduction, TelemetrySeverity::Info, simulationTimeSeconds, {}, {}, "adaptive timestep reduced"});
    }
}

void TelemetrySession::stop(double simulationTimeSeconds, TelemetryStatus finalStatus) {
    if (!active) return;
    metadata.simulationEndTimeSeconds = simulationTimeSeconds;
    metadata.status = finalStatus;
    active = false;
}

} // namespace bag
