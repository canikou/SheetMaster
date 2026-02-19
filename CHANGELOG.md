# Changelog

## v1.1.0 - Template workflow standardization

- Standardized build workflow to template-style CMake presets (`debug`, `release`) via `CMakePresets.json`.
- Refactored build layout into a core library target plus app executable target in `CMakeLists.txt`.
- Added baseline CTest coverage (`tests/core_tests.cpp`) and registered it in CMake.
- Standardized VS Code workflow (`.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/settings.json`) for preset-based build/debug.
- Added GitHub automation and governance files:
  - `.github/workflows/ci.yml`
  - `.github/CODEOWNERS`
  - `.github/dependabot.yml`
  - `CONTRIBUTING.md`
  - `SECURITY.md`
- Added `.editorconfig` and SPDX snippet templates (`.vscode/cpp.code-snippets`).
- Updated `README.md` to match the standardized layout/workflow and preset commands.
- Updated `scripts/package-portable.ps1` to use the `release` preset and release binary naming (`SheetMaster.exe`).

## v1.0.0 - Initial release

- Released SheetMaster as a Qt6 desktop assistant for Virtual Piano-style sheet playback.
- Added searchable/taggable song library with import, editing, grouping-mode support (`[]` / `()`), and sustain indicator support (`-` / `|`).
- Added always-on-top floating overlay with current/next line display, active-key highlighting, progress text, and pause state.
- Added duplicate display-name support through stable song IDs, allowing different format variants of the same song name.
- Standardized persistent storage to custom extensions:
  - `sheets/*.PADATA` for songs
  - `settings.PACFG` for app settings
  - `sheets/song_tags.PADISCRIM` for tag mappings
- Added migration paths from legacy `.txt` storage to the new formats.
- Integrated application icon resources for runtime window icons and Windows executable icon embedding.
- Performed release-quality polish:
  - removed obsolete API path for universal default tagging
  - improved safe integer conversions for UI rendering paths
  - tightened file-save guard checks for settings writes
  - updated project metadata/branding and repository hygiene (`.gitignore`, docs).
