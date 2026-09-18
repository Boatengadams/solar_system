# Optional MinGW-w64 cross-compile toolchain (Linux host → Windows x64).
# Requires a working MinGW toolchain, e.g.:
#   sudo apt install mingw-w64
#
# Configure:
#   cmake -S . -B build-windows-x64 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x64-mingw.cmake \
#     -DBAGS_LAB_BUNDLE_RAYLIB=ON
#
# Primary Windows RELEASE toolchain for Phase 3+: native MSVC on a Windows host
# (see platform/windows/README.md). MinGW is an optional cross path for packaging
# experiments and is NOT VERIFIED for runtime acceptance yet.
#
# Status: BUILD-READY (toolchain template). NOT YET TESTED / NOT VERIFIED.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BAGS_LAB_BUNDLE_RAYLIB ON CACHE BOOL "Static raylib for portable / cross builds" FORCE)
