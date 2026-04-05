#pragma once

#include <QRect>
#include <QString>

#include <EditorTheme.h>

#include <sweeteditor/layout.h>

class QPainter;

namespace sweeteditor::qt {

class EditorRenderer {
public:
  void paint(QPainter& painter,
             const QRect& bounds,
             const EditorTheme& theme,
             const QString& placeholder,
             const ::sweeteditor::FontMetrics& font_metrics) const;
};

} // namespace sweeteditor::qt
