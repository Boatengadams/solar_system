#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <string>

#include "curriculum/AssessmentEngine.hpp"
#include "curriculum/CurriculumLoader.hpp"
#include "curriculum/GradeIds.hpp"
#include "curriculum/LearningLabSession.hpp"
#include "curriculum/Localization.hpp"
#include "curriculum/MasteryModel.hpp"
#include "curriculum/MisconceptionEngine.hpp"

namespace {

std::filesystem::path curriculumRoot() {
#if defined(BAGSOLAR_SOURCE_DIR)
    return std::filesystem::path(BAGSOLAR_SOURCE_DIR) / "data" / "curriculum" / "ghana_nacca_2019";
#else
    return std::filesystem::path("data") / "curriculum" / "ghana_nacca_2019";
#endif
}

} // namespace

int main() {
    using namespace bag;

    assert(parseGradeId("JHS1").value() == GradeId::JHS1);
    assert(parseGradeId("B7").value() == GradeId::JHS1);
    assert(parseGradeId("B10").value() == GradeId::SHS1);
    assert(presentationLayerFor(GradeId::B2) == PresentationLayer::Foundation);
    assert(presentationLayerFor(GradeId::B5) == PresentationLayer::Explorer);
    assert(presentationLayerFor(GradeId::SHS2) == PresentationLayer::Scientist);

    const auto loaded = CurriculumLoader::loadFromDirectory(curriculumRoot());
    assert(loaded.ok);
    assert(loaded.catalog.manifest.curriculumVersion == "ghana_nacca_2019");
    assert(loaded.catalog.manifest.grades.size() == 12);
    assert(!loaded.catalog.activities.empty());
    assert(!loaded.catalog.questions.empty());
    assert(!loaded.catalog.missions.empty());

    const auto b1 = CurriculumLoader::activitiesForGrade(loaded.catalog, GradeId::B1);
    assert(b1.size() >= 2);
    for (const auto* activity : b1) {
        assert(activity->presentationLayer == PresentationLayer::Foundation);
        assert(activity->grade == GradeId::B1);
    }

    const auto jhs1 = CurriculumLoader::activitiesForGrade(loaded.catalog, GradeId::JHS1);
    assert(!jhs1.empty());
    const CurriculumActivity* inner = CurriculumLoader::findActivity(loaded.catalog, "jhs1_solar_inner_planets_001");
    assert(inner != nullptr);
    assert(inner->alignment == CurriculumAlignment::Official);
    assert(inner->curriculumReference == "B7.3.2.1.1");

    const CurriculumActivity* enrichment =
        CurriculumLoader::findActivity(loaded.catalog, "b5_ss_luminous");
    assert(enrichment != nullptr);
    assert(enrichment->alignment == CurriculumAlignment::SupplementaryEnrichment);
    assert(enrichment->curriculumReference == "Supplementary enrichment");

    LocalizationTable i18n;
    std::string i18nError;
    assert(i18n.loadFile(curriculumRoot() / "i18n" / "en.json", i18nError));
    assert(i18n.translate("solarSystem.sun") == "Sun");
    assert(i18n.translate("activity.b1_ss_001.title") == "What can you see in the sky?");

    const CurriculumQuestion* skyQuestion = CurriculumLoader::findQuestion(loaded.catalog, "q_b1_ss_001");
    assert(skyQuestion != nullptr);
    AssessmentSubmission correctSky;
    correctSky.questionId = skyQuestion->id;
    correctSky.selectedOptionIds = {"sun", "moon", "stars"};
    const AssessmentResult skyResult = AssessmentEngine::score(*skyQuestion, correctSky);
    assert(skyResult.correct && skyResult.score == 100.0);

    AssessmentSubmission wrongLight;
    wrongLight.questionId = "q_b3_ss_001";
    wrongLight.selectedOptionIds = {"moon"};
    const CurriculumQuestion* lightQuestion = CurriculumLoader::findQuestion(loaded.catalog, "q_b3_ss_001");
    assert(lightQuestion != nullptr);
    const AssessmentResult lightResult = AssessmentEngine::score(*lightQuestion, wrongLight);
    assert(!lightResult.correct);
    const auto misconception = MisconceptionEngine::detect(loaded.catalog, "q_b3_ss_001", {"moon"});
    assert(misconception.has_value());
    assert(misconception->misconceptionId == "mc_moon_own_light");
    assert(i18n.translate(misconception->responseKey) == "Let's investigate.");

    LearningLabSession session;
    const CurriculumActivity* dayNight = CurriculumLoader::findActivity(loaded.catalog, "b1_ss_002");
    assert(dayNight != nullptr);
    assert(session.start(*dayNight));
    assert(session.state().step == LearningLabStep::Instruction);
    assert(session.advance());
    assert(session.state().step == LearningLabStep::Predict);
    assert(!session.advance());
    assert(session.recordPrediction("It will be day"));
    assert(session.advance());
    assert(session.state().step == LearningLabStep::Experiment);

    MasteryModel mastery;
    TopicMasterySignals signals;
    signals.topicId = "inner_planets";
    signals.conceptUnderstanding = 90.0;
    signals.activityPerformance = 85.0;
    signals.assessmentPerformance = 80.0;
    signals.experimentPerformance = 75.0;
    signals.attempts = 3;
    signals.misconceptionsCorrected = 1;
    const TopicMastery topic = mastery.evaluate(signals);
    assert(topic.score >= 80.0);
    assert(topic.mastered);

    // Adding a new grade activity must not require engine changes: loader already filters by GradeId.
    assert(CurriculumLoader::activitiesForGrade(loaded.catalog, GradeId::SHS3).size() >= 1);

    return 0;
}
