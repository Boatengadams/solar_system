#!/usr/bin/env bash
# Portable package dispatcher for BAGS_LAB.
# Usage: ./tools/package_portable.sh [linux-x64|linux-arm64|windows-x64|macos-x64|macos-arm64]
#
# VERIFIED packaging path today: linux-x64 on a Linux x86_64 host.
# This script NEVER fabricates binaries for unverified targets.
set -euo pipefail

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PLATFORM=${1:-}
HOST_ARCH=$(uname -m 2>/dev/null || echo unknown)
HOST_OS=$(uname -s 2>/dev/null || echo unknown)

detect_default_platform() {
  case "$HOST_OS" in
    Linux)
      case "$HOST_ARCH" in
        x86_64|amd64) echo linux-x64 ;;
        aarch64|arm64) echo linux-arm64 ;;
        *) echo "" ;;
      esac
      ;;
    Darwin)
      case "$HOST_ARCH" in
        x86_64) echo macos-x64 ;;
        arm64) echo macos-arm64 ;;
        *) echo "" ;;
      esac
      ;;
    MINGW*|MSYS*|CYGWIN*)
      echo windows-x64
      ;;
    *)
      echo ""
      ;;
  esac
}

if [[ -z "$PLATFORM" ]]; then
  PLATFORM=$(detect_default_platform)
  if [[ -z "$PLATFORM" ]]; then
    printf '%s\n' "Unable to detect a default portable platform for host ${HOST_OS}/${HOST_ARCH}." >&2
    printf '%s\n' "Pass an explicit id: linux-x64 | linux-arm64 | windows-x64 | macos-x64 | macos-arm64" >&2
    exit 2
  fi
fi

case "$PLATFORM" in
  linux-x64|linux-arm64|windows-x64|macos-x64|macos-arm64) ;;
  *)
    printf '%s\n' "Unknown platform id: $PLATFORM" >&2
    exit 2
    ;;
esac

mkdir -p "$SOURCE_DIR/dist/$PLATFORM"

case "$PLATFORM" in
  linux-x64)
    if [[ "$HOST_OS" != "Linux" ]] || [[ "$HOST_ARCH" != "x86_64" && "$HOST_ARCH" != "amd64" ]]; then
      printf '%s\n' "linux-x64 portable packages must be built on a Linux x86_64 host (VERIFIED path)." >&2
      exit 2
    fi
    exec "$SOURCE_DIR/tools/package_linux.sh" "$PLATFORM"
    ;;
  linux-arm64)
    if [[ "$HOST_OS" != "Linux" ]]; then
      printf '%s\n' "linux-arm64 packaging requires a Linux host." >&2
      exit 2
    fi
    if [[ "$HOST_ARCH" != "aarch64" && "$HOST_ARCH" != "arm64" ]]; then
      printf '%s\n' "linux-arm64 is BUILD-READY via cmake/toolchains/linux-arm64.cmake but NOT YET TESTED on this x86_64 host." >&2
      printf '%s\n' "Build on an aarch64 machine, or complete a verified cross-compile smoke before packaging." >&2
      printf '%s\n' "Refusing to fabricate a linux-arm64 package." >&2
      exit 2
    fi
    exec "$SOURCE_DIR/tools/package_linux.sh" "$PLATFORM"
    ;;
  windows-x64|macos-x64|macos-arm64)
    printf '%s\n' "Platform '$PLATFORM' packaging is PLANNED / NOT YET TESTED." >&2
    printf '%s\n' "Produce the native package on a matching host. See platform/ and docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md." >&2
    printf '%s\n' "dist/$PLATFORM/ remains a documentation stub until a verified native build exists." >&2
    exit 2
    ;;
esac
