#pragma once

#include <optional>
#include <string>

#include "CurriculumTypes.hpp"

namespace bag {

// Inquiry pattern: Prediction → Experiment → Observation → Explanation → Assessment
enum class LearningLabStep {
    Selecting,
    Instruction,
    Predict,
    Experiment,
    Observe,
    Explain,
    Assess,
    Feedback,
    Complete,
};

struct LearningLabState {
    LearningLabStep step = LearningLabStep::Selecting;
    std::string activityId;
    int stepIndex = 0;
    std::string prediction;
    std::string observation;
    bool assessmentPassed = false;
    double assessmentScore = 0.0;
};

class LearningLabSession {
public:
    bool start(const CurriculumActivity& activity);
    bool advance();
    bool recordPrediction(std::string text);
    bool recordObservation(std::string text);
    bool completeAssessment(bool passed, double score);
    void reset();

    const LearningLabState& state() const { return current; }
    const CurriculumActivity* activity() const { return activeActivity; }
    static const char* stepName(LearningLabStep step);

private:
    const CurriculumActivity* activeActivity = nullptr;
    LearningLabState current;

    bool moveToConfiguredStep(int index);
};

} // namespace bag
