#include "LearningLabSession.hpp"

namespace bag {
namespace {

LearningLabStep mapStepType(ActivityStepType type) {
    switch (type) {
    case ActivityStepType::Instruction: return LearningLabStep::Instruction;
    case ActivityStepType::Predict: return LearningLabStep::Predict;
    case ActivityStepType::Explore:
    case ActivityStepType::Experiment: return LearningLabStep::Experiment;
    case ActivityStepType::Observe: return LearningLabStep::Observe;
    case ActivityStepType::Explain: return LearningLabStep::Explain;
    case ActivityStepType::Question:
    case ActivityStepType::Assessment: return LearningLabStep::Assess;
    case ActivityStepType::Feedback: return LearningLabStep::Feedback;
    }
    return LearningLabStep::Instruction;
}

} // namespace

bool LearningLabSession::start(const CurriculumActivity& activity) {
    if (activity.steps.empty()) return false;
    activeActivity = &activity;
    current = LearningLabState{};
    current.activityId = activity.id;
    return moveToConfiguredStep(0);
}

bool LearningLabSession::moveToConfiguredStep(int index) {
    if (!activeActivity || index < 0 || index >= static_cast<int>(activeActivity->steps.size())) {
        current.step = LearningLabStep::Complete;
        return false;
    }
    current.stepIndex = index;
    current.step = mapStepType(activeActivity->steps[static_cast<std::size_t>(index)].type);
    return true;
}

bool LearningLabSession::advance() {
    if (!activeActivity) return false;
    if (current.step == LearningLabStep::Complete) return false;
    if (current.step == LearningLabStep::Predict && current.prediction.empty()) return false;
    if (current.step == LearningLabStep::Observe && current.observation.empty()) return false;
    return moveToConfiguredStep(current.stepIndex + 1);
}

bool LearningLabSession::recordPrediction(std::string text) {
    if (current.step != LearningLabStep::Predict) return false;
    current.prediction = std::move(text);
    return !current.prediction.empty();
}

bool LearningLabSession::recordObservation(std::string text) {
    if (current.step != LearningLabStep::Observe) return false;
    current.observation = std::move(text);
    return !current.observation.empty();
}

bool LearningLabSession::completeAssessment(bool passed, double score) {
    if (current.step != LearningLabStep::Assess) return false;
    current.assessmentPassed = passed;
    current.assessmentScore = score;
    if (activeActivity) {
        for (int index = current.stepIndex + 1; index < static_cast<int>(activeActivity->steps.size()); ++index) {
            if (activeActivity->steps[static_cast<std::size_t>(index)].type == ActivityStepType::Feedback) {
                return moveToConfiguredStep(index);
            }
        }
    }
    current.step = LearningLabStep::Feedback;
    return true;
}

void LearningLabSession::reset() {
    activeActivity = nullptr;
    current = LearningLabState{};
}

const char* LearningLabSession::stepName(LearningLabStep step) {
    switch (step) {
    case LearningLabStep::Selecting: return "selecting";
    case LearningLabStep::Instruction: return "instruction";
    case LearningLabStep::Predict: return "predict";
    case LearningLabStep::Experiment: return "experiment";
    case LearningLabStep::Observe: return "observe";
    case LearningLabStep::Explain: return "explain";
    case LearningLabStep::Assess: return "assess";
    case LearningLabStep::Feedback: return "feedback";
    case LearningLabStep::Complete: return "complete";
    }
    return "unknown";
}

} // namespace bag
