# SheetMaster

C++23 Qt6 desktop app for Virtual Piano sheet playback, library management, and practice overlays.

## Summary

SheetMaster is a Virtual Piano assistant focused on quick sheet playback and iteration:

- Searchable/taggable song library.
- Sheet import/editor with configurable grouping (`[]` / `()`).
- Optional BPM metadata and song detail display.
- Always-on-top floating overlay with progress tracking and sheet view tab.
- Strict/non-strict advancement modes, manual navigation hotkeys, and persisted user settings.

## Feature Highlights

### Library and Song Management

- Search songs by text and filter by tag.
- Import songs with optional BPM and tags.
- Manage songs in a dedicated dialog:
  - edit/delete songs
  - batch export selected/all songs
  - import shared song payloads
- Share/export format: `SMX1:` encoded payload for easy cross-client transfer.
- Data sanitization flow (`Sanitize Data`) with:
  - pre-change summary
  - automatic `backup.zip` creation
  - convention normalization (metadata/grouping/sustain token cleanup)

### Playback and Controls

- `Shift+Enter`: pause/resume playback.
- `Left` / `Right`: step backward/forward by note.
- `Up` / `Down`: jump by whole overlay line.
- `Tab`: restart when playback is complete.
- Strict Mode requires exact expected key/chord before advancing.
- Non-strict mode advances on any monitored key press.

### Floating Overlay and Sheet View

- Base overlay remains a two-line always-on-top playback view.
- Header shows song/progress plus optional Tags/BPM detail blocks.
- Separate Sheet View tab can be shown/hidden from settings.
- Clicking the tab toggles an extended 10-line sheet panel.
- Sheet panel tracks current line position for scrolling context without per-key highlight.

### Safety and Runtime Behavior

- Single-instance lock prevents running multiple app instances simultaneously.
- Optional icon resources are auto-used when present.
- Runtime data remains file-based in the project directory.

## Technical Baseline

- Language: C++23
- Build: CMake + Ninja
- UI: Qt6 Widgets
- Tests: doctest + CTest
- Lint/format: clang-tidy + clang-format (`lint`, `format`, `format-check`)

## Getting Started

### Prerequisites (local Windows flow)

- MSYS2 UCRT64 at `C:/msys64/ucrt64`
- Required packages:
  - `gcc`
  - `cmake`
  - `ninja`
  - `gdb`
  - `qt6-base`
- VS Code extensions:
  - `ms-vscode.cpptools`
  - `ms-vscode.cmake-tools`

### Build and Run

1. Configure debug:
   - `cmake --preset debug`
2. Build debug:
   - `cmake --build --preset debug --parallel`
3. Run tests:
   - `ctest --preset debug`
4. Run app:
   - `build/debug/app.exe`

## Presets

- Local: `debug`, `release`
- Optional vcpkg: `vcpkg-debug`, `vcpkg-release`
- CI parity: `ci-debug`, `ci-release`, `ci-lint`

## VS Code Workflow

- `Ctrl+Shift+B` -> default `build (debug)` task
- `F5` -> `Debug (Preset Debug Binary)` launch profile
- Lint targets from tasks:
  - `format-check (debug)`
  - `lint (debug)`

## CI

GitHub Actions runs:

- Windows (MSYS2 UCRT64): debug build/test + release build
- Linux matrix (GCC/Clang): debug build/test + release build
- Lint job: `format-check` + `lint`

## Data Files

- Settings: `settings.PACFG`
- Songs: `sheets/*.PADATA`
- Song tags: `sheets/song_tags.PADISCRIM`
- Data sanitize backup: `backup.zip` (generated on demand)

## Packaging (Windows)

Use the portable packaging script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-portable.ps1
```

Outputs:

- `dist/SheetMaster/`
- `dist/SheetMaster-<version>-windows-portable.zip`
