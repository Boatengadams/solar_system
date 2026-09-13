#pragma once

#include <filesystem>
#include <random>
#include <string>
#include <vector>

#include "../core/Star.hpp"
#include "../physics/Body.hpp"
#include "../physics/PhysicsEngine.hpp"
#include "SimulationSettings.hpp"

namespace bag {

struct CustomBodyData;

class Simulation {
public:
    std::vector<Body> bodies;
    std::vector<Star> stars;
    std::mt19937 rng{20260908};
    SimulationSettings settings;
    ScenarioMetadata scenarioMetadata;
    std::string scenarioId = "default_solar_system";
    double simTime = 0.0;
    double accumulator = 0.0;
    double actualTimestep = 0.0;
    std::vector<double> timestepHistory;
    double totalEnergy = 0.0;
    double energyDrift = 0.0;
    Vec3 totalMomentum;
    PhysicsStepResult lastStepResult;
    double speed = 1.0;
    bool paused = false;
    bool showOrbits = true;
    bool showTrails = true;
    bool showVectors = false;
    bool showGrid = false;
    bool education = true;
    int selected = -1;
    int lesson = 0;
    int experiment = 0;
    int challenge = 0;
    double challengeScore = 0.0;

    explicit Simulation(std::filesystem::path dataRoot = "data");

    void reset();
    bool loadScenario(const std::string& id);
    void integrate(double realDeltaSeconds);
    void launchProbe(double delta);
    int addBody(const Body& body);
    bool addCustomBody(const CustomBodyData& data);
    void setSpeed(double value);
    bool saveSnapshot(const std::filesystem::path& path) const;
    bool loadSnapshot(const std::filesystem::path& path);

    double distanceFromSun(const Body& body) const;
    double specificEnergy(const Body& body) const;
    double escapeVelocity(const Body& body) const;
    double orbitalVelocity(const Body& body) const;
    double surfaceGravity(const Body& body) const;
    OrbitalElements orbitalElements(const Body& body, double centralMass = PhysicsEngine::SOLAR_MASS) const;
    InteractionReport interactionReport() const;
    double currentEnergy() const { return totalEnergy; }
    double currentEnergyDrift() const { return energyDrift; }
    Vec3 currentMomentum() const { return totalMomentum; }

private:
    std::filesystem::path dataRoot;
    PhysicsEngine physics;
    double nextTimestep = 0.0;
    double initialEnergy = 0.0;
    void refreshScientificState();
};

} // namespace bag
