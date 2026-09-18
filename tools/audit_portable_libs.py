#!/usr/bin/env python3
"""Audit ELF runtime dependencies for BAGS_LAB portable packages.

Fails if a required non-system library is missing from the package lib/
directory, or if RUNPATH/RPATH embeds an absolute build-machine path.

System libraries (glibc, libstdc++, X11/Wayland, OpenGL) are expected from the
host. Portable builds must statically link raylib's bundled GLFW — they must
not require host libglfw.so.3 and must not ship a copied developer-machine
GLFW. Project-bundled third-party libs (e.g. libraylib) must resolve from
$ORIGIN/lib / the package lib directory.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# Host graphics / C++ runtime — not shipped in lib/.
SYSTEM_SONAMES = {
    "linux-vdso.so.1",
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libm.so.6",
    "libdl.so.2",
    "librt.so.1",
    "libpthread.so.0",
    "libresolv.so.2",
    "libstdc++.so.6",
    "libgcc_s.so.1",
    "libatomic.so.1",
    "libGL.so.1",
    "libOpenGL.so.0",
    "libGLdispatch.so.0",
    "libGLX.so.0",
    "libEGL.so.1",
    "libX11.so.6",
    "libX11-xcb.so.1",
    "libxcb.so.1",
    "libXau.so.6",
    "libXdmcp.so.6",
    "libXrandr.so.2",
    "libXinerama.so.1",
    "libXcursor.so.1",
    "libXi.so.6",
    "libXext.so.6",
    "libXrender.so.1",
    "libXfixes.so.3",
    "libXxf86vm.so.1",
    "libwayland-client.so.0",
    "libwayland-cursor.so.0",
    "libwayland-egl.so.1",
    "libwayland-server.so.0",
    "libxkbcommon.so.0",
    "libxkbcommon-x11.so.0",
    "libffi.so.8",
    "libffi.so.7",
    "libdecor-0.so.0",
    "libdbus-1.so.3",
    "libpulse.so.0",
    "libpulse-simple.so.0",
    "libasound.so.2",
}

# Never allow these to be silently satisfied from the developer machine only,
# and never "fix" them by copying an arbitrary host .so into lib/.
FORBIDDEN_DYNAMIC = re.compile(r"^(libtiff\.so(\.|$)|libglfw\.so(\.|$)|libraylib\.so(\.|$))")


def readelf_dynamic(path: Path) -> str:
    return subprocess.check_output(["readelf", "-d", str(path)], text=True, errors="replace")


def needed_libs(path: Path) -> list[str]:
    out = readelf_dynamic(path)
    libs = []
    for line in out.splitlines():
        if "NEEDED" in line and "[" in line:
            libs.append(line.split("[", 1)[1].split("]", 1)[0])
    return libs


def runpaths(path: Path) -> list[str]:
    out = readelf_dynamic(path)
    paths: list[str] = []
    for line in out.splitlines():
        if "RUNPATH" in line or "RPATH" in line:
            if "[" in line:
                value = line.split("[", 1)[1].split("]", 1)[0]
                paths.extend(p for p in value.split(":") if p)
    return paths


def resolve_in_libdir(soname: str, libdir: Path) -> Path | None:
    direct = libdir / soname
    if direct.is_file():
        return direct
    # Accept versioned real files that provide the soname via identical content
    # (VFAT cannot store symlinks).
    for candidate in libdir.glob(soname + "*"):
        if candidate.is_file():
            return candidate
    # Also accept libraylib.so.6.0.0 when looking for libraylib.so.600 if both exist.
    if soname.startswith("libraylib.so"):
        for candidate in libdir.glob("libraylib.so*"):
            if candidate.is_file():
                return candidate
    return None


def is_system_soname(soname: str) -> bool:
    if soname in SYSTEM_SONAMES:
        return True
    # Allow numbered variants already listed; reject unknown libfoo.so.*
    return False


def audit(executable: Path, libdir: Path | None, require_origin_lib: bool) -> int:
    errors: list[str] = []
    notes: list[str] = []

    if not executable.is_file():
        print(f"missing executable: {executable}", file=sys.stderr)
        return 2

    roots = [executable]
    if libdir and libdir.is_dir():
        roots.extend(sorted(p for p in libdir.glob("*.so*") if p.is_file() and not p.is_symlink()))

    seen_files: set[Path] = set()
    queue = list(roots)
    while queue:
        path = queue.pop(0)
        real = path.resolve()
        if real in seen_files:
            continue
        seen_files.add(real)

        for soname in needed_libs(path):
            # Portable Linux must statically link GLFW. Never accept a dynamic
            # libglfw dependency — including a copied host libglfw.so.3 in lib/.
            if soname.startswith("libglfw.so"):
                errors.append(
                    f"{path.name} dynamically requires {soname}; portable builds must "
                    f"statically link raylib's bundled GLFW (do not ship host libglfw)"
                )
                continue

            if FORBIDDEN_DYNAMIC.search(soname):
                bundled = resolve_in_libdir(soname, libdir) if libdir else None
                if bundled is None:
                    errors.append(
                        f"{path.name} requires {soname}, which must be bundled under lib/ "
                        f"(portable hosts are not assumed to provide TIFF/raylib)"
                    )
                    continue

            if is_system_soname(soname):
                notes.append(f"system ok: {soname} (from {path.name})")
                continue

            if libdir is None or not libdir.is_dir():
                errors.append(f"{path.name} requires non-system {soname} but lib/ is missing")
                continue

            resolved = resolve_in_libdir(soname, libdir)
            if resolved is None:
                errors.append(
                    f"{path.name} requires non-system {soname} which is not present in {libdir}"
                )
            else:
                notes.append(f"bundled ok: {soname} -> {resolved.name} (from {path.name})")
                if resolved.resolve() not in seen_files:
                    queue.append(resolved)

        for rp in runpaths(path):
            if rp.startswith("/") and "$ORIGIN" not in rp:
                # Absolute runpath is not relocatable.
                if any(x in rp for x in ("/home/", "/Users/", "/tmp/", "/run/media/", "/mnt/")):
                    errors.append(f"{path.name} embeds machine-specific RUNPATH/RPATH entry: {rp}")
                else:
                    # Absolute system paths are also undesirable for USB portability.
                    errors.append(f"{path.name} embeds absolute RUNPATH/RPATH entry: {rp}")
            elif "$ORIGIN" in rp and "lib" in rp:
                notes.append(f"relocatable search path ok: {rp} (from {path.name})")

    if require_origin_lib:
        rps = runpaths(executable)
        origin_ok = any("$ORIGIN" in p and "lib" in p for p in rps)
        # Static raylib builds may have no bundled libs; still require ORIGIN/lib
        # so future bundled deps and shared raylib profiles remain relocatable.
        if not origin_ok:
            errors.append(
                f"{executable.name} is missing relocatable RUNPATH/RPATH containing $ORIGIN/lib"
            )

    for line in notes:
        print(line)
    if errors:
        for line in errors:
            print(line, file=sys.stderr)
        print("portable dependency audit FAILED", file=sys.stderr)
        return 1

    print("portable dependency audit ok")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument(
        "--libdir",
        type=Path,
        default=None,
        help="Package lib directory (optional if all deps are system allowlisted)",
    )
    parser.add_argument(
        "--require-origin-lib",
        action="store_true",
        help="Require DT_RUNPATH/RPATH to include $ORIGIN/lib",
    )
    args = parser.parse_args()
    libdir = args.libdir
    if libdir is not None and not libdir.is_dir():
        libdir = None
    if libdir is None:
        sibling = args.executable.parent / "lib"
        if sibling.is_dir():
            libdir = sibling
    return audit(args.executable, libdir, args.require_origin_lib)


if __name__ == "__main__":
    sys.exit(main())
