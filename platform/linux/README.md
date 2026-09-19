# Linux packaging notes

Status labels: **VERIFIED** | **BUILD-READY** | **PLANNED** | **NOT YET TESTED**

## Build configurations (do not mix audit rules)

| Configuration | CMake | raylib | GLFW | Validated by |
| --- | --- | --- | --- | --- |
| Native development | `BAGS_LAB_BUNDLE_RAYLIB=OFF` (default) | system shared (`pkg-config`) | host `libglfw` via distro raylib | `build/` + CTest (`shared-dev` package smoke) — **VERIFIED** |
| Portable USB release | `BAGS_LAB_BUNDLE_RAYLIB=ON` + `BAGS_LAB_PORTABLE=ON` | static FetchContent 6.0 | statically linked from raylib’s tree | `./tools/package_portable.sh linux-x64` + `--profile portable` audit — **VERIFIED** |

`bagsolar_package_smoke` follows the **current** CMake tree: shared-dev rules when
bundling is off, portable rules when bundling is on. It must not be read as
“USB release failed” when it audits a developer shared-raylib CPack tarball.

## Portable runtime (`dist/linux-x64`) — VERIFIED

Host development packages required for **portable** configure (bundled GLFW/X11):

| Package | Purpose |
| --- | --- |
| `libxinerama-dev` (Debian/Ubuntu/Kali) / `libXinerama-devel` (Fedora) | `X11/extensions/Xinerama.h` for GLFW’s X11 backend |
| Typical X11/OpenGL `-dev` stack already used by raylib desktop builds | link/compile against host graphics |

```sh
sudo apt-get install libxinerama-dev   # Kali / Debian / Ubuntu
./tools/package_portable.sh linux-x64
```

If you cannot install system packages, point CMake at a prefix that provides the
same header (official distro package contents only — do not invent headers):

```sh
export CMAKE_PREFIX_PATH=/path/to/prefix   # contains include/X11/extensions/Xinerama.h
./tools/package_portable.sh linux-x64
```

Do **not** set `CPATH`/`C_INCLUDE_PATH` to temporary staging directories.

The portable package includes:

- `BAGS_LAB` / `BAGS_LAB.EXE` executable (static raylib 6.0 + bundled GLFW)
- `BAGS_LAB.sh` / `BAGS_LAB.BAT` launcher (VFAT-safe temp-exec strategy)
- `data/` and `assets/planets/` (manifest-referenced GLBs)
- **No** dynamic `libraylib.so`
- **No** `libtiff`
- **No** `libglfw.so.3` (GLFW is statically linked from raylib’s tree)

When `lib/` exists, it holds only non-system runtime libraries that the
dependency audit requires. With the current static-raylib portable build,
`lib/` is typically empty/absent. Do **not** copy a host `libglfw.so.3` into
`lib/`.

## USB / VFAT behavior — VERIFIED (linux-x64)

- VFAT `showexec` typically grants `+x` only to `.EXE` / `.BAT` names.
- Launch with `bash BAGS_LAB.sh` on every filesystem. On a first run the
  launcher detects missing system runtime libraries and uses the host package
  manager (with an administrator-password prompt) to install them. Later runs
  only check and launch; they do not reinstall packages. The raw ELF cannot
  self-repair a missing dependency because Linux loads dependencies before the
  executable begins.
- Plain `BAGS_LAB` may lack execute permission on VFAT; that is filesystem policy, not a missing binary.

## Host libraries still required (portable)

This remains a dynamically linked desktop build for system graphics stacks.
Typical Linux hosts still provide:

- glibc / libstdc++ / libgcc
- X11 / OpenGL stacks used by raylib’s desktop backend

GLFW itself is **not** required from the host for the portable package.
Runtime `libXinerama.so.1` is commonly present with the X11 stack even when
only the `-dev` package was missing at build time.

The automatic first-run installer supports `apt-get` (Kali/Debian/Ubuntu),
`dnf` (Fedora/RHEL), `pacman` (Arch), and `zypper` (openSUSE). It installs
runtime graphics/C++ packages only; it never installs raylib, GLFW, TIFF,
compilers, or development headers on a target computer. It needs internet
access and administrator authorization only if a package is missing. Set
`BAGS_LAB_NO_AUTO_INSTALL=1` to make missing dependencies fail instead.

## Linux ARM64 (`dist/linux-arm64`) — NOT YET TESTED

Same conceptual layout. Package only on an aarch64 Linux host (or after a
verified cross-compile smoke). The dispatcher refuses to fabricate ARM64
packages on x86_64. Status: **BUILD-READY** tooling, **NOT YET TESTED** runtime.

## Wayland note

Automated window-manager close (`wmctrl` / `xdotool windowclose`) may be
unreliable under Wayland/XWayland. Application-level clean exit (`CloseWindow`
after the main loop) is the verified shutdown path. This does not affect normal
interactive use of the window close control.

## Verification checklist (linux-x64)

- Resource resolution from a foreign working directory
- USB VFAT launcher (`BAGS_LAB.sh` / `BAGS_LAB.EXE`) from `/tmp` or the stick
- Portable dependency audit profile `portable` (no libtiff / no dynamic libraylib / no libglfw)
- Shared-dev CPack smoke uses profile `shared-dev` (bundled libraylib; host glfw allowed)
- Clean-host / second-machine run without installing GLFW, raylib, or TIFF
- GUI start on a Linux host with display available
- Starfield uses `DrawPoint3D` (not per-star `DrawSphere`) — hang regression covered
