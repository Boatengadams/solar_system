#include "Simulation.hpp"

#include <algorithm>
#include <cmath>

#include "../core/Logger.hpp"
#include "../data/BodyFactory.hpp"
#include "../data/ScenarioLoader.hpp"
#include "../data/ScenarioSerializer.hpp"

namespace bag {
namespace {
constexpr double MAX_FRAME_DELTA = 1.0 / 30.0;
}

Simulation::Simulation(std::filesystem::path root) : dataRoot(std::move(root)) { reset(); }

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
    initialEnergy = 0.0;
    refreshScientificState();
    paused = false;
    challengeScore = 0.0;
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
            break;
        }
        actualTimestep = lastStepResult.timestepUsed;
        nextTimestep = settings.adaptiveTimestep ? lastStepResult.suggestedTimestep : settings.timestepSeconds;
        timestepHistory.push_back(actualTimestep);
        simTime += lastStepResult.timestepUsed;
        accumulator -= lastStepResult.timestepUsed;
        refreshScientificState();
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
    return true;
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
