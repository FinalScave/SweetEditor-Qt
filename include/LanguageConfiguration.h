#pragma once

#include <QString>
#include <QVector>

#include <sweeteditor/editor_types.h>

namespace sweeteditor::qt {

struct LanguageConfiguration {
  QString language_id;
  QString display_name;
  QVector<::sweeteditor::BracketPair> bracket_pairs;
  QVector<::sweeteditor::BracketPair> auto_closing_pairs;
};

} // namespace sweeteditor::qt
