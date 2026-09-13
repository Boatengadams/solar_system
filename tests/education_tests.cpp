#include <cassert>
#include <cmath>

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

    assert(challengeCount() == 4);
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

    EducationProgress challengeProgress(lessonCount(), experimentCount(), challengeCount());
    assert(challengeProgress.recordChallengeResult(0, escapeResult));
    assert(challengeProgress.recordChallengeResult(0, failedEscape));
    assert(challengeProgress.challengeComplete(0));
    assert(challengeProgress.challengeProgress(0)->attempts == 2);
    assert(challengeProgress.challengeProgress(0)->bestScore == 100.0);
    assert(challengeProgress.exportText() == challengeProgress.exportText());
    assert(challengeProgress.report().completedChallenges == 1);
    progress.reset();
    assert(progress.report().completedLessons == 0 && progress.report().completedExperiments == 0);
    return 0;
}
