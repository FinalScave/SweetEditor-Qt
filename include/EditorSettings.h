#pragma once

#include <QFont>

#include <sweeteditor/foundation.h>
#include <sweeteditor/visual.h>

namespace sweeteditor::qt {

class SweetEditorWidget;

class EditorSettings {
public:
  EditorSettings() = default;
  EditorSettings(const EditorSettings&) = delete;
  EditorSettings& operator=(const EditorSettings&) = delete;

  void setFont(const QFont& font);
  const QFont& font() const noexcept;

  void setTabSize(uint32_t tab_size);
  uint32_t tabSize() const noexcept;

  void setWrapMode(::sweeteditor::WrapMode mode);
  ::sweeteditor::WrapMode wrapMode() const noexcept;

  void setReadOnly(bool read_only);
  bool readOnly() const noexcept;

  void setCompositionEnabled(bool enabled);
  bool compositionEnabled() const noexcept;

  void setScale(float scale);
  float scale() const noexcept;

  void setLineSpacing(float add, float mult);
  float lineSpacingAdd() const noexcept;
  float lineSpacingMult() const noexcept;

  void setContentStartPadding(float padding);
  float contentStartPadding() const noexcept;

  void setShowSplitLine(bool show);
  bool showSplitLine() const noexcept;

  void setCurrentLineRenderMode(::sweeteditor::CurrentLineRenderMode mode);
  ::sweeteditor::CurrentLineRenderMode currentLineRenderMode() const noexcept;

  void setGutterSticky(bool sticky);
  bool gutterSticky() const noexcept;

  void setGutterVisible(bool visible);
  bool gutterVisible() const noexcept;

  void setSelectionHandlesEnabled(bool enabled);
  bool selectionHandlesEnabled() const noexcept;

  void setFoldArrowMode(::sweeteditor::FoldArrowMode mode);
  ::sweeteditor::FoldArrowMode foldArrowMode() const noexcept;

  void setAutoIndentMode(::sweeteditor::AutoIndentMode mode);
  ::sweeteditor::AutoIndentMode autoIndentMode() const noexcept;

  void setBackspaceUnindent(bool enabled);
  bool backspaceUnindent() const noexcept;

  void setInsertSpaces(bool enabled);
  bool insertSpaces() const noexcept;

  void setMaxGutterIcons(uint32_t count);
  uint32_t maxGutterIcons() const noexcept;

  void setDecorationScrollRefreshMinIntervalMs(int interval_ms);
  int decorationScrollRefreshMinIntervalMs() const noexcept;

  void setDecorationOverscanViewportMultiplier(float multiplier);
  float decorationOverscanViewportMultiplier() const noexcept;

private:
  friend class SweetEditorWidget;

  explicit EditorSettings(SweetEditorWidget* editor) noexcept;
  void bind(SweetEditorWidget* editor) noexcept;
  void syncFontFromWidget(const QFont& font) noexcept;

  SweetEditorWidget* editor_ {nullptr};
  QFont font_;
  uint32_t tab_size_ {4};
  ::sweeteditor::WrapMode wrap_mode_ {::sweeteditor::WrapMode::NONE};
  bool read_only_ {false};
  bool composition_enabled_ {true};
  float scale_ {1.0f};
  float line_spacing_add_ {0.0f};
  float line_spacing_mult_ {1.0f};
  float content_start_padding_ {0.0f};
  bool show_split_line_ {true};
  ::sweeteditor::CurrentLineRenderMode current_line_render_mode_ {
    ::sweeteditor::CurrentLineRenderMode::BACKGROUND
  };
  bool gutter_sticky_ {true};
  bool gutter_visible_ {true};
  bool selection_handles_enabled_ {false};
  ::sweeteditor::FoldArrowMode fold_arrow_mode_ {::sweeteditor::FoldArrowMode::ALWAYS};
  ::sweeteditor::AutoIndentMode auto_indent_mode_ {::sweeteditor::AutoIndentMode::NONE};
  bool backspace_unindent_ {true};
  bool insert_spaces_ {false};
  uint32_t max_gutter_icons_ {0};
  int decoration_scroll_refresh_min_interval_ms_ {16};
  float decoration_overscan_viewport_multiplier_ {1.5f};
};

} // namespace sweeteditor::qt
