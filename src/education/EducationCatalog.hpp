#pragma once

#include <string>
#include <vector>

namespace bag {

enum class EducationDifficulty {
    Introductory,
    Intermediate,
    Advanced,
};

struct AuthoredLesson {
    std::string id;
    std::string title;
    std::string shortDescription;
    std::vector<std::string> objectives;
    std::vector<std::string> prerequisites;
    std::vector<std::string> sections;
    std::vector<std::string> experimentIds;
    std::vector<std::string> challengeIds;
    EducationDifficulty difficulty = EducationDifficulty::Introductory;
    int estimatedMinutes = 0;
};

struct EducationCatalogValidation {
    bool valid = false;
    std::vector<std::string> errors;
};

const AuthoredLesson& authoredLessonAt(int index);
const AuthoredLesson* findAuthoredLesson(const std::string& id);
int authoredLessonCount();
EducationCatalogValidation validateEducationCatalog();
EducationCatalogValidation validateEducationCatalog(const std::vector<AuthoredLesson>& lessons);
const char* educationDifficultyName(EducationDifficulty difficulty);

} // namespace bag
