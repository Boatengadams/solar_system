#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CurriculumTypes.hpp"

namespace bag {

struct MisconceptionTrigger {
    std::string misconceptionId;
    std::string statementKey;
    std::string responseKey;
    std::string experimentActivityId;
    std::string correctionKey;
};

class MisconceptionEngine {
public:
    static std::optional<MisconceptionTrigger> detect(const CurriculumCatalog& catalog,
                                                      const std::string& questionId,
                                                      const std::vector<std::string>& selectedOptionIds);

    static MisconceptionTrigger fromDefinition(const MisconceptionDefinition& definition);
};

} // namespace bag
