#pragma once

#include <QString>

namespace sweeteditor::qt {

struct EditorMetadata {
  QString file_path;
  QString display_name;
  QString encoding {QStringLiteral("UTF-8")};
};

} // namespace sweeteditor::qt
