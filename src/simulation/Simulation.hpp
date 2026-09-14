#pragma once

#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "../core/Star.hpp"
#include "../astronomy/EphemerisTypes.hpp"
#include "../astronomy/EphemerisProvider.hpp"
#include "../astronomy/LocalEphemerisProvider.hpp"
#include "../physics/Body.hpp"
#include "../physics/PhysicsEngine.hpp"
#include "../validation/PredictionComparison.hpp"
#include "../telemetry/TelemetryTypes.hpp"
#include "../education/EducationChallenges.hpp"
#include "../education/ExperimentEvaluation.hpp"
#include "../education/EducationProgress.hpp"
#include "../education/EducationWorkflow.hpp"
#include "SimulationSettings.hpp"

namespace bag {

struct CustomBodyData;

enum class AppScreen {
    Simulation,
    Education,
    ScenarioBrowser,
    MissionDesigner,
    Telemetry,
    Settings,
    Help,
};

const char* appScreenName(AppScreen screen);

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
    TelemetrySession telemetry;
    double speed = 1.0;
    bool paused = false;
    bool showOrbits = false;
    bool showTrails = true;
    bool showVectors = false;
    bool showGrid = false;
    bool education = true;
    bool educationScreen = false;
    AppScreen screen = AppScreen::Simulation;
    int scenarioBrowserSelection = 0;
    int selected = -1;
    int lesson = 0;
    int experiment = 0;
    int challenge = 0;
    double challengeScore = 0.0;
    double challengeAnswer = 0.0;
    Integrator challengeIntegrator = Integrator::VelocityVerlet;
    EducationProgress educationProgress;
    EducationWorkflow educationWorkflow;
    std::optional<ChallengeResult> lastChallengeResult;
    std::optional<ExperimentEvaluation> lastExperimentEvaluation;
    std::optional<PredictionComparisonResult> lastPredictionComparison;
    std::optional<Epoch> ephemerisEpoch;
    Frame ephemerisFrame = Frame::heliocentric();
    std::string ephemerisSource;
    std::string ephemerisProvider;
    std::string ephemerisUnits = "SI";

    explicit Simulation(std::filesystem::path dataRoot = {});

    void reset();
    bool loadScenario(const std::string& id);
    bool initializeFromEphemeris(const EphemerisSnapshot& snapshot);
    void integrate(double realDeltaSeconds);
    void launchProbe(double delta);
    int addBody(const Body& body);
    bool addCustomBody(const CustomBodyData& data);
    void setSpeed(double value);
    void adjustTimestep(double factor);
    void cycleIntegrator(int direction = 1);
    void setScreen(AppScreen next);
    bool saveSnapshot(const std::filesystem::path& path) const;
    bool loadSnapshot(const std::filesystem::path& path);
    bool startTelemetry(double intervalSeconds, const std::string& referenceBodyId = {});
    void stopTelemetry(TelemetryStatus status = TelemetryStatus::COMPLETED);
    void clearTelemetry();
    bool telemetryEnabled() const { return telemetry.active; }
    bool exportTelemetryCsv(const std::filesystem::path& path) const;
    bool exportTelemetryJson(const std::filesystem::path& path) const;

    void setChallenge(int index);
    void adjustChallengeAnswer(double relativeChange);
    void cycleChallengeIntegrator(int direction = 1);
    bool submitChallenge();
    bool evaluateCurrentExperiment();
    bool runPredictionComparison(EphemerisProvider& provider, PredictionComparisonRequest request);
    void setEphemerisProvider(EphemerisProvider* provider);
    const EphemerisProvider* predictionReferenceProvider() const { return comparisonProvider; }
    bool selectEducationActivity(EducationActivityType type, int index);
    bool startEducationActivity();
    bool beginEducationObservation();
    bool readyEducationForEvaluation();
    bool retryEducationActivity();
    bool continueEducationActivity();
    bool saveEducationProgress(const std::filesystem::path& path) const;
    bool loadEducationProgress(const std::filesystem::path& path);

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
    LocalEphemerisProvider offlineReferenceProvider = LocalEphemerisProvider::deterministicFixture();
    EphemerisProvider* comparisonProvider = &offlineReferenceProvider;
    void refreshScientificState();
    bool runConfiguredPredictionComparison();
};

} // namespace bag
