<p align="center">
  <img src="resources/icon.png" width="120" alt="MagnifyFactory icon">
</p>

<h1 align="center">MagnifyFactory</h1>

<p align="center">
  A fast, native, modular file conversion desktop app for Windows.<br>
  Video, audio, and images today — PDF, documents, and archives are on the roadmap.
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/platform-Windows-0078D6">
  <img alt="language" src="https://img.shields.io/badge/language-C%2B%2B20-blue">
  <img alt="UI" src="https://img.shields.io/badge/UI-Qt6-41CD52">
  <img alt="license" src="https://img.shields.io/badge/license-MIT-green">
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
  popup, done.
- **Windows context menu integration** — right-click any file → *Convert
  with MagnifyFactory* (see [scripts/](scripts/)).
- **Real job queue** — configurable concurrency, live progress and ETA,
  pause/cancel, structured failure dialogs (not raw FFmpeg stderr).
- **Smart remux** — skips re-encoding when the source codec is already
  compatible with the target container (e.g. MKV H.264/AAC → MP4 is a
  stream copy, not a re-encode).
- **Video**: MP4, MKV, MOV, MPEG/MPG, WebM, FLV.
- **Audio**: MP3, WAV, FLAC, AAC, M4A, OGG — including audio extraction
  straight from a video file.
- **Images**: PNG, JPEG, WebP, AVIF, BMP, TIFF, GIF.
- **PDF ↔ image**: render a PDF's first page to PNG/JPEG (Poppler), or embed
  an image losslessly into a single-page PDF.
- **PDF compress**: recompress embedded images and streams with `qpdf`.
- **Output next to the source file** — no separate output folder to manage;
  converted files land beside the original.
- **Dark, information-dense UI** — a category sidebar and a queue table, no
  unnecessary chrome.

## Roadmap

- [x] PDF ↔ image conversion
- [x] PDF compress (via `qpdf`)
- [ ] PDF merge/split (needs multi-file selection in the UI, which doesn't
      exist yet — single-file drag-and-drop only)
- [ ] Documents and archive conversion
- [ ] Hardware-accelerated encoding (NVENC / AMF / Quick Sync)
- [ ] Presets (YouTube, Discord, WhatsApp, Instagram, ...)
- [ ] Batch folder processing and watch folders
- [ ] `magnify` CLI sharing the same core as the GUI
- [ ] Plugin API for third-party format/tool modules

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

## Contributing

Issues and PRs are welcome. Before opening a PR:

1. Build in Debug and run `ctest` — all tests should pass.
2. Keep conversion logic out of UI code; it belongs in `core/` or `engines/`.
3. New formats go through `FormatRegistry`, not hardcoded extension checks.

## License

[MIT](LICENSE)
