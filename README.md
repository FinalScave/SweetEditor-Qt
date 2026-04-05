<div align="center">

**English** | [简体中文](README_zh.md)

# SweetEditor-Qt

### Qt Adaptation of SweetEditor

**A Qt adaptation of SweetEditor built on top of the OpenSweetEditor C++ core.**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?logo=cplusplus)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-Widgets%20%2F%20QML-41CD52.svg?logo=qt)](https://www.qt.io/)
[![Status](https://img.shields.io/badge/Status-Bootstrapping-orange.svg)](#status)
[![License](https://img.shields.io/badge/License-GPL--3.0-red.svg)](LICENSE)

</div>

---

## Project Positioning

SweetEditor-Qt is the Qt adaptation of SweetEditor. It follows the architecture and capabilities established by OpenSweetEditor, and brings the SweetEditor editing core into the Qt ecosystem with Qt-native rendering, input handling, and integration style.

The current Qt path is based on direct integration with the OpenSweetEditor C++ core.

## Original Repository

The upstream multi-platform project is OpenSweetEditor:

[https://github.com/FinalScave/OpenSweetEditor](https://github.com/FinalScave/OpenSweetEditor)

## Design Goals

- Reuse the editing semantics and render-model architecture established by the SweetEditor core.
- Integrate the OpenSweetEditor C++ core directly from Qt.
- Expose a Qt-oriented API surface instead of inheriting Android, Swing, WinForms, or Apple UI conventions.
- Build a practical Qt-native path for rendering, IME, shortcuts, scrolling, selection, folding, and editor decorations.

## Architecture Direction

The intended Qt path follows this high-level structure:

```text
+--------------------------------------------------------------+
|                    Qt Layer (Input + Render)                 |
|                                                              |
| QWidget / QML, QPainter, QFontMetricsF, QKeyEvent, IME       |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                        Qt Adapter (C++)                      |
|                                                              |
| SweetEditorWidget, EditorRenderer, QtTextMeasurer            |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|               OpenSweetEditor Core API (C++17)               |
|                                                              |
| Document / Layout / Decoration / EditorCore / Undo / Gesture |
+--------------------------------------------------------------+
```

## Scope

This repository is focused on the Qt adaptation itself. The documentation, examples, and future public APIs here are all centered around Qt.

## Planned Feature Set

- Qt-native rendering based on the SweetEditor render model.
- Text measurement through Qt font APIs.
- Direct interaction with the OpenSweetEditor C++ core from Qt.
- Keyboard, mouse, wheel, and IME forwarding into the editor core.
- Decorations and advanced editor capabilities inherited from the core model:
  syntax highlighting, semantic highlighting, folding, inlay hints, phantom text, diagnostics, guides, and linked editing.
- A Qt-friendly widget API, with room for a later QML-facing integration path if needed.

## Status

This repository is currently in the bootstrap stage. The project has already adopted the direct Qt-to-C++ integration route, and the next steps are to continue filling in the Qt widget, rendering, and editor-core wiring.

## Minimal Qt Example

The snippet below reflects the current direction of the Qt-facing API:

```cpp
#include "SweetEditorWidget.h"
#include "EditorTheme.h"

auto* editor = new sweeteditor::qt::SweetEditorWidget(parent);
editor->applyTheme(sweeteditor::qt::EditorTheme::dark());
editor->show();
```

## Build

SweetEditor-Qt currently builds as a Qt project that links against the OpenSweetEditor C++ static library.

### CLion / CMake Prefix Path

When opening this repository in CLion, make sure the CMake profile points `CMAKE_PREFIX_PATH` to the Qt kit root so that `find_package(Qt6 REQUIRED ...)` can resolve correctly.

Example CMake option:

```text
-DCMAKE_PREFIX_PATH=D:\Qt\6.8.2\msvc2022_64
```

In CLion this is typically configured in:

```text
Settings / Build, Execution, Deployment / CMake / CMake options
```

You can also configure and build from the command line:

```powershell
cmake -S . -B .\cmake-build-release-visual-studio -DCMAKE_PREFIX_PATH=D:\Qt\6.8.2\msvc2022_64
cmake --build .\cmake-build-release-visual-studio --config Release
```

### Windows Deployment Script

This repository includes a PowerShell deployment script at [scripts/depoly-dist.ps1](scripts/depoly-dist.ps1).

Release package example:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\depoly-dist.ps1 `
  -QtPath D:\Qt\6.8.2\msvc2022_64 `
  -BuildDir .\cmake-build-release-visual-studio `
  -Config Release
```

Debug package example:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\depoly-dist.ps1 `
  -QtPath D:\Qt\6.8.2\msvc2022_64 `
  -BuildDir .\cmake-build-debug-visual-studio `
  -Config Debug
```

Notes:

- `QtPath` can be either the Qt kit root or its `bin` directory.
- The script uses the specified `Config` strictly and does not auto-infer Debug/Release.
- `BuildDir` and `Config` must match the actual executable you want to deploy.
- Release builds are the intended form for end-user distribution.
- The output directory is created under `dist/`, and the script also creates a zip package by default.

## Relationship to OpenSweetEditor

OpenSweetEditor is the source architecture reference and capability baseline. SweetEditor-Qt is the dedicated repository for bringing that editor architecture into Qt through direct C++ integration.

## License

This repository is currently distributed under the [GNU General Public License v3.0](LICENSE).
