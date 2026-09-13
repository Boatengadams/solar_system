#pragma once

#include <filesystem>

#include "ValidationTypes.hpp"

namespace bag {

ValidationReport runValidationSuite(const ValidationTolerances& tolerances = {});
bool writeValidationCsv(const std::filesystem::path& path, const ValidationReport& report);

} // namespace bag
