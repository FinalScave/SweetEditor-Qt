#pragma once

#include <QAbstractScrollArea>
#include <QPoint>
#include <QString>
#include <QVariant>

#include <memory>
#include <optional>
#include <utility>

#include <CompletionProvider.h>
#include <DecorationProvider.h>
#include <EditorIconProvider.h>
#include <EditorMetadata.h>
#include <EditorSettings.h>
#include <EditorTheme.h>
#include <LanguageConfiguration.h>
#include <NewLineActionProvider.h>

#include <sweeteditor/document.h>
#include <sweeteditor/editor_core.h>

class QEvent;
class QFocusEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace sweeteditor::qt {

class DecorationProviderManager;
class CompletionProviderManager;

class SweetEditorWidget final : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit SweetEditorWidget(QWidget* parent = nullptr);
  ~SweetEditorWidget() override;

  void loadDocument(const std::shared_ptr<::sweeteditor::Document>& document);
  bool loadFile(const QString& path);
  void loadText(const QString& text);
  QString text() const;

  void flush();

  void applyTheme(const EditorTheme& theme);
  const EditorTheme& theme() const noexcept;
  void setPerfOverlayEnabled(bool enabled);
  bool isPerfOverlayEnabled() const noexcept;

  EditorSettings& settings() noexcept;
  const EditorSettings& settings() const noexcept;

  void setLanguageConfiguration(const LanguageConfiguration& configuration);
  const LanguageConfiguration& languageConfiguration() const noexcept;

  void setEditorIconProvider(const EditorIconProvider* provider);
  const EditorIconProvider* editorIconProvider() const noexcept;

  void setMetadata(const EditorMetadata& metadata);
  const EditorMetadata& metadata() const noexcept;

  void addDecorationProvider(DecorationProvider* provider);
  void removeDecorationProvider(DecorationProvider* provider);
  void addCompletionProvider(CompletionProvider* provider);
  void removeCompletionProvider(CompletionProvider* provider);
  void triggerCompletion();
  void showCompletionItems(const QList<CompletionItem>& items);
  void dismissCompletion();
  void addNewLineActionProvider(NewLineActionProvider* provider);
  void removeNewLineActionProvider(NewLineActionProvider* provider);

  void undo();
  void redo();
  bool canUndo() const;
  bool canRedo() const;

  void setReadOnly(bool read_only);
  bool isReadOnly() const;
  void setWrapMode(::sweeteditor::WrapMode mode);
  void setTabSize(uint32_t tab_size);
  void setScale(float scale);
  float scale() const;

  void setCursorPosition(const ::sweeteditor::TextPosition& position);
  ::sweeteditor::TextPosition cursorPosition() const;
  void setSelection(const ::sweeteditor::TextRange& range);
  ::sweeteditor::TextRange selection() const;
  bool hasSelection() const;
  void clearSelection();
  void selectAll();
  void scrollToLine(size_t line,
                    ::sweeteditor::ScrollBehavior behavior = ::sweeteditor::ScrollBehavior::GOTO_CENTER);
  void gotoPosition(size_t line, size_t column);

  ::sweeteditor::EditorCore& editorCore() noexcept;
  const ::sweeteditor::EditorCore& editorCore() const noexcept;
  const ::sweeteditor::EditorRenderModel& renderModel() const noexcept;

Q_SIGNALS:
  void documentLoaded();
  void textChanged();
  void cursorChanged(int line, int column);
  void selectionChanged(int start_line, int start_column, int end_line, int end_column, bool has_selection);
  void scrollChanged(float scroll_x, float scroll_y);
  void scaleChanged(float scale);
  void longPressed(int line, int column, const QPoint& global_pos);
  void doubleTapped(int line, int column, bool has_selection, const QPoint& global_pos);
  void inlayHintClicked(int line, int column, int inlay_type, int value, const QPoint& global_pos);
  void gutterIconClicked(int line, int icon_id);
  void foldToggled(int line, bool from_gutter);
  void editorContextMenuRequested(const QPoint& global_pos);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void changeEvent(QEvent* event) override;
  void focusInEvent(QFocusEvent* event) override;
  void focusOutEvent(QFocusEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void inputMethodEvent(QInputMethodEvent* event) override;
  QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

private:
  friend class EditorSettings;
  friend class DecorationProviderManager;
  friend class CompletionProviderManager;

  void requestDecorationRefresh();
  void syncViewport();
  void ensureRenderModelUpToDate();
  void invalidateRenderModel() noexcept;
  void syncPlatformScale(float scale);
  std::pair<int, int> decorationVisibleLineRange();
  int decorationTotalLineCount() const;
  const ::sweeteditor::Document* decorationDocument() const noexcept;
  const ::sweeteditor::Document* completionDocument() const noexcept;
  QString completionLineText(const ::sweeteditor::TextPosition& position) const;
  std::optional<::sweeteditor::TextRange> completionWordRange() const;
  void dispatchTextEditResult(const ::sweeteditor::TextEditResult& result);
  void dispatchKeyEventResult(const ::sweeteditor::KeyEventResult& result);
  void dispatchGestureResult(const ::sweeteditor::GestureResult& result, const QPoint& global_pos);
  void emitCursorSignal(const ::sweeteditor::TextPosition& cursor);
  void emitSelectionSignal(bool has_selection, const ::sweeteditor::TextRange* selection);
  void syncCompletionPopup(bool text_changed, bool cursor_changed);
  void syncInputMethodState();
  bool handleNewLineAction();
  bool handleClipboardCommand(::sweeteditor::EditorCommand command);
  void handleGestureResult(const ::sweeteditor::GestureResult& result, const QPoint& global_pos);
  void triggerCompletionInternal(CompletionTriggerKind kind, const QString& trigger_character = {});
  void showCompletionPopup(const QList<CompletionItem>& items);
  void hideCompletionPopup();
  void applyCompletionItem(const CompletionItem& item);
  void repositionCompletionPopup();

  struct Private;
  std::unique_ptr<Private> d_;
};

} // namespace sweeteditor::qt
