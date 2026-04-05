#pragma once

#include <QString>

#include <EditorMetadata.h>

#include <sweeteditor/document.h>
#include <sweeteditor/foundation.h>

namespace sweeteditor::qt {

struct NewLineAction {
  bool handled {false};
  QString text;
};

class NewLineActionProvider {
public:
  virtual ~NewLineActionProvider() = default;

  virtual NewLineAction createAction(::sweeteditor::Document& document,
                                     const EditorMetadata& metadata,
                                     const ::sweeteditor::TextPosition& position) = 0;
};

} // namespace sweeteditor::qt
