#include "CompletionPopupController.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPalette>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace sweeteditor::qt {

namespace {

constexpr int kItemHeight = 28;
constexpr int kMaxVisibleItems = 6;
constexpr int kPopupMinWidth = 220;
constexpr int kPopupGap = 4;
constexpr int kBadgeSize = 18;
constexpr int kHorizontalPadding = 8;
constexpr int kBadgeGap = 8;
constexpr int kDetailGap = 12;
constexpr int kCornerRadius = 4;
constexpr int kPopupMargin = 4;

QColor kindColor(int kind) {
  switch (kind) {
    case CompletionItem::KIND_KEYWORD: return QColor(0xC6, 0x78, 0xDD);
    case CompletionItem::KIND_FUNCTION: return QColor(0x61, 0xAF, 0xEF);
    case CompletionItem::KIND_VARIABLE: return QColor(0xE5, 0xC0, 0x7B);
    case CompletionItem::KIND_CLASS: return QColor(0xE0, 0x6C, 0x75);
    case CompletionItem::KIND_INTERFACE: return QColor(0x56, 0xB6, 0xC2);
    case CompletionItem::KIND_MODULE: return QColor(0xD1, 0x9A, 0x66);
    case CompletionItem::KIND_PROPERTY: return QColor(0x98, 0xC3, 0x79);
    case CompletionItem::KIND_SNIPPET: return QColor(0xBE, 0x50, 0x46);
    default: return QColor(0x7A, 0x84, 0x94);
  }
}

QString kindLetter(int kind) {
  switch (kind) {
    case CompletionItem::KIND_KEYWORD: return QStringLiteral("K");
    case CompletionItem::KIND_FUNCTION: return QStringLiteral("F");
    case CompletionItem::KIND_VARIABLE: return QStringLiteral("V");
    case CompletionItem::KIND_CLASS: return QStringLiteral("C");
    case CompletionItem::KIND_INTERFACE: return QStringLiteral("I");
    case CompletionItem::KIND_MODULE: return QStringLiteral("M");
    case CompletionItem::KIND_PROPERTY: return QStringLiteral("P");
    case CompletionItem::KIND_SNIPPET: return QStringLiteral("S");
    default: return QStringLiteral("T");
  }
}

class CompletionPopupItemDelegate final : public QStyledItemDelegate {
public:
  explicit CompletionPopupItemDelegate(QObject* parent = nullptr)
    : QStyledItemDelegate(parent) {}

  void applyTheme(const EditorTheme& theme) {
    theme_ = theme;
  }

  QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    return {base.width(), kItemHeight};
  }

  void paint(QPainter* painter,
             const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    if (painter == nullptr) {
      return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(option.rect, theme_.completion_background);

    const bool is_selected = (option.state & QStyle::State_Selected) != 0;
    QRect content_rect = option.rect.adjusted(3, 1, -3, -1);
    if (is_selected) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(theme_.completion_selected_background);
      painter->drawRoundedRect(content_rect, kCornerRadius, kCornerRadius);
    }

    int x = option.rect.left() + kHorizontalPadding;
    const int center_y = option.rect.center().y();
    const int badge_y = center_y - kBadgeSize / 2;
    QRect badge_rect(x, badge_y, kBadgeSize, kBadgeSize);
    painter->setPen(Qt::NoPen);
    painter->setBrush(kindColor(index.data(Qt::UserRole + 2).toInt()));
    painter->drawRoundedRect(badge_rect, 6, 6);

    QFont badge_font = option.font;
    badge_font.setBold(true);
    badge_font.setPointSizeF(std::max(8.0, option.font.pointSizeF() - 1.0));
    painter->setFont(badge_font);
    painter->setPen(Qt::white);
    painter->drawText(badge_rect, Qt::AlignCenter, kindLetter(index.data(Qt::UserRole + 2).toInt()));
    x += kBadgeSize + kBadgeGap;

    const QString label = index.data(Qt::DisplayRole).toString();
    const QString detail = index.data(Qt::UserRole + 1).toString();

    QFont label_font = option.font;
    painter->setFont(label_font);
    const QFontMetrics label_metrics(label_font);

    QFont detail_font = option.font;
    detail_font.setPointSizeF(std::max(8.0, option.font.pointSizeF() - 1.0));
    painter->setFont(detail_font);
    const QFontMetrics detail_metrics(detail_font);

    int detail_width = detail.isEmpty() ? 0 : detail_metrics.horizontalAdvance(detail);
    if (detail_width > 0) {
      detail_width += kDetailGap;
    }

    QRect label_rect(
      x,
      option.rect.top(),
      std::max(0, option.rect.width() - (x - option.rect.left()) - detail_width - kHorizontalPadding),
      option.rect.height()
    );
    painter->setFont(label_font);
    painter->setPen(theme_.completion_label);
    painter->drawText(
      label_rect,
      Qt::AlignVCenter | Qt::AlignLeft,
      label_metrics.elidedText(label, Qt::ElideRight, label_rect.width())
    );

    if (!detail.isEmpty()) {
      QRect detail_rect(
        label_rect.right() + 1,
        option.rect.top(),
        option.rect.right() - label_rect.right() - kHorizontalPadding,
        option.rect.height()
      );
      painter->setFont(detail_font);
      painter->setPen(theme_.completion_detail);
      painter->drawText(
        detail_rect,
        Qt::AlignVCenter | Qt::AlignRight,
        detail_metrics.elidedText(detail, Qt::ElideRight, detail_rect.width())
      );
    }

    painter->restore();
  }

private:
  EditorTheme theme_;
};

} // namespace

CompletionPopupController::CompletionPopupController(QWidget* anchor, const EditorTheme& theme)
  : anchor_(anchor),
    theme_(theme) {
  popup_ = new QFrame(anchor_);
  popup_->hide();
  popup_->setFrameShape(QFrame::NoFrame);
  popup_->setAttribute(Qt::WA_ShowWithoutActivating);

  list_ = new QListWidget(popup_);
  list_->setFocusPolicy(Qt::NoFocus);
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
  list_->setFrameShape(QFrame::NoFrame);
  list_->setMouseTracking(true);

  auto* delegate = new CompletionPopupItemDelegate(list_);
  delegate->applyTheme(theme_);
  list_->setItemDelegate(delegate);

  QObject::connect(list_, &QListWidget::itemClicked, popup_, [this](QListWidgetItem*) {
    confirmSelected();
  });

  refreshPopupStyle();
}

CompletionPopupController::~CompletionPopupController() {
  delete popup_;
  popup_ = nullptr;
  list_ = nullptr;
}

void CompletionPopupController::applyTheme(const EditorTheme& theme) {
  theme_ = theme;
  if (auto* delegate = static_cast<CompletionPopupItemDelegate*>(list_->itemDelegate())) {
    delegate->applyTheme(theme_);
  }
  refreshPopupStyle();
  if (list_ != nullptr) {
    list_->viewport()->update();
  }
}

void CompletionPopupController::setConfirmedHandler(std::function<void(const CompletionItem&)> handler) {
  confirmed_handler_ = std::move(handler);
}

bool CompletionPopupController::isShowing() const {
  return popup_ != nullptr && popup_->isVisible();
}

bool CompletionPopupController::contains(const QPoint& point) const {
  return popup_ != nullptr && popup_->isVisible() && popup_->geometry().contains(point);
}

bool CompletionPopupController::handleKey(int key) {
  if (!isShowing() || items_.isEmpty() || list_ == nullptr) {
    return false;
  }

  switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
      confirmSelected();
      return true;
    case Qt::Key_Escape:
      dismiss();
      return true;
    case Qt::Key_Up:
      moveSelection(-1);
      return true;
    case Qt::Key_Down:
      moveSelection(1);
      return true;
    case Qt::Key_PageUp:
      moveSelection(-kMaxVisibleItems);
      return true;
    case Qt::Key_PageDown:
      moveSelection(kMaxVisibleItems);
      return true;
    default:
      return false;
  }
}

void CompletionPopupController::updateItems(const QList<CompletionItem>& items) {
  items_ = items;
  list_->clear();
  for (const CompletionItem& item : items_) {
    auto* list_item = new QListWidgetItem(item.label, list_);
    list_item->setData(Qt::UserRole + 1, item.detail);
    list_item->setData(Qt::UserRole + 2, item.kind);
    if (!item.detail.isEmpty()) {
      list_item->setToolTip(item.detail);
    }
  }

  if (items_.isEmpty()) {
    dismiss();
    return;
  }

  list_->setCurrentRow(0);
  show();
}

void CompletionPopupController::dismiss() {
  items_.clear();
  if (list_ != nullptr) {
    list_->clear();
  }
  if (popup_ != nullptr) {
    popup_->hide();
  }
}

void CompletionPopupController::updateCursorPosition(float cursor_x,
                                                     float cursor_y,
                                                     float cursor_height) {
  cached_cursor_x_ = cursor_x;
  cached_cursor_y_ = cursor_y;
  cached_cursor_height_ = cursor_height;
  if (isShowing()) {
    applyPosition();
  }
}

void CompletionPopupController::refreshPopupStyle() {
  if (popup_ == nullptr || list_ == nullptr) {
    return;
  }

  const QString popup_style = QStringLiteral(
    "QFrame {"
    "background-color: rgba(%1,%2,%3,%4);"
    "border: 1px solid rgba(%5,%6,%7,%8);"
    "border-radius: 6px;"
    "}"
  ).arg(theme_.completion_background.red())
   .arg(theme_.completion_background.green())
   .arg(theme_.completion_background.blue())
   .arg(theme_.completion_background.alpha())
   .arg(theme_.completion_border.red())
   .arg(theme_.completion_border.green())
   .arg(theme_.completion_border.blue())
   .arg(theme_.completion_border.alpha());
  popup_->setStyleSheet(popup_style);

  QPalette palette = list_->palette();
  palette.setColor(QPalette::Base, theme_.completion_background);
  palette.setColor(QPalette::Text, theme_.completion_label);
  list_->setPalette(palette);
  list_->setStyleSheet(QStringLiteral("QListWidget { background: transparent; border: none; }"));
}

void CompletionPopupController::show() {
  if (popup_ == nullptr || list_ == nullptr) {
    return;
  }

  QFont label_font = list_->font();
  QFont detail_font = list_->font();
  detail_font.setPointSizeF(std::max(8.0, detail_font.pointSizeF() - 1.0));
  const QFontMetrics label_metrics(label_font);
  const QFontMetrics detail_metrics(detail_font);

  int width = kPopupMinWidth;
  for (const CompletionItem& item : items_) {
    width = std::max(
      width,
      kHorizontalPadding * 2
        + kBadgeSize
        + kBadgeGap
        + label_metrics.horizontalAdvance(item.label)
        + (item.detail.isEmpty() ? 0 : detail_metrics.horizontalAdvance(item.detail) + kDetailGap)
    );
  }

  width = std::min(width, std::max(kPopupMinWidth, anchor_->width() - kPopupMargin * 2));
  const int visible_rows = std::min(kMaxVisibleItems, static_cast<int>(items_.size()));
  const int height = visible_rows * kItemHeight + kPopupMargin * 2;

  popup_->setGeometry(0, 0, width, height);
  list_->setGeometry(kPopupMargin, kPopupMargin, width - kPopupMargin * 2, height - kPopupMargin * 2);
  applyPosition();
  popup_->show();
  popup_->raise();
}

void CompletionPopupController::applyPosition() {
  if (popup_ == nullptr || anchor_ == nullptr) {
    return;
  }

  int x = static_cast<int>(std::lround(cached_cursor_x_));
  int y = static_cast<int>(std::lround(cached_cursor_y_ + cached_cursor_height_ + kPopupGap));

  if (x + popup_->width() > anchor_->width() - kPopupMargin) {
    x = std::max(kPopupMargin, anchor_->width() - popup_->width() - kPopupMargin);
  }
  if (y + popup_->height() > anchor_->height() - kPopupMargin) {
    y = std::max(kPopupMargin, static_cast<int>(std::lround(cached_cursor_y_ - popup_->height() - kPopupGap)));
  }

  popup_->move(std::max(kPopupMargin, x), std::max(kPopupMargin, y));
}

void CompletionPopupController::moveSelection(int delta) {
  if (items_.isEmpty() || list_ == nullptr) {
    return;
  }

  const int current = std::max(0, list_->currentRow());
  const int next = std::max(0, std::min(static_cast<int>(items_.size()) - 1, current + delta));
  list_->setCurrentRow(next);
}

void CompletionPopupController::confirmSelected() {
  if (items_.isEmpty() || list_ == nullptr) {
    return;
  }

  int row = list_->currentRow();
  if (row < 0) {
    row = 0;
  }
  if (row < 0 || row >= items_.size()) {
    return;
  }

  const CompletionItem item = items_.at(row);
  dismiss();
  if (confirmed_handler_) {
    confirmed_handler_(item);
  }
}

} // namespace sweeteditor::qt
