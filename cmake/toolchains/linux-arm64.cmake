# Optional Linux aarch64 cross-compile toolchain.
# Use ONLY with a real cross toolchain installed on the host, e.g.:
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
# Configure:
#   cmake -S . -B build-linux-arm64 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake \
#     -DBAGS_LAB_BUNDLE_RAYLIB=ON
#
# Status: BUILD-READY (toolchain template). NOT YET TESTED as a verified package.
# Do not claim linux-arm64 support until native or cross package smoke passes.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Cross builds typically need bundled raylib (no aarch64 system pkg-config raylib).
set(BAGS_LAB_BUNDLE_RAYLIB ON CACHE BOOL "Static raylib for portable / cross builds" FORCE)
