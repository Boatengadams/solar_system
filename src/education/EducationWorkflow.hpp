#pragma once

#include <optional>
#include <string>
#include <vector>

#include "EducationContent.hpp"
#include "EducationProgress.hpp"

namespace bag {

enum class EducationActivityType {
    Lesson,
    Experiment,
    Challenge,
};

enum class EducationWorkflowState {
    Selecting,
    Ready,
    Running,
    Observing,
    ReadyForEvaluation,
    Evaluated,
};

struct EducationActivity {
    EducationActivityType type = EducationActivityType::Lesson;
    int index = 0;
    std::string id;
    std::string title;
    std::string objective;
    std::string explanation;
    std::string instructions;
    std::string procedure;
    std::string expectedObservation;
    std::string evaluationMetric;
    std::string nextStep;
};

struct EducationActivityProgress {
    EducationActivity activity;
    bool completed = false;
    int attempts = 0;
    double bestScore = 0.0;
    double latestScore = 0.0;
};

class EducationWorkflow {
public:
    explicit EducationWorkflow(EducationProgress& progress,
                               int lessonCount,
                               int experimentCount,
                               int challengeCount);

    EducationWorkflowState state() const { return workflowState; }
    const EducationActivity& activity() const { return selected; }
    EducationActivity recommendedActivity() const;
    std::vector<EducationActivityProgress> home() const;

    bool select(EducationActivityType type, int index);
    bool start();
    bool beginObservation();
    bool readyForEvaluation();
    bool submitExperiment(const ExperimentEvaluation& result);
    bool submitChallenge(const ChallengeResult& result);
    bool completeLesson();
    bool retry();
    bool continueToRecommended();
    void resetToSelection();

    const std::optional<ExperimentEvaluation>& lastExperiment() const { return experimentResult; }
    const std::optional<ChallengeResult>& lastChallenge() const { return challengeResult; }

    static const char* stateName(EducationWorkflowState state);
    static const char* activityTypeName(EducationActivityType type);

private:
    EducationProgress& progress;
    int lessonTotal;
    int experimentTotal;
    int challengeTotal;
    EducationActivity selected;
    EducationWorkflowState workflowState = EducationWorkflowState::Selecting;
    std::optional<ExperimentEvaluation> experimentResult;
    std::optional<ChallengeResult> challengeResult;

    EducationActivity describe(EducationActivityType type, int index) const;
    int count(EducationActivityType type) const;
    bool selectedIndexValid() const;
};

} // namespace bag
