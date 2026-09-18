#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace bag {

class LocalizationTable {
public:
    bool loadJson(const std::string& jsonText, std::string& error);
    bool loadFile(const std::filesystem::path& path, std::string& error);
    void clear();
    bool contains(const std::string& key) const;
    std::string translate(const std::string& key) const;
    std::string translate(const std::string& key, const std::string& fallback) const;
    std::size_t size() const { return entries.size(); }

private:
    std::unordered_map<std::string, std::string> entries;
};

} // namespace bag
