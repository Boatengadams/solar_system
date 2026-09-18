cmake_minimum_required(VERSION 3.16)

project(BAGS_LAB LANGUAGES CXX)

set(BAGS_LAB_VERSION 1.0.0)
set(BAGSOLAR_VERSION ${BAGS_LAB_VERSION})  # scientific/compat alias

include(CTest)
include(GNUInstallDirs)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BagsLabPlatform.cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BAGSOLAR_ENABLE_SPICE "Enable CSPICE-backed ephemeris queries (optional; BAGS_LAB runs without it)" OFF)
option(BAGS_LAB_INSTALL_PLANET_MODELS "Install planet GLB presentation models with the package" ON)
option(BAGS_LAB_BUNDLE_RAYLIB "Statically link project-controlled raylib 6.0 (portable USB builds; no TIFF)" OFF)
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

# PkgConfig only when consuming a system raylib (typical developer builds).
if(NOT BAGS_LAB_BUNDLE_RAYLIB)
    find_package(PkgConfig REQUIRED)
endif()

# Relocatable private libs for portable/USB layouts (ignored when unused).
if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()

if(BAGS_LAB_BUNDLE_RAYLIB)
    if(CMAKE_VERSION VERSION_LESS "3.25")
        message(FATAL_ERROR "BAGS_LAB_BUNDLE_RAYLIB requires CMake >= 3.25 (raylib 6.0)")
    endif()
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(WITH_PIC ON CACHE BOOL "" FORCE)
    # PNG stays enabled (prepared GLB textures). JPEG via stb (no libjpeg). No TIFF.
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_PNG ON CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
    # Portable builds always use raylib's bundled GLFW (OBJECT → static into
    # libraylib). Do not depend on host libglfw.so.3 or copy a developer-machine
    # GLFW into the USB package. Linux still uses the X11 backend; X11/GL remain
    # host system libraries.
    set(USE_EXTERNAL_GLFW OFF CACHE BOOL "" FORCE)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/6.0.tar.gz
        URL_HASH SHA256=2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b
    )
    FetchContent_MakeAvailable(raylib)
    if(TARGET raylib)
        target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_PNG=1 SUPPORT_FILEFORMAT_JPG=1)
    endif()
    set(BAGS_LAB_RAYLIB_TARGET raylib)
    message(STATUS "BAGS_LAB: bundling static raylib 6.0 + bundled GLFW (PNG/JPEG via stb; no libtiff/libglfw.so) for ${BAGS_LAB_DIST_ID}")
else()
    if(WIN32)
        message(FATAL_ERROR "On Windows set -DBAGS_LAB_BUNDLE_RAYLIB=ON (system pkg-config raylib is not the release path)")
    endif()
    pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)
    set(BAGS_LAB_RAYLIB_TARGET PkgConfig::RAYLIB)
endif()

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
    src/data/ResourceRoot.cpp
    src/data/ScenarioLoader.cpp
    src/data/ScenarioSerializer.cpp
    src/education/EducationContent.cpp
    src/education/EducationCatalog.cpp
    src/education/EducationChallenges.cpp
    src/education/ExperimentEvaluation.cpp
    src/education/EducationProgress.cpp
    src/education/EducationWorkflow.cpp
    src/education/LearnerReport.cpp
    src/curriculum/Localization.cpp
    src/curriculum/CurriculumLoader.cpp
    src/curriculum/AssessmentEngine.cpp
    src/curriculum/MasteryModel.cpp
    src/curriculum/MisconceptionEngine.cpp
    src/curriculum/LearningLabSession.cpp
    src/input/InputController.cpp
    src/physics/PhysicsEngine.cpp
    src/physics/IntegratorBenchmark.cpp
    src/platform/ExternalCommand.cpp
    src/rendering/PlanetAssetManager.cpp
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
    src/validation/PredictionComparison.cpp
)

set(BAGSOLAR_VALIDATION_SOURCES
    src/validation/ValidationRunner.cpp
    src/validation/PredictionComparison.cpp
    src/astronomy/EphemerisTypes.cpp
    src/physics/PhysicsEngine.cpp
)

if(APPLE)
    add_executable(BAGS_LAB MACOSX_BUNDLE ${BAGSOLAR_SOURCES})
    set_target_properties(BAGS_LAB PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "BAGS_LAB"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.bags-lab.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${BAGS_LAB_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${BAGS_LAB_VERSION}"
        MACOSX_BUNDLE_INFO_STRING "BAGS_LAB portable scientific orbital laboratory"
    )
else()
    add_executable(BAGS_LAB ${BAGSOLAR_SOURCES})
endif()
target_include_directories(BAGS_LAB PRIVATE src)
target_include_directories(BAGS_LAB PRIVATE third_party/nlohmann-json3-dev/usr/include)
target_link_libraries(BAGS_LAB PRIVATE ${BAGS_LAB_RAYLIB_TARGET})
target_compile_definitions(BAGS_LAB PRIVATE BAGSOLAR_ENABLE_SPICE=$<BOOL:${BAGSOLAR_ENABLE_SPICE}>)
target_compile_definitions(BAGS_LAB PRIVATE BAGS_LAB_INSTALL_DATA_SUBDIR="${CMAKE_INSTALL_DATADIR}/bags_lab/data")
target_compile_definitions(BAGS_LAB PRIVATE BAGSOLAR_INSTALL_DATA_SUBDIR="${CMAKE_INSTALL_DATADIR}/bags_lab/data")
if(BAGSOLAR_ENABLE_SPICE)
    target_include_directories(BAGS_LAB PRIVATE "${CSPICE_INCLUDE_DIR}")
    target_link_libraries(BAGS_LAB PRIVATE "${CSPICE_LIBRARY}")
endif()

if(UNIX AND NOT APPLE)
    # Portable USB layout:   <root>/BAGS_LAB + <root>/lib
    # CPack/install layout:  <prefix>/bin/BAGS_LAB + <prefix>/bin/lib or <prefix>/lib
    set_target_properties(BAGS_LAB PROPERTIES
        BUILD_RPATH "$ORIGIN/lib"
        INSTALL_RPATH "$ORIGIN/lib:$ORIGIN/../lib"
        INSTALL_RPATH_USE_LINK_PATH FALSE
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(BAGS_LAB PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(BAGS_LAB PRIVATE /W4 /EHsc)
    target_compile_definitions(BAGS_LAB PRIVATE _CRT_SECURE_NO_WARNINGS)
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
        src/platform/ExternalCommand.cpp
        src/telemetry/TelemetryAnalyzer.cpp
        src/astronomy/LocalEphemerisProvider.cpp
        src/telemetry/TelemetryCollector.cpp
        src/telemetry/TelemetryExporter.cpp
        src/telemetry/TelemetrySession.cpp
        src/education/EducationChallenges.cpp
        src/education/ExperimentEvaluation.cpp
        src/education/EducationContent.cpp
        src/education/EducationCatalog.cpp
        src/education/EducationProgress.cpp
        src/education/EducationWorkflow.cpp
        src/education/LearnerReport.cpp
        src/curriculum/Localization.cpp
        src/curriculum/CurriculumLoader.cpp
        src/curriculum/AssessmentEngine.cpp
        src/curriculum/MasteryModel.cpp
        src/curriculum/MisconceptionEngine.cpp
        src/curriculum/LearningLabSession.cpp
        src/missions/Mission.cpp
        src/spacecraft/Spacecraft.cpp
        src/physics/PhysicsEngine.cpp
        src/physics/IntegratorBenchmark.cpp
        src/simulation/Simulation.cpp
        src/data/ResourceRoot.cpp
        src/validation/PredictionComparison.cpp
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
        src/education/EducationChallenges.cpp
        src/education/ExperimentEvaluation.cpp
        src/education/EducationContent.cpp
        src/education/EducationCatalog.cpp
        src/education/EducationProgress.cpp
        src/education/EducationWorkflow.cpp
        src/education/LearnerReport.cpp
        src/curriculum/Localization.cpp
        src/curriculum/CurriculumLoader.cpp
        src/curriculum/AssessmentEngine.cpp
        src/curriculum/MasteryModel.cpp
        src/curriculum/MisconceptionEngine.cpp
        src/curriculum/LearningLabSession.cpp
        src/missions/Mission.cpp
        src/spacecraft/Spacecraft.cpp
        src/physics/PhysicsEngine.cpp
        src/physics/IntegratorBenchmark.cpp
        src/simulation/Simulation.cpp
        src/data/ResourceRoot.cpp
        src/validation/PredictionComparison.cpp
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
        src/astronomy/EphemerisTypes.cpp
        src/education/EducationChallenges.cpp
        src/education/ExperimentEvaluation.cpp
        src/education/EducationContent.cpp
        src/education/EducationCatalog.cpp
        src/education/EducationProgress.cpp
        src/education/EducationWorkflow.cpp
        src/education/LearnerReport.cpp
        src/missions/Mission.cpp
        src/spacecraft/Spacecraft.cpp
        src/physics/PhysicsEngine.cpp
        src/physics/IntegratorBenchmark.cpp
        src/validation/PredictionComparison.cpp
    )
    target_include_directories(bagsolar_education_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_education_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_education_tests COMMAND bagsolar_education_tests)
    set_tests_properties(bagsolar_education_tests PROPERTIES LABELS "education;regression")

    add_executable(bagsolar_curriculum_tests
        tests/curriculum_tests.cpp
        src/curriculum/Localization.cpp
        src/curriculum/CurriculumLoader.cpp
        src/curriculum/AssessmentEngine.cpp
        src/curriculum/MasteryModel.cpp
        src/curriculum/MisconceptionEngine.cpp
        src/curriculum/LearningLabSession.cpp
    )
    target_include_directories(bagsolar_curriculum_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_curriculum_tests PRIVATE BAGSOLAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_curriculum_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_curriculum_tests COMMAND bagsolar_curriculum_tests)
    set_tests_properties(bagsolar_curriculum_tests PROPERTIES LABELS "education;curriculum;regression" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

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
            src/data/ResourceRoot.cpp
            src/data/BodyFactory.cpp
            src/data/ScenarioLoader.cpp
            src/data/ScenarioSerializer.cpp
            src/education/EducationContent.cpp
            src/education/EducationCatalog.cpp
            src/education/EducationChallenges.cpp
            src/education/ExperimentEvaluation.cpp
            src/education/EducationProgress.cpp
            src/education/EducationWorkflow.cpp
            src/education/LearnerReport.cpp
            src/curriculum/Localization.cpp
            src/curriculum/CurriculumLoader.cpp
            src/curriculum/AssessmentEngine.cpp
            src/curriculum/MasteryModel.cpp
            src/curriculum/MisconceptionEngine.cpp
            src/curriculum/LearningLabSession.cpp
            src/missions/Mission.cpp
            src/spacecraft/Spacecraft.cpp
            src/physics/PhysicsEngine.cpp
            src/physics/IntegratorBenchmark.cpp
            src/validation/PredictionComparison.cpp
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

add_executable(bagsolar_resource_smoke
    tests/resource_smoke.cpp
    src/data/BodyFactory.cpp
    src/data/ResourceRoot.cpp
    src/data/ScenarioLoader.cpp
    src/data/ScenarioSerializer.cpp
    src/physics/PhysicsEngine.cpp
)
target_include_directories(bagsolar_resource_smoke PRIVATE src third_party/nlohmann-json3-dev/usr/include)
target_compile_definitions(bagsolar_resource_smoke PRIVATE BAGS_LAB_INSTALL_DATA_SUBDIR="${CMAKE_INSTALL_DATADIR}/bags_lab/data")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bagsolar_resource_smoke PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(bagsolar_selection_tests tests/selection_tests.cpp)
    target_include_directories(bagsolar_selection_tests PRIVATE src)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_selection_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_selection_tests COMMAND bagsolar_selection_tests)
    set_tests_properties(bagsolar_selection_tests PROPERTIES LABELS "unit;rendering;input")

    add_executable(bagsolar_planet_presentation_tests tests/planet_presentation_tests.cpp)
    target_include_directories(bagsolar_planet_presentation_tests PRIVATE src third_party/nlohmann-json3-dev/usr/include)
    target_compile_definitions(bagsolar_planet_presentation_tests PRIVATE BAGSOLAR_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_planet_presentation_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_planet_presentation_tests COMMAND bagsolar_planet_presentation_tests)
    set_tests_properties(bagsolar_planet_presentation_tests PROPERTIES LABELS "unit;rendering;assets")

    add_executable(bagsolar_resource_root_tests
        tests/resource_root_tests.cpp
        src/data/ResourceRoot.cpp
    )
    target_include_directories(bagsolar_resource_root_tests PRIVATE src)
    target_compile_definitions(bagsolar_resource_root_tests PRIVATE BAGS_LAB_INSTALL_DATA_SUBDIR="${CMAKE_INSTALL_DATADIR}/bags_lab/data")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bagsolar_resource_root_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME bagsolar_resource_root_tests COMMAND bagsolar_resource_root_tests)
    set_tests_properties(bagsolar_resource_root_tests PROPERTIES LABELS "unit;resources")

    add_test(NAME bagsolar_resource_smoke COMMAND bagsolar_resource_smoke)
    set_tests_properties(bagsolar_resource_smoke PROPERTIES LABELS "integration;resources")

    configure_file(${CMAKE_SOURCE_DIR}/tests/install_smoke.cmake.in
                   ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_install_smoke.cmake @ONLY)
    add_test(NAME bagsolar_install_smoke
             COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_install_smoke.cmake)
    set_tests_properties(bagsolar_install_smoke PROPERTIES LABELS "integration;install")

    configure_file(${CMAKE_SOURCE_DIR}/tests/package_smoke.cmake.in
                   ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_package_smoke.cmake @ONLY)
    add_test(NAME bagsolar_package_smoke
             COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_package_smoke.cmake)
    set_tests_properties(bagsolar_package_smoke PROPERTIES LABELS "integration;package")

    configure_file(${CMAKE_SOURCE_DIR}/tests/launcher_vfat_smoke.cmake.in
                   ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_launcher_vfat_smoke.cmake @ONLY)
    add_test(NAME bagsolar_launcher_vfat_smoke
             COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/bagsolar_launcher_vfat_smoke.cmake)
    set_tests_properties(bagsolar_launcher_vfat_smoke PROPERTIES
        LABELS "integration;package;launcher"
        DEPENDS bagsolar_resource_smoke)
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
    src/platform/ExternalCommand.cpp
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

# Install rules differ by platform family. macOS uses BUNDLE destination for the
# MACOSX_BUNDLE target; Windows/Linux use RUNTIME. Resources stay relocatable.
if(APPLE)
    install(TARGETS BAGS_LAB
            BUNDLE DESTINATION .
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS bagsolar_validation bagsolar_ephemeris bagsolar_resource_smoke
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # Preferred .app resource layout (NOT YET TESTED on a macOS host):
    #   BAGS_LAB.app/Contents/Resources/{data,assets}
    install(DIRECTORY data/ DESTINATION "BAGS_LAB.app/Contents/Resources/data")
    install(FILES assets/planets/manifest.json
            DESTINATION "BAGS_LAB.app/Contents/Resources/assets/planets")
    if(BAGS_LAB_INSTALL_PLANET_MODELS)
        install(DIRECTORY assets/planets/
                DESTINATION "BAGS_LAB.app/Contents/Resources/assets/planets"
                FILES_MATCHING PATTERN "*.glb" PATTERN "manifest.json")
    endif()
else()
    install(TARGETS BAGS_LAB bagsolar_validation bagsolar_ephemeris bagsolar_resource_smoke
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/bags_lab/data)
    install(FILES assets/planets/manifest.json DESTINATION ${CMAKE_INSTALL_DATADIR}/bags_lab/assets/planets)
    if(BAGS_LAB_INSTALL_PLANET_MODELS)
        # Presentation models required by assets/planets/manifest.json (includes prepared/).
        # See docs/THIRD_PARTY_LICENSES.md for attribution. Source-tree originals remain untouched.
        install(DIRECTORY assets/planets/
                DESTINATION ${CMAKE_INSTALL_DATADIR}/bags_lab/assets/planets
                FILES_MATCHING
                PATTERN "*.glb"
                PATTERN "manifest.json")
    endif()
endif()

# When using the distro shared raylib on Linux, ship it beside the executable so
# CPack packages do not silently depend on a developer-machine libraylib/libtiff.
# Portable builds (BAGS_LAB_BUNDLE_RAYLIB=ON) skip this — verified linux-x64 path.
if(NOT BAGS_LAB_BUNDLE_RAYLIB AND UNIX AND NOT APPLE)
    pkg_get_variable(BAGS_LAB_RAYLIB_LIBDIR raylib libdir)
    set(_bags_raylib_candidates
        "${BAGS_LAB_RAYLIB_LIBDIR}/libraylib.so.6.0.0"
        "${BAGS_LAB_RAYLIB_LIBDIR}/libraylib.so.600"
        "${BAGS_LAB_RAYLIB_LIBDIR}/libraylib.so")
    set(_bags_raylib_real "")
    foreach(_cand IN LISTS _bags_raylib_candidates)
        if(EXISTS "${_cand}")
            get_filename_component(_bags_raylib_real "${_cand}" REALPATH)
            break()
        endif()
    endforeach()
    if(_bags_raylib_real)
        install(FILES "${_bags_raylib_real}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}/lib
                RENAME libraylib.so.6.0.0)
        # VFAT-safe second name (soname) as a real file.
        install(FILES "${_bags_raylib_real}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}/lib
                RENAME libraylib.so.600)
        message(STATUS "BAGS_LAB: will install shared raylib into \${CMAKE_INSTALL_BINDIR}/lib for relocatable packages")
    else()
        message(WARNING "BAGS_LAB: could not locate shared libraylib to install beside BAGS_LAB")
    endif()
endif()

if(NOT APPLE)
    install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/bags_lab/docs)
    install(FILES LICENSE README.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/bags_lab)
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/bags_lab)
else()
    install(FILES LICENSE README.md CHANGELOG.md docs/THIRD_PARTY_LICENSES.md
            DESTINATION "BAGS_LAB.app/Contents/Resources")
endif()

set(CPACK_PACKAGE_NAME "BAGS_LAB")
set(CPACK_PACKAGE_VENDOR "BAGS_LAB Project")
set(CPACK_PACKAGE_VERSION ${BAGS_LAB_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BAGS_LAB portable scientific orbital laboratory")
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
