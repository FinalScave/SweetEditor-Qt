#pragma once

#include <QList>
#include <QString>

#include <EditorMetadata.h>
#include <LanguageConfiguration.h>

#include <optional>

#include <sweeteditor/editor_types.h>
#include <sweeteditor/document.h>
#include <sweeteditor/foundation.h>

namespace sweeteditor::qt {

enum class CompletionTriggerKind {
  Invoked,
  Character,
  Retrigger,
};

struct CompletionItem {
  struct TextEdit {
    ::sweeteditor::TextRange range;
    QString new_text;
  };

  static constexpr int KIND_KEYWORD = 0;
  static constexpr int KIND_FUNCTION = 1;
  static constexpr int KIND_VARIABLE = 2;
  static constexpr int KIND_CLASS = 3;
  static constexpr int KIND_INTERFACE = 4;
  static constexpr int KIND_MODULE = 5;
  static constexpr int KIND_PROPERTY = 6;
  static constexpr int KIND_SNIPPET = 7;
  static constexpr int KIND_TEXT = 8;

  static constexpr int INSERT_TEXT_FORMAT_PLAIN_TEXT = 1;
  static constexpr int INSERT_TEXT_FORMAT_SNIPPET = 2;

  QString label;
  QString detail;
  QString insert_text;
  int insert_text_format {INSERT_TEXT_FORMAT_PLAIN_TEXT};
  std::optional<TextEdit> text_edit;
  QString filter_text;
  QString sort_key;
  int kind {KIND_TEXT};

  QString matchText() const {
    return filter_text.isEmpty() ? label : filter_text;
  }
};

struct CompletionContext {
  CompletionTriggerKind trigger_kind {CompletionTriggerKind::Invoked};
  QString trigger_character;
  ::sweeteditor::TextPosition cursor_position;
  QString line_text;
  std::optional<::sweeteditor::TextRange> word_range;
  const LanguageConfiguration* language_configuration {nullptr};
  const EditorMetadata* editor_metadata {nullptr};
  const ::sweeteditor::Document* document {nullptr};
};

struct CompletionResult {
  QList<CompletionItem> items;
  bool is_incomplete {false};
};

class CompletionProvider {
public:
  virtual ~CompletionProvider() = default;

  virtual bool isTriggerCharacter(const QString& text) const {
    (void)text;
    return false;
  }

  virtual CompletionResult provideCompletions(const CompletionContext& context) = 0;
};

} // namespace sweeteditor::qt
