#include "Logger.hpp"

#include <iostream>

namespace bag {

void logInfo(const std::string& message) {
    std::clog << "[INFO] " << message << '\n';
}

void logError(const std::string& message) {
    std::cerr << "[ERROR] " << message << '\n';
}

} // namespace bag
