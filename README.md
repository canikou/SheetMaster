# SheetMaster

SheetMaster is a Windows Qt6 desktop helper for Virtual Piano and similar keyboard-note formats.  
It lets you import sheet text, manage songs/tags, and use a draggable always-on-top overlay that highlights the current key group while you play.

## Features

- Song library with search + tag filtering.
- Import and edit songs directly from pasted text.
- Supports grouping modes:
  - `[]` chords
  - `()` chords (for formats where `[` and `]` are real keys)
- Supports sustain/delay indicators:
  - `-`
  - `|`
- Floating always-on-top overlay with:
  - current line + next line preview
  - highlighted current key group
  - song/progress info and pause indicator
- Strict mode and non-strict mode playback advancement.
- Enter-key pause/resume during playback.
- Persistent settings, songs, and per-song tags.

## Sheet Parsing Examples

- Single notes: `t r w t r w`
- Chords with square grouping: `[tf] [rd] [es]`
- Chords with parentheses grouping: `(R=) (6T)`
- Sustain markers: `t--- r-- [tf]-`
- Line-based chunking input:

```text
[tf]-u-o-s-
[rd]-y-o-a-
t r w t r w
```

## Data Storage

- Settings: `settings.PACFG`
- Song files: `sheets/*.PADATA`
- Song tags: `sheets/song_tags.PADISCRIM`

Song files use a metadata header (`id`, `name`, grouping, sustain) plus the raw sheet body.

## Build (MSYS2 UCRT64)

Open the **MSYS2 UCRT64** shell and install dependencies:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-qt6-base
```

Configure and build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
```

Run:

```bash
./build/app.exe
```

## Distribution Notes (Windows)

For sharing outside your dev machine, deploy Qt runtime files next to the executable:

```bash
C:/msys64/ucrt64/bin/windeployqt6.exe build/app.exe
```

If needed, also ship required UCRT64 runtime DLLs from `C:/msys64/ucrt64/bin`.