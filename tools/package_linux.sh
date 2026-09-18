#!/usr/bin/env bash
# Linux portable packaging implementation for BAGS_LAB.
# Invoked by tools/package_portable.sh — do not call for Windows/macOS.
# Usage: ./tools/package_linux.sh <linux-x64|linux-arm64>
set -euo pipefail

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PLATFORM=${1:?linux platform id required}
case "$PLATFORM" in
  linux-x64) DEFAULT_BUILD=build-portable ;;
  linux-arm64) DEFAULT_BUILD=build-linux-arm64 ;;
  *)
    printf '%s\n' "package_linux.sh supports linux-x64|linux-arm64 only (got: $PLATFORM)" >&2
    exit 2
    ;;
esac
BUILD_DIR=${BAGS_LAB_BUILD_DIR:-"$SOURCE_DIR/$DEFAULT_BUILD"}
DIST_DIR="$SOURCE_DIR/dist/$PLATFORM"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/bags-lab-package.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT

printf '%s\n' "== configure ($PLATFORM) =="
CMAKE_EXTRA=()
if [[ "$PLATFORM" == "linux-arm64" && "$(uname -m)" != "aarch64" && "$(uname -m)" != "arm64" ]]; then
  CMAKE_EXTRA+=(-DCMAKE_TOOLCHAIN_FILE="$SOURCE_DIR/cmake/toolchains/linux-arm64.cmake")
fi
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DBAGS_LAB_INSTALL_PLANET_MODELS=ON \
  -DBAGS_LAB_BUNDLE_RAYLIB=ON \
  -DBAGS_LAB_PORTABLE=ON \
  "${CMAKE_EXTRA[@]}"

printf '%s\n' "== build =="
cmake --build "$BUILD_DIR" --target BAGS_LAB bagsolar_resource_smoke -j"$(nproc 2>/dev/null || echo 2)"

printf '%s\n' "== stage portable tree =="
RUNTIME_README=""
if [[ -f "$DIST_DIR/README.md" ]]; then
  RUNTIME_README=$(mktemp "${TMPDIR:-/tmp}/bags-lab-readme.XXXXXX")
  cp -a "$DIST_DIR/README.md" "$RUNTIME_README"
fi
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib" "$DIST_DIR/data" "$DIST_DIR/assets/planets"

install -m 0755 "$BUILD_DIR/BAGS_LAB" "$DIST_DIR/BAGS_LAB"
# VFAT USB hosts ignore Unix +x except for EXE/COM/BAT names (showexec).
cp -a "$DIST_DIR/BAGS_LAB" "$DIST_DIR/BAGS_LAB.EXE"
chmod +x "$DIST_DIR/BAGS_LAB" "$DIST_DIR/BAGS_LAB.EXE" || true

cp -a "$SOURCE_DIR/data/." "$DIST_DIR/data/"
cp -a "$SOURCE_DIR/assets/planets/manifest.json" "$DIST_DIR/assets/planets/"
# Stage only the GLB paths referenced by the presentation manifest (includes
# prepared/*.glb). Do not copy unrelated development-only variants.
python3 - "$SOURCE_DIR/assets/planets" "$DIST_DIR/assets/planets" <<'PY'
import json
import shutil
import sys
from pathlib import Path

src = Path(sys.argv[1])
dst = Path(sys.argv[2])
manifest = json.loads((src / "manifest.json").read_text(encoding="utf-8"))
if manifest.get("format") != "bagsolar_planet_presentation_v1":
    raise SystemExit("unexpected planet presentation manifest format")

copied = 0
missing = []
for entry in manifest.get("models", []):
    if not entry.get("enabled", False):
        continue
    relative = entry.get("model", "")
    parts = Path(relative).parts
    if not relative or ".." in parts or Path(relative).is_absolute():
        raise SystemExit(f"refusing unsafe manifest model path: {relative!r}")
    source_file = src / relative
    dest_file = dst / relative
    if not source_file.is_file():
        missing.append(relative)
        continue
    dest_file.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_file, dest_file)
    copied += 1

if missing:
    raise SystemExit("manifest-referenced planet models missing from source tree: " + ", ".join(missing))
if copied == 0:
    raise SystemExit("no enabled planet models were staged from the presentation manifest")
print(f"staged {copied} manifest-referenced planet GLB(s)", flush=True)
PY
cp -a "$SOURCE_DIR/LICENSE" "$DIST_DIR/LICENSE"
cp -a "$SOURCE_DIR/docs/THIRD_PARTY_LICENSES.md" "$DIST_DIR/THIRD_PARTY_LICENSES.md"
if [[ -n "$RUNTIME_README" && -f "$RUNTIME_README" ]]; then
  cp -a "$RUNTIME_README" "$DIST_DIR/README.md"
  rm -f "$RUNTIME_README"
else
  cp -a "$SOURCE_DIR/README.md" "$DIST_DIR/README.md"
fi
install -m 0755 "$SOURCE_DIR/launcher/linux/BAGS_LAB.sh" "$DIST_DIR/BAGS_LAB.sh"
# FAT-executable launcher alias for USB showexec mounts.
cp -a "$DIST_DIR/BAGS_LAB.sh" "$DIST_DIR/BAGS_LAB.BAT"
chmod +x "$DIST_DIR/BAGS_LAB.sh" "$DIST_DIR/BAGS_LAB.BAT" || true

printf '%s\n' "== collect runtime shared libraries =="
# Copy a shared library into ./lib by value (dereference symlinks). Also store
# a second real file under the DT_NEEDED soname so VFAT (no symlinks) works.
copy_shared_lib() {
  local source=$1
  [[ -z "$source" || "$source" == "not" || ! -e "$source" ]] && return 0
  local real soname realname
  real=$(readlink -f -- "$source")
  soname=$(basename -- "$source")
  realname=$(basename -- "$real")
  cp -a -- "$real" "$DIST_DIR/lib/$realname"
  if [[ "$soname" != "$realname" ]]; then
    # Prefer a real file copy for USB/VFAT; keep symlink too for Unix filesystems.
    cp -f -- "$real" "$DIST_DIR/lib/$soname"
    ln -sfn -- "$realname" "$DIST_DIR/lib/$soname" 2>/dev/null || true
    # If ln replaced the file with a symlink, ensure a real soname file remains
    # for VFAT repacks: write a sibling then rename when symlink unsupported.
    if [[ -L "$DIST_DIR/lib/$soname" ]]; then
      :
    else
      cp -f -- "$real" "$DIST_DIR/lib/$soname"
    fi
  fi
}

# Bundle every non-allowlisted DT_NEEDED dependency of the executable (and of
# libs already staged). Host glibc / libstdc++ / X11 / GL stay on the system.
# libglfw.so.3 is intentionally NOT allowlisted and NOT copied from the host —
# portable builds must statically link raylib's bundled GLFW.
python3 - "$DIST_DIR/BAGS_LAB" "$DIST_DIR/lib" <<'PY'
import shutil
import subprocess
import sys
from pathlib import Path

exe = Path(sys.argv[1])
libdir = Path(sys.argv[2])
libdir.mkdir(parents=True, exist_ok=True)

# Never stage these from the developer machine into USB packages.
FORBIDDEN_HOST_COPY = {
    "libglfw.so.3",
    "libtiff.so.3",
    "libtiff.so.6",
    "libtiff.so",
}

SYSTEM = {
    "linux-vdso.so.1", "ld-linux-x86-64.so.2",
    "libc.so.6", "libm.so.6", "libdl.so.2", "librt.so.1", "libpthread.so.0",
    "libresolv.so.2", "libstdc++.so.6", "libgcc_s.so.1", "libatomic.so.1",
    "libGL.so.1", "libOpenGL.so.0", "libGLdispatch.so.0",
    "libGLX.so.0", "libEGL.so.1",
    "libX11.so.6", "libX11-xcb.so.1", "libxcb.so.1", "libXau.so.6", "libXdmcp.so.6",
    "libXrandr.so.2", "libXinerama.so.1", "libXcursor.so.1", "libXi.so.6",
    "libXext.so.6", "libXrender.so.1", "libXfixes.so.3", "libXxf86vm.so.1",
    "libwayland-client.so.0", "libwayland-cursor.so.0", "libwayland-egl.so.1",
    "libwayland-server.so.0", "libxkbcommon.so.0", "libxkbcommon-x11.so.0",
    "libffi.so.8", "libffi.so.7", "libdecor-0.so.0", "libdbus-1.so.3",
    "libpulse.so.0", "libpulse-simple.so.0", "libasound.so.2",
}

def needed(path: Path):
    out = subprocess.check_output(["readelf", "-d", str(path)], text=True, errors="replace")
    libs = []
    for line in out.splitlines():
        if "NEEDED" in line and "[" in line:
            libs.append(line.split("[", 1)[1].split("]", 1)[0])
    return libs

def locate(soname: str):
    # Prefer already-bundled, then ldconfig, then common lib dirs.
    for candidate in libdir.glob(soname + "*"):
        if candidate.is_file():
            return candidate.resolve()
    try:
        out = subprocess.check_output(["ldconfig", "-p"], text=True, errors="replace")
    except Exception:
        out = ""
    for line in out.splitlines():
        if f" {soname} " in f" {line} " or line.strip().startswith(soname + " "):
            # format: name (libc6,...) => /path
            if "=>" in line:
                return Path(line.split("=>", 1)[1].strip()).resolve()
    for root in (Path("/lib/x86_64-linux-gnu"), Path("/usr/lib/x86_64-linux-gnu"), Path("/lib64"), Path("/usr/lib")):
        cand = root / soname
        if cand.exists():
            return cand.resolve()
    return None

def stage(soname: str, source: Path):
    real = source.resolve()
    realname = real.name
    dest_real = libdir / realname
    shutil.copy2(real, dest_real)
    dest_soname = libdir / soname
    if soname != realname:
        shutil.copy2(real, dest_soname)
    print(f"bundled {soname} <- {real}", flush=True)

queue = [exe]
seen = set()
missing = []
while queue:
    path = queue.pop(0)
    key = str(path.resolve())
    if key in seen:
        continue
    seen.add(key)
    for soname in needed(path):
        if soname in FORBIDDEN_HOST_COPY or soname.startswith("libglfw.so") or soname.startswith("libtiff.so"):
            missing.append(
                f"{path.name} -> {soname} (forbidden dynamic dep; static-link / rebuild, do not copy host .so)"
            )
            continue
        if soname in SYSTEM:
            continue
        located = locate(soname)
        if located is None:
            missing.append(f"{path.name} -> {soname}")
            continue
        already = libdir / soname
        already_real = libdir / located.name
        if not already.is_file() and not already_real.is_file():
            stage(soname, located)
        # Recurse into newly bundled libs
        target = already if already.is_file() else already_real
        if target.is_file():
            queue.append(target)

if missing:
    raise SystemExit("portable package missing non-system runtime libraries:\n  " + "\n  ".join(missing))
print("runtime library collection ok", flush=True)
PY

# If lib/ is empty, remove it (fully system-linked / static raylib build).
if [[ -d "$DIST_DIR/lib" ]] && [[ -z "$(ls -A "$DIST_DIR/lib" 2>/dev/null || true)" ]]; then
  rmdir "$DIST_DIR/lib"
fi

printf '%s\n' "== audit portable dependencies =="
python3 "$SOURCE_DIR/tools/audit_portable_libs.py" \
  "$DIST_DIR/BAGS_LAB" \
  --libdir "$DIST_DIR/lib" \
  --require-origin-lib

# Explicit regression guard: portable Linux must not dynamically need GLFW/TIFF/raylib.
if readelf -d "$DIST_DIR/BAGS_LAB" 2>/dev/null | grep -E 'NEEDED.*(libglfw\.so|libtiff\.so|libraylib\.so)'; then
  printf '%s\n' "ERROR: portable BAGS_LAB still has dynamic NEEDED for glfw/tiff/raylib" >&2
  exit 1
fi
# Companion libs under lib/ must also not pull in unbundled libtiff.
if compgen -G "$DIST_DIR/lib/*.so*" > /dev/null; then
  if readelf -d "$DIST_DIR"/lib/*.so* 2>/dev/null | grep -E 'NEEDED.*libtiff\.so'; then
    if [[ ! -f "$DIST_DIR/lib/libtiff.so.3" && ! -f "$DIST_DIR/lib/libtiff.so.6" && ! -f "$DIST_DIR/lib/libtiff.so" ]]; then
      printf '%s\n' "ERROR: staged lib links libtiff but does not bundle it" >&2
      exit 1
    fi
  fi
fi

printf '%s\n' "== smoke from foreign working directory =="
# Confirm every enabled manifest model path exists in the staged portable tree.
python3 - "$SOURCE_DIR/assets/planets/manifest.json" "$DIST_DIR/assets/planets" <<'PY'
import json, sys
from pathlib import Path
manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
root = Path(sys.argv[2])
missing = [e["model"] for e in manifest.get("models", [])
           if e.get("enabled", False) and not (root / e["model"]).is_file()]
if missing:
    raise SystemExit("portable package missing manifest models: " + ", ".join(missing))
prepared = list((root / "prepared").glob("*.glb")) if (root / "prepared").is_dir() else []
print(f"portable planet models ok ({len(prepared)} prepared GLB file(s))", flush=True)
PY

SMOKE_WD="$STAGE/run-from-elsewhere"
mkdir -p "$SMOKE_WD"
# Do NOT launch the GUI binary here (it hangs without a controlled display).
# Verify resource resolution via the raylib-free resource smoke helper.
install -m 0755 "$BUILD_DIR/bagsolar_resource_smoke" "$STAGE/BAGS_LAB_smoke"
cp -a "$DIST_DIR/data" "$STAGE/data"
(cd "$SMOKE_WD" && "$STAGE/BAGS_LAB_smoke" | tee "$STAGE/smoke.out")
grep -q 'BAGS_LAB runtime data:' "$STAGE/smoke.out"

# Prove the portable binary resolves without developer library paths / system raylib.
printf '%s\n' "== clean library load check =="
CLEAN_ENV=(env -i
  "PATH=/usr/bin:/bin"
  "HOME=$STAGE/fake-home"
  "DISPLAY=${DISPLAY:-}"
  "XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-}"
  "WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-}"
  "LD_LIBRARY_PATH=$DIST_DIR/lib"
)
# ldd under clean-ish path: ensure no unresolved "not found", and no libtiff.
ldd "$DIST_DIR/BAGS_LAB" | tee "$STAGE/ldd.out"
if grep -q 'not found' "$STAGE/ldd.out"; then
  printf '%s\n' "ERROR: ldd reports unresolved libraries for portable BAGS_LAB" >&2
  exit 1
fi
if grep -E 'libtiff\.so|libglfw\.so' "$STAGE/ldd.out"; then
  printf '%s\n' "ERROR: portable BAGS_LAB still depends on libtiff or libglfw" >&2
  exit 1
fi
# Ensure dynamic loader would not need developer raylib when RUNPATH/lib is used.
if grep -E 'libraylib\.so' "$STAGE/ldd.out"; then
  if ! grep -E "libraylib\.so.*=$DIST_DIR/lib|libraylib\.so.*not found" "$STAGE/ldd.out"; then
    # If ldd resolves to system raylib, force check with LD_LIBRARY_PATH=lib only
    if ! LD_LIBRARY_PATH="$DIST_DIR/lib" ldd "$DIST_DIR/BAGS_LAB" | grep -E "libraylib\.so.*$DIST_DIR/lib"; then
      printf '%s\n' "ERROR: libraylib does not resolve from package lib/" >&2
      exit 1
    fi
  fi
else
  printf '%s\n' "BAGS_LAB has no dynamic libraylib dependency (static bundle ok)"
fi

printf '%s\n' "== portable package ready: $DIST_DIR =="
du -sh "$DIST_DIR" "$DIST_DIR/assets" "$DIST_DIR/data" 2>/dev/null || true
