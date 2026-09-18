CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
SOURCES := $(shell find src -name '*.cpp' ! -path 'src/validation/main.cpp' ! -path 'src/astronomy/main.cpp' -print)
CPPFLAGS := -Isrc -Ithird_party/nlohmann-json3-dev/usr/include

.PHONY: all run test validation ephemeris education clean package

ifeq ($(strip $(RAYLIB_LIBS)),)
$(error raylib is not installed; see README.md development instructions)
endif

all: BAGS_LAB

BAGS_LAB: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(RAYLIB_CFLAGS) $(SOURCES) -o $@ $(RAYLIB_LIBS)

# Compatibility alias
planets: BAGS_LAB
	cp -f BAGS_LAB planets

run: BAGS_LAB
	./BAGS_LAB

validation:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/validation/main.cpp src/validation/ValidationRunner.cpp src/validation/PredictionComparison.cpp src/astronomy/EphemerisTypes.cpp src/physics/PhysicsEngine.cpp -o bagsolar_validation
	./bagsolar_validation

ephemeris:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/astronomy/main.cpp src/astronomy/CurlHttpClient.cpp src/astronomy/EphemerisTypes.cpp src/astronomy/EphemerisComparison.cpp src/astronomy/HorizonsParser.cpp src/astronomy/HorizonsProvider.cpp src/astronomy/HorizonsRequest.cpp src/astronomy/JsonEphemerisProvider.cpp src/astronomy/LocalEphemerisProvider.cpp src/astronomy/SpiceProvider.cpp -o bagsolar_ephemeris

education:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/education_tests.cpp src/astronomy/EphemerisTypes.cpp src/education/EducationChallenges.cpp src/education/ExperimentEvaluation.cpp src/education/EducationContent.cpp src/education/EducationCatalog.cpp src/education/EducationProgress.cpp src/education/EducationWorkflow.cpp src/education/LearnerReport.cpp src/missions/Mission.cpp src/spacecraft/Spacecraft.cpp src/physics/PhysicsEngine.cpp src/physics/IntegratorBenchmark.cpp src/validation/PredictionComparison.cpp -o bagsolar_education_tests
	./bagsolar_education_tests

test:
	cmake --preset debug
	cmake --build --preset debug --parallel 2
	ctest --preset debug --output-on-failure

package:
	./tools/package_portable.sh linux-x64

clean:
	rm -f BAGS_LAB planets bagsolar_validation bagsolar_ephemeris bagsolar_education_tests
