cmake_minimum_required(VERSION 3.16)

project(planets LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)

set(BAGSOLAR_SOURCES
    src/app/main.cpp
    src/core/Logger.cpp
    src/data/BodyFactory.cpp
    src/data/ScenarioLoader.cpp
    src/data/ScenarioSerializer.cpp
    src/education/EducationContent.cpp
    src/input/InputController.cpp
    src/physics/PhysicsEngine.cpp
    src/physics/IntegratorBenchmark.cpp
    src/rendering/Renderer.cpp
    src/simulation/Simulation.cpp
    src/ui/HUD.cpp
)

add_executable(planets ${BAGSOLAR_SOURCES})
target_include_directories(planets PRIVATE src)
target_include_directories(planets PRIVATE third_party/nlohmann-json3-dev/usr/include)
target_link_libraries(planets PRIVATE PkgConfig::RAYLIB)

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
    set_tests_properties(bagsolar_data_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
