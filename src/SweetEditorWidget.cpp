#include <SweetEditorWidget.h>

#include "EditorRenderer.h"
#include "QtTextMeasurer.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

#include <utility>

namespace sweeteditor::qt {

struct SweetEditorWidget::Private {
  EditorTheme theme = EditorTheme::light();
  QString placeholder_text = QStringLiteral("Qt adapter bootstrap: rendering, input bridge, and core wiring will land here.");
  QtTextMeasurer text_measurer;
  EditorRenderer renderer;
};

SweetEditorWidget::SweetEditorWidget(QWidget* parent)
  : QAbstractScrollArea(parent),
    d_(std::make_unique<Private>()) {
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  viewport()->setAutoFillBackground(false);
  d_->text_measurer.setBaseFont(font());
}

SweetEditorWidget::~SweetEditorWidget() = default;

void SweetEditorWidget::applyTheme(const EditorTheme& theme) {
  d_->theme = theme;
  viewport()->update();
}

const EditorTheme& SweetEditorWidget::theme() const noexcept {
  return d_->theme;
}

void SweetEditorWidget::setPlaceholderText(QString text) {
  d_->placeholder_text = std::move(text);
  viewport()->update();
}

const QString& SweetEditorWidget::placeholderText() const noexcept {
  return d_->placeholder_text;
}

void SweetEditorWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  d_->text_measurer.setBaseFont(font());

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setFont(font());

  d_->renderer.paint(
    painter,
    viewport()->rect(),
    d_->theme,
    d_->placeholder_text,
    d_->text_measurer.getFontMetrics()
  );
}

void SweetEditorWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  viewport()->update();
}

} // namespace sweeteditor::qt
