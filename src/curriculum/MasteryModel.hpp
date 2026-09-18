#pragma once

#include <string>
#include <vector>

namespace bag {

struct MasteryWeights {
    double conceptUnderstanding = 0.25;
    double activityPerformance = 0.25;
    double assessmentPerformance = 0.30;
    double experimentPerformance = 0.20;

    bool valid() const {
        const double sum = conceptUnderstanding + activityPerformance + assessmentPerformance + experimentPerformance;
        return sum > 0.999 && sum < 1.001;
    }
};

struct TopicMasterySignals {
    std::string topicId;
    double conceptUnderstanding = 0.0; // 0–100
    double activityPerformance = 0.0;
    double assessmentPerformance = 0.0;
    double experimentPerformance = 0.0;
    int attempts = 0;
    int misconceptionsCorrected = 0;
};

struct TopicMastery {
    std::string topicId;
    double score = 0.0; // 0–100
    bool mastered = false;
};

class MasteryModel {
public:
    explicit MasteryModel(MasteryWeights weights = {}, double masteryThreshold = 80.0);

    TopicMastery evaluate(const TopicMasterySignals& signals) const;
    std::vector<TopicMastery> evaluateAll(const std::vector<TopicMasterySignals>& signals) const;
    double overallScore(const std::vector<TopicMastery>& topics) const;
    const MasteryWeights& weights() const { return configuredWeights; }
    double threshold() const { return masteryThreshold; }

private:
    MasteryWeights configuredWeights;
    double masteryThreshold;
};

} // namespace bag
