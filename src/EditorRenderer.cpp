#include "EditorRenderer.h"

#include <QPainter>

namespace sweeteditor::qt {

void EditorRenderer::paint(QPainter& painter,
                           const QRect& bounds,
                           const EditorTheme& theme,
                           const QString& placeholder,
                           const ::sweeteditor::FontMetrics& font_metrics) const {
  painter.fillRect(bounds, theme.background);

  painter.setPen(theme.border);
  painter.drawRect(bounds.adjusted(0, 0, -1, -1));

  const QRect content_rect = bounds.adjusted(20, 16, -20, -16);

  painter.setPen(theme.accent);
  painter.drawText(content_rect.left(), content_rect.top() + 22, QStringLiteral("SweetEditor-Qt"));

  painter.setPen(theme.placeholder);
  const int baseline = content_rect.top() + static_cast<int>(32.0f + font_metrics.ascent);
  painter.drawText(content_rect.left(), baseline, placeholder);
}

} // namespace sweeteditor::qt
