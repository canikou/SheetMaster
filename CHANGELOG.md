# Changelog

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
