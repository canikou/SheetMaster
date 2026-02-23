# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project adheres to Semantic Versioning where practical.

## [Unreleased]

- No entries yet.

## [1.5.2]

### Added
- Sanitize Data now audits and repairs tag data in addition to song-file conventions:
  - fills missing/empty tags with `Virtual Piano`
  - migrates legacy name-key tag entries to stable song IDs when unambiguous
  - removes orphan tag entries that no longer map to known songs
- Added sanitize-focused test coverage for:
  - missing-name + sustain/body normalization in song files
  - tag-repair workflow (missing tags, legacy key migration, orphan cleanup)

### Changed
- Import and edit flows now require a non-empty song name before saving.

### Fixed
- Sanitize preview now detects missing `name` metadata correctly instead of masking it during read.
- Sustain metadata normalization reporting now correctly detects legacy non-`-` indicators.

## [1.5.1]

### Fixed
- Auto Detect chunking now falls back to fixed 10-note lines when detected line-break data is not usable (for example, only one non-empty parsed line or very long wrapped lines).
- Overlay line detection no longer treats incidental newline artifacts as reliable split data.

## [1.5.0]

### Added
- Single-instance runtime lock to prevent launching multiple app instances at once.
- Expanded song metadata support with optional BPM storage and display controls.
- Overlay song detail blocks for Tags/BPM with per-field visibility settings.
- Sheet View tab on the floating overlay that toggles an extended 10-line sheet panel.
- Manage Songs workflows for:
  - batch export/import
  - cross-client share-string transfer (`SMX1:` payload format)
  - multi-song operations from a dedicated dialog
- Data convention migration flow (`Sanitize Data`) with preview summary and automatic `backup.zip`.
- Settings sections for playback, song details, and overlay sheet controls, including contextual tooltips.

### Changed
- Playback controls:
  - pause/resume moved to `Shift+Enter`
  - manual navigation added (`Left`/`Right` per note, `Up`/`Down` per line)
  - completion flow now supports `Tab` restart with on-overlay instruction text
- Overlay presentation was reworked to preserve the original 2-line behavior while supporting optional extended sheet view.
- Main UI labeling and playback details were updated for clearer runtime context.
- Packaging output naming updated to `SheetMaster-<version>-windows-portable.zip`.
- Release packaging kept on preset-based commands (`cmake --preset release`, `cmake --build --preset release`).

### Fixed
- Sustain token normalization now writes unified `-` conventions during sanitize/update paths.
- Sheet View toggle flow no longer mutates the base overlay layout when expanded/collapsed.
- Overlay tab seam/hover styling refined for clearer visual continuity and interaction feedback.

## [1.1.0]

- Standardized build workflow to template-style CMake presets (`debug`, `release`) via `CMakePresets.json`.
- Refactored build layout into a core library target plus app executable target in `CMakeLists.txt`.
- Added baseline CTest coverage (`tests/core_tests.cpp`) and registered it in CMake.
- Standardized VS Code workflow (`.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/settings.json`) for preset-based build/debug.
- Added GitHub automation and governance files (`.github/workflows/ci.yml`, `.github/CODEOWNERS`, `.github/dependabot.yml`, `CONTRIBUTING.md`, `SECURITY.md`).
- Updated `README.md` to match the standardized layout/workflow and preset commands.
- Updated `scripts/package-portable.ps1` to use the `release` preset and release binary naming (`SheetMaster.exe`).

## [1.0.0]

- Released SheetMaster as a Qt6 desktop assistant for Virtual Piano-style sheet playback.
- Added searchable/taggable song library with import, editing, grouping-mode support (`[]` / `()`), and sustain indicator support (`-` / `|`).
- Added always-on-top floating overlay with current/next line display, active-key highlighting, progress text, and pause state.
- Added duplicate display-name support through stable song IDs, allowing different format variants of the same song name.
- Standardized persistent storage to custom extensions (`sheets/*.PADATA`, `settings.PACFG`, `sheets/song_tags.PADISCRIM`).
- Added migration paths from legacy `.txt` storage to the new formats.
- Integrated application icon resources for runtime window icons and Windows executable icon embedding.
