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

SweetEditor-Qt 是 SweetEditor 的 Qt 适配仓库。它沿用 OpenSweetEditor 已经建立的架构与能力，把 SweetEditor 的编辑核心带入 Qt 生态，并以 Qt 原生的渲染、输入和集成方式对外提供能力。

当前的 Qt 实现路线是直接接入 OpenSweetEditor 的 C++ core。

## 原始仓库

上游多平台项目为 OpenSweetEditor：

[https://github.com/FinalScave/OpenSweetEditor](https://github.com/FinalScave/OpenSweetEditor)

## 设计目标

- 复用 SweetEditor core 已经建立好的编辑语义和 render model 架构。
- 在 Qt 中直接接入 OpenSweetEditor 的 C++ core。
- 对外提供 Qt 风格的 API，而不是沿用 Android、Swing、WinForms 或 Apple 平台的 UI 约定。
- 为渲染、IME、快捷键、滚动、选择、折叠和编辑器装饰建立一条可落地的 Qt 原生实现路径。

## 架构方向

当前目标中的 Qt 路线采用如下结构：

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
|              OpenSweetEditor Core API（C++17）               |
|                                                              |
| Document / Layout / Decoration / EditorCore / Undo / Gesture |
+--------------------------------------------------------------+
```

## 范围说明

这个仓库聚焦于 SweetEditor 的 Qt 适配本身。这里的文档、示例和后续公开 API 都围绕 Qt 展开。

## 计划特性

- 基于 SweetEditor render model 的 Qt 原生渲染。
- 通过 Qt 字体接口完成文本测量。
- 在 Qt 中直接调用 OpenSweetEditor C++ core。
- 将键盘、鼠标、滚轮和 IME 输入转发到编辑核心。
- 继承核心模型中的装饰和高级编辑能力：
  语法高亮、语义高亮、折叠、inlay hints、phantom text、diagnostic、guides 和 linked editing。
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

## 构建方向

SweetEditor-Qt 当前以 Qt 工程的形式构建，并链接 OpenSweetEditor 的 C++ 静态库。随着 Qt 适配继续推进，这里会逐步补齐构建和集成说明。

## 与 OpenSweetEditor 的关系

OpenSweetEditor 提供的是架构参考和能力基线，SweetEditor-Qt 则是把这套编辑器架构通过直接 C++ 集成带入 Qt 的专用适配仓库。

## 许可证

当前仓库使用 [GNU General Public License v3.0](LICENSE) 授权。
