# IKCLUT Studio

IKCLUT Studio is a Windows desktop tool for image-based color grading and LUT export. It can load reference images, build color grades, preview scopes and LUT shape, import LUT files, save projects, and export LUT assets for MMD/MME style workflows.

Author: 克里斯提亚娜

License: Apache-2.0

## Current Release

This release is intended as a friend/internal beta. The core color pipeline, project workflow, LUT import/export, node graph basics, and preview path are usable, but the UI has not yet had a full public-release QA pass.

## Quick Start

1. Run `IkClutStudio.exe`.
2. Open an image.
3. Adjust the grade from the panels.
4. Optionally import a `.cube` or Hald PNG LUT.
5. Export as ikClut PNG, CUBE, Hald PNG, or processed images.

## Known Limitations

- Mixed 1D+3D CUBE files are rejected.
- Local mask editing has been removed from this beta because the workflow was not reliable enough for release.
- Node graphs cover practical serial and mixer workflows, but they are not a fully general typed DAG engine yet.
- This beta does not include an installer.

## Build From Source

For a deeper explanation of the architecture, implementation principles, and feature design, see `HandBook.md`.

Requirements:

- CMake 3.25 or newer
- C++20 compiler
- Qt 6 with Core, Gui, Widgets, Concurrent, and LinguistTools
- Windows for the bundled DirectX preview backend

Typical build:

```powershell
cmake -S . -B build
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

For a Windows runtime folder, deploy Qt dependencies with `windeployqt` or enable `IKCLUT_ENABLE_QT_DEPLOY` in CMake.

## Third-Party Runtime

The Windows binary package includes Qt runtime libraries and MinGW runtime DLLs. See `THIRD_PARTY_NOTICES.md` for details.
