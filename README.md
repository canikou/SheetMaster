# SheetMaster

Production-style C++20 Qt6 desktop app using CMake presets, VS Code workflow, CTest, and GitHub CI.

## Goals

- Fast local iteration (`Ctrl+Shift+B`, `F5`).
- Standardized template-aligned layout and workflow.
- Debug and release preset parity across local + CI.

## Prerequisites

- Windows + MSYS2 UCRT64 toolchain installed at `C:/msys64/ucrt64`.
- Tools available in UCRT64:
  - `gcc`
  - `cmake`
  - `ninja`
  - `gdb`
  - `qt6-base`
- VS Code extensions:
  - `ms-vscode.cpptools`
  - `ms-vscode.cmake-tools`

## Project Structure

- `CMakeLists.txt`: app + core library targets and test registration.
- `CMakePresets.json`: configure/build/test presets (`debug`, `release`).
- `src/main.cpp`: executable entrypoint target.
- `src/*.cpp` + `include/piano_assist/*.hpp`: reusable app/core code.
- `tests/core_tests.cpp`: baseline CTest executable.
- `.vscode/tasks.json`: configure/build/test tasks.
- `.vscode/launch.json`: preset-based debug launch profiles.
- `.github/workflows/ci.yml`: GitHub Actions build/test pipeline.

## Binary Naming Convention

- Debug/dev-style builds output `app.exe`.
- Release builds output `SheetMaster.exe`.

## Getting Started

1. Configure Debug:
   - `cmake --preset debug`
2. Build Debug:
   - `cmake --build --preset debug --parallel`
3. Run tests:
   - `ctest --preset debug`
4. Run app:
   - `./build/debug/app.exe`

## VS Code Workflow

- `Ctrl+Shift+B`: runs default task `build (debug)`.
- `F5`: use launch profile `Debug (Preset Debug Binary)`.
  - Launches `${workspaceFolder}/build/debug/app.exe`
  - Runs `build (debug)` first as preLaunchTask.
- Manual tasks:
  - `configure (debug)` / `configure (release)`
  - `build (debug)` / `build (release)`
  - `test (debug)` / `test (release)`

## CMake Presets

- Configure presets: `debug`, `release`
- Build presets: `debug`, `release`
- Test presets: `debug`, `release`
- Generator: Ninja
- Toolchain path injected via preset environment:
  - `PATH=C:/msys64/ucrt64/bin;$penv{PATH}`

## App Features

- Song library with search + tag filtering.
- Import and edit songs from pasted text.
- Grouping modes: `[]` and `()`.
- Sustain indicators: `-` and `|`.
- Always-on-top floating overlay with line/chord progress UI.
- Strict and non-strict playback advancement.
- Enter-key pause/resume during playback.
- Persistent settings, songs, and per-song tags.

## Data Storage

- Settings: `settings.PACFG`
- Song files: `sheets/*.PADATA`
- Song tags: `sheets/song_tags.PADISCRIM`

## Distribution Notes (Windows)

Use the standardized packaging script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-portable.ps1
```

It builds release with presets and emits:

- `dist/SheetMaster/`
- `dist/SheetMaster-portable-win64.zip`

## For Coding Agents / LLMs

When modifying this repository:

1. Prefer presets over ad-hoc CMake commands.
   - Configure: `cmake --preset debug`
   - Build: `cmake --build --preset debug --parallel`
   - Test: `ctest --preset debug`
2. Keep debug output path assumption intact:
   - `build/debug/app.exe`
3. Keep release output named `SheetMaster.exe`.
4. Keep CI and local preset workflow in sync.
