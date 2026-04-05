<div align="center">

[English](README.md) | **简体中文**

# SweetEditor-Qt

### SweetEditor 的 Qt 适配

**基于 OpenSweetEditor C++ core 的 SweetEditor Qt 适配仓库。**

[![C++17](https://img.shields.io/badge/C++-17-blue.svg?logo=cplusplus)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/Qt-Widgets%20%2F%20QML-41CD52.svg?logo=qt)](https://www.qt.io/)
[![Status](https://img.shields.io/badge/Status-Bootstrapping-orange.svg)](#当前状态)
[![License](https://img.shields.io/badge/License-GPL--3.0-red.svg)](LICENSE)

</div>

---

## 项目定位

SweetEditor-Qt 是 SweetEditor 的 Qt 适配仓库。它延续 OpenSweetEditor 已经建立的架构与能力，把 SweetEditor 的编辑核心带入 Qt 生态，并以 Qt 原生的渲染、输入和集成方式对外提供能力。

当前的 Qt 路径基于对 OpenSweetEditor C++ core 的直接集成。

## 原始仓库

上游多平台项目是 OpenSweetEditor：

[https://github.com/FinalScave/OpenSweetEditor](https://github.com/FinalScave/OpenSweetEditor)

## 设计目标

- 复用 SweetEditor core 已经建立好的编辑语义和 render model 架构。
- 在 Qt 中直接接入 OpenSweetEditor 的 C++ core。
- 对外提供 Qt 风格的 API，而不是沿用 Android、Swing、WinForms 或 Apple 平台的 UI 约定。
- 为渲染、IME、快捷键、滚动、选择、折叠和编辑器装饰建立一条可落地的 Qt 原生实现路径。

## 架构方向

当前目标中的 Qt 路径采用如下结构：

```text
+--------------------------------------------------------------+
|                    Qt 层（输入 + 渲染）                      |
|                                                              |
| QWidget / QML, QPainter, QFontMetricsF, QKeyEvent, IME       |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|                        Qt 适配层（C++）                      |
|                                                              |
| SweetEditorWidget, EditorRenderer, QtTextMeasurer            |
+-----------------------------+--------------------------------+
                              |
                              v
+--------------------------------------------------------------+
|               OpenSweetEditor Core API（C++17）              |
|                                                              |
| Document / Layout / Decoration / EditorCore / Undo / Gesture |
+--------------------------------------------------------------+
```

## 范围说明

这个仓库聚焦于 SweetEditor 的 Qt 适配本身。这里的文档、示例和后续公开 API 都围绕 Qt 展开。

## 规划特性

- 基于 SweetEditor render model 的 Qt 原生渲染。
- 通过 Qt 字体接口完成文本测量。
- 在 Qt 中直接调用 OpenSweetEditor C++ core。
- 将键盘、鼠标、滚轮和 IME 输入转发到编辑核心。
- 继承核心模型中的装饰和高级编辑能力，包括语法高亮、语义高亮、折叠、inlay hints、phantom text、diagnostic、guides 和 linked editing。
- 提供 Qt 风格的 widget API，并为后续可能的 QML 集成保留空间。

## 当前状态

当前仓库仍处于起步阶段，但技术路线已经明确：Qt 侧直接接入 OpenSweetEditor 的 C++ core，后续继续补齐 widget、渲染和编辑器核心之间的连接。

## 最小 Qt 示例

下面的代码片段体现的是当前 Qt API 的方向：

```cpp
#include "SweetEditorWidget.h"
#include "EditorTheme.h"

auto* editor = new sweeteditor::qt::SweetEditorWidget(parent);
editor->applyTheme(sweeteditor::qt::EditorTheme::dark());
editor->show();
```

## 构建

SweetEditor-Qt 当前以 Qt 工程的形式构建，并链接 OpenSweetEditor 的 C++ 静态库。

### CLion / CMake Prefix Path

在 CLion 中打开本仓库时，需要在 CMake Profile 里把 `CMAKE_PREFIX_PATH` 指向 Qt kit 根目录，这样 `find_package(Qt6 REQUIRED ...)` 才能正确解析。

示例 CMake 选项：

```text
-DCMAKE_PREFIX_PATH=D:\Qt\6.8.2\msvc2022_64
```

在 CLion 中通常配置在这里：

```text
Settings / Build, Execution, Deployment / CMake / CMake options
```

也可以直接通过命令行配置并构建：

```powershell
cmake -S . -B .\cmake-build-release-visual-studio -DCMAKE_PREFIX_PATH=D:\Qt\6.8.2\msvc2022_64
cmake --build .\cmake-build-release-visual-studio --config Release
```

### Windows 发布脚本

仓库内提供了一个 PowerShell 发布脚本：[scripts/depoly-dist.ps1](scripts/depoly-dist.ps1)。

Release 打包示例：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\depoly-dist.ps1 `
  -QtPath D:\Qt\6.8.2\msvc2022_64 `
  -BuildDir .\cmake-build-release-visual-studio `
  -Config Release
```

Debug 打包示例：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\depoly-dist.ps1 `
  -QtPath D:\Qt\6.8.2\msvc2022_64 `
  -BuildDir .\cmake-build-debug-visual-studio `
  -Config Debug
```

说明：

- `QtPath` 可以传 Qt kit 根目录，也可以直接传其 `bin` 目录。
- 脚本会严格使用你指定的 `Config`，不会自动推断 Debug 或 Release。
- `BuildDir` 和 `Config` 必须与实际要部署的可执行文件一致。
- 面向最终用户分发时，应使用 Release 构建。
- 输出目录默认生成在 `dist/` 下，并且脚本默认会额外创建 zip 包。

## 与 OpenSweetEditor 的关系

OpenSweetEditor 提供的是架构参考和能力基线，SweetEditor-Qt 则是把这套编辑器架构通过直接 C++ 集成带入 Qt 的专用适配仓库。

## 许可证

当前仓库使用 [GNU General Public License v3.0](LICENSE) 授权。
