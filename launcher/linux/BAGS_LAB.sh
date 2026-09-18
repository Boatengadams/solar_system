#!/usr/bin/env bash
# Portable launcher for BAGS_LAB (Linux).
# Resolves its own directory; works on EXT4 and VFAT USB (no chmod/sudo/remount).
set -euo pipefail

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

trace() {
  if [[ "${BAGS_LAB_LAUNCHER_TRACE:-}" == "1" ]]; then
    printf '%s\n' "BAGS_LAB launcher: $*" >&2
  fi
}

# Prefer VFAT-friendly .EXE name, then the Unix-named binary, then nearby
# developer layouts. Directories are NEVER accepted as candidates.
CANDIDATES=(
  "$HERE/BAGS_LAB.EXE"
  "$HERE/BAGS_LAB"
  "$HERE/../dist/linux-x64/BAGS_LAB.EXE"
  "$HERE/../dist/linux-x64/BAGS_LAB"
  "$HERE/../../dist/linux-x64/BAGS_LAB.EXE"
  "$HERE/../../dist/linux-x64/BAGS_LAB"
  "$HERE/../../build/BAGS_LAB"
  "$HERE/../BAGS_LAB"
)

is_regular_file() {
  [[ -f "$1" && ! -d "$1" ]]
}

APP=""
for candidate in "${CANDIDATES[@]}"; do
  if is_regular_file "$candidate"; then
    APP=$candidate
    break
  fi
done

if [[ -z "$APP" ]]; then
  printf '%s\n' "BAGS_LAB: executable not found near launcher" >&2
  printf '%s\n' "  Package directory: $HERE" >&2
  printf '%s\n' "  Expected a regular file named:" >&2
  printf '%s\n' "    $HERE/BAGS_LAB.EXE" >&2
  printf '%s\n' "    $HERE/BAGS_LAB" >&2
  printf '%s\n' "On VFAT USB sticks run:  bash BAGS_LAB.sh   or   bash BAGS_LAB.BAT" >&2
  printf '%s\n' "Build the project or generate dist/linux-x64 with tools/package_portable.sh" >&2
  exit 1
fi

APP=$(CDPATH= cd -- "$(dirname -- "$APP")" && pwd)/$(basename -- "$APP")
PACKAGE_DIR=$HERE

# Prefer libraries shipped next to the *package* (USB), not a temp copy.
# The binary also carries DT_RUNPATH=$ORIGIN/lib for relocatable resolution.
if [[ -d "$PACKAGE_DIR/lib" ]]; then
  export LD_LIBRARY_PATH="$PACKAGE_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# Authoritative resource root is always the package beside this launcher.
# Required when the ELF runs from a temporary copy under /tmp (VFAT).
export BAGS_LAB_RESOURCE_ROOT="$PACKAGE_DIR"

can_exec_directly() {
  local path=$1
  # Must be a regular file with +x. Directories can be +x — already excluded.
  is_regular_file "$path" || return 1
  [[ -x "$path" ]] || return 1
  return 0
}

run_via_temp_copy() {
  local source=$1
  shift
  local tmp_root tmp_app
  tmp_root=$(mktemp -d "${TMPDIR:-/tmp}/bags_lab.run.XXXXXX")
  tmp_app="$tmp_root/BAGS_LAB"

  cleanup_temp() {
    rm -rf -- "$tmp_root" 2>/dev/null || true
  }
  trap cleanup_temp EXIT INT TERM HUP

  # Copy only the ELF — assets/data stay on the package (USB).
  cp -f -- "$source" "$tmp_app"
  chmod u+x -- "$tmp_app"

  if [[ ! -x "$tmp_app" ]]; then
    printf '%s\n' "BAGS_LAB: failed to make temporary executable runnable: $tmp_app" >&2
    exit 1
  fi

  trace "temp exec $tmp_app (resources=$BAGS_LAB_RESOURCE_ROOT)"
  # Do not exec: allow trap cleanup after the process exits.
  set +e
  "$tmp_app" "$@"
  local status=$?
  set -e
  cleanup_temp
  trap - EXIT INT TERM HUP
  exit "$status"
}

if can_exec_directly "$APP"; then
  trace "direct exec $APP (resources=$BAGS_LAB_RESOURCE_ROOT)"
  exec "$APP" "$@"
fi

trace "package ELF is not directly executable; using temporary copy"
run_via_temp_copy "$APP" "$@"
