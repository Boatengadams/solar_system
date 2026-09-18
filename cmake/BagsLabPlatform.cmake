# BAGS_LAB platform / architecture detection (Phase 2 foundation).
# Include after project() so CMAKE_SYSTEM_* is populated.
#
# Exports (cache / variables):
#   BAGS_LAB_TARGET_PLATFORM  - linux | windows | macos | unknown
#   BAGS_LAB_TARGET_ARCH      - x64 | arm64 | unknown
#   BAGS_LAB_DIST_ID          - e.g. linux-x64, windows-x64, macos-arm64
#   BAGS_LAB_HOST_MATCHES_TARGET - TRUE when this host can natively build the target
#
# Status vocabulary (documentation / messaging only):
#   VERIFIED | BUILD-READY | PLANNED | NOT YET TESTED

if(NOT DEFINED BAGS_LAB_PORTABLE)
    option(BAGS_LAB_PORTABLE "Hint that this configure is for a relocatable/USB package" OFF)
endif()

# --- Platform ---
if(WIN32)
    set(_bags_platform "windows")
elseif(APPLE)
    set(_bags_platform "macos")
elseif(UNIX)
    set(_bags_platform "linux")
else()
    set(_bags_platform "unknown")
endif()

# --- Architecture ---
# Prefer CMAKE_SYSTEM_PROCESSOR; on Apple honor CMAKE_OSX_ARCHITECTURES when set.
set(_bags_proc "${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${_bags_proc}" _bags_proc_l)

set(_bags_arch "unknown")
if(APPLE AND DEFINED CMAKE_OSX_ARCHITECTURES AND NOT "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
    # Single-arch packages only in Phase 2; universal binaries are Phase 4+.
    list(LENGTH CMAKE_OSX_ARCHITECTURES _bags_osx_arch_count)
    if(_bags_osx_arch_count EQUAL 1)
        list(GET CMAKE_OSX_ARCHITECTURES 0 _bags_osx_one)
        string(TOLOWER "${_bags_osx_one}" _bags_osx_one_l)
        if(_bags_osx_one_l STREQUAL "arm64" OR _bags_osx_one_l STREQUAL "aarch64")
            set(_bags_arch "arm64")
        elseif(_bags_osx_one_l STREQUAL "x86_64")
            set(_bags_arch "x64")
        endif()
    endif()
endif()

if(_bags_arch STREQUAL "unknown")
    if(_bags_proc_l MATCHES "^(x86_64|amd64|x64)$")
        set(_bags_arch "x64")
    elseif(_bags_proc_l MATCHES "^(aarch64|arm64)$")
        set(_bags_arch "arm64")
    elseif(_bags_proc_l MATCHES "^armv7")
        set(_bags_arch "arm32") # not a Phase 2 release target
    endif()
endif()

set(BAGS_LAB_TARGET_PLATFORM "${_bags_platform}" CACHE STRING "BAGS_LAB OS family: linux|windows|macos")
set(BAGS_LAB_TARGET_ARCH "${_bags_arch}" CACHE STRING "BAGS_LAB CPU family: x64|arm64|...")
mark_as_advanced(BAGS_LAB_TARGET_PLATFORM BAGS_LAB_TARGET_ARCH)

if(BAGS_LAB_TARGET_PLATFORM STREQUAL "unknown" OR BAGS_LAB_TARGET_ARCH STREQUAL "unknown")
    set(BAGS_LAB_DIST_ID "unknown")
else()
    set(BAGS_LAB_DIST_ID "${BAGS_LAB_TARGET_PLATFORM}-${BAGS_LAB_TARGET_ARCH}")
endif()

# Host match: same OS family + same arch (native build). Cross builds need a toolchain.
set(BAGS_LAB_HOST_MATCHES_TARGET FALSE)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND BAGS_LAB_TARGET_PLATFORM STREQUAL "linux")
    string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" _bags_host_proc)
    if(BAGS_LAB_TARGET_ARCH STREQUAL "x64" AND _bags_host_proc MATCHES "^(x86_64|amd64)$")
        set(BAGS_LAB_HOST_MATCHES_TARGET TRUE)
    elseif(BAGS_LAB_TARGET_ARCH STREQUAL "arm64" AND _bags_host_proc MATCHES "^(aarch64|arm64)$")
        set(BAGS_LAB_HOST_MATCHES_TARGET TRUE)
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND BAGS_LAB_TARGET_PLATFORM STREQUAL "macos")
    # On macOS, selecting CMAKE_OSX_ARCHITECTURES can still be a native SDK build.
    set(BAGS_LAB_HOST_MATCHES_TARGET TRUE)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND BAGS_LAB_TARGET_PLATFORM STREQUAL "windows")
    set(BAGS_LAB_HOST_MATCHES_TARGET TRUE)
endif()

message(STATUS "BAGS_LAB platform: ${BAGS_LAB_TARGET_PLATFORM} arch=${BAGS_LAB_TARGET_ARCH} dist=${BAGS_LAB_DIST_ID} host_match=${BAGS_LAB_HOST_MATCHES_TARGET} portable=${BAGS_LAB_PORTABLE}")
