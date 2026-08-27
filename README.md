<p align="center">
  <img src="resources/icon.png" width="120" alt="MagnifyFactory icon">
</p>

<h1 align="center">MagnifyFactory</h1>

<p align="center">
  A fast, native, modular file conversion desktop app for Windows.<br>
  Video, audio, images, PDF, documents, and archives — all in one place.
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/platform-Windows-0078D6">
  <img alt="language" src="https://img.shields.io/badge/language-C%2B%2B20-blue">
  <img alt="UI" src="https://img.shields.io/badge/UI-Qt6-41CD52">
  <img alt="license" src="https://img.shields.io/badge/license-GPL--3.0-blue">
</p>

---

## What it is

MagnifyFactory is a desktop conversion tool built around a real conversion
pipeline, not a themed wrapper around shell commands. Drop a file, pick a
target format from a popup, and it's queued and converted — powered by
FFmpeg under a typed C++ engine layer, with a job queue, real progress
reporting, and structured error messages instead of raw stderr dumps.

It's built to grow: an `IMediaEngine` abstraction sits between the UI and
FFmpeg, a `FormatRegistry` centralizes what the app knows how to convert, and
the job system (`ConversionJob` / `JobManager`) is decoupled from the UI and
routes each job to the right engine by name — FFmpeg for video/audio/images,
Poppler + a minimal in-process PDF writer for PDF ↔ image — so new engines
can be added without touching existing code.

## Features

- **Drag-and-drop conversion** — drop a file, pick the target format from a
  popup, done. Drop several at once (or use *Add Folder...*, with an
  optional recursive scan) and it's one popup per file type, not per file.
- **Windows context menu integration** — right-click any file → *Convert
  with MagnifyFactory* (see [scripts/](scripts/)).
- **Real job queue** — configurable concurrency, live progress and ETA,
  pause/cancel, structured failure dialogs (not raw FFmpeg stderr).
- **Smart remux** — skips re-encoding when the source codec is already
  compatible with the target container (e.g. MKV H.264/AAC → MP4 is a
  stream copy, not a re-encode).
- **GPU-accelerated encoding** — detects NVIDIA NVENC / AMD AMF / Intel Quick
  Sync by running a real test encode for each (not just checking what FFmpeg
  was compiled with), and lets you pick one from the queue controls.
- **Presets** — one-click bundles of format + settings (YouTube 1080p/4K,
  Discord, WhatsApp, MP3 320 kbps, FLAC, WebP/AVIF for web, PDF compress
  levels), shown right in the conversion popup. Drop your own `*.json`
  presets into a `presets/` folder next to the executable to add more.
- **Video**: MP4, MKV, MOV, MPEG/MPG, WebM, FLV.
- **Audio**: MP3, WAV, FLAC, AAC, M4A, OGG — including audio extraction
  straight from a video file.
- **Images**: PNG, JPEG, WebP, AVIF, BMP, TIFF, GIF.
- **PDF ↔ image**: render a PDF's first page to PNG/JPEG (Poppler), or embed
  an image losslessly into a single-page PDF.
- **PDF tools**: compress (recompress embedded images/streams), merge several
  PDFs into one, split one into a file per page — all via `qpdf`.
- **`magnify` CLI** — the same job system and engines as the GUI, scriptable
  from a terminal (see [Command-line interface](#command-line-interface)).
- **Watch folders** — point at a folder and a target format; anything
  dropped in there gets converted automatically a couple seconds later
  (settle delay, so a still-copying file isn't grabbed mid-write).
- **Archive tools** — extract zip/7z/rar/tar/gz with a click, or select
  multiple files of any type and compress them into a zip, via 7-Zip.
- **Document conversion**: Word (docx/doc), Excel (xlsx/xls), PowerPoint
  (pptx/ppt), OpenDocument (odt/ods/odp), RTF, and plain text — convert to
  PDF or between each other, via LibreOffice's headless CLI.
- **Plugin API** — drop a `.dll` implementing `IMagnifyPlugin` into
  `plugins/` next to the executable and it's loaded at startup; today a
  plugin contributes presets. See [Plugins](#plugins).
- **Output next to the source file** — no separate output folder to manage;
  converted files land beside the original.
- **Dark, information-dense UI** — a category sidebar and a queue table, no
  unnecessary chrome.

## Roadmap

- [x] PDF ↔ image conversion
- [x] PDF compress (via `qpdf`)
- [x] PDF merge/split (via `qpdf`)
- [x] Archive tools (extract zip/7z/rar/tar/gz, create zip/7z)
- [x] Document conversion (docx/xlsx/pptx ↔ PDF, etc., via LibreOffice)
- [x] Hardware-accelerated encoding (NVENC / AMF / Quick Sync)
- [x] Presets (YouTube, Discord, WhatsApp, MP3/FLAC, WebP/AVIF, PDF compress)
- [x] Batch processing (multi-file drop/select, "Add Folder..." with an
      optional recursive scan; one format popup per file-type group)
- [x] Watch folders (auto-convert on file arrival)
- [x] `magnify` CLI sharing the same core as the GUI
- [x] Plugin API (dynamically loaded, contributes presets today)

## Installing

Grab the latest `MagnifyFactory-Setup-<version>.exe` from the
[Releases](https://github.com/victorhmrod/MagnifyFactory/releases) page and
run it. It's a per-user install (no admin rights needed) with an optional
checkbox to add *Convert with MagnifyFactory* to the Explorer right-click
menu, and it registers an uninstaller.

MagnifyFactory shells out to external tools rather than bundling them —
make sure these are on your `PATH`:
- `ffmpeg` / `ffprobe` (e.g. `winget install Gyan.FFmpeg`) for video/audio/image conversion
- `pdftoppm` from Poppler (e.g. `winget install oschwartz10612.Poppler`) for PDF → image
- `qpdf` (e.g. `winget install QPDF.QPDF`) for PDF compression
- `7z` (e.g. `winget install 7zip.7zip`) for archive extract/create
- `soffice` from [LibreOffice](https://www.libreoffice.org/) (e.g.
  `winget install TheDocumentFoundation.LibreOffice`) for document conversion

## Building from source

### Prerequisites

- Windows 10/11
- [Visual Studio 2022+](https://visualstudio.microsoft.com/) with the
  "Desktop development with C++" workload
- [CMake](https://cmake.org/) ≥ 3.21
- [vcpkg](https://github.com/microsoft/vcpkg)
- [FFmpeg](https://ffmpeg.org/) (`ffmpeg` and `ffprobe` on your `PATH`) —
  MagnifyFactory shells out to these rather than linking libav directly
- [Poppler](https://github.com/oschwartz10612/poppler-windows) (`pdftoppm`
  on your `PATH`) for PDF → image rendering
- [QPDF](https://qpdf.readthedocs.io/) (`qpdf` on your `PATH`) for PDF compression
- [7-Zip](https://www.7-zip.org/) (`7z` on your `PATH`) for archive tools
- [LibreOffice](https://www.libreoffice.org/) (`soffice` on your `PATH`, or
  installed at its default location) for document conversion

### Get Qt6

```bash
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.bat
./vcpkg/vcpkg.exe install "qtbase[widgets]:x64-windows"
```

### Configure and build

From a **Developer PowerShell for VS** (so `cl.exe` is on `PATH`):

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --target MagnifyFactory
```

The binary lands at `build/MagnifyFactory.exe`.

### Run the tests

```powershell
cd build
ctest --output-on-failure
```

### Building the installer

Requires [Inno Setup 6](https://jrsoftware.org/isinfo.php)
(`winget install --id JRSoftware.InnoSetup -e`). This builds a Release
config and packages it in one step:

```powershell
./scripts/build_installer.ps1
```

The output lands at `dist/MagnifyFactory-Setup-<version>.exe`. See
[installer/MagnifyFactory.iss](installer/MagnifyFactory.iss).

## Windows context menu integration

The installer offers this as an opt-in checkbox during setup, registered
against the actual install path. For a dev build (running straight out of
`build/`), register/unregister it manually instead (per-user, no admin
rights needed — only touches `HKEY_CURRENT_USER`):

```powershell
./scripts/register_context_menu.ps1
./scripts/unregister_context_menu.ps1
```

## Command-line interface

`magnify` (`build/magnify.exe`) drives the exact same `JobManager` and
engines the GUI does — no separate conversion logic.

```powershell
magnify convert video.mp4 --to mp3
magnify convert input.mkv --to mp4 --codec h265 --hardware nvidia
magnify convert *.png --to webp --quality 85
magnify convert clip.mp4 --to mp4 --preset "Discord"
magnify convert --list-presets

magnify pdf document.pdf --to png --dpi 300
magnify pdf a.pdf b.pdf c.pdf --merge -o combined.pdf
magnify pdf document.pdf --split

magnify convert report.docx --to pdf
magnify convert sheet.xlsx --to ods
```

Run `magnify <command> --help` for the full option list.

## Plugins

A plugin is a Qt plugin DLL implementing
[`IMagnifyPlugin`](src/plugins/IMagnifyPlugin.h) (`name()`, `version()`,
`initialize()`, `shutdown()`, and today `presets()`). Drop the `.dll` into a
`plugins/` folder next to `MagnifyFactory.exe` and it's loaded automatically
at startup — no rebuild of the app needed.

[`src/plugins/sample/`](src/plugins/sample/) is a complete, working example:
a separately-built DLL that contributes a "WhatsApp Status" preset the core
app never hardcoded. Build it with the rest of the project
(`magnify_plugin_sample` target) — it lands in `build/plugins/` and the app
picks it up on the next launch.

## Architecture

```
UI (Qt widgets)
  ↓
JobManager / ConversionJob      — queue, concurrency, progress, status
  ↓
IMediaEngine                    — engine-agnostic conversion contract
  ↓
FFmpegMediaEngine
  ├── FFmpegCommandBuilder      — builds argv safely, never shells out to a string
  └── FFprobe                   — JSON-based media probing
  ↓
ffmpeg / ffprobe (external process, via QProcess)
```

Key pieces:

| Component | Responsibility |
|---|---|
| [`FormatRegistry`](src/core/FormatRegistry.h) | Central catalogue of every format the app knows — no extensions hardcoded elsewhere. |
| [`ConversionJob`](src/core/ConversionJob.h) | One unit of work: input/output paths, status, progress, ETA, error message. |
| [`JobManager`](src/core/JobManager.h) | Owns the queue, enforces a concurrency limit, drives jobs against an engine. |
| [`IMediaEngine`](src/engines/IMediaEngine.h) | Abstraction any conversion backend implements — swappable, testable in isolation. |
| [`FFmpegCommandBuilder`](src/engines/ffmpeg/FFmpegCommandBuilder.h) | Produces `QStringList` argv, never a concatenated shell string. |
| [`PdfEngine`](src/engines/pdf/PdfEngine.h) | PDF ↔ image via Poppler's `pdftoppm`, plus a minimal in-process PDF writer for image → PDF. |
| [`HardwareAccelerationManager`](src/hardware/HardwareAccelerationManager.h) | Verifies GPU encoders with a real test encode rather than trusting what FFmpeg was compiled with. |
| [`PresetRegistry`](src/presets/PresetRegistry.h) | Built-in + user-supplied presets — a name mapped to a target format and a bundle of engine parameters. |
| [`WatchFolderManager`](src/watch/WatchFolderManager.h) | Monitors folders via `QFileSystemWatcher`, debounces new files with a settle delay, reports them for conversion. |
| [`ArchiveEngine`](src/engines/archive/ArchiveEngine.h) | Extract/create zip/7z/rar/tar/gz via the 7-Zip CLI. |
| [`DocumentEngine`](src/engines/document/DocumentEngine.h) | Converts docx/xlsx/pptx/odt/ods/odp/rtf/txt to PDF or each other via LibreOffice's headless CLI. |
| [`PluginManager`](src/plugins/PluginManager.h) | Loads `IMagnifyPlugin` DLLs from `plugins/` via `QPluginLoader`, merges what they contribute into the core registries. |

## Contributing

Issues and PRs are welcome. Before opening a PR:

1. Build in Debug and run `ctest` — all tests should pass.
2. Keep conversion logic out of UI code; it belongs in `core/` or `engines/`.
3. New formats go through `FormatRegistry`, not hardcoded extension checks.

## License

[GPL-3.0](LICENSE) — Copyright (C) 2026 MagnifyFactory contributors
