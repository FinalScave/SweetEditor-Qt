#pragma once

#include <cstdint>

#include <QColor>
#include <QRectF>

class QPainter;

namespace sweeteditor::qt {

class EditorIconProvider {
public:
  virtual ~EditorIconProvider() = default;

  virtual bool paintIcon(QPainter& painter,
                         int32_t icon_id,
                         const QRectF& rect,
                         const QColor& tint) const = 0;
};

} // namespace sweeteditor::qt
