#pragma once

#include <string>
#include <vector>

#include "EducationChallenges.hpp"

namespace bag {

struct EducationReport {
    int completedLessons = 0;
    int totalLessons = 0;
    int completedExperiments = 0;
    int totalExperiments = 0;
    int completedChallenges = 0;
    int totalChallenges = 0;
    double completionRatio = 0.0;
    std::vector<std::string> observations;
};

struct ChallengeProgress {
    int attempts = 0;
    bool completed = false;
    double bestScore = 0.0;
    double latestScore = 0.0;
    ChallengeMetrics latestMetrics;
};

class EducationProgress {
public:
    EducationProgress(int lessonCount, int experimentCount, int challengeCount = 0);

    bool completeLesson(int index);
    bool completeExperiment(int index);
    bool recordObservation(int experimentIndex, std::string observation);
    bool recordChallengeResult(int challengeIndex, const ChallengeResult& result);
    bool lessonComplete(int index) const;
    bool experimentComplete(int index) const;
    bool challengeComplete(int index) const;
    const ChallengeProgress* challengeProgress(int index) const;
    void reset();
    EducationReport report() const;
    std::string exportText() const;

private:
    std::vector<bool> lessons;
    std::vector<bool> experiments;
    std::vector<std::string> experimentObservations;
    std::vector<ChallengeProgress> challenges;
};

} // namespace bag
