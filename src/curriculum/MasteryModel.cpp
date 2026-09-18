#include "MasteryModel.hpp"

#include <algorithm>

namespace bag {

MasteryModel::MasteryModel(MasteryWeights weights, double masteryThreshold)
    : configuredWeights(weights), masteryThreshold(masteryThreshold) {
    if (!configuredWeights.valid()) {
        configuredWeights = MasteryWeights{};
    }
}

TopicMastery MasteryModel::evaluate(const TopicMasterySignals& signals) const {
    TopicMastery mastery;
    mastery.topicId = signals.topicId;
    mastery.score =
        configuredWeights.conceptUnderstanding * signals.conceptUnderstanding +
        configuredWeights.activityPerformance * signals.activityPerformance +
        configuredWeights.assessmentPerformance * signals.assessmentPerformance +
        configuredWeights.experimentPerformance * signals.experimentPerformance;

    // Repeated successful misconception corrections boost concept understanding signal gently.
    if (signals.misconceptionsCorrected > 0) {
        mastery.score = std::min(100.0, mastery.score + std::min(5.0, signals.misconceptionsCorrected * 1.5));
    }

    mastery.mastered = mastery.score >= masteryThreshold && signals.attempts > 0;
    return mastery;
}

std::vector<TopicMastery> MasteryModel::evaluateAll(const std::vector<TopicMasterySignals>& signals) const {
    std::vector<TopicMastery> results;
    results.reserve(signals.size());
    for (const auto& signal : signals) results.push_back(evaluate(signal));
    return results;
}

double MasteryModel::overallScore(const std::vector<TopicMastery>& topics) const {
    if (topics.empty()) return 0.0;
    double total = 0.0;
    for (const auto& topic : topics) total += topic.score;
    return total / static_cast<double>(topics.size());
}

} // namespace bag
