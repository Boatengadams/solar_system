cmake_minimum_required(VERSION 3.16)

project(planets LANGUAGES CXX)

set(BAGSOLAR_VERSION 0.10.0)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BAGSOLAR_ENABLE_SPICE "Enable CSPICE-backed ephemeris queries" OFF)
set(CSPICE_INCLUDE_DIR "" CACHE PATH "Directory containing SpiceUsr.h")
set(CSPICE_LIBRARY "" CACHE FILEPATH "Path to the CSPICE library")

if(BAGSOLAR_ENABLE_SPICE)
    if(NOT CSPICE_INCLUDE_DIR OR NOT EXISTS "${CSPICE_INCLUDE_DIR}/SpiceUsr.h")
        message(FATAL_ERROR "BAGSOLAR_ENABLE_SPICE=ON requires CSPICE_INCLUDE_DIR containing SpiceUsr.h")
    endif()
    if(NOT CSPICE_LIBRARY OR NOT EXISTS "${CSPICE_LIBRARY}")
        message(FATAL_ERROR "BAGSOLAR_ENABLE_SPICE=ON requires CSPICE_LIBRARY pointing to CSPICE")
    endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)

set(BAGSOLAR_SOURCES
    src/app/main.cpp
    src/astronomy/EphemerisTypes.cpp
    src/astronomy/EphemerisComparison.cpp
    src/astronomy/CurlHttpClient.cpp
    src/astronomy/HorizonsParser.cpp
    src/astronomy/HorizonsProvider.cpp
    src/astronomy/HorizonsRequest.cpp
    src/astronomy/JsonEphemerisProvider.cpp
    src/astronomy/LocalEphemerisProvider.cpp
    src/astronomy/SpiceProvider.cpp
    src/core/Logger.cpp
    src/data/BodyFactory.cpp
    src/data/ScenarioLoader.cpp
    src/data/ScenarioSerializer.cpp
    src/education/EducationContent.cpp
    src/education/EducationProgress.cpp
    src/input/InputController.cpp
    src/physics/PhysicsEngine.cpp
    src/physics/IntegratorBenchmark.cpp
    src/rendering/Renderer.cpp
    src/simulation/Simulation.cpp
    src/spacecraft/Spacecraft.cpp
    src/missions/Mission.cpp
    src/missions/TrajectoryPrediction.cpp
    src/telemetry/TelemetryAnalyzer.cpp
    src/telemetry/TelemetryCollector.cpp
    src/telemetry/TelemetryExporter.cpp
    src/telemetry/TelemetrySession.cpp
    src/ui/HUD.cpp
)

set(BAGSOLAR_VALIDATION_SOURCES
    src/validation/ValidationRunner.cpp
    src/physics/PhysicsEngine.cpp
)

add_executable(planets ${BAGSOLAR_SOURCES})
target_include_directories(planets PRIVATE src)
target_include_directories(planets PRIVATE third_party/nlohmann-json3-dev/usr/include)
target_link_libraries(planets PRIVATE PkgConfig::RAYLIB)
target_compile_definitions(planets PRIVATE BAGSOLAR_ENABLE_SPICE=$<BOOL:${BAGSOLAR_ENABLE_SPICE}>)
if(BAGSOLAR_ENABLE_SPICE)
    target_include_directories(planets PRIVATE "${CSPICE_INCLUDE_DIR}")
    target_link_libraries(planets PRIVATE "${CSPICE_LIBRARY}")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(planets PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(bagsolar_physics_tests
        tests/physics_tests.cpp
        src/data/BodyFactory.cpp
        src/physics/PhysicsEngine.cpp
        src/physics/IntegratorBenchmark.cpp
    )
    target_include_directories(bagsolar_physics_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_physics_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_physics_tests COMMAND bagsolar_physics_tests)
    set_tests_properties(bagsolar_physics_tests PROPERTIES LABELS "unit;regression")

    add_executable(bagsolar_data_tests
        tests/data_tests.cpp
        src/data/BodyFactory.cpp
        src/data/ScenarioLoader.cpp
        src/data/ScenarioSerializer.cpp
        src/physics/PhysicsEngine.cpp
    )
    target_include_directories(bagsolar_data_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_data_tests PRIVATE BAGSOLAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_data_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_data_tests COMMAND bagsolar_data_tests)
    set_tests_properties(bagsolar_data_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "integration;regression")

    add_executable(bagsolar_validation_tests
        tests/validation_tests.cpp
        ${BAGSOLAR_VALIDATION_SOURCES}
    )
    target_include_directories(bagsolar_validation_tests PRIVATE src)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_validation_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_validation_tests COMMAND bagsolar_validation_tests)
    set_tests_properties(bagsolar_validation_tests PROPERTIES LABELS "validation")

    add_executable(bagsolar_telemetry_tests
        tests/telemetry_tests.cpp
        src/astronomy/EphemerisTypes.cpp
        src/astronomy/CurlHttpClient.cpp
        src/telemetry/TelemetryAnalyzer.cpp
        src/telemetry/TelemetryCollector.cpp
        src/telemetry/TelemetryExporter.cpp
        src/telemetry/TelemetrySession.cpp
        src/physics/PhysicsEngine.cpp
        src/simulation/Simulation.cpp
        src/data/BodyFactory.cpp
        src/data/ScenarioLoader.cpp
        src/data/ScenarioSerializer.cpp
        src/core/Logger.cpp
    )
    target_include_directories(bagsolar_telemetry_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_telemetry_tests PRIVATE BAGSOLAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_telemetry_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_telemetry_tests COMMAND bagsolar_telemetry_tests)
    set_tests_properties(bagsolar_telemetry_tests PROPERTIES LABELS "telemetry;regression" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_executable(bagsolar_ephemeris_tests
        tests/ephemeris_tests.cpp
        src/astronomy/EphemerisTypes.cpp
        src/astronomy/HorizonsParser.cpp
        src/astronomy/HorizonsProvider.cpp
        src/astronomy/HorizonsRequest.cpp
        src/astronomy/JsonEphemerisProvider.cpp
        src/astronomy/LocalEphemerisProvider.cpp
        src/telemetry/TelemetryAnalyzer.cpp
        src/telemetry/TelemetryCollector.cpp
        src/telemetry/TelemetryExporter.cpp
        src/telemetry/TelemetrySession.cpp
        src/physics/PhysicsEngine.cpp
        src/simulation/Simulation.cpp
        src/data/BodyFactory.cpp
        src/data/ScenarioLoader.cpp
        src/data/ScenarioSerializer.cpp
        src/core/Logger.cpp
    )
    target_include_directories(bagsolar_ephemeris_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_ephemeris_tests PRIVATE BAGSOLAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_ephemeris_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_ephemeris_tests COMMAND bagsolar_ephemeris_tests)
    set_tests_properties(bagsolar_ephemeris_tests PROPERTIES LABELS "astronomy;ephemeris;regression" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    add_executable(bagsolar_spacecraft_tests
        tests/spacecraft_tests.cpp
        src/spacecraft/Spacecraft.cpp
        src/missions/Mission.cpp
        src/missions/TrajectoryPrediction.cpp
        src/physics/PhysicsEngine.cpp
    )
    target_include_directories(bagsolar_spacecraft_tests PRIVATE src)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_spacecraft_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_spacecraft_tests COMMAND bagsolar_spacecraft_tests)
    set_tests_properties(bagsolar_spacecraft_tests PROPERTIES LABELS "mission;regression;spacecraft")

    add_executable(bagsolar_education_tests
        tests/education_tests.cpp
        src/education/EducationContent.cpp
        src/education/EducationProgress.cpp
    )
    target_include_directories(bagsolar_education_tests PRIVATE src)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_education_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_education_tests COMMAND bagsolar_education_tests)
    set_tests_properties(bagsolar_education_tests PROPERTIES LABELS "education;regression")

    add_executable(bagsolar_advanced_data_tests
        tests/advanced_data_tests.cpp
        src/astronomy/EphemerisTypes.cpp
        src/astronomy/EphemerisComparison.cpp
        src/astronomy/SpiceProvider.cpp
    )
    target_include_directories(bagsolar_advanced_data_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_advanced_data_tests PRIVATE BAGSOLAR_ENABLE_SPICE=$<BOOL:${BAGSOLAR_ENABLE_SPICE}>)
    if(BAGSOLAR_ENABLE_SPICE)
        target_include_directories(bagsolar_advanced_data_tests PRIVATE "${CSPICE_INCLUDE_DIR}")
        target_link_libraries(bagsolar_advanced_data_tests PRIVATE "${CSPICE_LIBRARY}")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_advanced_data_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_advanced_data_tests COMMAND bagsolar_advanced_data_tests)
    set_tests_properties(bagsolar_advanced_data_tests PROPERTIES LABELS "astronomy;spice;regression")

    if(BAGSOLAR_ENABLE_SPICE)
        add_executable(bagsolar_spice_live_tests
            tests/spice_live_tests.cpp
            src/astronomy/EphemerisTypes.cpp
            src/astronomy/SpiceProvider.cpp
            src/telemetry/TelemetryAnalyzer.cpp
            src/telemetry/TelemetryCollector.cpp
            src/telemetry/TelemetryExporter.cpp
            src/telemetry/TelemetrySession.cpp
            src/simulation/Simulation.cpp
            src/data/BodyFactory.cpp
            src/data/ScenarioLoader.cpp
            src/data/ScenarioSerializer.cpp
            src/physics/PhysicsEngine.cpp
            src/core/Logger.cpp
        )
        target_include_directories(bagsolar_spice_live_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include "${CSPICE_INCLUDE_DIR}")
        target_compile_definitions(bagsolar_spice_live_tests PRIVATE BAGSOLAR_ENABLE_SPICE=1)
        target_link_libraries(bagsolar_spice_live_tests PRIVATE "${CSPICE_LIBRARY}")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(bagsolar_spice_live_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME bagsolar_spice_live_tests COMMAND bagsolar_spice_live_tests)
        set_tests_properties(bagsolar_spice_live_tests PROPERTIES LABELS "spice;live" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endif()
endif()

add_executable(bagsolar_validation
    src/validation/main.cpp
    ${BAGSOLAR_VALIDATION_SOURCES}
)
target_include_directories(bagsolar_validation PRIVATE src)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bagsolar_validation PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(bagsolar_ephemeris
    src/astronomy/main.cpp
    src/astronomy/CurlHttpClient.cpp
    src/astronomy/EphemerisTypes.cpp
    src/astronomy/HorizonsParser.cpp
    src/astronomy/HorizonsProvider.cpp
    src/astronomy/HorizonsRequest.cpp
    src/astronomy/JsonEphemerisProvider.cpp
    src/astronomy/LocalEphemerisProvider.cpp
    src/astronomy/SpiceProvider.cpp
)
target_include_directories(bagsolar_ephemeris PRIVATE src third_party/nlohmann-json3-dev/usr/include)
target_compile_definitions(bagsolar_ephemeris PRIVATE BAGSOLAR_ENABLE_SPICE=$<BOOL:${BAGSOLAR_ENABLE_SPICE}>)
if(BAGSOLAR_ENABLE_SPICE)
    target_include_directories(bagsolar_ephemeris PRIVATE "${CSPICE_INCLUDE_DIR}")
    target_link_libraries(bagsolar_ephemeris PRIVATE "${CSPICE_LIBRARY}")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bagsolar_ephemeris PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS planets bagsolar_validation bagsolar_ephemeris
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/bagsolar/data)
install(DIRECTORY BAGSOLAR_codex_docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/bagsolar/docs)

set(CPACK_PACKAGE_NAME "BAGSOLAR")
set(CPACK_PACKAGE_VENDOR "BAGSOLAR Project")
set(CPACK_PACKAGE_VERSION ${BAGSOLAR_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Scientific astrodynamics laboratory")
set(CPACK_GENERATOR "TGZ")
include(CPack)
