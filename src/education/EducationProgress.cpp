#include "EducationProgress.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <nlohmann/json.hpp>

namespace bag {

namespace {

using Json = nlohmann::json;

bool finiteScore(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 100.0;
}

bool finiteMetric(double value) { return std::isfinite(value); }

Json optionalNumber(double value) { return finiteMetric(value) ? Json(value) : Json(nullptr); }

bool optionalNumberFromJson(const Json& object, const char* name, double& output) {
    if (!object.contains(name)) return false;
    if (object[name].is_null()) {
        output = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    if (!object[name].is_number()) return false;
    output = object[name].get<double>();
    return finiteMetric(output);
}

Json metricsToJson(const ChallengeMetrics& metrics) {
    return {
        {"learner_primary_value", metrics.learnerPrimaryValue},
        {"learner_secondary_value", metrics.learnerSecondaryValue},
        {"expected_primary_value", metrics.expectedPrimaryValue},
        {"expected_secondary_value", metrics.expectedSecondaryValue},
        {"absolute_primary_error", metrics.absolutePrimaryError},
        {"absolute_secondary_error", metrics.absoluteSecondaryError},
        {"primary_relative_error", metrics.primaryRelativeError},
        {"secondary_relative_error", metrics.secondaryRelativeError},
        {"energy_drift", metrics.energyDrift},
        {"angular_momentum_drift", metrics.angularMomentumDrift},
        {"position_error", metrics.positionError},
        {"velocity_error", metrics.velocityError},
        {"orbital_period_error", metrics.orbitalPeriodError},
        {"normalized_numerical_error", metrics.normalizedNumericalError},
        {"numerically_stable", metrics.numericallyStable},
        {"reference_departure_delta_v", metrics.referenceDepartureDeltaV},
        {"reference_arrival_delta_v", metrics.referenceArrivalDeltaV},
        {"reference_total_delta_v", metrics.referenceTotalDeltaV},
    };
}

bool metricsFromJson(const Json& value, ChallengeMetrics& metrics) {
    const char* fields[] = {
        "learner_primary_value", "learner_secondary_value", "expected_primary_value",
        "expected_secondary_value", "absolute_primary_error", "absolute_secondary_error",
        "primary_relative_error", "secondary_relative_error", "energy_drift",
        "angular_momentum_drift", "position_error", "velocity_error", "orbital_period_error",
        "normalized_numerical_error", "reference_departure_delta_v", "reference_arrival_delta_v",
        "reference_total_delta_v",
    };
    if (!value.is_object() || !value.contains("numerically_stable")) return false;
    for (const char* field : fields) if (!value.contains(field) || !value[field].is_number()) return false;
    if (!value["numerically_stable"].is_boolean()) return false;
    const double values[] = {
        value["learner_primary_value"].get<double>(), value["learner_secondary_value"].get<double>(),
        value["expected_primary_value"].get<double>(), value["expected_secondary_value"].get<double>(),
        value["absolute_primary_error"].get<double>(), value["absolute_secondary_error"].get<double>(),
        value["primary_relative_error"].get<double>(), value["secondary_relative_error"].get<double>(),
        value["energy_drift"].get<double>(), value["angular_momentum_drift"].get<double>(),
        value["position_error"].get<double>(), value["velocity_error"].get<double>(),
        value["orbital_period_error"].get<double>(), value["normalized_numerical_error"].get<double>(),
        value["reference_departure_delta_v"].get<double>(), value["reference_arrival_delta_v"].get<double>(),
        value["reference_total_delta_v"].get<double>(),
    };
    for (double number : values) if (!finiteMetric(number) || number < 0.0) return false;
    metrics.learnerPrimaryValue = values[0];
    metrics.learnerSecondaryValue = values[1];
    metrics.expectedPrimaryValue = values[2];
    metrics.expectedSecondaryValue = values[3];
    metrics.absolutePrimaryError = values[4];
    metrics.absoluteSecondaryError = values[5];
    metrics.primaryRelativeError = values[6];
    metrics.secondaryRelativeError = values[7];
    metrics.energyDrift = values[8];
    metrics.angularMomentumDrift = values[9];
    metrics.positionError = values[10];
    metrics.velocityError = values[11];
    metrics.orbitalPeriodError = values[12];
    metrics.normalizedNumericalError = values[13];
    metrics.referenceDepartureDeltaV = values[14];
    metrics.referenceArrivalDeltaV = values[15];
    metrics.referenceTotalDeltaV = values[16];
    metrics.numericallyStable = value["numerically_stable"].get<bool>();
    return true;
}

bool boolVectorFromJson(const Json& value, std::size_t expected, std::vector<bool>& output) {
    if (!value.is_array() || value.size() != expected) return false;
    std::vector<bool> parsed;
    parsed.reserve(expected);
    for (const Json& item : value) {
        if (!item.is_boolean()) return false;
        parsed.push_back(item.get<bool>());
    }
    output = std::move(parsed);
    return true;
}

bool stringVectorFromJson(const Json& value, std::size_t expected, std::vector<std::string>& output) {
    if (!value.is_array() || value.size() != expected) return false;
    std::vector<std::string> parsed;
    parsed.reserve(expected);
    for (const Json& item : value) {
        if (!item.is_string()) return false;
        parsed.push_back(item.get<std::string>());
    }
    output = std::move(parsed);
    return true;
}

Json experimentMetricsToJson(const ExperimentEvaluationMetrics& metrics) {
    return {
        {"measured_primary_value", optionalNumber(metrics.measuredPrimaryValue)},
        {"measured_secondary_value", optionalNumber(metrics.measuredSecondaryValue)},
        {"reference_primary_value", optionalNumber(metrics.referencePrimaryValue)},
        {"reference_secondary_value", optionalNumber(metrics.referenceSecondaryValue)},
        {"absolute_primary_error", optionalNumber(metrics.absolutePrimaryError)},
        {"absolute_secondary_error", optionalNumber(metrics.absoluteSecondaryError)},
        {"primary_relative_error", optionalNumber(metrics.primaryRelativeError)},
        {"secondary_relative_error", optionalNumber(metrics.secondaryRelativeError)},
        {"energy_drift", optionalNumber(metrics.energyDrift)},
        {"angular_momentum_drift", optionalNumber(metrics.angularMomentumDrift)},
        {"position_error", optionalNumber(metrics.positionErrorM)},
        {"velocity_error", optionalNumber(metrics.velocityErrorMps)},
        {"orbital_period_error", optionalNumber(metrics.orbitalPeriodError)},
        {"normalized_error", optionalNumber(metrics.normalizedError)},
        {"improvement_ratio", optionalNumber(metrics.improvementRatio)},
        {"numerically_stable", metrics.numericallyStable},
    };
}

bool experimentMetricsFromJson(const Json& value, ExperimentEvaluationMetrics& metrics) {
    if (!value.is_object() || !value.contains("numerically_stable") || !value["numerically_stable"].is_boolean()) return false;
    struct Field { const char* name; double* target; } fields[] = {
        {"measured_primary_value", &metrics.measuredPrimaryValue},
        {"measured_secondary_value", &metrics.measuredSecondaryValue},
        {"reference_primary_value", &metrics.referencePrimaryValue},
        {"reference_secondary_value", &metrics.referenceSecondaryValue},
        {"absolute_primary_error", &metrics.absolutePrimaryError},
        {"absolute_secondary_error", &metrics.absoluteSecondaryError},
        {"primary_relative_error", &metrics.primaryRelativeError},
        {"secondary_relative_error", &metrics.secondaryRelativeError},
        {"energy_drift", &metrics.energyDrift},
        {"angular_momentum_drift", &metrics.angularMomentumDrift},
        {"position_error", &metrics.positionErrorM},
        {"velocity_error", &metrics.velocityErrorMps},
        {"orbital_period_error", &metrics.orbitalPeriodError},
        {"normalized_error", &metrics.normalizedError},
        {"improvement_ratio", &metrics.improvementRatio},
    };
    for (const Field& field : fields) if (!optionalNumberFromJson(value, field.name, *field.target)) return false;
    metrics.numericallyStable = value["numerically_stable"].get<bool>();
    return true;
}

bool experimentProgressFromJson(const Json& value, ExperimentProgress& progress) {
    if (!value.is_object() || !value.contains("attempts") || !value["attempts"].is_number_integer() ||
        !value.contains("completed") || !value["completed"].is_boolean() ||
        !value.contains("best_score") || !value["best_score"].is_number() ||
        !value.contains("latest_score") || !value["latest_score"].is_number() ||
        !value.contains("latest_grade") || !value["latest_grade"].is_string() ||
        !value.contains("latest_status") || !value["latest_status"].is_number_integer() ||
        !value.contains("latest_mode") || !value["latest_mode"].is_number_integer() ||
        !value.contains("latest_metrics")) return false;
    const int attempts = value["attempts"].get<int>();
    const int status = value["latest_status"].get<int>();
    const int mode = value["latest_mode"].get<int>();
    const double best = value["best_score"].get<double>();
    const double latest = value["latest_score"].get<double>();
    if (attempts < 0 || !finiteScore(best) || !finiteScore(latest) || status < 0 || status > static_cast<int>(ExperimentEvaluationStatus::Unsupported) || mode < 0 || mode > static_cast<int>(ExperimentEvaluationMode::NumericalComparison) || !experimentMetricsFromJson(value["latest_metrics"], progress.latestMetrics)) return false;
    progress.attempts = attempts;
    progress.completed = value["completed"].get<bool>();
    progress.bestScore = best;
    progress.latestScore = latest;
    progress.latestGrade = value["latest_grade"].get<std::string>();
    progress.latestStatus = static_cast<ExperimentEvaluationStatus>(status);
    progress.latestMode = static_cast<ExperimentEvaluationMode>(mode);
    return true;
}

} // namespace

EducationProgress::EducationProgress(int lessonCount, int experimentCount, int challengeCount)
    : lessons(static_cast<std::size_t>(std::max(0, lessonCount)), false),
      experiments(static_cast<std::size_t>(std::max(0, experimentCount)), false),
      experimentObservations(static_cast<std::size_t>(std::max(0, experimentCount))),
      experimentEvaluations(static_cast<std::size_t>(std::max(0, experimentCount))),
      challenges(static_cast<std::size_t>(std::max(0, challengeCount))) {}

bool EducationProgress::completeLesson(int index) {
    if (index < 0 || index >= static_cast<int>(lessons.size())) return false;
    lessons[static_cast<std::size_t>(index)] = true;
    return true;
}

bool EducationProgress::completeExperiment(int index) {
    if (index < 0 || index >= static_cast<int>(experiments.size())) return false;
    experiments[static_cast<std::size_t>(index)] = true;
    return true;
}

bool EducationProgress::recordObservation(int experimentIndex, std::string observation) {
    if (experimentIndex < 0 || experimentIndex >= static_cast<int>(experiments.size()) || observation.empty()) return false;
    experimentObservations[static_cast<std::size_t>(experimentIndex)] = std::move(observation);
    experiments[static_cast<std::size_t>(experimentIndex)] = true;
    return true;
}

bool EducationProgress::recordChallengeResult(int challengeIndex, const ChallengeResult& result) {
    if (challengeIndex < 0 || challengeIndex >= static_cast<int>(challenges.size()) || !result.valid ||
        !finiteScore(result.score)) return false;
    ChallengeProgress& progress = challenges[static_cast<std::size_t>(challengeIndex)];
    ++progress.attempts;
    progress.latestScore = result.score;
    progress.bestScore = std::max(progress.bestScore, result.score);
    progress.completed = progress.completed || result.passed;
    progress.latestMetrics = result.metrics;
    return true;
}

bool EducationProgress::recordExperimentEvaluation(int experimentIndex, const ExperimentEvaluation& result) {
    if (experimentIndex < 0 || experimentIndex >= static_cast<int>(experimentEvaluations.size()) ||
        !experimentEvaluationIsPersistable(result)) return false;
    ExperimentProgress& progress = experimentEvaluations[static_cast<std::size_t>(experimentIndex)];
    ++progress.attempts;
    progress.latestScore = result.score;
    progress.bestScore = std::max(progress.bestScore, result.score);
    progress.completed = progress.completed || result.passed;
    progress.latestGrade = result.grade;
    progress.latestStatus = result.status;
    progress.latestMode = result.mode;
    progress.latestMetrics = result.metrics;
    if (result.passed) experiments[static_cast<std::size_t>(experimentIndex)] = true;
    return true;
}

bool EducationProgress::lessonComplete(int index) const { return index >= 0 && index < static_cast<int>(lessons.size()) && lessons[static_cast<std::size_t>(index)]; }
bool EducationProgress::experimentComplete(int index) const { return index >= 0 && index < static_cast<int>(experiments.size()) && experiments[static_cast<std::size_t>(index)]; }
bool EducationProgress::challengeComplete(int index) const { return index >= 0 && index < static_cast<int>(challenges.size()) && challenges[static_cast<std::size_t>(index)].completed; }
const ChallengeProgress* EducationProgress::challengeProgress(int index) const {
    if (index < 0 || index >= static_cast<int>(challenges.size())) return nullptr;
    return &challenges[static_cast<std::size_t>(index)];
}
const ExperimentProgress* EducationProgress::experimentProgress(int index) const {
    if (index < 0 || index >= static_cast<int>(experimentEvaluations.size())) return nullptr;
    return &experimentEvaluations[static_cast<std::size_t>(index)];
}

void EducationProgress::reset() {
    std::fill(lessons.begin(), lessons.end(), false);
    std::fill(experiments.begin(), experiments.end(), false);
    std::fill(experimentObservations.begin(), experimentObservations.end(), std::string{});
    experimentEvaluations.assign(experimentEvaluations.size(), ExperimentProgress{});
    challenges.assign(challenges.size(), ChallengeProgress{});
}

EducationReport EducationProgress::report() const {
    EducationReport result;
    result.totalLessons = static_cast<int>(lessons.size());
    result.totalExperiments = static_cast<int>(experiments.size());
    result.totalChallenges = static_cast<int>(challenges.size());
    result.completedLessons = static_cast<int>(std::count(lessons.begin(), lessons.end(), true));
    result.completedExperiments = static_cast<int>(std::count(experiments.begin(), experiments.end(), true));
    result.completedChallenges = static_cast<int>(std::count_if(challenges.begin(), challenges.end(), [](const ChallengeProgress& progress) { return progress.completed; }));
    const int total = result.totalLessons + result.totalExperiments + result.totalChallenges;
    result.completionRatio = total == 0 ? 0.0 : static_cast<double>(result.completedLessons + result.completedExperiments + result.completedChallenges) / total;
    for (const std::string& observation : experimentObservations) if (!observation.empty()) result.observations.push_back(observation);
    return result;
}

std::string EducationProgress::serialize() const {
    Json root;
    root["schema_version"] = CURRENT_SCHEMA_VERSION;
    root["lessons"] = lessons;
    root["experiments"] = experiments;
    root["observations"] = experimentObservations;
    root["experiment_evaluations"] = Json::array();
    for (const ExperimentProgress& progress : experimentEvaluations) {
        root["experiment_evaluations"].push_back({
            {"attempts", progress.attempts}, {"completed", progress.completed},
            {"best_score", progress.bestScore}, {"latest_score", progress.latestScore},
            {"latest_grade", progress.latestGrade},
            {"latest_status", static_cast<int>(progress.latestStatus)},
            {"latest_mode", static_cast<int>(progress.latestMode)},
            {"latest_metrics", experimentMetricsToJson(progress.latestMetrics)},
        });
    }
    root["challenges"] = Json::array();
    for (const ChallengeProgress& progress : challenges) {
        root["challenges"].push_back({
            {"attempts", progress.attempts},
            {"completed", progress.completed},
            {"best_score", progress.bestScore},
            {"latest_score", progress.latestScore},
            {"latest_metrics", metricsToJson(progress.latestMetrics)},
        });
    }
    return root.dump(2) + "\n";
}

bool EducationProgress::deserialize(const std::string& serialized) {
    Json root;
    try {
        root = Json::parse(serialized);
    } catch (...) {
        return false;
    }
    if (!root.is_object() || !root.contains("schema_version") ||
        !root["schema_version"].is_number_integer() ||
        root["schema_version"].get<int>() != CURRENT_SCHEMA_VERSION ||
        !root.contains("lessons") || !root.contains("experiments") ||
        !root.contains("observations") || !root.contains("challenges") ||
        !root["challenges"].is_array() || root["challenges"].size() != challenges.size()) return false;

    std::vector<bool> parsedLessons;
    std::vector<bool> parsedExperiments;
    std::vector<std::string> parsedObservations;
    std::vector<ExperimentProgress> parsedExperimentEvaluations(experimentEvaluations.size());
    std::vector<ChallengeProgress> parsedChallenges(challenges.size());
    if (!boolVectorFromJson(root["lessons"], lessons.size(), parsedLessons) ||
        !boolVectorFromJson(root["experiments"], experiments.size(), parsedExperiments) ||
        !stringVectorFromJson(root["observations"], experimentObservations.size(), parsedObservations)) return false;

    if (root.contains("experiment_evaluations")) {
        if (!root["experiment_evaluations"].is_array() || root["experiment_evaluations"].size() != parsedExperimentEvaluations.size()) return false;
        for (std::size_t index = 0; index < parsedExperimentEvaluations.size(); ++index) {
            if (!experimentProgressFromJson(root["experiment_evaluations"][index], parsedExperimentEvaluations[index])) return false;
        }
    }

    for (std::size_t index = 0; index < parsedChallenges.size(); ++index) {
        const Json& item = root["challenges"][index];
        if (!item.is_object() || !item.contains("attempts") || !item["attempts"].is_number_integer() ||
            !item.contains("completed") || !item["completed"].is_boolean() ||
            !item.contains("best_score") || !item["best_score"].is_number() ||
            !item.contains("latest_score") || !item["latest_score"].is_number() ||
            !item.contains("latest_metrics")) return false;
        const int attempts = item["attempts"].get<int>();
        const double bestScore = item["best_score"].get<double>();
        const double latestScore = item["latest_score"].get<double>();
        if (attempts < 0 || !finiteScore(bestScore) || !finiteScore(latestScore) ||
            !metricsFromJson(item["latest_metrics"], parsedChallenges[index].latestMetrics)) return false;
        parsedChallenges[index].attempts = attempts;
        parsedChallenges[index].completed = item["completed"].get<bool>();
        parsedChallenges[index].bestScore = bestScore;
        parsedChallenges[index].latestScore = latestScore;
    }

    lessons = std::move(parsedLessons);
    experiments = std::move(parsedExperiments);
    experimentObservations = std::move(parsedObservations);
    experimentEvaluations = std::move(parsedExperimentEvaluations);
    challenges = std::move(parsedChallenges);
    return true;
}

bool EducationProgress::save(const std::filesystem::path& path) const {
    std::ofstream output(path);
    if (!output) return false;
    output << serialize();
    return output.good();
}

bool EducationProgress::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return false;
    std::ostringstream serialized;
    serialized << input.rdbuf();
    if (!input.good() && !input.eof()) return false;
    return deserialize(serialized.str());
}

std::string EducationProgress::exportText() const {
    std::ostringstream output;
    output << "BAGSOLAR_EDUCATION_PROGRESS_V1\n";
    output << "lessons," << lessons.size() << "\n";
    output << "experiments," << experiments.size() << "\n";
    output << "challenges," << challenges.size() << "\n";
    output << "evaluated_experiments," << experimentEvaluations.size() << "\n";
    for (std::size_t index = 0; index < experimentEvaluations.size(); ++index) {
        const ExperimentProgress& progress = experimentEvaluations[index];
        output << "experiment," << index << ',' << progress.attempts << ',' << (progress.completed ? 1 : 0)
               << ',' << std::fixed << std::setprecision(6) << progress.bestScore << ',' << progress.latestScore << ',' << progress.latestGrade << "\n";
    }
    for (std::size_t index = 0; index < challenges.size(); ++index) {
        const ChallengeProgress& progress = challenges[index];
        output << "challenge," << index << ',' << progress.attempts << ',' << (progress.completed ? 1 : 0)
               << ',' << std::fixed << std::setprecision(6) << progress.bestScore << ',' << progress.latestScore << "\n";
    }
    return output.str();
}

} // namespace bag
