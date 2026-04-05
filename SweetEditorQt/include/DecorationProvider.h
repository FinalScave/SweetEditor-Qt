#pragma once

#include <EditorMetadata.h>
#include <LanguageConfiguration.h>

#include <map>
#include <optional>
#include <vector>

#include <sweeteditor/decoration.h>
#include <sweeteditor/document.h>
#include <sweeteditor/editor_types.h>

namespace sweeteditor::qt {

enum class DecorationType : uint32_t {
  NONE = 0,
  SYNTAX_HIGHLIGHT = 1u << 0,
  SEMANTIC_HIGHLIGHT = 1u << 1,
  INLAY_HINT = 1u << 2,
  DIAGNOSTIC = 1u << 3,
  FOLD_REGION = 1u << 4,
  INDENT_GUIDE = 1u << 5,
  BRACKET_GUIDE = 1u << 6,
  FLOW_GUIDE = 1u << 7,
  SEPARATOR_GUIDE = 1u << 8,
  GUTTER_ICON = 1u << 9,
  PHANTOM_TEXT = 1u << 10,
};

inline DecorationType operator|(DecorationType lhs, DecorationType rhs) {
  return static_cast<DecorationType>(
    static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
  );
}

inline DecorationType& operator|=(DecorationType& lhs, DecorationType rhs) {
  lhs = lhs | rhs;
  return lhs;
}

enum class DecorationApplyMode {
  MERGE = 0,
  REPLACE_ALL = 1,
  REPLACE_RANGE = 2,
};

struct DecorationContext {
  int visible_start_line {0};
  int visible_end_line {-1};
  int total_line_count {0};
  std::vector<::sweeteditor::TextChange> text_changes;
  const LanguageConfiguration* language_configuration {nullptr};
  const EditorMetadata* editor_metadata {nullptr};
  const ::sweeteditor::Document* document {nullptr};
};

template<typename T>
using DecorationLineMap = std::map<size_t, ::sweeteditor::Vector<T>>;

struct DecorationResult {
  std::optional<DecorationLineMap<::sweeteditor::StyleSpan>> syntax_spans;
  std::optional<DecorationLineMap<::sweeteditor::StyleSpan>> semantic_spans;
  std::optional<DecorationLineMap<::sweeteditor::InlayHint>> inlay_hints;
  std::optional<DecorationLineMap<::sweeteditor::DiagnosticSpan>> diagnostics;
  std::optional<::sweeteditor::Vector<::sweeteditor::IndentGuide>> indent_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::BracketGuide>> bracket_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::FlowGuide>> flow_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::SeparatorGuide>> separator_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::FoldRegion>> fold_regions;
  std::optional<DecorationLineMap<::sweeteditor::GutterIcon>> gutter_icons;
  std::optional<DecorationLineMap<::sweeteditor::PhantomText>> phantom_texts;

  DecorationApplyMode syntax_spans_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode semantic_spans_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode inlay_hints_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode diagnostics_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode indent_guides_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode bracket_guides_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode flow_guides_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode separator_guides_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode fold_regions_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode gutter_icons_mode {DecorationApplyMode::MERGE};
  DecorationApplyMode phantom_texts_mode {DecorationApplyMode::MERGE};
};

class DecorationProvider {
public:
  virtual ~DecorationProvider() = default;

  virtual DecorationType capabilities() const noexcept {
    return DecorationType::NONE;
  }

  virtual DecorationResult provideDecorations(const DecorationContext& context) = 0;
};

} // namespace sweeteditor::qt
