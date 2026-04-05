#pragma once

#include <QColor>

namespace sweeteditor::qt {

struct EditorTheme {
  QColor background {0xFF, 0xFF, 0xFF};
  QColor foreground {0x1F, 0x23, 0x28};
  QColor border {0xD0, 0xD7, 0xDE};
  QColor accent {0x09, 0x66, 0xDA};
  QColor placeholder {0x57, 0x6A, 0x7A};

  static EditorTheme light();
  static EditorTheme dark();
};

inline EditorTheme EditorTheme::light() {
  return {};
}

inline EditorTheme EditorTheme::dark() {
  return {
    QColor(0x0D, 0x11, 0x17),
    QColor(0xE6, 0xED, 0xF3),
    QColor(0x30, 0x36, 0x3D),
    QColor(0x2F, 0x81, 0xF7),
    QColor(0x8B, 0x94, 0x9E),
  };
}

} // namespace sweeteditor::qt
