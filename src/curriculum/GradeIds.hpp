#pragma once

#include <optional>
#include <string>

namespace bag {

// Stable grade identifiers used for logic. Display strings come from i18n.
enum class GradeId {
    B1,
    B2,
    B3,
    B4,
    B5,
    B6,
    JHS1, // CCP Basic 7
    JHS2, // CCP Basic 8
    JHS3, // CCP Basic 9
    SHS1, // CCP Basic 10
    SHS2,
    SHS3,
};

enum class EducationPhase {
    LowerPrimary,  // B1–B3
    UpperPrimary,  // B4–B6
    JuniorHigh,    // JHS1–JHS3
    SeniorHigh,    // SHS1–SHS3
};

enum class PresentationLayer {
    Foundation, // B1–B3
    Explorer,   // B4–JHS3
    Scientist,  // SHS1–SHS3
};

enum class CurriculumAlignment {
    Official,               // verified NaCCA / MoE indicator
    SupplementaryEnrichment // related but not claimed as official
};

inline const char* gradeIdString(GradeId grade) {
    switch (grade) {
    case GradeId::B1: return "B1";
    case GradeId::B2: return "B2";
    case GradeId::B3: return "B3";
    case GradeId::B4: return "B4";
    case GradeId::B5: return "B5";
    case GradeId::B6: return "B6";
    case GradeId::JHS1: return "JHS1";
    case GradeId::JHS2: return "JHS2";
    case GradeId::JHS3: return "JHS3";
    case GradeId::SHS1: return "SHS1";
    case GradeId::SHS2: return "SHS2";
    case GradeId::SHS3: return "SHS3";
    }
    return "UNKNOWN";
}

inline std::optional<GradeId> parseGradeId(const std::string& value) {
    if (value == "B1") return GradeId::B1;
    if (value == "B2") return GradeId::B2;
    if (value == "B3") return GradeId::B3;
    if (value == "B4") return GradeId::B4;
    if (value == "B5") return GradeId::B5;
    if (value == "B6") return GradeId::B6;
    if (value == "JHS1" || value == "B7") return GradeId::JHS1;
    if (value == "JHS2" || value == "B8") return GradeId::JHS2;
    if (value == "JHS3" || value == "B9") return GradeId::JHS3;
    if (value == "SHS1" || value == "B10") return GradeId::SHS1;
    if (value == "SHS2") return GradeId::SHS2;
    if (value == "SHS3") return GradeId::SHS3;
    return std::nullopt;
}

inline EducationPhase educationPhaseFor(GradeId grade) {
    switch (grade) {
    case GradeId::B1:
    case GradeId::B2:
    case GradeId::B3:
        return EducationPhase::LowerPrimary;
    case GradeId::B4:
    case GradeId::B5:
    case GradeId::B6:
        return EducationPhase::UpperPrimary;
    case GradeId::JHS1:
    case GradeId::JHS2:
    case GradeId::JHS3:
        return EducationPhase::JuniorHigh;
    case GradeId::SHS1:
    case GradeId::SHS2:
    case GradeId::SHS3:
        return EducationPhase::SeniorHigh;
    }
    return EducationPhase::LowerPrimary;
}

inline PresentationLayer presentationLayerFor(GradeId grade) {
    switch (educationPhaseFor(grade)) {
    case EducationPhase::LowerPrimary:
        return PresentationLayer::Foundation;
    case EducationPhase::UpperPrimary:
    case EducationPhase::JuniorHigh:
        return PresentationLayer::Explorer;
    case EducationPhase::SeniorHigh:
        return PresentationLayer::Scientist;
    }
    return PresentationLayer::Foundation;
}

inline const char* educationPhaseName(EducationPhase phase) {
    switch (phase) {
    case EducationPhase::LowerPrimary: return "lower_primary";
    case EducationPhase::UpperPrimary: return "upper_primary";
    case EducationPhase::JuniorHigh: return "junior_high";
    case EducationPhase::SeniorHigh: return "senior_high";
    }
    return "unknown";
}

inline const char* presentationLayerName(PresentationLayer layer) {
    switch (layer) {
    case PresentationLayer::Foundation: return "foundation";
    case PresentationLayer::Explorer: return "explorer";
    case PresentationLayer::Scientist: return "scientist";
    }
    return "unknown";
}

inline const char* curriculumAlignmentName(CurriculumAlignment alignment) {
    switch (alignment) {
    case CurriculumAlignment::Official: return "official";
    case CurriculumAlignment::SupplementaryEnrichment: return "supplementary_enrichment";
    }
    return "unknown";
}

} // namespace bag
