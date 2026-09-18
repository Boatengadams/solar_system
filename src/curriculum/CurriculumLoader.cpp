#include "CurriculumLoader.hpp"

#include <fstream>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace bag {
namespace {

using json = nlohmann::json;

std::string readFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "unable to open file: " + path.string();
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool requireObject(const json& value, const std::string& context, std::string& error) {
    if (!value.is_object()) {
        error = context + " must be a JSON object";
        return false;
    }
    return true;
}

bool requireString(const json& object, const char* key, std::string& out, const std::string& context, std::string& error) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        error = context + " missing string field '" + key + "'";
        return false;
    }
    out = object.at(key).get<std::string>();
    return true;
}

bool optionalString(const json& object, const char* key, std::string& out) {
    if (!object.contains(key)) return true;
    if (!object.at(key).is_string()) return false;
    out = object.at(key).get<std::string>();
    return true;
}

bool requireStringArray(const json& object, const char* key, std::vector<std::string>& out,
                        const std::string& context, std::string& error) {
    if (!object.contains(key) || !object.at(key).is_array()) {
        error = context + " missing array field '" + key + "'";
        return false;
    }
    out.clear();
    for (const auto& item : object.at(key)) {
        if (!item.is_string()) {
            error = context + " array '" + key + "' must contain only strings";
            return false;
        }
        out.push_back(item.get<std::string>());
    }
    return true;
}

bool optionalStringArray(const json& object, const char* key, std::vector<std::string>& out) {
    if (!object.contains(key)) return true;
    if (!object.at(key).is_array()) return false;
    out.clear();
    for (const auto& item : object.at(key)) {
        if (!item.is_string()) return false;
        out.push_back(item.get<std::string>());
    }
    return true;
}

std::optional<CurriculumAlignment> parseAlignment(const std::string& value) {
    if (value == "official") return CurriculumAlignment::Official;
    if (value == "supplementary_enrichment" || value == "Supplementary enrichment")
        return CurriculumAlignment::SupplementaryEnrichment;
    return std::nullopt;
}

std::optional<ActivityType> parseActivityType(const std::string& value) {
    if (value == "identify") return ActivityType::Identify;
    if (value == "classify") return ActivityType::Classify;
    if (value == "drag_drop") return ActivityType::DragDrop;
    if (value == "ordering") return ActivityType::Ordering;
    if (value == "matching") return ActivityType::Matching;
    if (value == "labeling") return ActivityType::Labeling;
    if (value == "explore") return ActivityType::Explore;
    if (value == "construction") return ActivityType::Construction;
    if (value == "mission") return ActivityType::Mission;
    if (value == "experiment") return ActivityType::Experiment;
    if (value == "prediction_experiment") return ActivityType::PredictionExperiment;
    if (value == "comparison") return ActivityType::Comparison;
    if (value == "calculation") return ActivityType::Calculation;
    if (value == "interactive_simulation") return ActivityType::InteractiveSimulation;
    return std::nullopt;
}

std::optional<QuestionType> parseQuestionType(const std::string& value) {
    if (value == "multiple_choice") return QuestionType::MultipleChoice;
    if (value == "multiple_select") return QuestionType::MultipleSelect;
    if (value == "drag_drop") return QuestionType::DragDrop;
    if (value == "ordering") return QuestionType::Ordering;
    if (value == "matching") return QuestionType::Matching;
    if (value == "labeling") return QuestionType::Labeling;
    if (value == "prediction") return QuestionType::Prediction;
    if (value == "numerical") return QuestionType::Numerical;
    if (value == "short_answer") return QuestionType::ShortAnswer;
    if (value == "observation") return QuestionType::Observation;
    if (value == "experiment_based") return QuestionType::ExperimentBased;
    if (value == "scenario") return QuestionType::Scenario;
    return std::nullopt;
}

std::optional<DifficultyLevel> parseDifficulty(const std::string& value) {
    if (value == "introductory") return DifficultyLevel::Introductory;
    if (value == "intermediate") return DifficultyLevel::Intermediate;
    if (value == "advanced") return DifficultyLevel::Advanced;
    if (value == "challenge") return DifficultyLevel::Challenge;
    return std::nullopt;
}

std::optional<ActivityStepType> parseStepType(const std::string& value) {
    if (value == "instruction") return ActivityStepType::Instruction;
    if (value == "explore") return ActivityStepType::Explore;
    if (value == "predict") return ActivityStepType::Predict;
    if (value == "experiment") return ActivityStepType::Experiment;
    if (value == "observe") return ActivityStepType::Observe;
    if (value == "explain") return ActivityStepType::Explain;
    if (value == "question") return ActivityStepType::Question;
    if (value == "assessment") return ActivityStepType::Assessment;
    if (value == "feedback") return ActivityStepType::Feedback;
    return std::nullopt;
}

bool parseSource(const json& object, CurriculumSourceMeta& source, const std::string& context, std::string& error) {
    if (!object.contains("source")) {
        error = context + " missing object field 'source'";
        return false;
    }
    const json& sourceJson = object.at("source");
    if (!requireObject(sourceJson, context + ".source", error)) return false;
    return requireString(sourceJson, "organization", source.organization, context + ".source", error) &&
           requireString(sourceJson, "document", source.document, context + ".source", error) &&
           requireString(sourceJson, "version", source.version, context + ".source", error) &&
           requireString(sourceJson, "reference", source.reference, context + ".source", error) &&
           requireString(sourceJson, "verifiedAt", source.verifiedAt, context + ".source", error);
}

bool parseManifest(const json& root, CurriculumManifest& manifest, std::string& error) {
    if (!requireObject(root, "manifest", error)) return false;
    if (!requireString(root, "curriculumVersion", manifest.curriculumVersion, "manifest", error)) return false;
    if (!optionalString(root, "localeDefault", manifest.localeDefault)) {
        error = "manifest.localeDefault must be a string";
        return false;
    }
    if (!parseSource(root, manifest.source, "manifest", error)) return false;

    if (!root.contains("grades") || !root.at("grades").is_array() || root.at("grades").empty()) {
        error = "manifest.grades must be a non-empty array";
        return false;
    }

    for (const auto& gradeJson : root.at("grades")) {
        if (!requireObject(gradeJson, "manifest.grades[]", error)) return false;
        GradeDescriptor grade;
        std::string id;
        std::string phase;
        std::string layer;
        if (!requireString(gradeJson, "id", id, "manifest.grades[]", error)) return false;
        const auto parsedId = parseGradeId(id);
        if (!parsedId) {
            error = "unknown grade id in manifest: " + id;
            return false;
        }
        grade.id = *parsedId;
        if (!requireString(gradeJson, "displayKey", grade.displayKey, "manifest.grades[]", error)) return false;
        if (!requireString(gradeJson, "phase", phase, "manifest.grades[]", error)) return false;
        if (!requireString(gradeJson, "presentationLayer", layer, "manifest.grades[]", error)) return false;
        if (!optionalString(gradeJson, "curriculumCode", grade.curriculumCode)) {
            error = "manifest.grades[].curriculumCode must be a string";
            return false;
        }
        if (phase == "lower_primary") grade.phase = EducationPhase::LowerPrimary;
        else if (phase == "upper_primary") grade.phase = EducationPhase::UpperPrimary;
        else if (phase == "junior_high") grade.phase = EducationPhase::JuniorHigh;
        else if (phase == "senior_high") grade.phase = EducationPhase::SeniorHigh;
        else {
            error = "unknown education phase: " + phase;
            return false;
        }
        if (layer == "foundation") grade.layer = PresentationLayer::Foundation;
        else if (layer == "explorer") grade.layer = PresentationLayer::Explorer;
        else if (layer == "scientist") grade.layer = PresentationLayer::Scientist;
        else {
            error = "unknown presentation layer: " + layer;
            return false;
        }
        manifest.grades.push_back(grade);
    }

    return requireStringArray(root, "activityFiles", manifest.activityFiles, "manifest", error) &&
           requireStringArray(root, "questionFiles", manifest.questionFiles, "manifest", error) &&
           requireStringArray(root, "missionFiles", manifest.missionFiles, "manifest", error) &&
           requireStringArray(root, "misconceptionFiles", manifest.misconceptionFiles, "manifest", error);
}

bool parseActivity(const json& object, CurriculumActivity& activity, const std::string& curriculumVersion,
                   std::string& error) {
    if (!requireObject(object, "activity", error)) return false;
    std::string grade;
    std::string alignment;
    std::string type;
    std::string difficulty;
    std::string layer;
    if (!requireString(object, "id", activity.id, "activity", error)) return false;
    if (!requireString(object, "grade", grade, "activity", error)) return false;
    const auto parsedGrade = parseGradeId(grade);
    if (!parsedGrade) {
        error = "activity '" + activity.id + "' has unknown grade: " + grade;
        return false;
    }
    activity.grade = *parsedGrade;
    if (!requireString(object, "subject", activity.subject, "activity", error)) return false;
    if (!requireString(object, "strand", activity.strand, "activity", error)) return false;
    if (!requireString(object, "subStrand", activity.subStrand, "activity", error)) return false;
    if (!requireString(object, "contentStandard", activity.contentStandard, "activity", error)) return false;
    if (!requireString(object, "learningIndicator", activity.learningIndicator, "activity", error)) return false;
    if (!requireString(object, "curriculumReference", activity.curriculumReference, "activity", error)) return false;
    if (!requireString(object, "alignment", alignment, "activity", error)) return false;
    const auto parsedAlignment = parseAlignment(alignment);
    if (!parsedAlignment) {
        error = "activity '" + activity.id + "' has unknown alignment: " + alignment;
        return false;
    }
    activity.alignment = *parsedAlignment;
    if (!requireString(object, "learningObjective", activity.learningObjective, "activity", error)) return false;
    if (!requireString(object, "titleKey", activity.titleKey, "activity", error)) return false;
    if (!requireString(object, "instructionsKey", activity.instructionsKey, "activity", error)) return false;
    if (!requireString(object, "type", type, "activity", error)) return false;
    const auto parsedType = parseActivityType(type);
    if (!parsedType) {
        error = "activity '" + activity.id + "' has unknown type: " + type;
        return false;
    }
    activity.type = *parsedType;
    if (!requireString(object, "difficulty", difficulty, "activity", error)) return false;
    const auto parsedDifficulty = parseDifficulty(difficulty);
    if (!parsedDifficulty) {
        error = "activity '" + activity.id + "' has unknown difficulty: " + difficulty;
        return false;
    }
    activity.difficulty = *parsedDifficulty;
    if (!requireString(object, "presentationLayer", layer, "activity", error)) return false;
    if (layer == "foundation") activity.presentationLayer = PresentationLayer::Foundation;
    else if (layer == "explorer") activity.presentationLayer = PresentationLayer::Explorer;
    else if (layer == "scientist") activity.presentationLayer = PresentationLayer::Scientist;
    else {
        error = "activity '" + activity.id + "' has unknown presentationLayer: " + layer;
        return false;
    }
    if (!requireString(object, "simulation", activity.simulationId, "activity", error)) return false;
    if (!optionalString(object, "curriculumVersion", activity.curriculumVersion)) {
        error = "activity.curriculumVersion must be a string";
        return false;
    }
    if (activity.curriculumVersion.empty()) activity.curriculumVersion = curriculumVersion;
    if (!parseSource(object, activity.source, "activity '" + activity.id + "'", error)) return false;
    if (!object.contains("estimatedMinutes") || !object.at("estimatedMinutes").is_number_integer()) {
        error = "activity '" + activity.id + "' missing integer estimatedMinutes";
        return false;
    }
    activity.estimatedMinutes = object.at("estimatedMinutes").get<int>();
    if (activity.estimatedMinutes <= 0) {
        error = "activity '" + activity.id + "' estimatedMinutes must be positive";
        return false;
    }
    if (!optionalStringArray(object, "learningObjectiveIds", activity.learningObjectiveIds)) {
        error = "activity.learningObjectiveIds must be a string array";
        return false;
    }
    if (!optionalStringArray(object, "assessmentIds", activity.assessmentIds)) {
        error = "activity.assessmentIds must be a string array";
        return false;
    }
    if (!optionalStringArray(object, "misconceptionIds", activity.misconceptionIds)) {
        error = "activity.misconceptionIds must be a string array";
        return false;
    }
    if (!optionalStringArray(object, "unlockOnMastery", activity.unlockOnMastery)) {
        error = "activity.unlockOnMastery must be a string array";
        return false;
    }

    if (!object.contains("steps") || !object.at("steps").is_array() || object.at("steps").empty()) {
        error = "activity '" + activity.id + "' must contain a non-empty steps array";
        return false;
    }
    for (const auto& stepJson : object.at("steps")) {
        if (!requireObject(stepJson, "activity.steps[]", error)) return false;
        ActivityStep step;
        std::string stepType;
        if (!requireString(stepJson, "type", stepType, "activity.steps[]", error)) return false;
        const auto parsedStep = parseStepType(stepType);
        if (!parsedStep) {
            error = "activity '" + activity.id + "' has unknown step type: " + stepType;
            return false;
        }
        step.type = *parsedStep;
        optionalString(stepJson, "titleKey", step.titleKey);
        optionalString(stepJson, "bodyKey", step.bodyKey);
        optionalString(stepJson, "simulationAction", step.simulationAction);
        optionalStringArray(stepJson, "payload", step.payload);
        activity.steps.push_back(step);
    }

    if (activity.alignment == CurriculumAlignment::Official &&
        (activity.curriculumReference.empty() || activity.curriculumReference == "Supplementary enrichment")) {
        error = "official activity '" + activity.id + "' must provide a real curriculumReference";
        return false;
    }
    return true;
}

bool parseQuestion(const json& object, CurriculumQuestion& question, std::string& error) {
    if (!requireObject(object, "question", error)) return false;
    std::string grade;
    std::string alignment;
    std::string difficulty;
    std::string type;
    if (!requireString(object, "id", question.id, "question", error)) return false;
    if (!requireString(object, "grade", grade, "question", error)) return false;
    const auto parsedGrade = parseGradeId(grade);
    if (!parsedGrade) {
        error = "question '" + question.id + "' has unknown grade: " + grade;
        return false;
    }
    question.grade = *parsedGrade;
    if (!requireString(object, "subject", question.subject, "question", error)) return false;
    if (!requireString(object, "strand", question.strand, "question", error)) return false;
    if (!requireString(object, "subStrand", question.subStrand, "question", error)) return false;
    if (!requireString(object, "curriculumReference", question.curriculumReference, "question", error)) return false;
    if (!requireString(object, "alignment", alignment, "question", error)) return false;
    const auto parsedAlignment = parseAlignment(alignment);
    if (!parsedAlignment) {
        error = "question '" + question.id + "' has unknown alignment";
        return false;
    }
    question.alignment = *parsedAlignment;
    if (!requireString(object, "objective", question.objective, "question", error)) return false;
    if (!requireString(object, "difficulty", difficulty, "question", error)) return false;
    const auto parsedDifficulty = parseDifficulty(difficulty);
    if (!parsedDifficulty) {
        error = "question '" + question.id + "' has unknown difficulty";
        return false;
    }
    question.difficulty = *parsedDifficulty;
    if (!requireString(object, "questionType", type, "question", error)) return false;
    const auto parsedType = parseQuestionType(type);
    if (!parsedType) {
        error = "question '" + question.id + "' has unknown questionType";
        return false;
    }
    question.questionType = *parsedType;
    if (!requireString(object, "questionKey", question.questionKey, "question", error)) return false;
    if (!requireString(object, "explanationKey", question.explanationKey, "question", error)) return false;
    optionalString(object, "misconceptionId", question.misconceptionId);
    optionalString(object, "activityId", question.activityId);
    if (object.contains("numericalTolerance") && object.at("numericalTolerance").is_number())
        question.numericalTolerance = object.at("numericalTolerance").get<double>();
    if (object.contains("numericalAnswer") && object.at("numericalAnswer").is_number())
        question.numericalAnswer = object.at("numericalAnswer").get<double>();

    if (object.contains("options") && object.at("options").is_array()) {
        for (const auto& optionJson : object.at("options")) {
            if (!requireObject(optionJson, "question.options[]", error)) return false;
            QuestionOption option;
            if (!requireString(optionJson, "id", option.id, "question.options[]", error)) return false;
            if (!requireString(optionJson, "labelKey", option.labelKey, "question.options[]", error)) return false;
            if (optionJson.contains("correct") && optionJson.at("correct").is_boolean())
                option.correct = optionJson.at("correct").get<bool>();
            question.options.push_back(option);
            if (option.correct) question.correctAnswerIds.push_back(option.id);
        }
    }
    if (object.contains("correctAnswer")) {
        if (!object.at("correctAnswer").is_array() ||
            !optionalStringArray(object, "correctAnswer", question.correctAnswerIds)) {
            error = "question.correctAnswer must be a string array";
            return false;
        }
    } else if (object.contains("correctAnswerIds")) {
        if (!object.at("correctAnswerIds").is_array() ||
            !optionalStringArray(object, "correctAnswerIds", question.correctAnswerIds)) {
            error = "question.correctAnswerIds must be a string array";
            return false;
        }
    }
    return true;
}

bool parseMission(const json& object, CurriculumMission& mission, std::string& error) {
    if (!requireObject(object, "mission", error)) return false;
    std::string grade;
    std::string difficulty;
    std::string alignment;
    if (!requireString(object, "id", mission.id, "mission", error)) return false;
    if (!requireString(object, "grade", grade, "mission", error)) return false;
    const auto parsedGrade = parseGradeId(grade);
    if (!parsedGrade) {
        error = "mission '" + mission.id + "' has unknown grade";
        return false;
    }
    mission.grade = *parsedGrade;
    if (!requireString(object, "titleKey", mission.titleKey, "mission", error)) return false;
    if (!requireString(object, "objectiveKey", mission.objectiveKey, "mission", error)) return false;
    if (!requireString(object, "instructionsKey", mission.instructionsKey, "mission", error)) return false;
    if (!requireString(object, "simulation", mission.simulationId, "mission", error)) return false;
    if (!requireString(object, "challengeKey", mission.challengeKey, "mission", error)) return false;
    if (!requireString(object, "difficulty", difficulty, "mission", error)) return false;
    const auto parsedDifficulty = parseDifficulty(difficulty);
    if (!parsedDifficulty) {
        error = "mission '" + mission.id + "' has unknown difficulty";
        return false;
    }
    mission.difficulty = *parsedDifficulty;
    if (!requireString(object, "alignment", alignment, "mission", error)) return false;
    const auto parsedAlignment = parseAlignment(alignment);
    if (!parsedAlignment) {
        error = "mission '" + mission.id + "' has unknown alignment";
        return false;
    }
    mission.alignment = *parsedAlignment;
    if (!requireString(object, "curriculumReference", mission.curriculumReference, "mission", error)) return false;
    if (!parseSource(object, mission.source, "mission '" + mission.id + "'", error)) return false;
    if (!optionalStringArray(object, "activityIds", mission.activityIds)) {
        error = "mission.activityIds must be a string array";
        return false;
    }
    if (!optionalStringArray(object, "assessmentIds", mission.assessmentIds)) {
        error = "mission.assessmentIds must be a string array";
        return false;
    }
    return true;
}

bool parseMisconception(const json& object, MisconceptionDefinition& misconception, std::string& error) {
    if (!requireObject(object, "misconception", error)) return false;
    if (!requireString(object, "id", misconception.id, "misconception", error)) return false;
    if (!requireString(object, "statementKey", misconception.statementKey, "misconception", error)) return false;
    if (!requireString(object, "responseKey", misconception.responseKey, "misconception", error)) return false;
    if (!requireString(object, "experimentActivityId", misconception.experimentActivityId, "misconception", error))
        return false;
    if (!requireString(object, "correctionKey", misconception.correctionKey, "misconception", error)) return false;
    if (!optionalStringArray(object, "triggerAnswerIds", misconception.triggerAnswerIds)) {
        error = "misconception.triggerAnswerIds must be a string array";
        return false;
    }
    return true;
}

template <typename T, typename Parser>
bool loadJsonArrayFile(const std::filesystem::path& path, const char* arrayKey, std::vector<T>& out, Parser parser,
                       std::string& error) {
    const std::string text = readFile(path, error);
    if (!error.empty()) return false;
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& exception) {
        error = std::string("JSON parse failed for ") + path.string() + ": " + exception.what();
        return false;
    }
    if (!root.is_object() || !root.contains(arrayKey) || !root.at(arrayKey).is_array()) {
        error = path.string() + " must contain array '" + arrayKey + "'";
        return false;
    }
    for (const auto& item : root.at(arrayKey)) {
        T value;
        if (!parser(item, value, error)) return false;
        out.push_back(std::move(value));
    }
    return true;
}

} // namespace

CurriculumLoadResult CurriculumLoader::loadManifestOnly(const std::filesystem::path& manifestPath) {
    CurriculumLoadResult result;
    const std::string text = readFile(manifestPath, result.error);
    if (!result.error.empty()) return result;
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& exception) {
        result.error = std::string("manifest parse failed: ") + exception.what();
        return result;
    }
    if (!parseManifest(root, result.catalog.manifest, result.error)) return result;
    result.ok = true;
    return result;
}

CurriculumLoadResult CurriculumLoader::loadFromDirectory(const std::filesystem::path& curriculumRoot) {
    CurriculumLoadResult result = loadManifestOnly(curriculumRoot / "manifest.json");
    if (!result) return result;

    const auto& manifest = result.catalog.manifest;
    for (const auto& relative : manifest.activityFiles) {
        const auto path = curriculumRoot / relative;
        if (!loadJsonArrayFile(path, "activities", result.catalog.activities,
                               [&](const json& item, CurriculumActivity& activity, std::string& error) {
                                   return parseActivity(item, activity, manifest.curriculumVersion, error);
                               },
                               result.error)) {
            result.ok = false;
            return result;
        }
    }
    for (const auto& relative : manifest.questionFiles) {
        const auto path = curriculumRoot / relative;
        if (!loadJsonArrayFile(path, "questions", result.catalog.questions,
                               [&](const json& item, CurriculumQuestion& question, std::string& error) {
                                   return parseQuestion(item, question, error);
                               },
                               result.error)) {
            result.ok = false;
            return result;
        }
    }
    for (const auto& relative : manifest.missionFiles) {
        const auto path = curriculumRoot / relative;
        if (!loadJsonArrayFile(path, "missions", result.catalog.missions,
                               [&](const json& item, CurriculumMission& mission, std::string& error) {
                                   return parseMission(item, mission, error);
                               },
                               result.error)) {
            result.ok = false;
            return result;
        }
    }
    for (const auto& relative : manifest.misconceptionFiles) {
        const auto path = curriculumRoot / relative;
        if (!loadJsonArrayFile(path, "misconceptions", result.catalog.misconceptions,
                               [&](const json& item, MisconceptionDefinition& misconception, std::string& error) {
                                   return parseMisconception(item, misconception, error);
                               },
                               result.error)) {
            result.ok = false;
            return result;
        }
    }

    const auto validationErrors = validateCatalog(result.catalog);
    if (!validationErrors.empty()) {
        result.ok = false;
        result.error = validationErrors.front();
        return result;
    }

    result.ok = true;
    result.error.clear();
    return result;
}

std::vector<const CurriculumActivity*> CurriculumLoader::activitiesForGrade(const CurriculumCatalog& catalog,
                                                                            GradeId grade) {
    std::vector<const CurriculumActivity*> filtered;
    for (const auto& activity : catalog.activities) {
        if (activity.grade == grade) filtered.push_back(&activity);
    }
    return filtered;
}

std::vector<const CurriculumActivity*> CurriculumLoader::activitiesForLayer(const CurriculumCatalog& catalog,
                                                                            PresentationLayer layer) {
    std::vector<const CurriculumActivity*> filtered;
    for (const auto& activity : catalog.activities) {
        if (activity.presentationLayer == layer) filtered.push_back(&activity);
    }
    return filtered;
}

const CurriculumActivity* CurriculumLoader::findActivity(const CurriculumCatalog& catalog, const std::string& id) {
    for (const auto& activity : catalog.activities) {
        if (activity.id == id) return &activity;
    }
    return nullptr;
}

const CurriculumQuestion* CurriculumLoader::findQuestion(const CurriculumCatalog& catalog, const std::string& id) {
    for (const auto& question : catalog.questions) {
        if (question.id == id) return &question;
    }
    return nullptr;
}

const CurriculumMission* CurriculumLoader::findMission(const CurriculumCatalog& catalog, const std::string& id) {
    for (const auto& mission : catalog.missions) {
        if (mission.id == id) return &mission;
    }
    return nullptr;
}

const MisconceptionDefinition* CurriculumLoader::findMisconception(const CurriculumCatalog& catalog,
                                                                   const std::string& id) {
    for (const auto& misconception : catalog.misconceptions) {
        if (misconception.id == id) return &misconception;
    }
    return nullptr;
}

std::vector<std::string> CurriculumLoader::validateCatalog(const CurriculumCatalog& catalog) {
    std::vector<std::string> errors;
    std::unordered_set<std::string> activityIds;
    std::unordered_set<std::string> questionIds;
    std::unordered_set<std::string> missionIds;
    std::unordered_set<std::string> misconceptionIds;

    for (const auto& activity : catalog.activities) {
        if (!activityIds.insert(activity.id).second)
            errors.push_back("duplicate activity id: " + activity.id);
        if (activity.curriculumVersion != catalog.manifest.curriculumVersion)
            errors.push_back("activity '" + activity.id + "' curriculumVersion mismatch");
        if (activity.presentationLayer != presentationLayerFor(activity.grade))
            errors.push_back("activity '" + activity.id + "' presentationLayer does not match grade");
    }
    for (const auto& question : catalog.questions) {
        if (!questionIds.insert(question.id).second)
            errors.push_back("duplicate question id: " + question.id);
        if (!question.activityId.empty() && activityIds.find(question.activityId) == activityIds.end())
            errors.push_back("question '" + question.id + "' references missing activity");
    }
    for (const auto& mission : catalog.missions) {
        if (!missionIds.insert(mission.id).second)
            errors.push_back("duplicate mission id: " + mission.id);
        for (const auto& activityId : mission.activityIds) {
            if (activityIds.find(activityId) == activityIds.end())
                errors.push_back("mission '" + mission.id + "' references missing activity " + activityId);
        }
        for (const auto& assessmentId : mission.assessmentIds) {
            if (questionIds.find(assessmentId) == questionIds.end())
                errors.push_back("mission '" + mission.id + "' references missing question " + assessmentId);
        }
    }
    for (const auto& misconception : catalog.misconceptions) {
        if (!misconceptionIds.insert(misconception.id).second)
            errors.push_back("duplicate misconception id: " + misconception.id);
        if (activityIds.find(misconception.experimentActivityId) == activityIds.end())
            errors.push_back("misconception '" + misconception.id + "' references missing experiment activity");
    }
    for (const auto& activity : catalog.activities) {
        for (const auto& assessmentId : activity.assessmentIds) {
            if (questionIds.find(assessmentId) == questionIds.end())
                errors.push_back("activity '" + activity.id + "' references missing question " + assessmentId);
        }
        for (const auto& misconceptionId : activity.misconceptionIds) {
            if (misconceptionIds.find(misconceptionId) == misconceptionIds.end())
                errors.push_back("activity '" + activity.id + "' references missing misconception " + misconceptionId);
        }
    }
    return errors;
}

} // namespace bag
