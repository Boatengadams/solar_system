#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CurriculumTypes.hpp"

namespace bag {

struct CurriculumLoadResult {
    bool ok = false;
    std::string error;
    CurriculumCatalog catalog;

    explicit operator bool() const { return ok; }
};

class CurriculumLoader {
public:
    static CurriculumLoadResult loadFromDirectory(const std::filesystem::path& curriculumRoot);
    static CurriculumLoadResult loadManifestOnly(const std::filesystem::path& manifestPath);

    static std::vector<const CurriculumActivity*> activitiesForGrade(const CurriculumCatalog& catalog,
                                                                    GradeId grade);
    static std::vector<const CurriculumActivity*> activitiesForLayer(const CurriculumCatalog& catalog,
                                                                    PresentationLayer layer);
    static const CurriculumActivity* findActivity(const CurriculumCatalog& catalog, const std::string& id);
    static const CurriculumQuestion* findQuestion(const CurriculumCatalog& catalog, const std::string& id);
    static const CurriculumMission* findMission(const CurriculumCatalog& catalog, const std::string& id);
    static const MisconceptionDefinition* findMisconception(const CurriculumCatalog& catalog,
                                                            const std::string& id);

    static std::vector<std::string> validateCatalog(const CurriculumCatalog& catalog);
};

} // namespace bag
