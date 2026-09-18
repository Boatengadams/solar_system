#include "Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

#include "../core/Logger.hpp"
#include "../data/BodyFactory.hpp"
#include "../data/ResourceRoot.hpp"
#include "../data/ScenarioLoader.hpp"
#include "../data/ScenarioSerializer.hpp"
#include "../education/EducationContent.hpp"
#include "../missions/Mission.hpp"
#include "../telemetry/TelemetryExporter.hpp"

namespace bag {
namespace {
constexpr double MAX_FRAME_DELTA = 1.0 / 30.0;

constexpr GradeId kAllGrades[] = {
    GradeId::B1, GradeId::B2, GradeId::B3, GradeId::B4, GradeId::B5, GradeId::B6,
    GradeId::JHS1, GradeId::JHS2, GradeId::JHS3, GradeId::SHS1, GradeId::SHS2, GradeId::SHS3,
};

int gradeIndex(GradeId grade) {
    for (int index = 0; index < static_cast<int>(std::size(kAllGrades)); ++index) {
        if (kAllGrades[index] == grade) return index;
    }
    return 6;
}
} // namespace

const char* appScreenName(AppScreen screen) {
    switch (screen) {
    case AppScreen::Simulation: return "SIMULATION";
    case AppScreen::Education: return "EDUCATION";
    case AppScreen::LearningLab: return "LEARNING LAB";
    case AppScreen::ScenarioBrowser: return "SCENARIOS";
    case AppScreen::MissionDesigner: return "MISSION TOOLS";
    case AppScreen::Telemetry: return "TELEMETRY";
    case AppScreen::Settings: return "SETTINGS";
    case AppScreen::Help: return "HELP";
    }
    return "SCREEN";
}

Simulation::Simulation(std::filesystem::path root)
    : educationProgress(lessonCount(), experimentCount(), challengeCount()),
      educationWorkflow(educationProgress, lessonCount(), experimentCount(), challengeCount()),
      dataRoot(std::move(root)) {
    if (dataRoot.empty()) {
        const auto resources = ResourceRoot::resolve();
        if (resources) {
            dataRoot = resources.dataRoot;
        } else {
            logError(resources.error);
        }
    }

    const auto curriculumPath = dataRoot / "curriculum" / "ghana_nacca_2019";
    const auto loaded = CurriculumLoader::loadFromDirectory(curriculumPath);
    if (loaded) {
        curriculumCatalog = loaded.catalog;
        curriculumReady = true;
        std::string i18nError;
        if (!curriculumI18n.loadFile(curriculumPath / "i18n" / "en.json", i18nError)) {
            logError(i18nError);
        }
        applyLearnerPresentationMode();
    } else {
        curriculumReady = false;
        curriculumLoadError = loaded.error;
        logError("curriculum load failed: " + loaded.error);
    }

    reset();
    setChallenge(0);
}

int Simulation::addBody(const Body& body) {
    bodies.push_back(body);
    lastPredictionComparison.reset();
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
    // Keep the educational probe dormant until P pulses/launches it so it does not
    // look like a mystery body circling with the planets.
    for (Body& body : bodies) {
        if (body.type == "Spacecraft" || body.id == "bagsolar-1") {
            body.active = false;
            body.trail.clear();
        }
    }
    speed = settings.timeScale;
    showTrails = settings.trailsEnabled;
    showVectors = settings.vectorsEnabled;
    selected = -1;
    soloStudy = false;
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
    lastPredictionComparison.reset();
    initialEnergy = 0.0;
    refreshScientificState();
    paused = false;
    challengeScore = 0.0;
    lastChallengeResult.reset();
    lastExperimentEvaluation.reset();
    lastPredictionComparison.reset();
    educationWorkflow.resetToSelection();
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
    // High time-scales need more substeps so ×10000 / ×100000 still advance noticeably.
    const int maxSteps = std::clamp(100 + static_cast<int>(speed / 40.0), 100, 2500);
    int steps = 0;
    while (accumulator >= (settings.adaptiveTimestep ? nextTimestep : settings.timestepSeconds) && steps++ < maxSteps) {
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
        lastPredictionComparison.reset();
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
            const Vec3 trailPosition = body.position / PhysicsEngine::AU;
            if (body.trail.empty() || body.trail.back().x != static_cast<float>(trailPosition.x) ||
                body.trail.back().y != static_cast<float>(trailPosition.y) ||
                body.trail.back().z != static_cast<float>(trailPosition.z)) {
                body.trail.push_back(trailPosition);
                if (static_cast<int>(body.trail.size()) > PhysicsEngine::MAX_TRAIL) {
                    body.trail.erase(body.trail.begin());
                }
            }
        }
    }
}

bool Simulation::launchProbe(double deltaV) {
    auto earth = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) { return body.id == "earth"; });
    if (earth == bodies.end() || !earth->active) return false;

    auto probe = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) { return body.id == "bagsolar-1"; });
    if (probe == bodies.end()) {
        ScenarioLoader loader(dataRoot);
        const auto definition = loader.loadBodyDefinition("bagsolar-1");
        if (!definition) {
            logError(definition.error);
            return false;
        }
        const auto created = BodyFactory::create(*definition.value);
        if (!created) {
            logError(created.error);
            return false;
        }
        addBody(*created.value);
        probe = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) { return body.id == "bagsolar-1"; });
        if (probe == bodies.end()) return false;
    }

    // Inject outside Earth on a near-circular path, then add the pulse Δv so the
    // burn is obvious in the live system.
    constexpr double altitudeM = 4.2e8;
    const Vec3 radial{0.0, altitudeM, 0.0};
    const double circularSpeed = std::sqrt(PhysicsEngine::G * earth->mass / altitudeM);
    const double pulse = std::isfinite(deltaV) ? std::abs(deltaV) : 3500.0;
    probe->position = earth->position + radial;
    probe->velocity = earth->velocity + Vec3{-(circularSpeed + pulse), 0.0, 0.0};
    probe->active = true;
    probe->trail.clear();
    soloStudy = false;
    selected = static_cast<int>(std::distance(bodies.begin(), probe));
    lastPredictionComparison.reset();
    return true;
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
    lastPredictionComparison.reset();
    refreshScientificState();
    selected = -1;
    soloStudy = false;
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
    const int next = ((index % challengeCount()) + challengeCount()) % challengeCount();
    if (!educationWorkflow.select(EducationActivityType::Challenge, next)) return;
    challenge = next;
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

bool Simulation::evaluateCurrentExperiment() {
    if (experiment < 0 || experiment >= experimentCount()) return false;
    if (educationWorkflow.activity().type == EducationActivityType::Lesson &&
        educationWorkflow.activity().index == lesson) return educationWorkflow.completeLesson();
    if (educationWorkflow.state() != EducationWorkflowState::ReadyForEvaluation ||
        educationWorkflow.activity().type != EducationActivityType::Experiment ||
        educationWorkflow.activity().index != experiment) return false;
    const std::string id = experimentAt(experiment).id;
    ExperimentObservation observation;
    int bodyIndex = selected;
    const Body* observedBody = nullptr;
    if (id != "prediction-reference") {
        if (bodyIndex < 0 || bodyIndex >= static_cast<int>(bodies.size()) || !bodies[static_cast<std::size_t>(bodyIndex)].active) {
            const auto found = std::find_if(bodies.begin(), bodies.end(), [](const Body& body) {
                return body.active && body.mass > 0.0 && body.id != "sun";
            });
            if (found == bodies.end()) return false;
            bodyIndex = static_cast<int>(std::distance(bodies.begin(), found));
        }
        observedBody = &bodies[static_cast<std::size_t>(bodyIndex)];
        observation.radiusM = distanceFromSun(*observedBody);
    }
    if (id == "prediction-reference") {
        if (lastPredictionComparison && lastPredictionComparison->success()) {
            observation.comparisonAvailable = true;
            observation.comparisonPositionErrorM = lastPredictionComparison->positionErrorMagnitudeM;
            observation.comparisonVelocityErrorMps = lastPredictionComparison->velocityErrorMagnitudeMps;
            observation.comparisonRelativePositionError = lastPredictionComparison->relativePositionError;
            observation.comparisonRelativeVelocityError = lastPredictionComparison->relativeVelocityError;
            observation.comparisonRelativeEnergyDifference = lastPredictionComparison->relativeEnergyDifference;
        }
    } else if (id == "escape-velocity") observation.measuredPrimaryValue = length(observedBody->velocity);
    else if (id == "kepler-test") {
        observation.measuredPrimaryValue = orbitalElements(*observedBody).period;
    } else if (id == "gravity-lab") {
        bool valid = false;
        observation.measuredPrimaryValue = length(PhysicsEngine::acceleration(bodies, bodyIndex, {}, &valid));
        if (!valid) observation.measuredPrimaryValue = std::numeric_limits<double>::quiet_NaN();
    } else if (id == "orbit-energy") observation.measuredPrimaryValue = specificEnergy(*observedBody);
    else if (id == "hohmann-lab") {
        const HohmannTransfer reference = PhysicsEngine::hohmannTransfer(
            PhysicsEngine::AU, 1.524 * PhysicsEngine::AU, PhysicsEngine::SOLAR_MASS);
        observation.radiusM = PhysicsEngine::AU;
        observation.targetRadiusM = 1.524 * PhysicsEngine::AU;
        observation.measuredPrimaryValue = reference.valid ? reference.totalDeltaV : std::numeric_limits<double>::quiet_NaN();
    } else if (id == "numerical-methods") {
        if (bodyIndex == 0) return false;
        std::vector<Body> fixture{bodies.front(), *observedBody};
        IntegratorBenchmarkConfig config;
        config.timestep = 6.0 * 3600.0;
        config.duration = 10.0 * PhysicsEngine::DAY;
        config.referenceTimestep = 3.0 * 3600.0;
        observation.integratorMetrics = compareIntegrators(fixture, config);
    } else {
        lastExperimentEvaluation = evaluateExperiment(id, observation);
        educationWorkflow.submitExperiment(*lastExperimentEvaluation);
        return true;
    }
    lastExperimentEvaluation = evaluateExperiment(id, observation);
    educationWorkflow.submitExperiment(*lastExperimentEvaluation);
    return true;
}

bool Simulation::runPredictionComparison(EphemerisProvider& provider, PredictionComparisonRequest request) {
    request.referenceProvider = &provider;
    lastPredictionComparison = comparePredictionToReference(request);
    return lastPredictionComparison->success();
}

void Simulation::setEphemerisProvider(EphemerisProvider* provider) {
    comparisonProvider = provider;
    lastPredictionComparison.reset();
}

bool Simulation::runConfiguredPredictionComparison() {
    PredictionComparisonRequest request;
    request.bodyId = "earth";
    request.initialEpoch = Epoch::julianDate(2451545.0);
    request.finalEpoch = Epoch::julianDate(2451545.0 + 1.0);
    request.frame = Frame::heliocentric();
    request.durationSeconds = PhysicsEngine::DAY;
    request.requestedTimestepSeconds = settings.timestepSeconds;
    request.centralMassKg = PhysicsEngine::SOLAR_MASS;
    request.referenceSource = "configured application ephemeris provider";
    Integrator integrator = Integrator::VelocityVerlet;
    if (parseIntegrator(settings.integrator, integrator)) request.integrator = integrator;
    else request.integrator = static_cast<Integrator>(-1);
    if (!comparisonProvider) {
        lastPredictionComparison = comparePredictionToReference(request);
        return false;
    }
    return runPredictionComparison(*comparisonProvider, request);
}

bool Simulation::submitChallenge() {
    if (challengeCount() == 0 || educationWorkflow.state() != EducationWorkflowState::ReadyForEvaluation ||
        educationWorkflow.activity().type != EducationActivityType::Challenge ||
        educationWorkflow.activity().index != challenge) return false;
    const ChallengeDefinition& definition = challengeAt(challenge);
    ChallengeAnswer answer;
    answer.primaryValue = challengeAnswer;
    answer.timestepSeconds = challengeAnswer;
    answer.integrator = challengeIntegrator;
    lastChallengeResult = evaluateChallenge(definition, answer);
    const bool accepted = educationWorkflow.submitChallenge(*lastChallengeResult);
    if (accepted && lastChallengeResult->valid) challengeScore = lastChallengeResult->score;
    return accepted;
}

bool Simulation::selectEducationActivity(EducationActivityType type, int index) {
    if (!educationWorkflow.select(type, index)) return false;
    if (type == EducationActivityType::Experiment) {
        experiment = educationWorkflow.activity().index;
        lastExperimentEvaluation.reset();
        lastPredictionComparison.reset();
    } else if (type == EducationActivityType::Challenge) {
        challenge = educationWorkflow.activity().index;
        const ChallengeDefinition& definition = challengeAt(challenge);
        challengeAnswer = definition.defaultAnswer;
        challengeIntegrator = definition.defaultIntegrator;
        challengeScore = 0.0;
        lastChallengeResult.reset();
    } else {
        lesson = educationWorkflow.activity().index;
    }
    return true;
}

bool Simulation::startEducationActivity() {
    lastPredictionComparison.reset();
    lastExperimentEvaluation.reset();
    return educationWorkflow.start();
}

bool Simulation::beginEducationObservation() {
    if (!educationWorkflow.beginObservation()) return false;
    if (educationWorkflow.activity().type == EducationActivityType::Experiment &&
        educationWorkflow.activity().id == "prediction-reference") runConfiguredPredictionComparison();
    return true;
}
bool Simulation::readyEducationForEvaluation() { return educationWorkflow.readyForEvaluation(); }
bool Simulation::retryEducationActivity() {
    const bool result = educationWorkflow.retry();
    if (result) {
        lastExperimentEvaluation.reset();
        lastChallengeResult.reset();
        lastPredictionComparison.reset();
    }
    return result;
}
bool Simulation::continueEducationActivity() {
    const bool result = educationWorkflow.continueToRecommended();
    if (result) {
        const EducationActivity& next = educationWorkflow.activity();
        if (next.type == EducationActivityType::Experiment) experiment = next.index;
        else if (next.type == EducationActivityType::Challenge) challenge = next.index;
        else lesson = next.index;
        lastExperimentEvaluation.reset();
        lastChallengeResult.reset();
        lastPredictionComparison.reset();
    }
    return result;
}

bool Simulation::saveEducationProgress(const std::filesystem::path& path) const {
    return educationProgress.save(path);
}

bool Simulation::loadEducationProgress(const std::filesystem::path& path) {
    return educationProgress.load(path);
}

bool Simulation::setLearnerGrade(GradeId grade) {
    learnerGrade = grade;
    curriculumActivityIndex = 0;
    learningLabSession.reset();
    curriculumAnswerSelection.clear();
    lastCurriculumAssessment.reset();
    lastCurriculumMisconception.reset();
    applyLearnerPresentationMode();
    return true;
}

bool Simulation::cycleLearnerGrade(int direction) {
    if (direction == 0) return false;
    const int count = static_cast<int>(std::size(kAllGrades));
    const int next = (gradeIndex(learnerGrade) + (direction > 0 ? 1 : -1) + count) % count;
    return setLearnerGrade(kAllGrades[next]);
}

PresentationLayer Simulation::learnerPresentationLayer() const {
    return presentationLayerFor(learnerGrade);
}

void Simulation::applyLearnerPresentationMode() {
    switch (learnerPresentationLayer()) {
    case PresentationLayer::Foundation:
        settings.labelsEnabled = true;
        settings.vectorsEnabled = false;
        showVectors = false;
        showOrbits = false;
        showGrid = false;
        break;
    case PresentationLayer::Explorer:
        settings.labelsEnabled = true;
        settings.vectorsEnabled = false;
        showVectors = false;
        showOrbits = true;
        showGrid = false;
        break;
    case PresentationLayer::Scientist:
        settings.labelsEnabled = true;
        settings.vectorsEnabled = true;
        showVectors = true;
        showOrbits = true;
        showGrid = true;
        break;
    }
}

std::vector<const CurriculumActivity*> Simulation::activitiesForLearnerGrade() const {
    if (!curriculumReady) return {};
    return CurriculumLoader::activitiesForGrade(curriculumCatalog, learnerGrade);
}

const CurriculumActivity* Simulation::selectedCurriculumActivity() const {
    const auto activities = activitiesForLearnerGrade();
    if (activities.empty()) return nullptr;
    const int index = std::clamp(curriculumActivityIndex, 0, static_cast<int>(activities.size()) - 1);
    return activities[static_cast<std::size_t>(index)];
}

const CurriculumQuestion* Simulation::selectedCurriculumQuestion() const {
    const CurriculumActivity* activity = learningLabSession.activity();
    if (!activity && !(activity = selectedCurriculumActivity())) return nullptr;
    if (activity->assessmentIds.empty()) return nullptr;
    return CurriculumLoader::findQuestion(curriculumCatalog, activity->assessmentIds.front());
}

bool Simulation::selectCurriculumActivity(int index) {
    const auto activities = activitiesForLearnerGrade();
    if (activities.empty()) return false;
    curriculumActivityIndex = std::clamp(index, 0, static_cast<int>(activities.size()) - 1);
    learningLabSession.reset();
    curriculumAnswerSelection.clear();
    lastCurriculumAssessment.reset();
    lastCurriculumMisconception.reset();
    return true;
}

bool Simulation::startCurriculumActivity() {
    const CurriculumActivity* activity = selectedCurriculumActivity();
    if (!activity) return false;
    curriculumAnswerSelection.clear();
    lastCurriculumAssessment.reset();
    lastCurriculumMisconception.reset();
    return learningLabSession.start(*activity);
}

bool Simulation::advanceCurriculumActivity() {
    const LearningLabStep step = learningLabSession.state().step;
    if (step == LearningLabStep::Predict && learningLabSession.state().prediction.empty()) {
        recordCurriculumPrediction("Learner prediction recorded");
    }
    if (step == LearningLabStep::Observe && learningLabSession.state().observation.empty()) {
        recordCurriculumObservation("Learner observation recorded");
    }
    const bool advanced = learningLabSession.advance();
    if (advanced && learningLabSession.state().step == LearningLabStep::Assess) {
        curriculumAnswerSelection.clear();
        lastCurriculumAssessment.reset();
        lastCurriculumMisconception.reset();
    }
    return advanced;
}

bool Simulation::recordCurriculumPrediction(std::string text) {
    return learningLabSession.recordPrediction(std::move(text));
}

bool Simulation::recordCurriculumObservation(std::string text) {
    return learningLabSession.recordObservation(std::move(text));
}

bool Simulation::toggleCurriculumAnswerOption(int optionIndex) {
    const CurriculumQuestion* question = selectedCurriculumQuestion();
    if (!question || learningLabSession.state().step != LearningLabStep::Assess) return false;
    if (optionIndex < 0 || optionIndex >= static_cast<int>(question->options.size())) return false;
    const std::string& optionId = question->options[static_cast<std::size_t>(optionIndex)].id;
    const bool multi = question->questionType == QuestionType::MultipleSelect;
    if (!multi) {
        curriculumAnswerSelection = {optionId};
        return true;
    }
    const auto found = std::find(curriculumAnswerSelection.begin(), curriculumAnswerSelection.end(), optionId);
    if (found != curriculumAnswerSelection.end()) curriculumAnswerSelection.erase(found);
    else curriculumAnswerSelection.push_back(optionId);
    return true;
}

bool Simulation::submitCurriculumAssessment() {
    const CurriculumQuestion* question = selectedCurriculumQuestion();
    if (!question || learningLabSession.state().step != LearningLabStep::Assess) return false;
    AssessmentSubmission submission;
    submission.questionId = question->id;
    submission.selectedOptionIds = curriculumAnswerSelection;
    lastCurriculumAssessment = AssessmentEngine::score(*question, submission);
    lastCurriculumMisconception = MisconceptionEngine::detect(curriculumCatalog, question->id, curriculumAnswerSelection);
    return learningLabSession.completeAssessment(lastCurriculumAssessment->correct, lastCurriculumAssessment->score);
}

void Simulation::setSpeed(double value) {
    // Time scaling only feeds the existing bounded accumulator. Integration
    // still uses the configured/adaptive timestep and safety policies.
    speed = std::max(0.1, std::min(100000.0, value));
    settings.timeScale = speed;
}

void Simulation::adjustSpeed(int direction) {
    if (direction == 0) return;
    constexpr double steps[] = {
        0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 50.0, 100.0, 500.0,
        1000.0, 5000.0, 10000.0, 50000.0, 100000.0,
    };
    int nearest = 0;
    for (int index = 1; index < static_cast<int>(std::size(steps)); ++index) {
        if (std::abs(steps[index] - speed) < std::abs(steps[nearest] - speed)) nearest = index;
    }
    const int next = std::clamp(nearest + (direction > 0 ? 1 : -1), 0, static_cast<int>(std::size(steps)) - 1);
    setSpeed(steps[next]);
}

bool Simulation::selectBody(int index) {
    if (index < 0 || index >= static_cast<int>(bodies.size())) return false;
    if (!bodies[static_cast<std::size_t>(index)].active) return false;
    selected = index;
    // Isolation is owned by Z / Q — selecting must not silently cancel zoom state here.
    // Callers that need to leave isolate restore the camera explicitly.
    return true;
}

bool Simulation::isolateSelected() {
    if (selected < 0 || selected >= static_cast<int>(bodies.size())) return false;
    if (!bodies[static_cast<std::size_t>(selected)].active) return false;
    soloStudy = true;
    return true;
}

void Simulation::exitIsolation() {
    soloStudy = false;
}

void Simulation::clearSelection() {
    selected = -1;
    soloStudy = false;
}

bool Simulation::stepBack() {
    if (soloStudy) {
        exitIsolation();
        return true;
    }
    if (selected >= 0) {
        clearSelection();
        return true;
    }
    return false;
}

bool Simulation::bodyVisibleInView(int index) const {
    if (index < 0 || index >= static_cast<int>(bodies.size())) return false;
    const Body& candidate = bodies[static_cast<std::size_t>(index)];
    if (!candidate.active) return false;
    if (!soloStudy || selected < 0) return true;
    if (index == selected) return true;

    // Probes/spacecraft never tag along in Z study — only when they are the focus.
    if (candidate.type == "Spacecraft" || candidate.id == "bagsolar-1") return false;

    const Body& selectedBody = bodies[static_cast<std::size_t>(selected)];
    // Earth study keeps Moon; Moon study keeps Earth. Other bodies stay alone.
    if (!selectedBody.parentId.empty() && candidate.id == selectedBody.parentId &&
        candidate.type != "Star" && candidate.type != "Spacecraft") {
        return true;
    }
    if (!candidate.parentId.empty() && candidate.parentId == selectedBody.id &&
        (candidate.type == "Moon" || candidate.id == "moon")) {
        return true;
    }
    return false;
}

std::string Simulation::selectedBodyLesson() const {
    if (selected < 0 || selected >= static_cast<int>(bodies.size())) return {};
    const Body& body = bodies[static_cast<std::size_t>(selected)];
    const PresentationLayer layer = learnerPresentationLayer();
    if (body.id == "sun" || body.type == "Star") {
        if (layer == PresentationLayer::Foundation) return "The Sun is a star. It makes its own light and keeps the planets moving around it.";
        if (layer == PresentationLayer::Explorer) return "The Sun is the central star of the Solar System. Its gravity dominates planetary orbits.";
        return "Treat the Sun as the dominant central mass: orbital energy and period scale with GM⊙ and heliocentric distance.";
    }
    if (body.id == "mercury") {
        if (layer == PresentationLayer::Foundation) {
            return "Mercury spins very slowly — one day lasts about 59 Earth days — and stands nearly upright with almost no tilt.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Mercury's sidereal day is ~59 Earth days and its axial tilt is nearly 0°. Compare that with Earth's 24 h day and 23.5° tilt.";
        }
        return "Mercury: long sidereal day (~59 d) and near-zero obliquity. Spin is prograde but extremely slow versus orbital motion.";
    }
    if (body.id == "venus") {
        if (layer == PresentationLayer::Foundation) {
            return "Venus spins backward (retrograde). Its day is longer than its year — about 243 Earth days to spin once.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Venus is retrograde: it spins opposite Earth's west-to-east sense, and a sidereal day (~243 Earth days) exceeds its year.";
        }
        return "Venus: retrograde rotation (negative sidereal period) with obliquity ~177°. Day length exceeds the orbital period.";
    }
    if (body.id == "earth") {
        if (layer == PresentationLayer::Foundation) {
            return "Earth's day is about 24 hours. It spins west-to-east (prograde) with a moderate tilt of about 23.5° — that tilt makes seasons.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Earth orbits the Sun once a year. Its ~24 h prograde spin and ~23.5° tilt are the baseline for comparing other planets.";
        }
        return "Compare Earth's specific orbital energy, escape speed, and circular-orbit speed at 1 AU, plus its sidereal day and obliquity.";
    }
    if (body.id == "moon" || body.type == "Moon") {
        if (layer == PresentationLayer::Foundation) return "The Moon is Earth's neighbour in space. It does not make its own light.";
        if (layer == PresentationLayer::Explorer) return "The Moon is a natural satellite: it orbits Earth while Earth orbits the Sun.";
        return "Satellite motion is hierarchical: lunar state is relative to Earth, while the Earth–Moon barycentre orbits the Sun.";
    }
    if (body.id == "mars") {
        if (layer == PresentationLayer::Foundation) {
            return "Mars is almost like Earth: a day lasts about 24.6 hours, and its tilt (~25°) is close to Earth's 23.5°.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Mars has a prograde day (~24.6 h) and obliquity (~25°) nearly identical to Earth — a useful twin for spin comparisons.";
        }
        return "Mars: sidereal day ~24.6 h and obliquity ~25°. Prograde spin closely mirrors Earth's rotation geometry.";
    }
    if (body.id == "jupiter") {
        if (layer == PresentationLayer::Foundation) {
            return "Jupiter spins super fast — its day is only about 10 hours, the shortest day in the Solar System.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Jupiter's ~10 h day is the shortest among the planets. Watch how quickly its globe turns compared with Earth.";
        }
        return "Jupiter: gas-giant rotation with a ~10 h sidereal day — much faster angular rate than the terrestrial planets.";
    }
    if (body.id == "saturn") {
        if (layer == PresentationLayer::Foundation) {
            return "Saturn also spins very fast (about 10.7 hours) and leans about 27° — similar tilt to Earth, but a much quicker day.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Saturn's ~10.7 h day and ~27° tilt make a fast, Earth-like lean. Its rings follow the same tilted spin axis.";
        }
        return "Saturn: rapid ~10.7 h sidereal day with obliquity ~27°. Ring plane shares the body's oriented equator.";
    }
    if (body.id == "uranus") {
        if (layer == PresentationLayer::Foundation) {
            return "Uranus rolls on its side. It is tilted about 98°, so its spin axis lies almost in its orbit plane. A day lasts ~17 hours.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Uranus has a sideways spin (~98° tilt) and a ~17 h day. The amber pole axis in study view shows that extreme lean.";
        }
        return "Uranus: obliquity ~98° (sideways rotator) with a ~17 h sidereal day; sense of spin is retrograde in IAU convention.";
    }
    if (body.id == "neptune") {
        if (layer == PresentationLayer::Foundation) {
            return "Neptune spins quickly — about 16 hours for one day — with a moderate tilt near 28°, a bit more than Earth.";
        }
        if (layer == PresentationLayer::Explorer) {
            return "Neptune's ~16 h day and ~28° tilt are a fast, moderately tipped spin compared with Earth's 24 h / 23.5°.";
        }
        return "Neptune: ~16 h sidereal day and obliquity ~28°. Prograde ice-giant rotation with Earth-comparable tilt.";
    }
    if (body.type == "Planet") {
        if (layer == PresentationLayer::Foundation) return body.name + " is a planet. Planets move around the Sun and spin on their axes.";
        if (layer == PresentationLayer::Explorer) {
            return body.name + " is a planet. Compare its day length, tilt, and spin direction with Earth's.";
        }
        return "Inspect " + body.name + ": heliocentric distance, orbital speed, sidereal day, obliquity, and bound/unbound energy.";
    }
    if (body.type == "Spacecraft") {
        if (layer == PresentationLayer::Foundation) return body.name + " is a spacecraft made by people to explore space.";
        if (layer == PresentationLayer::Explorer) return body.name + " is an artificial satellite / probe. Watch how burns change its path.";
        return "Spacecraft trajectories respond to gravity and impulsive Δv. Track energy and whether the path stays bound.";
    }
    if (layer == PresentationLayer::Foundation) return "Look carefully at " + body.name + ". What is it, and how does it move?";
    if (layer == PresentationLayer::Explorer) return "Study " + body.name + ": identify its type, path, and relationship to the Sun.";
    return "Use " + body.name + " as a measurement target: radius, mass, distance, velocity, energy, and orbit class.";
}

void Simulation::adjustTimestep(double factor) {
    if (!std::isfinite(factor) || factor <= 0.0) return;
    settings.timestepSeconds = std::clamp(settings.timestepSeconds * factor, 1.0, PhysicsEngine::DAY);
    actualTimestep = settings.timestepSeconds;
    nextTimestep = settings.timestepSeconds;
    lastPredictionComparison.reset();
}

void Simulation::cycleIntegrator(int direction) {
    constexpr const char* integrators[] = {"euler", "semi_implicit_euler", "velocity_verlet", "rk4"};
    int current = 0;
    for (int index = 0; index < 4; ++index) {
        if (settings.integrator == integrators[index]) current = index;
    }
    current = (current + (direction >= 0 ? 1 : -1) + 4) % 4;
    settings.integrator = integrators[current];
    lastPredictionComparison.reset();
}

void Simulation::setScreen(AppScreen next) {
    screen = next;
    educationScreen = next == AppScreen::Education;
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
