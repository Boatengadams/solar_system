#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CurriculumTypes.hpp"

namespace bag {

struct AssessmentSubmission {
    std::string questionId;
    std::vector<std::string> selectedOptionIds;
    std::string shortAnswer;
    std::optional<double> numericalValue;
};

struct AssessmentResult {
    std::string questionId;
    bool correct = false;
    double score = 0.0;
    std::string explanationKey;
    std::string feedbackKey;
    std::string triggeredMisconceptionId;
};

class AssessmentEngine {
public:
    static AssessmentResult score(const CurriculumQuestion& question, const AssessmentSubmission& submission);
    static double scoreBatch(const std::vector<CurriculumQuestion>& questions,
                             const std::vector<AssessmentSubmission>& submissions,
                             std::vector<AssessmentResult>& results);
};

} // namespace bag
