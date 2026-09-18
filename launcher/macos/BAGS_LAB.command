#!/bin/bash
# Portable launcher for BAGS_LAB (macOS).
# Status: BUILD-READY launcher / .app package NOT YET TESTED.
set -euo pipefail
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

CANDIDATES=(
  "$HERE/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/BAGS_LAB"
  "$HERE/../dist/macos-arm64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../dist/macos-x64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../dist/macos-arm64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../dist/macos-x64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../dist/macos/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../build-macos-arm64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../build-macos-x64/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../build/BAGS_LAB.app/Contents/MacOS/BAGS_LAB"
  "$HERE/../../build/BAGS_LAB"
)

APP=""
for candidate in "${CANDIDATES[@]}"; do
  if [[ -x "$candidate" ]]; then
    APP=$candidate
    break
  fi
done

if [[ -z "$APP" ]]; then
  echo "BAGS_LAB not found near launcher: $HERE" >&2
  echo "Build on macOS (presets macos-arm64 / macos-x64) or place a package under dist/macos-*/" >&2
  echo "Do not claim macOS support until a native .app package exists." >&2
  exit 1
fi

# Prefer Resources next to the .app binary; ResourceRoot also resolves this layout.
if [[ "$APP" == *".app/Contents/MacOS/"* ]]; then
  export BAGS_LAB_RESOURCE_ROOT="${BAGS_LAB_RESOURCE_ROOT:-$(CDPATH= cd -- "$(dirname -- "$APP")/../Resources" && pwd)}"
fi

exec "$APP" "$@"
