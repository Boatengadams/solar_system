#pragma once

#include <optional>
#include <string>
#include <vector>

#include "GradeIds.hpp"

namespace bag {

enum class ActivityType {
    Identify,
    Classify,
    DragDrop,
    Ordering,
    Matching,
    Labeling,
    Explore,
    Construction,
    Mission,
    Experiment,
    PredictionExperiment,
    Comparison,
    Calculation,
    InteractiveSimulation,
};

enum class QuestionType {
    MultipleChoice,
    MultipleSelect,
    DragDrop,
    Ordering,
    Matching,
    Labeling,
    Prediction,
    Numerical,
    ShortAnswer,
    Observation,
    ExperimentBased,
    Scenario,
};

enum class ActivityStepType {
    Instruction,
    Explore,
    Predict,
    Experiment,
    Observe,
    Explain,
    Question,
    Assessment,
    Feedback,
};

enum class DifficultyLevel {
    Introductory,
    Intermediate,
    Advanced,
    Challenge,
};

struct CurriculumSourceMeta {
    std::string organization;
    std::string document;
    std::string version;
    std::string reference;
    std::string verifiedAt;
};

struct ActivityStep {
    ActivityStepType type = ActivityStepType::Instruction;
    std::string titleKey;
    std::string bodyKey;
    std::string simulationAction;
    std::vector<std::string> payload;
};

struct CurriculumActivity {
    std::string id;
    GradeId grade = GradeId::B1;
    std::string subject;
    std::string strand;
    std::string subStrand;
    std::string contentStandard;
    std::string learningIndicator;
    std::string curriculumReference;
    CurriculumAlignment alignment = CurriculumAlignment::SupplementaryEnrichment;
    std::string learningObjective;
    std::string titleKey;
    std::string instructionsKey;
    ActivityType type = ActivityType::Explore;
    DifficultyLevel difficulty = DifficultyLevel::Introductory;
    PresentationLayer presentationLayer = PresentationLayer::Foundation;
    std::string simulationId;
    std::string curriculumVersion;
    CurriculumSourceMeta source;
    int estimatedMinutes = 10;
    std::vector<std::string> learningObjectiveIds;
    std::vector<ActivityStep> steps;
    std::vector<std::string> assessmentIds;
    std::vector<std::string> misconceptionIds;
    std::vector<std::string> unlockOnMastery;
};

struct QuestionOption {
    std::string id;
    std::string labelKey;
    bool correct = false;
};

struct CurriculumQuestion {
    std::string id;
    GradeId grade = GradeId::B1;
    std::string subject;
    std::string strand;
    std::string subStrand;
    std::string curriculumReference;
    CurriculumAlignment alignment = CurriculumAlignment::SupplementaryEnrichment;
    std::string objective;
    DifficultyLevel difficulty = DifficultyLevel::Introductory;
    QuestionType questionType = QuestionType::MultipleChoice;
    std::string questionKey;
    std::vector<QuestionOption> options;
    std::vector<std::string> correctAnswerIds;
    std::string explanationKey;
    std::string misconceptionId;
    std::string activityId;
    double numericalTolerance = 0.05;
    std::optional<double> numericalAnswer;
};

struct CurriculumMission {
    std::string id;
    GradeId grade = GradeId::JHS1;
    std::string titleKey;
    std::string objectiveKey;
    std::string instructionsKey;
    std::string simulationId;
    std::string challengeKey;
    std::vector<std::string> activityIds;
    std::vector<std::string> assessmentIds;
    DifficultyLevel difficulty = DifficultyLevel::Intermediate;
    CurriculumAlignment alignment = CurriculumAlignment::Official;
    std::string curriculumReference;
    CurriculumSourceMeta source;
};

struct MisconceptionDefinition {
    std::string id;
    std::string statementKey;
    std::string responseKey;
    std::string experimentActivityId;
    std::string correctionKey;
    std::vector<std::string> triggerAnswerIds;
};

struct GradeDescriptor {
    GradeId id = GradeId::B1;
    std::string displayKey;
    EducationPhase phase = EducationPhase::LowerPrimary;
    PresentationLayer layer = PresentationLayer::Foundation;
    std::string curriculumCode; // e.g. B7 for JHS1
};

struct CurriculumManifest {
    std::string curriculumVersion;
    std::string localeDefault = "en";
    CurriculumSourceMeta source;
    std::vector<GradeDescriptor> grades;
    std::vector<std::string> activityFiles;
    std::vector<std::string> questionFiles;
    std::vector<std::string> missionFiles;
    std::vector<std::string> misconceptionFiles;
};

struct CurriculumCatalog {
    CurriculumManifest manifest;
    std::vector<CurriculumActivity> activities;
    std::vector<CurriculumQuestion> questions;
    std::vector<CurriculumMission> missions;
    std::vector<MisconceptionDefinition> misconceptions;
};

inline const char* activityTypeName(ActivityType type) {
    switch (type) {
    case ActivityType::Identify: return "identify";
    case ActivityType::Classify: return "classify";
    case ActivityType::DragDrop: return "drag_drop";
    case ActivityType::Ordering: return "ordering";
    case ActivityType::Matching: return "matching";
    case ActivityType::Labeling: return "labeling";
    case ActivityType::Explore: return "explore";
    case ActivityType::Construction: return "construction";
    case ActivityType::Mission: return "mission";
    case ActivityType::Experiment: return "experiment";
    case ActivityType::PredictionExperiment: return "prediction_experiment";
    case ActivityType::Comparison: return "comparison";
    case ActivityType::Calculation: return "calculation";
    case ActivityType::InteractiveSimulation: return "interactive_simulation";
    }
    return "unknown";
}

inline const char* questionTypeName(QuestionType type) {
    switch (type) {
    case QuestionType::MultipleChoice: return "multiple_choice";
    case QuestionType::MultipleSelect: return "multiple_select";
    case QuestionType::DragDrop: return "drag_drop";
    case QuestionType::Ordering: return "ordering";
    case QuestionType::Matching: return "matching";
    case QuestionType::Labeling: return "labeling";
    case QuestionType::Prediction: return "prediction";
    case QuestionType::Numerical: return "numerical";
    case QuestionType::ShortAnswer: return "short_answer";
    case QuestionType::Observation: return "observation";
    case QuestionType::ExperimentBased: return "experiment_based";
    case QuestionType::Scenario: return "scenario";
    }
    return "unknown";
}

inline const char* difficultyLevelName(DifficultyLevel level) {
    switch (level) {
    case DifficultyLevel::Introductory: return "introductory";
    case DifficultyLevel::Intermediate: return "intermediate";
    case DifficultyLevel::Advanced: return "advanced";
    case DifficultyLevel::Challenge: return "challenge";
    }
    return "unknown";
}

} // namespace bag
