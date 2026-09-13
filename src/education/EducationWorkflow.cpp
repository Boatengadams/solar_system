#include "EducationWorkflow.hpp"

#include <algorithm>
#include <utility>

#include "EducationChallenges.hpp"

namespace bag {
namespace {

int wrapIndex(int index, int count) {
    if (count <= 0) return 0;
    return ((index % count) + count) % count;
}

const char* typeName(EducationActivityType type) {
    switch (type) {
    case EducationActivityType::Lesson: return "LESSON";
    case EducationActivityType::Experiment: return "EXPERIMENT";
    case EducationActivityType::Challenge: return "CHALLENGE";
    }
    return "ACTIVITY";
}

} // namespace

EducationWorkflow::EducationWorkflow(EducationProgress& progressValue,
                                     int lessonCountValue,
                                     int experimentCountValue,
                                     int challengeCountValue)
    : progress(progressValue), lessonTotal(std::max(0, lessonCountValue)),
      experimentTotal(std::max(0, experimentCountValue)),
      challengeTotal(std::max(0, challengeCountValue)) {}

int EducationWorkflow::count(EducationActivityType type) const {
    switch (type) {
    case EducationActivityType::Lesson: return lessonTotal;
    case EducationActivityType::Experiment: return experimentTotal;
    case EducationActivityType::Challenge: return challengeTotal;
    }
    return 0;
}

EducationActivity EducationWorkflow::describe(EducationActivityType type, int index) const {
    EducationActivity result;
    result.type = type;
    result.index = wrapIndex(index, count(type));
    if (type == EducationActivityType::Lesson && lessonTotal > 0) {
        const Lesson& lesson = lessonAt(result.index);
        result.title = lesson.title;
        result.objective = "Build a physical intuition for " + std::string(lesson.title) + ".";
        result.explanation = lesson.body;
        result.instructions = "Read the objective, then use the simulation view to inspect the stated concept.";
        result.procedure = "Start the activity, observe the model, and record what changes when the relevant state changes.";
        result.expectedObservation = "The observed trend should agree with the lesson explanation within the Newtonian model.";
        result.evaluationMetric = "Learner completion and optional observation; no fabricated numerical score.";
        result.nextStep = "Continue to the next lesson or select a related experiment.";
    } else if (type == EducationActivityType::Experiment && experimentTotal > 0) {
        const Experiment& experiment = experimentAt(result.index);
        result.id = experiment.id;
        result.title = experiment.title;
        result.objective = experiment.prompt;
        result.explanation = "Use the existing simulation state and the documented " + std::string(experiment.equation) + " relationship.";
        result.instructions = "Select a suitable body or scenario, start the activity, and observe the relevant metric.";
        result.procedure = "Run the simulation long enough to obtain the required observation, then mark it ready and evaluate.";
        result.expectedObservation = "The result is model-dependent and is compared with the experiment's analytical, bounded, or numerical reference.";
        result.evaluationMetric = "The existing ExperimentEvaluation result: measured value, reference/error, stability metric, and score where defined.";
        result.nextStep = "Inspect the explanation, retry with a changed parameter, or continue to the next experiment.";
    } else if (type == EducationActivityType::Challenge && challengeTotal > 0) {
        const ChallengeDefinition& challenge = challengeAt(result.index);
        result.id = challenge.id;
        result.title = challenge.title;
        result.objective = challenge.learningObjective;
        result.explanation = challenge.explanation;
        result.instructions = challenge.description;
        result.procedure = challenge.scenario;
        result.expectedObservation = "The selected answer or method should remain within the challenge's documented scoring envelope.";
        result.evaluationMetric = "The existing ChallengeResult score, grade, pass/fail state, and scientific metrics.";
        result.nextStep = challenge.nextStep;
    }
    return result;
}

bool EducationWorkflow::selectedIndexValid() const {
    return selected.index >= 0 && selected.index < count(selected.type);
}

bool EducationWorkflow::select(EducationActivityType type, int index) {
    if (workflowState == EducationWorkflowState::Running ||
        workflowState == EducationWorkflowState::Observing ||
        workflowState == EducationWorkflowState::ReadyForEvaluation || count(type) == 0) return false;
    selected = describe(type, index);
    workflowState = EducationWorkflowState::Ready;
    experimentResult.reset();
    challengeResult.reset();
    return true;
}

bool EducationWorkflow::start() {
    if (workflowState != EducationWorkflowState::Ready || !selectedIndexValid()) return false;
    workflowState = EducationWorkflowState::Running;
    experimentResult.reset();
    challengeResult.reset();
    return true;
}

bool EducationWorkflow::beginObservation() {
    if (workflowState != EducationWorkflowState::Running) return false;
    workflowState = EducationWorkflowState::Observing;
    return true;
}

bool EducationWorkflow::readyForEvaluation() {
    if (workflowState != EducationWorkflowState::Observing) return false;
    workflowState = EducationWorkflowState::ReadyForEvaluation;
    return true;
}

bool EducationWorkflow::submitExperiment(const ExperimentEvaluation& result) {
    if (workflowState != EducationWorkflowState::ReadyForEvaluation ||
        selected.type != EducationActivityType::Experiment || result.experimentId != selected.id) return false;
    experimentResult = result;
    if (experimentEvaluationIsPersistable(result)) progress.recordExperimentEvaluation(selected.index, result);
    workflowState = EducationWorkflowState::Evaluated;
    return true;
}

bool EducationWorkflow::submitChallenge(const ChallengeResult& result) {
    if (workflowState != EducationWorkflowState::ReadyForEvaluation ||
        selected.type != EducationActivityType::Challenge) return false;
    challengeResult = result;
    if (result.valid) progress.recordChallengeResult(selected.index, result);
    workflowState = EducationWorkflowState::Evaluated;
    return true;
}

bool EducationWorkflow::completeLesson() {
    if (workflowState != EducationWorkflowState::Observing || selected.type != EducationActivityType::Lesson) return false;
    if (!progress.completeLesson(selected.index)) return false;
    workflowState = EducationWorkflowState::Evaluated;
    return true;
}

EducationActivity EducationWorkflow::recommendedActivity() const {
    return describe(selected.type, selected.index + 1);
}

bool EducationWorkflow::retry() {
    if (workflowState != EducationWorkflowState::Evaluated) return false;
    selected = describe(selected.type, selected.index);
    workflowState = EducationWorkflowState::Ready;
    experimentResult.reset();
    challengeResult.reset();
    return true;
}

bool EducationWorkflow::continueToRecommended() {
    if (workflowState != EducationWorkflowState::Evaluated) return false;
    const EducationActivity next = recommendedActivity();
    return select(next.type, next.index);
}

void EducationWorkflow::resetToSelection() {
    workflowState = EducationWorkflowState::Selecting;
    experimentResult.reset();
    challengeResult.reset();
}

std::vector<EducationActivityProgress> EducationWorkflow::home() const {
    std::vector<EducationActivityProgress> result;
    for (int type = 0; type < 3; ++type) {
        const EducationActivityType activityType = static_cast<EducationActivityType>(type);
        for (int index = 0; index < count(activityType); ++index) {
            EducationActivityProgress item;
            item.activity = describe(activityType, index);
            if (activityType == EducationActivityType::Lesson) {
                item.completed = progress.lessonComplete(index);
            } else if (activityType == EducationActivityType::Experiment) {
                const ExperimentProgress* stored = progress.experimentProgress(index);
                if (stored) {
                    item.completed = stored->completed;
                    item.attempts = stored->attempts;
                    item.bestScore = stored->bestScore;
                    item.latestScore = stored->latestScore;
                }
            } else {
                const ChallengeProgress* stored = progress.challengeProgress(index);
                if (stored) {
                    item.completed = stored->completed;
                    item.attempts = stored->attempts;
                    item.bestScore = stored->bestScore;
                    item.latestScore = stored->latestScore;
                }
            }
            result.push_back(std::move(item));
        }
    }
    return result;
}

const char* EducationWorkflow::stateName(EducationWorkflowState state) {
    switch (state) {
    case EducationWorkflowState::Selecting: return "SELECTING";
    case EducationWorkflowState::Ready: return "READY";
    case EducationWorkflowState::Running: return "RUNNING";
    case EducationWorkflowState::Observing: return "OBSERVING";
    case EducationWorkflowState::ReadyForEvaluation: return "READY_FOR_EVALUATION";
    case EducationWorkflowState::Evaluated: return "EVALUATED";
    }
    return "UNKNOWN";
}

const char* EducationWorkflow::activityTypeName(EducationActivityType type) { return typeName(type); }

} // namespace bag
