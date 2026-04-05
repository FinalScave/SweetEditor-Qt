#pragma once

#include <QAbstractScrollArea>
#include <QString>

#include <memory>

#include <EditorTheme.h>

namespace sweeteditor::qt {

class SweetEditorWidget final : public QAbstractScrollArea {
public:
  explicit SweetEditorWidget(QWidget* parent = nullptr);
  ~SweetEditorWidget() override;

  void applyTheme(const EditorTheme& theme);
  const EditorTheme& theme() const noexcept;

  void setPlaceholderText(QString text);
  const QString& placeholderText() const noexcept;

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  struct Private;
  std::unique_ptr<Private> d_;
};

} // namespace sweeteditor::qt
