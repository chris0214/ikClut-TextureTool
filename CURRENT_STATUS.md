# IKCLUT Studio Current Status

Date: 2026-05-05

## Summary

IKCLUT Studio is ready for a friend/internal beta release. The main workflow is usable: load an image, build a grade, preview the result, inspect scopes/3D LUT, import LUTs, save projects/presets, and export ikClut PNG, CUBE, Hald PNG, or batch-processed images.

Local mask editing has been removed from this beta because the workflow was not reliable enough for release. Older project mask fields are ignored by the current UI/runtime path.

The node editor has also been adjusted for the open-source beta: reverse drag connections are supported, multi-input mixer ports are individually connectable, node labels are shown as readable UI names instead of internal type strings, and the final output node is treated as a single-input terminal with no outgoing port.

Imported LUTs are no longer forced into the project without recourse: importing a LUT records an undo checkpoint, and the LUT mix panel includes a button to remove the imported LUT.

## Verification

Commands run successfully:

```powershell
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Result: 100% tests passed, 0 tests failed out of 1.

## Release Notes

- Author: 克里斯提亚娜
- License: Apache-2.0
- Windows binary package includes Qt and MinGW runtime files.
- Single-file EXE package is a self-extracting launcher that prepares the portable runtime under `%LOCALAPPDATA%\IkClutStudio\0.1.0-beta`.

## Known Limitations

- Local mask editing is not available in this beta.
- Mixed 1D+3D CUBE files are rejected.
- Node graphs cover practical serial and mixer workflows, but they are not a fully general typed DAG engine.
- No installer is included.
