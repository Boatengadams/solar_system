#include "MisconceptionEngine.hpp"

#include "CurriculumLoader.hpp"

#include <algorithm>

namespace bag {

MisconceptionTrigger MisconceptionEngine::fromDefinition(const MisconceptionDefinition& definition) {
    MisconceptionTrigger trigger;
    trigger.misconceptionId = definition.id;
    trigger.statementKey = definition.statementKey;
    trigger.responseKey = definition.responseKey;
    trigger.experimentActivityId = definition.experimentActivityId;
    trigger.correctionKey = definition.correctionKey;
    return trigger;
}

std::optional<MisconceptionTrigger> MisconceptionEngine::detect(const CurriculumCatalog& catalog,
                                                                const std::string& questionId,
                                                                const std::vector<std::string>& selectedOptionIds) {
    const CurriculumQuestion* question = CurriculumLoader::findQuestion(catalog, questionId);
    if (!question || question->misconceptionId.empty()) return std::nullopt;

    const MisconceptionDefinition* definition =
        CurriculumLoader::findMisconception(catalog, question->misconceptionId);
    if (!definition) return std::nullopt;

    if (!definition->triggerAnswerIds.empty()) {
        const bool matched = std::any_of(
            selectedOptionIds.begin(), selectedOptionIds.end(), [&](const std::string& selected) {
                return std::find(definition->triggerAnswerIds.begin(), definition->triggerAnswerIds.end(),
                                 selected) != definition->triggerAnswerIds.end();
            });
        if (!matched) return std::nullopt;
    } else if (!selectedOptionIds.empty()) {
        // Fall back: any incorrect selection on a misconception-tagged question.
        const bool anyIncorrect = !std::all_of(
            selectedOptionIds.begin(), selectedOptionIds.end(), [&](const std::string& selected) {
                return std::find(question->correctAnswerIds.begin(), question->correctAnswerIds.end(), selected) !=
                       question->correctAnswerIds.end();
            });
        if (!anyIncorrect) return std::nullopt;
    } else {
        return std::nullopt;
    }

    return fromDefinition(*definition);
}

} // namespace bag
