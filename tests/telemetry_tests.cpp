#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "physics/PhysicsEngine.hpp"
#include "simulation/Simulation.hpp"
#include "telemetry/TelemetryAnalyzer.hpp"
#include "telemetry/TelemetryCollector.hpp"
#include "telemetry/TelemetryExporter.hpp"
#include "telemetry/TelemetryTypes.hpp"

namespace {

std::vector<bag::Body> earthSystem() {
    bag::Body sun;
    sun.id = "sun";
    sun.mass = bag::PhysicsEngine::SOLAR_MASS;
    sun.realRadius = 1.0;
    bag::Body earth;
    earth.id = "earth";
    earth.mass = bag::PhysicsEngine::EARTH_MASS;
    earth.realRadius = bag::PhysicsEngine::EARTH_RADIUS;
    earth.position = {bag::PhysicsEngine::AU, 0.0, 0.0};
    earth.velocity = {0.0, bag::PhysicsEngine::orbitalVelocity(earth), 0.0};
    return {sun, earth};
}

} // namespace

int main() {
    using namespace bag;
    TelemetrySession emptySession;
    assert(emptySession.samples.empty());
    assert(analyzeTelemetry(emptySession).sampleCount == 0);
    assert(telemetryJson(emptySession).find("\"schema_version\": 1") != std::string::npos);

    std::vector<Body> bodies = earthSystem();
    TelemetryConfig config;
    config.enabled = true;
    config.referenceBodyId = "sun";
    config.samplingIntervalSeconds = 10.0;

    TelemetrySessionMetadata metadata;
    metadata.sessionId = "telemetry-test-session";
    metadata.scenarioId = "earth-orbit";
    metadata.scenarioName = "Earth Orbit";
    metadata.epoch = "J2000";
    metadata.referenceFrame = "heliocentric";
    metadata.referenceBodyId = "sun";
    metadata.integrator = "rk4";
    metadata.initialTimestepSeconds = 1.0;

    TelemetrySession session;
    assert(session.start(metadata, 10.0, bodies, 0.0, config));
    assert(session.samples.size() == 2);
    assert(session.samples[1].bodyId == "earth");
    assert(std::isfinite(session.samples[1].distanceToReferenceM));
    assert(std::isfinite(session.samples[1].relativeSpeedMps));
    assert(std::isfinite(session.samples[1].accelerationMps2.x));
    assert(std::isfinite(session.samples[1].specificEnergyJPerKg));
    assert(std::isfinite(session.samples[1].angularMomentumMagnitudeM2PerS));
    assert(std::isfinite(session.samples[1].eccentricity));
    assert(std::isfinite(session.samples[1].semiMajorAxisM));
    assert(std::isfinite(session.samples[1].periapsisM));
    assert(std::isfinite(session.samples[1].apoapsisM));
    assert(session.samples[1].integrator == Integrator::RK4);
    assert(session.samples[1].status == TelemetryStatus::OK);

    assert(session.observe(bodies, 5.0, 1.0, 1.0, Integrator::RK4, TelemetryStatus::OK, config));
    assert(session.samples.size() == 2);
    assert(session.observe(bodies, 10.0, 1.0, 1.0, Integrator::RK4, TelemetryStatus::OK, config));
    assert(session.samples.size() == 4);
    assert(session.observe(bodies, 20.0, 2.0, 2.0, Integrator::RK4, TelemetryStatus::WARNING, config));
    assert(session.samples.size() == 6);

    PhysicsStepResult eventResult;
    eventResult.success = true;
    eventResult.interactions.closeApproach.triggered = true;
    eventResult.interactions.closeApproach.bodyA = 0;
    eventResult.interactions.closeApproach.bodyB = 1;
    eventResult.interactions.collisions.push_back({0, 1, 1.0, 2.0});
    eventResult.suggestedTimestep = 0.5;
    session.recordPhysicsEvents(20.0, 2.0, eventResult, bodies);
    assert(session.events.size() == 3);
    session.stop(20.0);
    assert(!session.active);
    assert(session.metadata.status == TelemetryStatus::COMPLETED);

    const TelemetryAnalysis analysis = analyzeTelemetry(session, "earth");
    assert(analysis.sampleCount == 3);
    assert(analysis.simulationDurationSeconds == 20.0);
    assert(analysis.eventCount == 3);
    assert(std::isfinite(analysis.minimumDistanceM));
    assert(std::isfinite(analysis.relativeEnergyDrift));

    const std::string csv = telemetryCsv(session);
    assert(csv == telemetryCsv(session));
    assert(csv.find("# bagsolar_telemetry_schema_version=1") == 0);
    assert(csv.find("simulation_time_s") != std::string::npos);
    assert(csv.find("angular_momentum_magnitude_m2_per_s") != std::string::npos);

    const std::string jsonText = telemetryJson(session);
    const auto json = nlohmann::json::parse(jsonText);
    assert(json.at("schema_version") == TELEMETRY_SCHEMA_VERSION);
    assert(json.at("session").at("session_id") == "telemetry-test-session");
    assert(json.at("samples").size() == 6);
    assert(json.at("events").size() == 3);

    const std::filesystem::path csvPath = std::filesystem::temp_directory_path() / "bagsolar_telemetry_test.csv";
    const std::filesystem::path jsonPath = std::filesystem::temp_directory_path() / "bagsolar_telemetry_test.json";
    assert(writeTelemetryCsv(csvPath, session));
    assert(writeTelemetryJson(jsonPath, session));
    assert(std::filesystem::file_size(csvPath) > 0);
    assert(std::filesystem::file_size(jsonPath) > 0);
    std::filesystem::remove(csvPath);
    std::filesystem::remove(jsonPath);

    TelemetryConfig missingConfig = config;
    missingConfig.referenceBodyId = "missing";
    std::vector<TelemetrySample> missingSamples;
    assert(TelemetryCollector::collect(bodies, 0.0, 1.0, 1.0, Integrator::RK4,
                                       TelemetryStatus::OK, missingConfig, "J2000", "heliocentric", missingSamples));
    assert(std::isnan(missingSamples[1].distanceToReferenceM));

    std::vector<Body> invalidBodies = bodies;
    invalidBodies[1].position.x = std::numeric_limits<double>::quiet_NaN();
    std::vector<TelemetrySample> invalidSamples;
    assert(TelemetryCollector::collect(invalidBodies, 0.0, 1.0, 1.0, Integrator::RK4,
                                       TelemetryStatus::INVALID_STATE, config, "J2000", "heliocentric", invalidSamples));
    assert(invalidSamples[1].status == TelemetryStatus::INVALID_STATE);

    TelemetrySession second = session;
    second.samples.back().positionM.x += 100.0;
    const TelemetryComparison comparison = compareTelemetry(session, second, "earth");
    assert(comparison.valid);
    assert(comparison.status == TelemetryComparison::Status::Compatible);
    assert(std::string(telemetryComparisonStatusName(comparison.status)) == "COMPATIBLE");
    assert(comparison.finalPositionDifferenceM > 0.0);
    assert(comparison.firstSampleCount == comparison.secondSampleCount);

    const TelemetrySession identical = session;
    const TelemetryComparison identicalComparison = compareTelemetry(session, identical, "earth");
    const TelemetryComparison repeatedComparison = compareTelemetry(session, identical, "earth");
    assert(identicalComparison.valid);
    assert(identicalComparison.finalPositionDifferenceM == 0.0);
    assert(identicalComparison.finalVelocityDifferenceMps == 0.0);
    assert(identicalComparison.message == repeatedComparison.message);
    assert(identicalComparison.finalPositionDifferenceM == repeatedComparison.finalPositionDifferenceM);

    const auto expectStatus = [&](TelemetrySession candidate, TelemetryComparison::Status expected) {
        const TelemetryComparison rejected = compareTelemetry(session, candidate, "earth");
        assert(!rejected.valid);
        assert(rejected.status == expected);
        assert(!rejected.message.empty());
        assert(std::isnan(rejected.finalPositionDifferenceM));
        assert(std::isnan(rejected.finalVelocityDifferenceMps));
    };

    TelemetrySession frameMismatch = identical;
    frameMismatch.metadata.referenceFrame = "geocentric";
    expectStatus(frameMismatch, TelemetryComparison::Status::IncompatibleFrame);

    TelemetrySession originMismatch = identical;
    originMismatch.metadata.referenceBodyId = "earth";
    expectStatus(originMismatch, TelemetryComparison::Status::IncompatibleOrigin);

    TelemetrySession unitsMismatch = identical;
    unitsMismatch.metadata.ephemerisUnits = "km";
    expectStatus(unitsMismatch, TelemetryComparison::Status::IncompatibleUnits);

    TelemetrySession epochMismatch = identical;
    epochMismatch.metadata.epoch = "J2000+1d";
    expectStatus(epochMismatch, TelemetryComparison::Status::IncompatibleEpoch);

    TelemetrySession referenceMismatch = identical;
    referenceMismatch.metadata.ephemerisProvider = "LocalEphemerisProvider";
    referenceMismatch.metadata.ephemerisSource = "deterministic-fixture";
    expectStatus(referenceMismatch, TelemetryComparison::Status::IncompatibleReference);

    TelemetrySession scenarioMismatch = identical;
    scenarioMismatch.metadata.scenarioId = "default-solar-system";
    expectStatus(scenarioMismatch, TelemetryComparison::Status::IncompatibleScenario);

    TelemetrySession schemaMismatch = identical;
    schemaMismatch.metadata.schemaVersion = TELEMETRY_SCHEMA_VERSION + 1;
    expectStatus(schemaMismatch, TelemetryComparison::Status::IncompatibleSchema);

    TelemetrySession timeMismatch = identical;
    timeMismatch.metadata.simulationEndTimeSeconds += 1.0;
    expectStatus(timeMismatch, TelemetryComparison::Status::IncompatibleTime);

    TelemetrySession finalSampleTimeMismatch = identical;
    finalSampleTimeMismatch.samples.back().simulationTimeSeconds += 1.0;
    expectStatus(finalSampleTimeMismatch, TelemetryComparison::Status::IncompatibleTime);

    TelemetrySession invalidTelemetrySession = identical;
    invalidTelemetrySession.samples.back().positionM.x = std::numeric_limits<double>::quiet_NaN();
    expectStatus(invalidTelemetrySession, TelemetryComparison::Status::InvalidData);

    TelemetrySession malformedSamples = identical;
    malformedSamples.samples.back().referenceFrame.clear();
    expectStatus(malformedSamples, TelemetryComparison::Status::InvalidData);

    TelemetrySession firstWrongBody = identical;
    TelemetrySession secondWrongBody = identical;
    for (TelemetrySample& sample : firstWrongBody.samples) sample.bodyId = "mars";
    for (TelemetrySample& sample : secondWrongBody.samples) sample.bodyId = "mars";
    const TelemetryComparison bodyMismatch = compareTelemetry(firstWrongBody, secondWrongBody, "earth");
    assert(!bodyMismatch.valid);
    assert(bodyMismatch.status == TelemetryComparison::Status::IncompatibleBody);

    TelemetrySession emptyComparisonSession;
    const TelemetryComparison insufficient = compareTelemetry(session, emptyComparisonSession, "earth");
    assert(!insufficient.valid);
    assert(insufficient.status == TelemetryComparison::Status::InsufficientData);

    session.clear();
    assert(session.samples.empty());
    assert(session.events.empty());
    assert(!session.active);

    std::vector<Body> withoutTelemetry = earthSystem();
    std::vector<Body> withTelemetry = earthSystem();
    TelemetryConfig observationalConfig = config;
    for (int step = 0; step < 20; ++step) {
        assert(PhysicsEngine{}.integrate(withoutTelemetry, 1.0, Integrator::RK4).success);
        assert(PhysicsEngine{}.integrate(withTelemetry, 1.0, Integrator::RK4).success);
        std::vector<TelemetrySample> ignored;
        assert(TelemetryCollector::collect(withTelemetry, step + 1.0, 1.0, 1.0, Integrator::RK4,
                                           TelemetryStatus::OK, observationalConfig, "J2000", "heliocentric", ignored));
    }
    assert(withoutTelemetry[1].position.x == withTelemetry[1].position.x);
    assert(withoutTelemetry[1].position.y == withTelemetry[1].position.y);
    assert(withoutTelemetry[1].velocity.x == withTelemetry[1].velocity.x);
    assert(withoutTelemetry[1].velocity.y == withTelemetry[1].velocity.y);

    const std::filesystem::path dataRoot = std::filesystem::path(BAGSOLAR_SOURCE_DIR) / "data";
    Simulation simulationWithTelemetry(dataRoot);
    Simulation simulationWithoutTelemetry(dataRoot);
    simulationWithTelemetry.setSpeed(3600.0);
    simulationWithoutTelemetry.setSpeed(3600.0);
    assert(simulationWithTelemetry.speed == 3600.0);
    simulationWithTelemetry.setSpeed(1.0e9);
    assert(simulationWithTelemetry.speed == 100000.0);
    simulationWithTelemetry.adjustSpeed(1);
    assert(simulationWithTelemetry.speed == 100000.0);
    simulationWithTelemetry.setSpeed(5000.0);
    simulationWithTelemetry.adjustSpeed(1);
    assert(simulationWithTelemetry.speed == 10000.0);
    simulationWithTelemetry.adjustSpeed(1);
    assert(simulationWithTelemetry.speed == 50000.0);
    simulationWithTelemetry.adjustSpeed(1);
    assert(simulationWithTelemetry.speed == 100000.0);
    assert(!simulationWithTelemetry.bodies.empty());
    const int earthIndex = [&]() {
        for (int i = 0; i < static_cast<int>(simulationWithTelemetry.bodies.size()); ++i) {
            if (simulationWithTelemetry.bodies[static_cast<std::size_t>(i)].id == "earth") return i;
        }
        return -1;
    }();
    assert(earthIndex >= 0);
    assert(simulationWithTelemetry.selectBody(earthIndex));
    assert(!simulationWithTelemetry.soloStudy);
    assert(simulationWithTelemetry.selected == earthIndex);
    assert(simulationWithTelemetry.bodyVisibleInView(1)); // selection alone does not isolate
    assert(simulationWithTelemetry.isolateSelected());
    assert(simulationWithTelemetry.soloStudy);
    assert(simulationWithTelemetry.bodyVisibleInView(earthIndex));
    assert(!simulationWithTelemetry.bodyVisibleInView(0)); // Sun is hidden in isolate (no blur reference)
    int moonIndex = -1;
    int bagsolarIndex = -1;
    for (int i = 0; i < static_cast<int>(simulationWithTelemetry.bodies.size()); ++i) {
        const auto& body = simulationWithTelemetry.bodies[static_cast<std::size_t>(i)];
        if (body.id == "moon") moonIndex = i;
        if (body.id == "bagsolar-1") bagsolarIndex = i;
    }
    assert(moonIndex >= 0);
    assert(simulationWithTelemetry.bodyVisibleInView(moonIndex)); // Earth study keeps Moon
    if (bagsolarIndex >= 0) {
        assert(!simulationWithTelemetry.bodyVisibleInView(bagsolarIndex)); // probes stay out of isolate
    }
    for (int i = 0; i < static_cast<int>(simulationWithTelemetry.bodies.size()); ++i) {
        if (i == earthIndex || i == moonIndex) continue;
        assert(!simulationWithTelemetry.bodyVisibleInView(i));
    }
    assert(!simulationWithTelemetry.selectedBodyLesson().empty());
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(earthIndex)].rotationPeriod > 0.0);
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(earthIndex)].axialTilt > 20.0);
    assert(simulationWithTelemetry.selectedBodyLesson().find("23.5") != std::string::npos ||
           simulationWithTelemetry.selectedBodyLesson().find("24") != std::string::npos);
    assert(simulationWithTelemetry.stepBack());
    assert(!simulationWithTelemetry.soloStudy);
    assert(simulationWithTelemetry.selected == earthIndex);
    // Moon study keeps Earth as the companion body.
    assert(simulationWithTelemetry.selectBody(moonIndex));
    assert(simulationWithTelemetry.isolateSelected());
    assert(simulationWithTelemetry.bodyVisibleInView(moonIndex));
    assert(simulationWithTelemetry.bodyVisibleInView(earthIndex));
    assert(!simulationWithTelemetry.bodyVisibleInView(0));
    assert(simulationWithTelemetry.stepBack());
    assert(simulationWithTelemetry.selected == moonIndex);
    assert(simulationWithTelemetry.stepBack());
    assert(simulationWithTelemetry.selected < 0);
    simulationWithTelemetry.clearSelection();
    assert(!simulationWithTelemetry.soloStudy);
    assert(simulationWithTelemetry.selected < 0);
    if (bagsolarIndex >= 0) {
        assert(!simulationWithTelemetry.bodies[static_cast<std::size_t>(bagsolarIndex)].active);
    }
    simulationWithTelemetry.setSpeed(3600.0);
    assert(simulationWithTelemetry.startTelemetry(3600.0, "sun"));
    simulationWithTelemetry.integrate(1.0);
    simulationWithoutTelemetry.integrate(1.0);
    assert(simulationWithTelemetry.telemetryEnabled());
    simulationWithTelemetry.stopTelemetry();
    assert(!simulationWithTelemetry.telemetryEnabled());
    assert(!simulationWithTelemetry.telemetry.samples.empty());
    const std::size_t activeBodies = static_cast<std::size_t>(std::count_if(
        simulationWithTelemetry.bodies.begin(), simulationWithTelemetry.bodies.end(),
        [](const Body& body) { return body.active; }));
    assert(simulationWithTelemetry.telemetry.samples.size() >= activeBodies);
    const std::filesystem::path simulationCsv = std::filesystem::temp_directory_path() / "bagsolar_simulation_telemetry.csv";
    const std::filesystem::path simulationJson = std::filesystem::temp_directory_path() / "bagsolar_simulation_telemetry.json";
    assert(simulationWithTelemetry.exportTelemetryCsv(simulationCsv));
    assert(simulationWithTelemetry.exportTelemetryJson(simulationJson));
    assert(std::filesystem::file_size(simulationCsv) > 0);
    assert(std::filesystem::file_size(simulationJson) > 0);
    std::filesystem::remove(simulationCsv);
    std::filesystem::remove(simulationJson);
    assert(simulationWithTelemetry.bodies.size() == simulationWithoutTelemetry.bodies.size());
    for (std::size_t i = 0; i < simulationWithTelemetry.bodies.size(); ++i) {
        assert(simulationWithTelemetry.bodies[i].position.x == simulationWithoutTelemetry.bodies[i].position.x);
        assert(simulationWithTelemetry.bodies[i].position.y == simulationWithoutTelemetry.bodies[i].position.y);
        assert(simulationWithTelemetry.bodies[i].velocity.x == simulationWithoutTelemetry.bodies[i].velocity.x);
        assert(simulationWithTelemetry.bodies[i].velocity.y == simulationWithoutTelemetry.bodies[i].velocity.y);
    }
    // P / launchProbe wakes the dormant spacecraft and applies a visible burn.
    assert(simulationWithTelemetry.launchProbe(3500.0));
    assert(simulationWithTelemetry.selected >= 0);
    const int probeIndex = simulationWithTelemetry.selected;
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(probeIndex)].id == "bagsolar-1");
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(probeIndex)].active);
    // Active probe must still stay hidden when Z-studying Earth (Moon companion only).
    assert(simulationWithTelemetry.selectBody(earthIndex));
    assert(simulationWithTelemetry.isolateSelected());
    assert(simulationWithTelemetry.bodyVisibleInView(earthIndex));
    assert(simulationWithTelemetry.bodyVisibleInView(moonIndex));
    assert(!simulationWithTelemetry.bodyVisibleInView(probeIndex));
    assert(simulationWithTelemetry.stepBack());
    assert(simulationWithTelemetry.launchProbe(3500.0));
    const auto earthAfter = std::find_if(
        simulationWithTelemetry.bodies.begin(), simulationWithTelemetry.bodies.end(),
        [](const Body& body) { return body.id == "earth"; });
    assert(earthAfter != simulationWithTelemetry.bodies.end());
    const double probeAltitude = length(
        simulationWithTelemetry.bodies[static_cast<std::size_t>(simulationWithTelemetry.selected)].position -
        earthAfter->position);
    assert(probeAltitude > 1.0e8);
    assert(probeAltitude < 1.0e9);
    // Second pulse re-arms from Earth so P always has a visible effect.
    simulationWithTelemetry.integrate(2.0);
    assert(simulationWithTelemetry.launchProbe(5000.0));
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(simulationWithTelemetry.selected)].active);
    assert(simulationWithTelemetry.bodies[static_cast<std::size_t>(simulationWithTelemetry.selected)].id ==
           "bagsolar-1");

    return 0;
}
