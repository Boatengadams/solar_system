#include <cassert>
#include <iostream>

#include "validation/ValidationRunner.hpp"

int main() {
    const bag::ValidationReport report = bag::runValidationSuite();
    std::cout << report.toCsv();
    assert(report.allPassed());
    return 0;
}
