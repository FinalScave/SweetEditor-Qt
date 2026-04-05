#pragma once

#include <CompletionProvider.h>
#include <EditorTheme.h>

#include <QPoint>

#include <functional>

class QFrame;
class QListWidget;
class QWidget;

namespace sweeteditor::qt {

class CompletionPopupController {
public:
  CompletionPopupController(QWidget* anchor, const EditorTheme& theme);
  ~CompletionPopupController();

  void applyTheme(const EditorTheme& theme);
  void setConfirmedHandler(std::function<void(const CompletionItem&)> handler);

  bool isShowing() const;
  bool contains(const QPoint& point) const;
  bool handleKey(int key);

  void updateItems(const QList<CompletionItem>& items);
  void dismiss();
  void updateCursorPosition(float cursor_x, float cursor_y, float cursor_height);

private:
  void refreshPopupStyle();
  void show();
  void applyPosition();
  void moveSelection(int delta);
  void confirmSelected();

  QWidget* anchor_ {nullptr};
  EditorTheme theme_;
  QFrame* popup_ {nullptr};
  QListWidget* list_ {nullptr};
  QList<CompletionItem> items_;
  std::function<void(const CompletionItem&)> confirmed_handler_;
  float cached_cursor_x_ {0.0f};
  float cached_cursor_y_ {0.0f};
  float cached_cursor_height_ {0.0f};
};

} // namespace sweeteditor::qt
