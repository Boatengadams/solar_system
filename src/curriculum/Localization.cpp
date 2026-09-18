#include "Localization.hpp"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace bag {

bool LocalizationTable::loadJson(const std::string& jsonText, std::string& error) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(jsonText);
    } catch (const std::exception& exception) {
        error = std::string("localization JSON parse failed: ") + exception.what();
        return false;
    }

    if (!root.is_object()) {
        error = "localization root must be a JSON object of key/value strings";
        return false;
    }

    std::unordered_map<std::string, std::string> next;
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().is_string()) {
            error = "localization value for key '" + it.key() + "' must be a string";
            return false;
        }
        next.emplace(it.key(), it.value().get<std::string>());
    }

    entries = std::move(next);
    error.clear();
    return true;
}

bool LocalizationTable::loadFile(const std::filesystem::path& path, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "unable to open localization file: " + path.string();
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return loadJson(buffer.str(), error);
}

void LocalizationTable::clear() {
    entries.clear();
}

bool LocalizationTable::contains(const std::string& key) const {
    return entries.find(key) != entries.end();
}

std::string LocalizationTable::translate(const std::string& key) const {
    const auto found = entries.find(key);
    if (found == entries.end()) return key;
    return found->second;
}

std::string LocalizationTable::translate(const std::string& key, const std::string& fallback) const {
    const auto found = entries.find(key);
    if (found == entries.end()) return fallback;
    return found->second;
}

} // namespace bag
