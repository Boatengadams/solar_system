#include <cassert>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "astronomy/EphemerisTypes.hpp"
#include "astronomy/HorizonsParser.hpp"
#include "astronomy/HorizonsProvider.hpp"
#include "astronomy/JsonEphemerisProvider.hpp"
#include "astronomy/LocalEphemerisProvider.hpp"
#include "education/EducationContent.hpp"
#include "education/LearnerReport.hpp"
#include "simulation/Simulation.hpp"

namespace {
using namespace bag;

class FakeHttpClient final : public HttpClient {
public:
    int calls = 0;
    int status = 200;
    std::string payload;
    HttpResponse get(const std::string&, double) override { ++calls; return {status, payload, status == 0 ? "offline" : ""}; }
};

EphemerisRequest earthRequest() { return {"earth", Epoch::julianDate(2451545.0), Frame::heliocentric()}; }

void testLocalProviderAndSnapshot() {
    LocalEphemerisProvider provider = LocalEphemerisProvider::deterministicFixture();
    const EphemerisResult earth = provider.getState(earthRequest());
    assert(earth);
    assert(earth.state.positionM.x > 1.0e11);
    assert(provider.getState({"unknown", earthRequest().epoch, earthRequest().frame}).status == EphemerisStatus::BODY_NOT_FOUND);
    EphemerisSnapshot snapshot;
    assert(snapshot.add(earth.state));
    const EphemerisResult mars = provider.getState({"mars", earthRequest().epoch, earthRequest().frame});
    assert(snapshot.add(mars.state));
    assert(snapshot.valid() && snapshot.states.size() == 2);
    assert(!snapshot.add({"venus", Epoch::julianDate(2451546.0), Frame::heliocentric(), {}, {}, "", "", "", EphemerisStatus::SUCCESS, {}}));
}

void testJsonProvider() {
    const auto path = std::filesystem::temp_directory_path() / "bagsolar_ephemeris_test.json";
    std::ofstream output(path);
    output << R"({"schema_version":1,"body_id":"earth","epoch":{"type":"julian_date","value":2451545.0},"frame":{"name":"heliocentric","origin_body_id":"sun"},"units":{"position":"km","velocity":"km/s"},"position":[149597870.7,0,0],"velocity":[0,29.7846918,0],"source":"fixture"})";
    output.close();
    JsonEphemerisProvider provider(path);
    const EphemerisResult result = provider.getState(earthRequest());
    assert(result && result.state.positionM.x > 1.495e11 && result.state.velocityMps.y > 29000.0);
    assert(provider.getState({"earth", Epoch::julianDate(2451546.0), Frame::heliocentric()}).status == EphemerisStatus::EPOCH_MISMATCH);
    std::filesystem::remove(path);
}

void testHorizonsProviderAndParser() {
    const std::string payload = R"({"signature":{"version":"1.0"},"result":"$$SOE\n2451545.000000000 = A.D. 2000-Jan-01 12:00:00.0000 TDB\n X = 1.0E+08 Y = 2.0E+08 Z = 3.0E+08\n VX= 1.0E+02 VY= 2.0E+02 VZ= 3.0E+02\n$$EOE"})";
    const EphemerisResult parsed = parseHorizonsResponse(payload, earthRequest());
    assert(parsed);
    assert(parsed.state.positionM.x == 1.0e11 && parsed.state.velocityMps.z == 3.0e5);
    auto client = std::make_shared<FakeHttpClient>();
    client->payload = payload;
    HorizonsProvider provider(client);
    assert(provider.getState(earthRequest()));
    assert(provider.getState(earthRequest()));
    assert(client->calls == 1);
    client->status = 500;
    provider.clearCache();
    assert(provider.getState(earthRequest()).status == EphemerisStatus::REMOTE_ERROR);
    client->status = 0;
    assert(provider.getState(earthRequest()).status == EphemerisStatus::NETWORK_ERROR);
}

void testSimulationInitialization() {
    Simulation simulation("data");
    LocalEphemerisProvider provider = LocalEphemerisProvider::deterministicFixture();
    EphemerisSnapshot snapshot;
    assert(snapshot.add(provider.getState({"sun", Epoch::julianDate(2451545.0), Frame::heliocentric()}).state));
    assert(snapshot.add(provider.getState(earthRequest()).state));
    assert(simulation.initializeFromEphemeris(snapshot));
    assert(simulation.ephemerisEpoch && simulation.ephemerisEpoch->value == 2451545.0);
    assert(simulation.ephemerisProvider == "LocalEphemerisProvider");
    assert(simulation.startTelemetry(10.0, "sun"));
    assert(simulation.telemetry.metadata.ephemerisProvider == "LocalEphemerisProvider");
    simulation.stopTelemetry();
}

void testLearnerPredictionReferenceWorkflow() {
    int predictionIndex = -1;
    for (int index = 0; index < experimentCount(); ++index) {
        if (std::string(experimentAt(index).id) == "prediction-reference") predictionIndex = index;
    }
    assert(predictionIndex >= 0);

    Simulation simulation("data");
    assert(simulation.selectEducationActivity(EducationActivityType::Experiment, predictionIndex));
    assert(simulation.startEducationActivity());
    assert(simulation.beginEducationObservation());
    assert(simulation.educationWorkflow.state() == EducationWorkflowState::Observing);
    assert(simulation.lastPredictionComparison.has_value());
    assert(simulation.lastPredictionComparison->success());
    assert(simulation.lastPredictionComparison->referenceProvider == "LocalEphemerisProvider");
    assert(simulation.lastPredictionComparison->samples.size() == 2);
    assert(simulation.readyEducationForEvaluation());
    assert(simulation.evaluateCurrentExperiment());
    assert(simulation.educationWorkflow.state() == EducationWorkflowState::Evaluated);
    const ExperimentProgress* progress = simulation.educationProgress.experimentProgress(predictionIndex);
    assert(progress && progress->attempts == 1 && progress->completed && progress->bestScore == 100.0);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "bagsolar-prediction-reference-progress.json";
    assert(simulation.saveEducationProgress(path));
    Simulation restored("data");
    assert(restored.loadEducationProgress(path));
    const LearnerReport report = buildLearnerReport(restored.educationProgress);
    const auto learnerActivity = std::find_if(report.activities.begin(), report.activities.end(), [](const LearnerActivityReport& activity) {
        return activity.id == "prediction-reference";
    });
    assert(learnerActivity != report.activities.end());
    assert(learnerActivity->outcome == LearnerActivityOutcome::Passed && learnerActivity->bestScore == 100.0);
    std::filesystem::remove(path);

    assert(simulation.retryEducationActivity());
    assert(simulation.lastPredictionComparison.has_value() == false);
    assert(simulation.startEducationActivity() && simulation.beginEducationObservation());
    assert(simulation.lastPredictionComparison && simulation.lastPredictionComparison->success());
    assert(simulation.readyEducationForEvaluation() && simulation.evaluateCurrentExperiment());
    progress = simulation.educationProgress.experimentProgress(predictionIndex);
    assert(progress && progress->attempts == 2 && progress->bestScore == 100.0);
}

void testLearnerPredictionReferenceProviderFailure() {
    int predictionIndex = -1;
    for (int index = 0; index < experimentCount(); ++index) {
        if (std::string(experimentAt(index).id) == "prediction-reference") predictionIndex = index;
    }
    Simulation simulation("data");
    simulation.setEphemerisProvider(nullptr);
    assert(simulation.selectEducationActivity(EducationActivityType::Experiment, predictionIndex));
    assert(simulation.startEducationActivity() && simulation.beginEducationObservation());
    assert(simulation.lastPredictionComparison);
    assert(simulation.lastPredictionComparison->status == PredictionComparisonStatus::ProviderUnavailable);
    assert(simulation.readyEducationForEvaluation() && simulation.evaluateCurrentExperiment());
    assert(simulation.educationProgress.experimentProgress(predictionIndex)->attempts == 0);
}
}

int main() {
    testLocalProviderAndSnapshot();
    testJsonProvider();
    testHorizonsProviderAndParser();
    testSimulationInitialization();
    testLearnerPredictionReferenceWorkflow();
    testLearnerPredictionReferenceProviderFailure();
    return 0;
}
