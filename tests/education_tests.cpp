#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include <nlohmann/json.hpp>

#include "education/EducationChallenges.hpp"
#include "education/EducationContent.hpp"
#include "education/EducationProgress.hpp"

int main() {
    using namespace bag;
    assert(lessonCount() >= 9);
    assert(experimentCount() >= 6);
    EducationProgress progress(lessonCount(), experimentCount());
    assert(progress.completeLesson(0));
    assert(progress.completeExperiment(0));
    assert(progress.recordObservation(1, "period increased after moving the orbit outward"));
    assert(!progress.completeLesson(-1));
    const EducationReport report = progress.report();
    assert(report.completedLessons == 1 && report.completedExperiments == 2);
    assert(report.completionRatio > 0.0 && report.observations.size() == 1);

    assert(challengeCount() == 5);
    assert(findChallenge("escape-velocity") != nullptr);
    assert(findChallenge("missing") == nullptr);
    for (int index = 0; index < challengeCount(); ++index) assert(validChallengeDefinition(challengeAt(index)));
    ChallengeDefinition invalidChallenge = challengeAt(0);
    invalidChallenge.scoring.passingRelativeError = -1.0;
    assert(!validChallengeDefinition(invalidChallenge));

    const ChallengeDefinition& escape = *findChallenge("escape-velocity");
    ChallengeAnswer escapeAnswer;
    escapeAnswer.primaryValue = escape.defaultAnswer;
    const ChallengeResult escapeResult = evaluateChallenge(escape, escapeAnswer);
    assert(escapeResult.valid && escapeResult.passed && escapeResult.score == 100.0);
    const ChallengeResult repeatEscape = evaluateChallenge(escape, escapeAnswer);
    assert(repeatEscape.score == escapeResult.score && repeatEscape.feedback == escapeResult.feedback);
    escapeAnswer.primaryValue *= 1.10;
    const ChallengeResult failedEscape = evaluateChallenge(escape, escapeAnswer);
    assert(failedEscape.valid && !failedEscape.passed && failedEscape.score == 0.0);

    const ChallengeDefinition& circular = *findChallenge("circular-orbit");
    ChallengeAnswer circularAnswer;
    circularAnswer.primaryValue = circular.defaultAnswer;
    assert(evaluateChallenge(circular, circularAnswer).passed);

    const ChallengeDefinition& integrator = *findChallenge("integrator-comparison");
    ChallengeAnswer integratorAnswer;
    integratorAnswer.integrator = Integrator::VelocityVerlet;
    const ChallengeResult integratorResult = evaluateChallenge(integrator, integratorAnswer);
    assert(integratorResult.valid && integratorResult.comparison.size() == 4);
    assert(std::isfinite(integratorResult.metrics.normalizedNumericalError));

    const ChallengeDefinition& timestep = *findChallenge("timestep-selection");
    ChallengeAnswer timestepAnswer;
    timestepAnswer.timestepSeconds = timestep.defaultTimestepSeconds;
    const ChallengeResult timestepResult = evaluateChallenge(timestep, timestepAnswer);
    assert(timestepResult.valid && std::isfinite(timestepResult.metrics.positionError));

    const ChallengeDefinition& hohmann = *findChallenge("hohmann-transfer");
    const HohmannTransfer reference = PhysicsEngine::hohmannTransfer(
        hohmann.innerRadiusM, hohmann.outerRadiusM, hohmann.centralMassKg);
    assert(reference.valid && reference.departureDeltaV > 0.0 && reference.arrivalDeltaV > 0.0);
    ChallengeAnswer hohmannAnswer;
    hohmannAnswer.primaryValue = reference.totalDeltaV;
    const ChallengeResult hohmannResult = evaluateChallenge(hohmann, hohmannAnswer);
    assert(hohmannResult.valid && hohmannResult.passed && hohmannResult.score == 100.0);
    assert(hohmannResult.metrics.referenceDepartureDeltaV == reference.departureDeltaV);
    assert(hohmannResult.metrics.referenceArrivalDeltaV == reference.arrivalDeltaV);
    hohmannAnswer.primaryValue = reference.totalDeltaV * (1.0 + hohmann.scoring.fullCreditRelativeError * 0.5);
    const ChallengeResult closeHohmann = evaluateChallenge(hohmann, hohmannAnswer);
    assert(closeHohmann.valid && closeHohmann.passed && closeHohmann.score == 100.0);
    hohmannAnswer.primaryValue = reference.totalDeltaV * (1.0 + hohmann.scoring.passingRelativeError * 1.1);
    const ChallengeResult failedHohmann = evaluateChallenge(hohmann, hohmannAnswer);
    assert(failedHohmann.valid && !failedHohmann.passed && failedHohmann.score == 0.0);
    hohmannAnswer.primaryValue = std::numeric_limits<double>::quiet_NaN();
    assert(!evaluateChallenge(hohmann, hohmannAnswer).valid);
    ChallengeDefinition invalidHohmann = hohmann;
    invalidHohmann.innerRadiusM = invalidHohmann.centralBodyRadiusM;
    assert(!validChallengeDefinition(invalidHohmann));
    invalidHohmann = hohmann;
    invalidHohmann.outerRadiusM = invalidHohmann.innerRadiusM;
    assert(!validChallengeDefinition(invalidHohmann));

    EducationProgress challengeProgress(lessonCount(), experimentCount(), challengeCount());
    assert(challengeProgress.recordChallengeResult(0, escapeResult));
    assert(challengeProgress.recordChallengeResult(0, failedEscape));
    assert(challengeProgress.recordChallengeResult(4, hohmannResult));
    assert(challengeProgress.challengeComplete(0));
    assert(challengeProgress.challengeProgress(0)->attempts == 2);
    assert(challengeProgress.challengeProgress(0)->bestScore == 100.0);
    assert(challengeProgress.exportText() == challengeProgress.exportText());
    assert(challengeProgress.report().completedChallenges == 2);

    assert(challengeProgress.completeLesson(2));
    assert(challengeProgress.recordObservation(1, "the transfer uses two tangential burns"));
    const std::string serialized = challengeProgress.serialize();
    const auto json = nlohmann::json::parse(serialized);
    assert(json["schema_version"] == EducationProgress::CURRENT_SCHEMA_VERSION);
    EducationProgress restored(lessonCount(), experimentCount(), challengeCount());
    assert(restored.deserialize(serialized));
    assert(restored.serialize() == serialized);
    assert(restored.challengeProgress(0)->attempts == 2);
    assert(restored.challengeProgress(4)->completed);
    assert(restored.challengeProgress(4)->bestScore == 100.0);
    assert(restored.report().observations.size() == 1);

    const std::filesystem::path progressPath = std::filesystem::temp_directory_path() / "bagsolar-education-progress-test.json";
    assert(challengeProgress.save(progressPath));
    EducationProgress loaded(lessonCount(), experimentCount(), challengeCount());
    assert(loaded.load(progressPath));
    assert(loaded.serialize() == serialized);
    std::filesystem::remove(progressPath);

    const std::string beforeInvalidLoad = restored.serialize();
    assert(!restored.deserialize("not json"));
    assert(restored.serialize() == beforeInvalidLoad);
    assert(!restored.deserialize(R"({"schema_version":999})"));
    assert(restored.serialize() == beforeInvalidLoad);
    nlohmann::json invalidMetrics = nlohmann::json::parse(serialized);
    invalidMetrics["challenges"][0]["latest_metrics"]["energy_drift"] = "not-a-number";
    assert(!restored.deserialize(invalidMetrics.dump()));
    assert(restored.serialize() == beforeInvalidLoad);
    invalidMetrics["challenges"][0]["latest_metrics"]["energy_drift"] = -1.0;
    assert(!restored.deserialize(invalidMetrics.dump()));
    assert(restored.serialize() == beforeInvalidLoad);
    progress.reset();
    assert(progress.report().completedLessons == 0 && progress.report().completedExperiments == 0);
    return 0;
}
