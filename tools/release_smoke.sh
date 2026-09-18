#!/usr/bin/env bash

set -euo pipefail

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SMOKE_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/bagsolar-release-smoke.XXXXXX")
trap 'rm -rf "$SMOKE_ROOT"' EXIT

CHECKOUT_DIR="$SMOKE_ROOT/checkout"
BUILD_DIR="$CHECKOUT_DIR/build"
INSTALL_DIR="$SMOKE_ROOT/install"
INSTALL_RUN_DIR="$SMOKE_ROOT/install-run"
PACKAGE_EXTRACT_DIR="$SMOKE_ROOT/package"
PACKAGE_RUN_DIR="$SMOKE_ROOT/package-run"

mkdir -p "$CHECKOUT_DIR" "$INSTALL_RUN_DIR" "$PACKAGE_EXTRACT_DIR" "$PACKAGE_RUN_DIR"

printf '%s\n' '== fresh checkout =='
git -C "$SOURCE_DIR" archive --format=tar HEAD | tar -x -C "$CHECKOUT_DIR"

printf '%s\n' '== configure =='
cmake -S "$CHECKOUT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic'

printf '%s\n' '== build =='
cmake --build "$BUILD_DIR" --parallel 2

printf '%s\n' '== tests =='
ctest --test-dir "$BUILD_DIR" --output-on-failure

printf '%s\n' '== scientific validation =='
"$BUILD_DIR/bagsolar_validation" >/dev/null

printf '%s\n' '== education validation =='
"$BUILD_DIR/bagsolar_education_tests"

printf '%s\n' '== offline local ephemeris =='
"$BUILD_DIR/bagsolar_ephemeris" --local earth 2451545.0 heliocentric >/dev/null

printf '%s\n' '== install =='
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"
test -f "$INSTALL_DIR/share/bags_lab/data/bodies/sun.json"
test -f "$INSTALL_DIR/share/bags_lab/docs/BUILD.md"
(cd "$INSTALL_RUN_DIR" && \
    "$INSTALL_DIR/bin/bagsolar_resource_smoke" >/dev/null && \
    "$INSTALL_DIR/bin/bagsolar_validation" >/dev/null && \
    "$INSTALL_DIR/bin/bagsolar_ephemeris" --local earth 2451545.0 heliocentric >/dev/null)

printf '%s\n' '== package =='
cmake --build "$BUILD_DIR" --target package --parallel 2
PACKAGE_FILE=$(find "$BUILD_DIR" -maxdepth 1 -type f -name 'BAGS_LAB-*.tar.gz' -print -quit)
test -n "$PACKAGE_FILE"
(cd "$PACKAGE_EXTRACT_DIR" && cmake -E tar xzf "$PACKAGE_FILE")
PACKAGE_DIR=$(find "$PACKAGE_EXTRACT_DIR" -mindepth 1 -maxdepth 1 -type d -name 'BAGS_LAB-*' -print -quit)
test -n "$PACKAGE_DIR"
test -f "$PACKAGE_DIR/share/bags_lab/data/bodies/sun.json"
test -f "$PACKAGE_DIR/share/bags_lab/docs/BUILD.md"
(cd "$PACKAGE_RUN_DIR" && \
    "$PACKAGE_DIR/bin/bagsolar_resource_smoke" >/dev/null && \
    "$PACKAGE_DIR/bin/bagsolar_validation" >/dev/null && \
    "$PACKAGE_DIR/bin/bagsolar_ephemeris" --local earth 2451545.0 heliocentric >/dev/null)

printf '%s\n' '== release smoke passed =='
