#include "AssessmentEngine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace bag {
namespace {

std::vector<std::string> sortedUnique(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

bool sameAnswerSet(std::vector<std::string> left, std::vector<std::string> right) {
    return sortedUnique(std::move(left)) == sortedUnique(std::move(right));
}

} // namespace

AssessmentResult AssessmentEngine::score(const CurriculumQuestion& question, const AssessmentSubmission& submission) {
    AssessmentResult result;
    result.questionId = question.id;
    result.explanationKey = question.explanationKey;

    if (submission.questionId != question.id) {
        result.feedbackKey = "assessment.feedback.question_mismatch";
        return result;
    }

    switch (question.questionType) {
    case QuestionType::MultipleChoice:
    case QuestionType::MultipleSelect:
    case QuestionType::DragDrop:
    case QuestionType::Ordering:
    case QuestionType::Matching:
    case QuestionType::Labeling:
    case QuestionType::Prediction:
    case QuestionType::Observation:
    case QuestionType::ExperimentBased:
    case QuestionType::Scenario: {
        result.correct = sameAnswerSet(submission.selectedOptionIds, question.correctAnswerIds);
        break;
    }
    case QuestionType::Numerical: {
        if (!question.numericalAnswer || !submission.numericalValue) {
            result.feedbackKey = "assessment.feedback.missing_numerical";
            return result;
        }
        const double expected = *question.numericalAnswer;
        const double actual = *submission.numericalValue;
        if (!std::isfinite(expected) || !std::isfinite(actual)) {
            result.feedbackKey = "assessment.feedback.invalid_numerical";
            return result;
        }
        const double denom = std::max(std::abs(expected), 1.0e-12);
        result.correct = std::abs(actual - expected) / denom <= question.numericalTolerance;
        break;
    }
    case QuestionType::ShortAnswer: {
        std::string answer = submission.shortAnswer;
        std::transform(answer.begin(), answer.end(), answer.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        for (const auto& expectedId : question.correctAnswerIds) {
            std::string expected = expectedId;
            std::transform(expected.begin(), expected.end(), expected.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (answer == expected) {
                result.correct = true;
                break;
            }
        }
        break;
    }
    }

    result.score = result.correct ? 100.0 : 0.0;
    result.feedbackKey = result.correct ? "assessment.feedback.correct" : "assessment.feedback.incorrect";
    if (!result.correct && !question.misconceptionId.empty()) {
        for (const auto& selected : submission.selectedOptionIds) {
            // Trigger metadata is resolved by MisconceptionEngine using option ids.
            (void)selected;
        }
        result.triggeredMisconceptionId = question.misconceptionId;
    }
    return result;
}

double AssessmentEngine::scoreBatch(const std::vector<CurriculumQuestion>& questions,
                                    const std::vector<AssessmentSubmission>& submissions,
                                    std::vector<AssessmentResult>& results) {
    results.clear();
    if (questions.empty()) return 0.0;

    std::unordered_map<std::string, const AssessmentSubmission*> byId;
    for (const auto& submission : submissions) byId[submission.questionId] = &submission;

    double total = 0.0;
    for (const auto& question : questions) {
        AssessmentSubmission empty;
        empty.questionId = question.id;
        const AssessmentSubmission* submission = &empty;
        const auto found = byId.find(question.id);
        if (found != byId.end()) submission = found->second;
        AssessmentResult scored = score(question, *submission);
        total += scored.score;
        results.push_back(std::move(scored));
    }
    return total / static_cast<double>(questions.size());
}

} // namespace bag
