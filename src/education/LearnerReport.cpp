#include "LearnerReport.hpp"

#include <algorithm>
#include <array>

#include "EducationCatalog.hpp"
#include "EducationChallenges.hpp"
#include "EducationContent.hpp"

namespace bag {
namespace {

constexpr double WEAK_SCORE_THRESHOLD = 75.0;
constexpr std::size_t MAX_AREAS = 3;

LearnerActivityReport lessonReport(const EducationProgress& progress, int index) {
    const AuthoredLesson& lesson = authoredLessonAt(index);
    LearnerActivityReport result;
    result.type = EducationActivityType::Lesson;
    result.index = index;
    result.id = lesson.id;
    result.title = lesson.title;
    result.outcome = progress.lessonComplete(index) ? LearnerActivityOutcome::Completed : LearnerActivityOutcome::NoAttempt;
    return result;
}

LearnerActivityReport experimentReport(const EducationProgress& progress, int index) {
    const Experiment& experiment = experimentAt(index);
    const ExperimentProgress* stored = progress.experimentProgress(index);
    LearnerActivityReport result;
    result.type = EducationActivityType::Experiment;
    result.index = index;
    result.id = experiment.id;
    result.title = experiment.title;
    if (stored) {
        result.attempts = stored->attempts;
        result.bestScore = stored->bestScore;
        result.latestScore = stored->latestScore;
        result.outcome = stored->attempts == 0 ? LearnerActivityOutcome::NoAttempt :
            (stored->completed ? LearnerActivityOutcome::Passed : LearnerActivityOutcome::Failed);
    }
    return result;
}

LearnerActivityReport challengeReport(const EducationProgress& progress, int index) {
    const ChallengeDefinition& challenge = challengeAt(index);
    const ChallengeProgress* stored = progress.challengeProgress(index);
    LearnerActivityReport result;
    result.type = EducationActivityType::Challenge;
    result.index = index;
    result.id = challenge.id;
    result.title = challenge.title;
    if (stored) {
        result.attempts = stored->attempts;
        result.bestScore = stored->bestScore;
        result.latestScore = stored->latestScore;
        result.outcome = stored->attempts == 0 ? LearnerActivityOutcome::NoAttempt :
            (stored->completed ? LearnerActivityOutcome::Passed : LearnerActivityOutcome::Failed);
    }
    return result;
}

bool weak(const LearnerActivityReport& activity) {
    return activity.attempts > 0 && (activity.outcome == LearnerActivityOutcome::Failed || activity.latestScore < WEAK_SCORE_THRESHOLD);
}

LearnerRecommendation makeRecommendation(const LearnerReport& report) {
    const auto lessonIndex = [](const std::string& id) {
        for (int index = 0; index < authoredLessonCount(); ++index) if (authoredLessonAt(index).id == id) return index;
        return -1;
    };
    const auto isComplete = [&](EducationActivityType type, int index) {
        for (const LearnerActivityReport& activity : report.activities) if (activity.type == type && activity.index == index) return activity.outcome == LearnerActivityOutcome::Completed || activity.outcome == LearnerActivityOutcome::Passed;
        return false;
    };
    for (int index = 0; index < authoredLessonCount(); ++index) {
        const AuthoredLesson& lesson = authoredLessonAt(index);
        if (isComplete(EducationActivityType::Lesson, index)) continue;
        for (const std::string& prerequisite : lesson.prerequisites) {
            const int prerequisiteIndex = lessonIndex(prerequisite);
            if (prerequisiteIndex >= 0 && !isComplete(EducationActivityType::Lesson, prerequisiteIndex)) {
                return {true, EducationActivityType::Lesson, prerequisiteIndex, authoredLessonAt(prerequisiteIndex).id,
                        authoredLessonAt(prerequisiteIndex).title, "Complete the prerequisite before starting " + lesson.title + "."};
            }
        }
    }
    for (int index = 0; index < authoredLessonCount(); ++index) if (!isComplete(EducationActivityType::Lesson, index)) {
        return {true, EducationActivityType::Lesson, index, authoredLessonAt(index).id, authoredLessonAt(index).title, "This lesson is the next incomplete concept."};
    }
    for (const LearnerActivityReport& activity : report.activities) if ((activity.type == EducationActivityType::Experiment || activity.type == EducationActivityType::Challenge) && weak(activity)) {
        return {true, activity.type, activity.index, activity.id, activity.title, "Review the feedback and retry this weak result."};
    }
    for (const LearnerActivityReport& activity : report.activities) if ((activity.type == EducationActivityType::Experiment || activity.type == EducationActivityType::Challenge) && activity.outcome == LearnerActivityOutcome::NoAttempt) {
        return {true, activity.type, activity.index, activity.id, activity.title, "This activity follows the completed lesson sequence."};
    }
    return {true, EducationActivityType::Lesson, 0, authoredLessonAt(0).id, authoredLessonAt(0).title, "All activities are complete; revisit the catalog to reinforce the model."};
}

} // namespace

LearnerReport buildLearnerReport(const EducationProgress& progress) {
    LearnerReport result;
    result.totalLessons = authoredLessonCount();
    result.totalExperiments = experimentCount();
    result.totalChallenges = challengeCount();
    for (int index = 0; index < result.totalLessons; ++index) result.activities.push_back(lessonReport(progress, index));
    for (int index = 0; index < result.totalExperiments; ++index) result.activities.push_back(experimentReport(progress, index));
    for (int index = 0; index < result.totalChallenges; ++index) result.activities.push_back(challengeReport(progress, index));
    for (const LearnerActivityReport& activity : result.activities) {
        const bool complete = activity.outcome == LearnerActivityOutcome::Completed || activity.outcome == LearnerActivityOutcome::Passed;
        if (activity.type == EducationActivityType::Lesson) result.completedLessons += complete ? 1 : 0;
        else if (activity.type == EducationActivityType::Experiment) result.completedExperiments += complete ? 1 : 0;
        else result.completedChallenges += complete ? 1 : 0;
        result.totalAttempts += activity.attempts;
        if (activity.attempts > 0) {
            result.scoredActivities++;
            result.averageScore += activity.latestScore;
        }
    }
    if (result.scoredActivities > 0) {
        result.averageScore /= static_cast<double>(result.scoredActivities);
        result.hasAverageScore = true;
    }
    std::vector<LearnerActivityReport> scored;
    for (const LearnerActivityReport& activity : result.activities) if (activity.attempts > 0) scored.push_back(activity);
    std::stable_sort(scored.begin(), scored.end(), [](const LearnerActivityReport& left, const LearnerActivityReport& right) { return left.bestScore > right.bestScore; });
    for (std::size_t index = 0; index < std::min(MAX_AREAS, scored.size()); ++index) result.strongestAreas.push_back(scored[index].title);
    for (const LearnerActivityReport& activity : result.activities) if (weak(activity) && result.areasNeedingImprovement.size() < MAX_AREAS) result.areasNeedingImprovement.push_back(activity.title);
    result.recommendation = makeRecommendation(result);
    return result;
}

const char* learnerActivityOutcomeName(LearnerActivityOutcome outcome) {
    switch (outcome) {
    case LearnerActivityOutcome::NoAttempt: return "NO_ATTEMPT";
    case LearnerActivityOutcome::Attempted: return "ATTEMPTED";
    case LearnerActivityOutcome::Completed: return "COMPLETED";
    case LearnerActivityOutcome::Passed: return "PASSED";
    case LearnerActivityOutcome::Failed: return "FAILED";
    }
    return "UNKNOWN";
}

} // namespace bag
