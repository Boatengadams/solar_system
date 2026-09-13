#include <iostream>
#include <string>

#include "ValidationRunner.hpp"

int main(int argc, char** argv) {
    const bag::ValidationReport report = bag::runValidationSuite();
    if (argc == 3 && std::string(argv[1]) == "--csv" && !bag::writeValidationCsv(argv[2], report)) {
        std::cerr << "unable to write validation CSV\n";
        return 2;
    }
    std::cout << report.toCsv();
    return report.allPassed() ? 0 : 1;
}
