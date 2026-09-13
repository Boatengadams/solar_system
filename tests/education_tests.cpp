#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include <nlohmann/json.hpp>

#include "education/EducationChallenges.hpp"
#include "education/EducationContent.hpp"
#include "education/EducationProgress.hpp"
#include "education/ExperimentEvaluation.hpp"
#include "education/EducationWorkflow.hpp"
#include "education/EducationCatalog.hpp"
#include "education/LearnerReport.hpp"
#include "missions/Mission.hpp"

int main() {
    using namespace bag;
    assert(lessonCount() >= 9);
    assert(experimentCount() >= 6);
    const EducationCatalogValidation catalogValidation = validateEducationCatalog();
    assert(catalogValidation.valid && catalogValidation.errors.empty());
    assert(authoredLessonCount() == 9);
    assert(authoredLessonAt(0).id == "gravity");
    assert(authoredLessonAt(1).id == "escape-velocity");
    std::vector<AuthoredLesson> invalidCatalog = {authoredLessonAt(0), authoredLessonAt(1)};
    invalidCatalog[1].id = invalidCatalog[0].id;
    assert(!validateEducationCatalog(invalidCatalog).valid);
    invalidCatalog = {authoredLessonAt(0)};
    invalidCatalog[0].estimatedMinutes = 0;
    assert(!validateEducationCatalog(invalidCatalog).valid);
    invalidCatalog = {authoredLessonAt(0)};
    invalidCatalog[0].prerequisites = {"missing"};
    assert(!validateEducationCatalog(invalidCatalog).valid);
    invalidCatalog = {authoredLessonAt(0), authoredLessonAt(1)};
    invalidCatalog[0].prerequisites = {invalidCatalog[1].id};
    invalidCatalog[1].prerequisites = {invalidCatalog[0].id};
    assert(!validateEducationCatalog(invalidCatalog).valid);
    EducationProgress progress(lessonCount(), experimentCount());
    assert(progress.completeLesson(0));
    assert(progress.completeExperiment(0));
    assert(progress.recordObservation(1, "period increased after moving the orbit outward"));
    assert(!progress.completeLesson(-1));
    const EducationReport report = progress.report();
    assert(report.completedLessons == 1 && report.completedExperiments == 2);
    assert(report.completionRatio > 0.0 && report.observations.size() == 1);

    ExperimentObservation escapeObservation;
    escapeObservation.radiusM = PhysicsEngine::AU;
    Body sunDistance;
    sunDistance.position = {PhysicsEngine::AU, 0.0, 0.0};
    escapeObservation.measuredPrimaryValue = PhysicsEngine::escapeVelocity(sunDistance);
    const ExperimentEvaluation escapeEvaluation = evaluateExperiment("escape-velocity", escapeObservation);
    assert(escapeEvaluation.valid && escapeEvaluation.passed && escapeEvaluation.score == 100.0);
    const ExperimentEvaluation repeatEscapeEvaluation = evaluateExperiment("escape-velocity", escapeObservation);
    assert(repeatEscapeEvaluation.score == escapeEvaluation.score && repeatEscapeEvaluation.feedback == escapeEvaluation.feedback);
    escapeObservation.measuredPrimaryValue *= 1.10;
    assert(!evaluateExperiment("escape-velocity", escapeObservation).passed);
    escapeObservation.measuredPrimaryValue = std::numeric_limits<double>::quiet_NaN();
    assert(evaluateExperiment("escape-velocity", escapeObservation).status == ExperimentEvaluationStatus::InvalidInput);
    escapeObservation.radiusM = PhysicsEngine::MIN_PHYSICS_DISTANCE;
    escapeObservation.measuredPrimaryValue = 1.0;
    assert(evaluateExperiment("escape-velocity", escapeObservation).status == ExperimentEvaluationStatus::InvalidInput);

    ExperimentObservation keplerObservation;
    keplerObservation.radiusM = PhysicsEngine::AU;
    Body circularBody;
    circularBody.position = {keplerObservation.radiusM, 0.0, 0.0};
    circularBody.velocity = {0.0, PhysicsEngine::orbitalVelocity(circularBody), 0.0};
    keplerObservation.measuredPrimaryValue = PhysicsEngine::orbitalElements(circularBody).period;
    assert(evaluateExperiment("kepler-test", keplerObservation).passed);

    ExperimentObservation gravityObservation;
    gravityObservation.radiusM = PhysicsEngine::AU;
    gravityObservation.measuredPrimaryValue = PhysicsEngine::G * PhysicsEngine::SOLAR_MASS /
        (gravityObservation.radiusM * gravityObservation.radiusM);
    assert(evaluateExperiment("gravity-lab", gravityObservation).passed);

    ExperimentObservation energyObservation;
    energyObservation.measuredPrimaryValue = -1.0;
    assert(evaluateExperiment("orbit-energy", energyObservation).passed);
    energyObservation.measuredPrimaryValue = 0.0;
    assert(!evaluateExperiment("orbit-energy", energyObservation).passed);

    ExperimentObservation hohmannObservation;
    hohmannObservation.radiusM = PhysicsEngine::AU;
    hohmannObservation.targetRadiusM = 1.524 * PhysicsEngine::AU;
    const HohmannTransfer earthMars = PhysicsEngine::hohmannTransfer(
        hohmannObservation.radiusM, hohmannObservation.targetRadiusM, PhysicsEngine::SOLAR_MASS);
    hohmannObservation.measuredPrimaryValue = earthMars.totalDeltaV;
    const ExperimentEvaluation hohmannEvaluation = evaluateExperiment("hohmann-lab", hohmannObservation);
    assert(hohmannEvaluation.valid && hohmannEvaluation.passed && hohmannEvaluation.score == 100.0);
    assert(hohmannEvaluation.metrics.absolutePrimaryError == 0.0);
    hohmannObservation.measuredPrimaryValue *= 1.10;
    assert(!evaluateExperiment("hohmann-lab", hohmannObservation).passed);
    hohmannObservation.radiusM = hohmannObservation.targetRadiusM;
    assert(evaluateExperiment("hohmann-lab", hohmannObservation).status == ExperimentEvaluationStatus::InvalidInput);

    ExperimentObservation assistObservation;
    assistObservation.centralMassKg = 5.972e24;
    assistObservation.periapsisRadiusM = 7.0e6;
    assistObservation.incomingSpeedMps = 11000.0;
    const GravityAssistResult assistReference = gravityAssistTurn(
        PhysicsEngine::G * assistObservation.centralMassKg,
        assistObservation.periapsisRadiusM, assistObservation.incomingSpeedMps);
    assistObservation.measuredPrimaryValue = assistReference.turnAngleRadians;
    assert(evaluateExperiment("assist-lab", assistObservation).passed);

    std::vector<Body> benchmarkBodies(2);
    benchmarkBodies[0].mass = PhysicsEngine::SOLAR_MASS;
    benchmarkBodies[1].mass = 5.972e24;
    benchmarkBodies[1].position = {PhysicsEngine::AU, 0.0, 0.0};
    benchmarkBodies[1].velocity = {0.0, PhysicsEngine::orbitalVelocity(benchmarkBodies[1]), 0.0};
    IntegratorBenchmarkConfig benchmarkConfig;
    benchmarkConfig.timestep = 6.0 * 3600.0;
    benchmarkConfig.duration = 10.0 * PhysicsEngine::DAY;
    benchmarkConfig.referenceTimestep = 3.0 * 3600.0;
    ExperimentObservation numericalObservation;
    numericalObservation.integrator = Integrator::VelocityVerlet;
    numericalObservation.integratorMetrics = compareIntegrators(benchmarkBodies, benchmarkConfig);
    const ExperimentEvaluation numericalEvaluation = evaluateExperiment("numerical-methods", numericalObservation);
    assert(numericalEvaluation.valid && std::isfinite(numericalEvaluation.score));
    numericalObservation.integratorMetrics.clear();
    assert(evaluateExperiment("numerical-methods", numericalObservation).status == ExperimentEvaluationStatus::InsufficientData);
    numericalObservation.integratorMetrics = compareIntegrators(benchmarkBodies, benchmarkConfig);
    numericalObservation.integratorMetrics.front().positionError = std::numeric_limits<double>::infinity();
    assert(evaluateExperiment("numerical-methods", numericalObservation).status == ExperimentEvaluationStatus::InvalidInput);

    bool foundPredictionReference = false;
    for (int index = 0; index < experimentCount(); ++index) foundPredictionReference = foundPredictionReference || std::string(experimentAt(index).id) == "prediction-reference";
    assert(foundPredictionReference);
    ExperimentObservation predictionObservation;
    predictionObservation.comparisonAvailable = true;
    predictionObservation.comparisonPositionErrorM = 12.0;
    predictionObservation.comparisonVelocityErrorMps = 0.25;
    predictionObservation.comparisonRelativePositionError = 1.0e-9;
    predictionObservation.comparisonRelativeVelocityError = 2.0e-5;
    predictionObservation.comparisonRelativeEnergyDifference = 3.0e-6;
    const ExperimentEvaluation predictionEvaluation = evaluateExperiment("prediction-reference", predictionObservation);
    assert(predictionEvaluation.valid && predictionEvaluation.passed);
    assert(predictionEvaluation.mode == ExperimentEvaluationMode::NumericalComparison);
    assert(predictionEvaluation.metrics.positionErrorM == 12.0);
    assert(evaluateExperiment("prediction-reference", ExperimentObservation{}).status == ExperimentEvaluationStatus::InsufficientData);

    ExperimentObservation timestepObservation;
    timestepObservation.measuredPrimaryValue = 1.0;
    timestepObservation.measuredSecondaryValue = 0.5;
    assert(evaluateExperiment("timestep-sensitivity", timestepObservation).passed);
    timestepObservation.measuredSecondaryValue = 0.99;
    assert(!evaluateExperiment("timestep-sensitivity", timestepObservation).passed);
    timestepObservation.measuredSecondaryValue = std::numeric_limits<double>::quiet_NaN();
    assert(evaluateExperiment("timestep-sensitivity", timestepObservation).status == ExperimentEvaluationStatus::InvalidInput);

    ExperimentObservation conservationObservation;
    conservationObservation.energyDrift = 0.01;
    conservationObservation.angularMomentumDrift = 0.02;
    const ExperimentEvaluation conservationEvaluation = evaluateExperiment("conservation", conservationObservation);
    assert(conservationEvaluation.valid && conservationEvaluation.passed);
    conservationObservation.energyDrift = 0.10;
    assert(!evaluateExperiment("conservation", conservationObservation).passed);
    conservationObservation.energyDrift = -1.0;
    assert(evaluateExperiment("conservation", conservationObservation).status == ExperimentEvaluationStatus::InvalidInput);
    conservationObservation.energyDrift = 0.01;
    conservationObservation.angularMomentumDrift = std::numeric_limits<double>::quiet_NaN();
    assert(evaluateExperiment("conservation", conservationObservation).status == ExperimentEvaluationStatus::InsufficientData);
    const ExperimentEvaluation repeatEvaluation = evaluateExperiment("hohmann-lab", hohmannObservation);
    assert(repeatEvaluation.status == ExperimentEvaluationStatus::InvalidInput);

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
    assert(challengeProgress.recordExperimentEvaluation(0, escapeEvaluation));
    assert(challengeProgress.recordExperimentEvaluation(0, escapeEvaluation));
    assert(challengeProgress.experimentProgress(0)->attempts == 2);
    assert(challengeProgress.experimentProgress(0)->bestScore == 100.0);
    assert(challengeProgress.experimentComplete(0));
    assert(challengeProgress.recordChallengeResult(0, escapeResult));
    assert(challengeProgress.recordChallengeResult(0, failedEscape));
    assert(challengeProgress.recordChallengeResult(4, hohmannResult));
    assert(challengeProgress.challengeComplete(0));
    assert(challengeProgress.challengeProgress(0)->attempts == 2);
    assert(challengeProgress.challengeProgress(0)->bestScore == 100.0);
    assert(challengeProgress.exportText() == challengeProgress.exportText());
    assert(challengeProgress.report().completedChallenges == 2);

    EducationProgress emptyReportProgress(lessonCount(), experimentCount(), challengeCount());
    const LearnerReport emptyReport = buildLearnerReport(emptyReportProgress);
    assert(emptyReport.completedLessons == 0 && emptyReport.completedExperiments == 0 && emptyReport.completedChallenges == 0);
    assert(!emptyReport.hasAverageScore && emptyReport.recommendation.id == "gravity");
    assert(learnerActivityOutcomeName(LearnerActivityOutcome::NoAttempt) == std::string("NO_ATTEMPT"));
    assert(emptyReportProgress.completeLesson(0));
    assert(emptyReportProgress.recordExperimentEvaluation(0, escapeEvaluation));
    assert(emptyReportProgress.recordChallengeResult(0, failedEscape));
    const LearnerReport partialReport = buildLearnerReport(emptyReportProgress);
    assert(partialReport.completedLessons == 1 && partialReport.completedExperiments == 1);
    assert(partialReport.hasAverageScore && partialReport.averageScore == 50.0);
    assert(partialReport.recommendation.id == "circular-orbits");
    assert(!partialReport.areasNeedingImprovement.empty());
    const std::string reportProgressSerialized = emptyReportProgress.serialize();
    EducationProgress reportRestored(lessonCount(), experimentCount(), challengeCount());
    assert(reportRestored.deserialize(reportProgressSerialized));
    const LearnerReport restoredReport = buildLearnerReport(reportRestored);
    assert(restoredReport.recommendation.id == partialReport.recommendation.id);
    assert(restoredReport.averageScore == partialReport.averageScore);

    EducationProgress workflowProgress(lessonCount(), experimentCount(), challengeCount());
    EducationWorkflow workflow(workflowProgress, lessonCount(), experimentCount(), challengeCount());
    assert(workflow.state() == EducationWorkflowState::Selecting);
    assert(!workflow.start());
    assert(workflow.select(EducationActivityType::Experiment, 0));
    assert(workflow.state() == EducationWorkflowState::Ready);
    assert(workflow.activity().id == "escape-velocity");
    assert(!workflow.readyForEvaluation());
    assert(workflow.start() && workflow.beginObservation() && workflow.readyForEvaluation());
    assert(workflow.submitExperiment(escapeEvaluation));
    assert(workflow.state() == EducationWorkflowState::Evaluated);
    assert(workflowProgress.experimentProgress(0)->attempts == 1);
    assert(workflowProgress.experimentComplete(0));
    const EducationActivity nextExperiment = workflow.recommendedActivity();
    assert(nextExperiment.index == 1 && nextExperiment.type == EducationActivityType::Experiment);
    assert(workflow.retry() && workflow.state() == EducationWorkflowState::Ready);
    ExperimentEvaluation failedEvaluation = escapeEvaluation;
    failedEvaluation.score = 0.0;
    failedEvaluation.passed = false;
    failedEvaluation.grade = "RETRY";
    assert(workflow.start() && workflow.beginObservation() && workflow.readyForEvaluation());
    assert(workflow.submitExperiment(failedEvaluation));
    assert(workflowProgress.experimentProgress(0)->attempts == 2);
    assert(workflowProgress.experimentProgress(0)->bestScore == 100.0);
    assert(workflowProgress.experimentProgress(0)->latestScore == 0.0);
    assert(workflowProgress.experimentComplete(0));
    assert(workflow.continueToRecommended());
    assert(workflow.activity().index == 1 && workflow.state() == EducationWorkflowState::Ready);
    assert(!workflow.continueToRecommended());
    assert(workflow.start() && workflow.beginObservation() && workflow.readyForEvaluation());
    ExperimentEvaluation insufficientEvaluation;
    insufficientEvaluation.experimentId = "kepler-test";
    insufficientEvaluation.status = ExperimentEvaluationStatus::InsufficientData;
    assert(workflow.submitExperiment(insufficientEvaluation));
    assert(workflowProgress.experimentProgress(1)->attempts == 0);

    EducationWorkflow challengeWorkflow(workflowProgress, lessonCount(), experimentCount(), challengeCount());
    assert(challengeWorkflow.select(EducationActivityType::Challenge, 0));
    assert(challengeWorkflow.start() && challengeWorkflow.beginObservation() && challengeWorkflow.readyForEvaluation());
    assert(challengeWorkflow.submitChallenge(failedEscape));
    assert(workflowProgress.challengeProgress(0)->attempts == 1);
    assert(challengeWorkflow.retry());
    assert(challengeWorkflow.activity().id == "escape-velocity");
    assert(challengeWorkflow.start() && challengeWorkflow.beginObservation() && challengeWorkflow.readyForEvaluation());
    ChallengeResult invalidWorkflowChallenge;
    assert(challengeWorkflow.submitChallenge(invalidWorkflowChallenge));
    assert(workflowProgress.challengeProgress(0)->attempts == 1);

    EducationWorkflow lessonWorkflow(workflowProgress, lessonCount(), experimentCount(), challengeCount());
    assert(lessonWorkflow.select(EducationActivityType::Lesson, 0));
    assert(lessonWorkflow.start() && lessonWorkflow.beginObservation());
    assert(lessonWorkflow.completeLesson());
    assert(workflowProgress.lessonComplete(0));
    const std::vector<EducationActivityProgress> home = lessonWorkflow.home();
    assert(home.size() == static_cast<std::size_t>(lessonCount() + experimentCount() + challengeCount()));
    assert(home.front().completed);

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
    assert(restored.experimentProgress(0)->attempts == 2);
    assert(restored.experimentProgress(0)->completed);
    assert(restored.report().observations.size() == 1);

    nlohmann::json legacySchema = nlohmann::json::parse(serialized);
    legacySchema.erase("experiment_evaluations");
    EducationProgress legacyRestored(lessonCount(), experimentCount(), challengeCount());
    assert(legacyRestored.deserialize(legacySchema.dump()));
    assert(legacyRestored.experimentProgress(0)->attempts == 0);

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
    nlohmann::json invalidExperiment = nlohmann::json::parse(serialized);
    invalidExperiment["experiment_evaluations"][0]["latest_metrics"]["energy_drift"] = "not-a-number";
    assert(!restored.deserialize(invalidExperiment.dump()));
    assert(restored.serialize() == beforeInvalidLoad);
    progress.reset();
    assert(progress.report().completedLessons == 0 && progress.report().completedExperiments == 0);
    return 0;
}
