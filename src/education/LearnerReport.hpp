#pragma once

#include <string>
#include <vector>

#include "EducationWorkflow.hpp"

namespace bag {

enum class LearnerActivityOutcome {
    NoAttempt,
    Attempted,
    Completed,
    Passed,
    Failed,
};

struct LearnerActivityReport {
    EducationActivityType type = EducationActivityType::Lesson;
    int index = 0;
    std::string id;
    std::string title;
    LearnerActivityOutcome outcome = LearnerActivityOutcome::NoAttempt;
    int attempts = 0;
    double bestScore = 0.0;
    double latestScore = 0.0;
};

struct LearnerRecommendation {
    bool available = false;
    EducationActivityType type = EducationActivityType::Lesson;
    int index = 0;
    std::string id;
    std::string title;
    std::string reason;
};

struct LearnerReport {
    int totalLessons = 0;
    int completedLessons = 0;
    int totalExperiments = 0;
    int completedExperiments = 0;
    int totalChallenges = 0;
    int completedChallenges = 0;
    int totalAttempts = 0;
    int scoredActivities = 0;
    bool hasAverageScore = false;
    double averageScore = 0.0;
    std::vector<LearnerActivityReport> activities;
    std::vector<std::string> strongestAreas;
    std::vector<std::string> areasNeedingImprovement;
    LearnerRecommendation recommendation;
};

LearnerReport buildLearnerReport(const EducationProgress& progress);
const char* learnerActivityOutcomeName(LearnerActivityOutcome outcome);

} // namespace bag
