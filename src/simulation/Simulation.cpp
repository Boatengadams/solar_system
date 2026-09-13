#include "Simulation.hpp"

#include <algorithm>
#include <cmath>

#include "../core/Logger.hpp"
#include "../data/BodyFactory.hpp"
#include "../data/ScenarioLoader.hpp"
#include "../data/ScenarioSerializer.hpp"
#include "../education/EducationContent.hpp"
#include "../telemetry/TelemetryExporter.hpp"

namespace bag {
namespace {
constexpr double MAX_FRAME_DELTA = 1.0 / 30.0;
}

Simulation::Simulation(std::filesystem::path root)
    : educationProgress(lessonCount(), experimentCount(), challengeCount()), dataRoot(std::move(root)) {
    reset();
    setChallenge(0);
}

int Simulation::addBody(const Body& body) {
    bodies.push_back(body);
    return static_cast<int>(bodies.size()) - 1;
}

void Simulation::reset() {
    loadScenario(scenarioId);
}

bool Simulation::loadScenario(const std::string& id) {
    ScenarioLoader loader(dataRoot);
    const auto result = loader.loadScenario(id, rng);
    if (!result) {
        logError(result.error);
        return false;
    }
    scenarioId = id;
    scenarioMetadata = result.value->metadata;
    settings = result.value->settings;
    bodies = result.value->bodies;
    stars = result.value->stars;
    speed = settings.timeScale;
    showTrails = settings.trailsEnabled;
    showVectors = settings.vectorsEnabled;
    selected = -1;
    simTime = 0.0;
    accumulator = 0.0;
    actualTimestep = settings.timestepSeconds;
    nextTimestep = settings.timestepSeconds;
    timestepHistory.clear();
    lastStepResult = {};
    telemetry.clear();
    ephemerisEpoch.reset();
    ephemerisFrame = Frame::heliocentric();
    ephemerisSource.clear();
    ephemerisProvider.clear();
    ephemerisUnits = "SI";
    initialEnergy = 0.0;
    refreshScientificState();
    paused = false;
    challengeScore = 0.0;
    lastChallengeResult.reset();
    return true;
}

bool Simulation::initializeFromEphemeris(const EphemerisSnapshot& snapshot) {
    if (!snapshot.valid()) return false;
    for (const EphemerisState& state : snapshot.states) {
        const auto body = std::find_if(bodies.begin(), bodies.end(), [&](const Body& candidate) { return candidate.id == state.bodyId; });
        if (body == bodies.end() || !state.valid()) return false;
    }
    for (const EphemerisState& state : snapshot.states) {
        auto body = std::find_if(bodies.begin(), bodies.end(), [&](const Body& candidate) { return candidate.id == state.bodyId; });
        body->position = state.positionM;
        body->velocity = state.velocityMps;
        body->trail.clear();
    }
    ephemerisEpoch = snapshot.epoch;
    ephemerisFrame = snapshot.frame;
    ephemerisSource = snapshot.states.front().source;
    ephemerisProvider = snapshot.states.front().provider;
    ephemerisUnits = snapshot.states.front().units;
    simTime = 0.0;
    accumulator = 0.0;
    actualTimestep = settings.timestepSeconds;
    nextTimestep = settings.timestepSeconds;
    timestepHistory.clear();
    lastStepResult = {};
    initialEnergy = 0.0;
    refreshScientificState();
    telemetry.clear();
    return true;
}

void Simulation::integrate(double realDeltaSeconds) {
    const double scaled = std::min(realDeltaSeconds, MAX_FRAME_DELTA) * speed;
    accumulator += scaled;
    int steps = 0;
    while (accumulator >= (settings.adaptiveTimestep ? nextTimestep : settings.timestepSeconds) && steps++ < 100) {
        Integrator integrator = Integrator::VelocityVerlet;
        if (!parseIntegrator(settings.integrator, integrator)) {
            logError("unsupported integrator '" + settings.integrator + "'");
            break;
        }
        double step = settings.adaptiveTimestep ? nextTimestep : settings.timestepSeconds;
        if (settings.adaptiveTimestep) {
            AdaptiveTimestepSettings adaptive;
            adaptive.minimumTimestep = settings.minimumTimestepSeconds;
            adaptive.maximumTimestep = settings.maximumTimestepSeconds;
            adaptive.errorTolerance = settings.timestepErrorTolerance;
            adaptive.closeApproach.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
            lastStepResult = physics.integrateAdaptive(bodies, step, integrator, adaptive);
        } else {
            CloseApproachPolicy policy;
            policy.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
            lastStepResult = physics.integrate(bodies, step, integrator, policy);
        }
        if (!lastStepResult.success) {
            logError("physics step failed: " + lastStepResult.error);
            if (telemetry.active) {
                telemetry.recordPhysicsEvents(simTime, step, lastStepResult, bodies);
                TelemetryConfig config;
                config.enabled = true;
                config.samplingIntervalSeconds = telemetry.metadata.samplingIntervalSeconds;
                config.referenceBodyId = telemetry.metadata.referenceBodyId;
                config.closeApproach.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
                telemetry.observe(bodies, simTime, 0.0, step, integrator, TelemetryStatus::INTEGRATION_FAILURE, config, true);
            }
            break;
        }
        actualTimestep = lastStepResult.timestepUsed;
        nextTimestep = settings.adaptiveTimestep ? lastStepResult.suggestedTimestep : settings.timestepSeconds;
        timestepHistory.push_back(actualTimestep);
        simTime += lastStepResult.timestepUsed;
        accumulator -= lastStepResult.timestepUsed;
        refreshScientificState();
        if (telemetry.active) {
            TelemetryConfig config;
            config.enabled = true;
            config.samplingIntervalSeconds = telemetry.metadata.samplingIntervalSeconds;
            config.referenceBodyId = telemetry.metadata.referenceBodyId;
            config.closeApproach.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
            const TelemetryStatus status = !lastStepResult.interactions.collisions.empty() ? TelemetryStatus::COLLISION :
                (lastStepResult.interactions.closeApproach.triggered ? TelemetryStatus::WARNING : TelemetryStatus::OK);
            telemetry.recordPhysicsEvents(simTime, step, lastStepResult, bodies);
            telemetry.observe(bodies, simTime, lastStepResult.timestepUsed, step, integrator, status, config);
        }
    }

    if (!paused) {
        for (Body& body : bodies) {
            if (!body.active || static_cast<int>(simTime) % 21600 >= 3600) {
                continue;
            }
            if (body.trail.empty() || body.trail.back().x != static_cast<float>(body.position.x / PhysicsEngine::AU)) {
                body.trail.push_back({static_cast<float>(body.position.x / PhysicsEngine::AU),
                                      static_cast<float>(body.position.y / PhysicsEngine::AU)});
                if (static_cast<int>(body.trail.size()) > PhysicsEngine::MAX_TRAIL) {
                    body.trail.erase(body.trail.begin());
                }
            }
        }
    }
}

void Simulation::launchProbe(double delta) {
    auto probe = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) { return body.id == "bagsolar-1"; });
    auto earth = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) { return body.id == "earth"; });
    if (probe == bodies.end() || earth == bodies.end()) {
        return;
    }
    probe->position = earth->position + Vec3{0, 4.2e8, 0};
    probe->velocity = earth->velocity + Vec3{-delta, 0, 0};
    probe->trail.clear();
}

bool Simulation::addCustomBody(const CustomBodyData& data) {
    const auto result = BodyFactory::createCustom(data);
    if (!result) {
        logError(result.error);
        return false;
    }
    addBody(*result.value);
    return true;
}

bool Simulation::saveSnapshot(const std::filesystem::path& path) const {
    SimulationSnapshot snapshot;
    snapshot.metadata = scenarioMetadata;
    snapshot.settings = settings;
    snapshot.settings.timeScale = speed;
    snapshot.simulationTime = simTime;
    snapshot.paused = paused;
    snapshot.showOrbits = showOrbits;
    snapshot.showTrails = showTrails;
    snapshot.showVectors = showVectors;
    snapshot.showGrid = showGrid;
    snapshot.bodies = bodies;
    const SaveResult result = ScenarioSerializer::save(path, snapshot);
    if (!result.success) logError(result.error);
    return result.success;
}

bool Simulation::loadSnapshot(const std::filesystem::path& path) {
    const auto result = ScenarioSerializer::load(path);
    if (!result) {
        logError(result.error);
        return false;
    }
    const SimulationSnapshot& snapshot = *result.value;
    scenarioId = snapshot.metadata.id;
    scenarioMetadata = snapshot.metadata;
    settings = snapshot.settings;
    simTime = snapshot.simulationTime;
    paused = snapshot.paused;
    showOrbits = snapshot.showOrbits;
    showTrails = snapshot.showTrails;
    showVectors = snapshot.showVectors;
    showGrid = snapshot.showGrid;
    speed = settings.timeScale;
    bodies = snapshot.bodies;
    for (Body& body : bodies) {
        if (!body.parentId.empty()) {
            auto parent = std::find_if(bodies.begin(), bodies.end(), [&](const Body& candidate) { return candidate.id == body.parentId; });
            if (parent == bodies.end()) {
                logError("snapshot references unknown parent body '" + body.parentId + "'");
                return false;
            }
            body.parent = static_cast<int>(std::distance(bodies.begin(), parent));
        }
    }
    accumulator = 0.0;
    actualTimestep = settings.timestepSeconds;
    nextTimestep = settings.timestepSeconds;
    timestepHistory.clear();
    lastStepResult = {};
    initialEnergy = 0.0;
    refreshScientificState();
    selected = -1;
    telemetry.clear();
    return true;
}

bool Simulation::startTelemetry(double intervalSeconds, const std::string& referenceBodyId) {
    Integrator integrator = Integrator::VelocityVerlet;
    if (!parseIntegrator(settings.integrator, integrator)) return false;
    TelemetryConfig config;
    config.enabled = true;
    config.samplingIntervalSeconds = intervalSeconds;
    config.referenceBodyId = referenceBodyId;
    config.closeApproach.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
    TelemetrySessionMetadata metadata;
    metadata.sessionId = scenarioId + "-telemetry";
    metadata.scenarioId = scenarioMetadata.id;
    metadata.scenarioName = scenarioMetadata.name;
    metadata.epoch = scenarioMetadata.epoch;
    metadata.referenceFrame = ephemerisEpoch ? ephemerisFrame.orientation : scenarioMetadata.referenceFrame;
    metadata.referenceBodyId = referenceBodyId;
    metadata.ephemerisProvider = ephemerisProvider;
    metadata.ephemerisSource = ephemerisSource;
    metadata.ephemerisOriginBodyId = ephemerisFrame.originBodyId;
    metadata.ephemerisUnits = ephemerisUnits;
    metadata.ephemerisEpochJulianDate = ephemerisEpoch ? ephemerisEpoch->value : TELEMETRY_UNAVAILABLE;
    metadata.integrator = integratorName(integrator);
    metadata.initialTimestepSeconds = settings.timestepSeconds;
    return telemetry.start(metadata, intervalSeconds, bodies, simTime, config);
}

void Simulation::stopTelemetry(TelemetryStatus status) {
    if (!telemetry.active) return;
    TelemetryConfig config;
    config.enabled = true;
    config.samplingIntervalSeconds = telemetry.metadata.samplingIntervalSeconds;
    config.referenceBodyId = telemetry.metadata.referenceBodyId;
    config.closeApproach.minimumSafeSeparation = settings.minimumSafeSeparationMeters;
    Integrator integrator = Integrator::VelocityVerlet;
    parseIntegrator(settings.integrator, integrator);
    telemetry.observe(bodies, simTime, actualTimestep, actualTimestep, integrator, status, config, true);
    telemetry.stop(simTime, status);
}

void Simulation::clearTelemetry() { telemetry.clear(); }
bool Simulation::exportTelemetryCsv(const std::filesystem::path& path) const { return writeTelemetryCsv(path, telemetry); }
bool Simulation::exportTelemetryJson(const std::filesystem::path& path) const { return writeTelemetryJson(path, telemetry); }

void Simulation::setChallenge(int index) {
    if (challengeCount() == 0) return;
    challenge = ((index % challengeCount()) + challengeCount()) % challengeCount();
    const ChallengeDefinition& definition = challengeAt(challenge);
    challengeAnswer = definition.defaultAnswer;
    challengeIntegrator = definition.defaultIntegrator;
    lastChallengeResult.reset();
    challengeScore = 0.0;
}

void Simulation::adjustChallengeAnswer(double relativeChange) {
    if (!std::isfinite(relativeChange) || relativeChange <= -1.0) return;
    challengeAnswer = std::max(0.0, challengeAnswer * (1.0 + relativeChange));
}

void Simulation::cycleChallengeIntegrator(int direction) {
    constexpr Integrator integrators[] = {Integrator::Euler, Integrator::SemiImplicitEuler,
                                          Integrator::VelocityVerlet, Integrator::RK4};
    int current = 0;
    for (int index = 0; index < 4; ++index) if (integrators[index] == challengeIntegrator) current = index;
    current = (current + (direction >= 0 ? 1 : -1) + 4) % 4;
    challengeIntegrator = integrators[current];
    lastChallengeResult.reset();
}

bool Simulation::submitChallenge() {
    if (challengeCount() == 0) return false;
    const ChallengeDefinition& definition = challengeAt(challenge);
    ChallengeAnswer answer;
    answer.primaryValue = challengeAnswer;
    answer.timestepSeconds = challengeAnswer;
    answer.integrator = challengeIntegrator;
    lastChallengeResult = evaluateChallenge(definition, answer);
    if (!lastChallengeResult->valid) return false;
    challengeScore = lastChallengeResult->score;
    educationProgress.recordChallengeResult(challenge, *lastChallengeResult);
    return true;
}

bool Simulation::saveEducationProgress(const std::filesystem::path& path) const {
    return educationProgress.save(path);
}

bool Simulation::loadEducationProgress(const std::filesystem::path& path) {
    return educationProgress.load(path);
}

void Simulation::setSpeed(double value) {
    speed = std::max(0.01, std::min(100000.0, value));
    settings.timeScale = speed;
}

double Simulation::distanceFromSun(const Body& body) const { return PhysicsEngine::distanceFromSun(body); }
double Simulation::specificEnergy(const Body& body) const { return PhysicsEngine::specificEnergy(body); }
double Simulation::escapeVelocity(const Body& body) const { return PhysicsEngine::escapeVelocity(body); }
double Simulation::orbitalVelocity(const Body& body) const { return PhysicsEngine::orbitalVelocity(body); }
double Simulation::surfaceGravity(const Body& body) const { return PhysicsEngine::surfaceGravity(body); }
OrbitalElements Simulation::orbitalElements(const Body& body, double centralMass) const { return PhysicsEngine::orbitalElements(body, centralMass); }
InteractionReport Simulation::interactionReport() const { return PhysicsEngine::inspectInteractions(bodies, {settings.minimumSafeSeparationMeters}); }

void Simulation::refreshScientificState() {
    totalEnergy = PhysicsEngine::totalEnergy(bodies);
    totalMomentum = PhysicsEngine::totalMomentum(bodies);
    if (initialEnergy == 0.0) initialEnergy = totalEnergy;
    energyDrift = std::abs(totalEnergy - initialEnergy) / std::max(std::abs(initialEnergy), 1.0);
}

} // namespace bag
