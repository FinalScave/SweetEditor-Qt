#include <SweetEditorWidget.h>

#include "EditorRenderer.h"
#include "QtTextMeasurer.h"
#include "completion/CompletionPopupController.h"
#include "completion/CompletionProviderManager.h"
#include "decoration/DecorationProviderManager.h"
#include "newline/NewLineActionProviderManager.h"

#include <Perf.h>

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QDebug>
#include <QResizeEvent>
#include <QStyleHints>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace {

QString toQString(const ::sweeteditor::U8String& text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString toQString(const ::sweeteditor::U16String& text) {
#ifdef _WIN32
  return QString::fromWCharArray(text.data(), static_cast<qsizetype>(text.size()));
#else
  return QString::fromUtf16(reinterpret_cast<const char16_t*>(text.data()),
                            static_cast<qsizetype>(text.size()));
#endif
}

::sweeteditor::U8String toUtf8(const QString& text) {
  const QByteArray bytes = text.toUtf8();
  return {bytes.constData(), static_cast<size_t>(bytes.size())};
}

::sweeteditor::KeyModifier toKeyModifiers(Qt::KeyboardModifiers modifiers) {
  ::sweeteditor::KeyModifier result = ::sweeteditor::KeyModifier::NONE;
  if (modifiers.testFlag(Qt::ShiftModifier)) {
    result = result | ::sweeteditor::KeyModifier::SHIFT;
  }
  if (modifiers.testFlag(Qt::ControlModifier)) {
    result = result | ::sweeteditor::KeyModifier::CTRL;
  }
  if (modifiers.testFlag(Qt::AltModifier)) {
    result = result | ::sweeteditor::KeyModifier::ALT;
  }
  if (modifiers.testFlag(Qt::MetaModifier)) {
    result = result | ::sweeteditor::KeyModifier::META;
  }
  return result;
}

::sweeteditor::KeyCode toKeyCode(int key) {
  switch (key) {
    case Qt::Key_Backspace: return ::sweeteditor::KeyCode::BACKSPACE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: return ::sweeteditor::KeyCode::TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter: return ::sweeteditor::KeyCode::ENTER;
    case Qt::Key_Escape: return ::sweeteditor::KeyCode::ESCAPE;
    case Qt::Key_Delete: return ::sweeteditor::KeyCode::DELETE_KEY;
    case Qt::Key_Left: return ::sweeteditor::KeyCode::LEFT;
    case Qt::Key_Up: return ::sweeteditor::KeyCode::UP;
    case Qt::Key_Right: return ::sweeteditor::KeyCode::RIGHT;
    case Qt::Key_Down: return ::sweeteditor::KeyCode::DOWN;
    case Qt::Key_Home: return ::sweeteditor::KeyCode::HOME;
    case Qt::Key_End: return ::sweeteditor::KeyCode::END;
    case Qt::Key_PageUp: return ::sweeteditor::KeyCode::PAGE_UP;
    case Qt::Key_PageDown: return ::sweeteditor::KeyCode::PAGE_DOWN;
    case Qt::Key_A: return ::sweeteditor::KeyCode::A;
    case Qt::Key_C: return ::sweeteditor::KeyCode::C;
    case Qt::Key_D: return ::sweeteditor::KeyCode::D;
    case Qt::Key_K: return ::sweeteditor::KeyCode::K;
    case Qt::Key_Space: return ::sweeteditor::KeyCode::SPACE;
    case Qt::Key_V: return ::sweeteditor::KeyCode::V;
    case Qt::Key_X: return ::sweeteditor::KeyCode::X;
    case Qt::Key_Y: return ::sweeteditor::KeyCode::Y;
    case Qt::Key_Z: return ::sweeteditor::KeyCode::Z;
    default: return ::sweeteditor::KeyCode::NONE;
  }
}

::sweeteditor::GestureEvent makeMouseGesture(::sweeteditor::EventType type,
                                             const QPointF& point,
                                             Qt::KeyboardModifiers modifiers) {
  ::sweeteditor::GestureEvent event;
  event.type = type;
  event.points.push_back({
    static_cast<float>(point.x()),
    static_cast<float>(point.y()),
  });
  event.modifiers = toKeyModifiers(modifiers);
  return event;
}

bool sameFloat(float lhs, float rhs) {
  return std::fabs(lhs - rhs) < 0.01f;
}

bool hasControlLikeModifier(Qt::KeyboardModifiers modifiers) {
  return modifiers.testFlag(Qt::ControlModifier)
    || modifiers.testFlag(Qt::AltModifier)
    || modifiers.testFlag(Qt::MetaModifier);
}

bool isPlainTextInput(const QKeyEvent* event) {
  if (event == nullptr || hasControlLikeModifier(event->modifiers()) || event->text().isEmpty()) {
    return false;
  }

  return std::all_of(event->text().cbegin(), event->text().cend(), [](QChar ch) {
    return ch == QChar(u' ') || ch.isPrint();
  });
}

bool isCompletionWordInput(const QString& text) {
  return text.size() == 1 && (text.front().isLetterOrNumber() || text.front() == QChar(u'_'));
}

QFont scaledFont(const QFont& base_font, float scale) {
  const float clamped_scale = scale > 0.0f ? scale : 1.0f;
  QFont font(base_font);
  if (font.pointSizeF() > 0.0) {
    font.setPointSizeF(std::max(1.0, font.pointSizeF() * clamped_scale));
  } else if (font.pixelSize() > 0) {
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * clamped_scale))));
  }
  return font;
}

::sweeteditor::EditorOptions makeEditorOptions() {
  ::sweeteditor::EditorOptions options;
  if (qApp != nullptr) {
    if (const QStyleHints* style_hints = qApp->styleHints()) {
      options.touch_slop = static_cast<float>(std::max({
        20,
        style_hints->mouseDoubleClickDistance(),
        style_hints->startDragDistance()
      }));
      options.double_tap_timeout = style_hints->mouseDoubleClickInterval();
    }
  }
  return options;
}

::sweeteditor::ScrollbarConfig makeDesktopScrollbarConfig() {
  ::sweeteditor::ScrollbarConfig config;
  config.thickness = 10.0f;
  config.min_thumb = 24.0f;
  config.thumb_hit_padding = 0.0f;
  config.mode = ::sweeteditor::ScrollbarMode::ALWAYS;
  config.thumb_draggable = true;
  config.track_tap_mode = ::sweeteditor::ScrollbarTrackTapMode::JUMP;
  config.fade_delay_ms = 700;
  config.fade_duration_ms = 300;
  return config;
}

class InputPerfScope {
public:
  InputPerfScope(sweeteditor::qt::EditorRenderer& renderer, QString tag)
    : renderer_(renderer),
      tag_(std::move(tag)),
      enabled_(renderer_.isPerfOverlayEnabled()),
      start_(enabled_ ? Clock::now() : Clock::time_point {}) {}

  ~InputPerfScope() {
    if (!enabled_) {
      return;
    }
    const double elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    renderer_.perfOverlay().recordInput(tag_, elapsed_ms);
  }

private:
  using Clock = std::chrono::steady_clock;

  sweeteditor::qt::EditorRenderer& renderer_;
  QString tag_;
  bool enabled_ {false};
  Clock::time_point start_;
};

} // namespace

namespace sweeteditor::qt {

struct SweetEditorWidget::Private {
  explicit Private(SweetEditorWidget* owner)
    : settings(owner),
      decoration_manager(owner),
      completion_manager(owner) {}

  EditorTheme theme = EditorTheme::light();
  EditorSettings settings;
  LanguageConfiguration language_configuration;
  EditorMetadata metadata;
  const EditorIconProvider* icon_provider {nullptr};
  DecorationProviderManager decoration_manager;
  CompletionProviderManager completion_manager;
  NewLineActionProviderManager newline_manager;
  std::shared_ptr<QtTextMeasurer> text_measurer {std::make_shared<QtTextMeasurer>()};
  EditorRenderer renderer;
  std::shared_ptr<::sweeteditor::Document> document {
    std::make_shared<::sweeteditor::LineArrayDocument>(::sweeteditor::U8String {})
  };
  std::unique_ptr<::sweeteditor::EditorCore> core {
    std::make_unique<::sweeteditor::EditorCore>(text_measurer, makeEditorOptions())
  };
  ::sweeteditor::EditorRenderModel render_model;
  bool render_model_dirty {true};
  QTimer animation_timer;
  bool left_button_down {false};
  QPointF last_pointer_position;
  bool has_last_pointer_position {false};
  bool syncing_platform_font {false};
  bool font_metrics_dirty {true};
  std::unique_ptr<CompletionPopupController> completion_popup;
};

EditorSettings::EditorSettings(SweetEditorWidget* editor) noexcept
  : editor_(editor) {}

void EditorSettings::bind(SweetEditorWidget* editor) noexcept {
  editor_ = editor;
}

void EditorSettings::syncFontFromWidget(const QFont& font) noexcept {
  font_ = font;
}

void EditorSettings::setFont(const QFont& font) {
  if (font_ == font) {
    return;
  }

  font_ = font;
  if (editor_ != nullptr) {
    editor_->syncPlatformScale(scale_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

const QFont& EditorSettings::font() const noexcept {
  return font_;
}

void EditorSettings::setTabSize(uint32_t tab_size) {
  tab_size = std::max<uint32_t>(1, tab_size);
  if (tab_size_ == tab_size) {
    return;
  }

  tab_size_ = tab_size;
  if (editor_ != nullptr) {
    editor_->d_->core->setTabSize(tab_size_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

uint32_t EditorSettings::tabSize() const noexcept {
  return tab_size_;
}

void EditorSettings::setWrapMode(::sweeteditor::WrapMode mode) {
  if (wrap_mode_ == mode) {
    return;
  }

  wrap_mode_ = mode;
  if (editor_ != nullptr) {
    editor_->d_->core->setWrapMode(wrap_mode_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

::sweeteditor::WrapMode EditorSettings::wrapMode() const noexcept {
  return wrap_mode_;
}

void EditorSettings::setReadOnly(bool read_only) {
  if (read_only_ == read_only) {
    return;
  }

  read_only_ = read_only;
  if (editor_ != nullptr) {
    editor_->d_->core->setReadOnly(read_only_);
    editor_->syncInputMethodState();
  }
}

bool EditorSettings::readOnly() const noexcept {
  return read_only_;
}

void EditorSettings::setCompositionEnabled(bool enabled) {
  if (composition_enabled_ == enabled) {
    return;
  }

  composition_enabled_ = enabled;
  if (editor_ != nullptr) {
    editor_->d_->core->setCompositionEnabled(composition_enabled_);
    if (!composition_enabled_ && editor_->d_->core->isComposing()) {
      editor_->d_->core->compositionCancel();
    }
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

bool EditorSettings::compositionEnabled() const noexcept {
  return composition_enabled_;
}

void EditorSettings::setScale(float scale) {
  if (sameFloat(scale_, scale)) {
    return;
  }

  scale_ = scale;
  if (editor_ != nullptr) {
    editor_->d_->core->setScale(scale_);
    editor_->syncPlatformScale(scale_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

float EditorSettings::scale() const noexcept {
  return scale_;
}

void EditorSettings::setLineSpacing(float add, float mult) {
  if (sameFloat(line_spacing_add_, add) && sameFloat(line_spacing_mult_, mult)) {
    return;
  }

  line_spacing_add_ = add;
  line_spacing_mult_ = mult;
  if (editor_ != nullptr) {
    editor_->d_->core->setLineSpacing(line_spacing_add_, line_spacing_mult_);
    editor_->d_->font_metrics_dirty = true;
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

float EditorSettings::lineSpacingAdd() const noexcept {
  return line_spacing_add_;
}

float EditorSettings::lineSpacingMult() const noexcept {
  return line_spacing_mult_;
}

void EditorSettings::setContentStartPadding(float padding) {
  padding = std::max(0.0f, padding);
  if (sameFloat(content_start_padding_, padding)) {
    return;
  }

  content_start_padding_ = padding;
  if (editor_ != nullptr) {
    editor_->d_->core->setContentStartPadding(content_start_padding_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

float EditorSettings::contentStartPadding() const noexcept {
  return content_start_padding_;
}

void EditorSettings::setShowSplitLine(bool show) {
  if (show_split_line_ == show) {
    return;
  }

  show_split_line_ = show;
  if (editor_ != nullptr) {
    editor_->d_->core->setShowSplitLine(show_split_line_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

bool EditorSettings::showSplitLine() const noexcept {
  return show_split_line_;
}

void EditorSettings::setCurrentLineRenderMode(::sweeteditor::CurrentLineRenderMode mode) {
  if (current_line_render_mode_ == mode) {
    return;
  }

  current_line_render_mode_ = mode;
  if (editor_ != nullptr) {
    editor_->d_->core->setCurrentLineRenderMode(current_line_render_mode_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

::sweeteditor::CurrentLineRenderMode EditorSettings::currentLineRenderMode() const noexcept {
  return current_line_render_mode_;
}

void EditorSettings::setGutterSticky(bool sticky) {
  if (gutter_sticky_ == sticky) {
    return;
  }

  gutter_sticky_ = sticky;
  if (editor_ != nullptr) {
    editor_->d_->core->setGutterSticky(gutter_sticky_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

bool EditorSettings::gutterSticky() const noexcept {
  return gutter_sticky_;
}

void EditorSettings::setGutterVisible(bool visible) {
  if (gutter_visible_ == visible) {
    return;
  }

  gutter_visible_ = visible;
  if (editor_ != nullptr) {
    editor_->d_->core->setGutterVisible(gutter_visible_);
    editor_->flush();
    editor_->syncInputMethodState();
  }
}

bool EditorSettings::gutterVisible() const noexcept {
  return gutter_visible_;
}

void EditorSettings::setSelectionHandlesEnabled(bool enabled) {
  if (selection_handles_enabled_ == enabled) {
    return;
  }

  selection_handles_enabled_ = enabled;
  if (editor_ != nullptr) {
    editor_->viewport()->update();
  }
}

bool EditorSettings::selectionHandlesEnabled() const noexcept {
  return selection_handles_enabled_;
}

void EditorSettings::setFoldArrowMode(::sweeteditor::FoldArrowMode mode) {
  if (fold_arrow_mode_ == mode) {
    return;
  }

  fold_arrow_mode_ = mode;
  if (editor_ != nullptr) {
    editor_->d_->core->setFoldArrowMode(fold_arrow_mode_);
  }
}

::sweeteditor::FoldArrowMode EditorSettings::foldArrowMode() const noexcept {
  return fold_arrow_mode_;
}

void EditorSettings::setAutoIndentMode(::sweeteditor::AutoIndentMode mode) {
  if (auto_indent_mode_ == mode) {
    return;
  }

  auto_indent_mode_ = mode;
  if (editor_ != nullptr) {
    editor_->d_->core->setAutoIndentMode(auto_indent_mode_);
  }
}

::sweeteditor::AutoIndentMode EditorSettings::autoIndentMode() const noexcept {
  return auto_indent_mode_;
}

void EditorSettings::setBackspaceUnindent(bool enabled) {
  if (backspace_unindent_ == enabled) {
    return;
  }

  backspace_unindent_ = enabled;
  if (editor_ != nullptr) {
    editor_->d_->core->setBackspaceUnindent(backspace_unindent_);
  }
}

bool EditorSettings::backspaceUnindent() const noexcept {
  return backspace_unindent_;
}

void EditorSettings::setInsertSpaces(bool enabled) {
  if (insert_spaces_ == enabled) {
    return;
  }

  insert_spaces_ = enabled;
  if (editor_ != nullptr) {
    editor_->d_->core->setInsertSpaces(insert_spaces_);
  }
}

bool EditorSettings::insertSpaces() const noexcept {
  return insert_spaces_;
}

void EditorSettings::setMaxGutterIcons(uint32_t count) {
  if (max_gutter_icons_ == count) {
    return;
  }

  max_gutter_icons_ = count;
  if (editor_ != nullptr) {
    editor_->d_->core->setMaxGutterIcons(max_gutter_icons_);
  }
}

uint32_t EditorSettings::maxGutterIcons() const noexcept {
  return max_gutter_icons_;
}

void EditorSettings::setDecorationScrollRefreshMinIntervalMs(int interval_ms) {
  interval_ms = std::max(0, interval_ms);
  if (decoration_scroll_refresh_min_interval_ms_ == interval_ms) {
    return;
  }

  decoration_scroll_refresh_min_interval_ms_ = interval_ms;
  if (editor_ != nullptr) {
    editor_->requestDecorationRefresh();
  }
}

int EditorSettings::decorationScrollRefreshMinIntervalMs() const noexcept {
  return decoration_scroll_refresh_min_interval_ms_;
}

void EditorSettings::setDecorationOverscanViewportMultiplier(float multiplier) {
  multiplier = std::max(0.0f, multiplier);
  if (sameFloat(decoration_overscan_viewport_multiplier_, multiplier)) {
    return;
  }

  decoration_overscan_viewport_multiplier_ = multiplier;
  if (editor_ != nullptr) {
    editor_->requestDecorationRefresh();
  }
}

float EditorSettings::decorationOverscanViewportMultiplier() const noexcept {
  return decoration_overscan_viewport_multiplier_;
}

SweetEditorWidget::SweetEditorWidget(QWidget* parent)
  : QAbstractScrollArea(parent),
    d_(std::make_unique<Private>(this)) {
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAttribute(Qt::WA_InputMethodEnabled);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  viewport()->setAutoFillBackground(false);
  viewport()->setMouseTracking(true);
  viewport()->setAttribute(Qt::WA_InputMethodEnabled);
  viewport()->setCursor(Qt::IBeamCursor);

  QFont monospace_font(QStringLiteral("Consolas"), 11);
  monospace_font.setStyleHint(QFont::Monospace, QFont::PreferMatch);
  setFont(monospace_font);

  d_->settings.syncFontFromWidget(font());
  d_->settings.setContentStartPadding(3);
  d_->text_measurer->setBaseFont(font());
  d_->core->loadDocument(d_->document);
  d_->core->setScrollbarConfig(makeDesktopScrollbarConfig());
  d_->core->setTabSize(d_->settings.tabSize());
  d_->core->setWrapMode(d_->settings.wrapMode());
  d_->core->setReadOnly(d_->settings.readOnly());
  d_->core->setCompositionEnabled(d_->settings.compositionEnabled());
  d_->core->setScale(d_->settings.scale());
  d_->core->setLineSpacing(d_->settings.lineSpacingAdd(), d_->settings.lineSpacingMult());
  d_->core->setContentStartPadding(d_->settings.contentStartPadding());
  d_->core->setShowSplitLine(d_->settings.showSplitLine());
  d_->core->setCurrentLineRenderMode(d_->settings.currentLineRenderMode());
  d_->core->setGutterSticky(d_->settings.gutterSticky());
  d_->core->setGutterVisible(d_->settings.gutterVisible());
  d_->core->setFoldArrowMode(d_->settings.foldArrowMode());
  d_->core->setAutoIndentMode(d_->settings.autoIndentMode());
  d_->core->setBackspaceUnindent(d_->settings.backspaceUnindent());
  d_->core->setInsertSpaces(d_->settings.insertSpaces());
  d_->core->setMaxGutterIcons(d_->settings.maxGutterIcons());

  d_->completion_popup = std::make_unique<CompletionPopupController>(viewport(), d_->theme);
  d_->completion_popup->setConfirmedHandler([this](const CompletionItem& item) {
    applyCompletionItem(item);
  });
  d_->completion_manager.setItemsUpdatedHandler([this](const QList<CompletionItem>& items) {
    ensureRenderModelUpToDate();
    showCompletionPopup(items);
  });
  d_->completion_manager.setDismissedHandler([this]() {
    hideCompletionPopup();
  });

  syncViewport();
  ensureRenderModelUpToDate();

  d_->animation_timer.setInterval(16);
  connect(&d_->animation_timer, &QTimer::timeout, this, [this]() {
    handleGestureResult(d_->core->tickAnimations(), {});
  });
}

SweetEditorWidget::~SweetEditorWidget() = default;

void SweetEditorWidget::loadDocument(const std::shared_ptr<::sweeteditor::Document>& document) {
  d_->document = document != nullptr
    ? document
    : std::make_shared<::sweeteditor::LineArrayDocument>(::sweeteditor::U8String {});

  d_->core->loadDocument(d_->document);
  d_->font_metrics_dirty = true;
  if (!d_->language_configuration.bracket_pairs.isEmpty()
      || !d_->language_configuration.auto_closing_pairs.isEmpty()) {
    setLanguageConfiguration(d_->language_configuration);
  }
  syncViewport();
  d_->decoration_manager.onDocumentLoaded();
  dismissCompletion();
  flush();
  syncInputMethodState();
  Q_EMIT documentLoaded();
}

bool SweetEditorWidget::loadFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray bytes = file.readAll();
  d_->metadata.file_path = path;
  d_->metadata.display_name = QFileInfo(path).fileName();
  loadDocument(std::make_shared<::sweeteditor::LineArrayDocument>(
    ::sweeteditor::U8String(bytes.constData(), static_cast<size_t>(bytes.size()))
  ));
  return true;
}

void SweetEditorWidget::loadText(const QString& text) {
  loadDocument(std::make_shared<::sweeteditor::LineArrayDocument>(toUtf8(text)));
}

QString SweetEditorWidget::text() const {
  return d_->document != nullptr ? toQString(d_->document->getU8Text()) : QString {};
}

void SweetEditorWidget::flush() {
  invalidateRenderModel();
  viewport()->update();
}

void SweetEditorWidget::applyTheme(const EditorTheme& theme) {
  d_->theme = theme;
  if (d_->completion_popup != nullptr) {
    d_->completion_popup->applyTheme(theme);
  }
  viewport()->update();
}

const EditorTheme& SweetEditorWidget::theme() const noexcept {
  return d_->theme;
}

void SweetEditorWidget::setPerfOverlayEnabled(bool enabled) {
  d_->renderer.setPerfOverlayEnabled(enabled);
  viewport()->update();
}

bool SweetEditorWidget::isPerfOverlayEnabled() const noexcept {
  return d_->renderer.isPerfOverlayEnabled();
}

EditorSettings& SweetEditorWidget::settings() noexcept {
  return d_->settings;
}

const EditorSettings& SweetEditorWidget::settings() const noexcept {
  return d_->settings;
}

void SweetEditorWidget::setLanguageConfiguration(const LanguageConfiguration& configuration) {
  d_->language_configuration = configuration;

  if (!configuration.bracket_pairs.isEmpty()) {
    ::sweeteditor::Vector<::sweeteditor::BracketPair> pairs;
    pairs.reserve(static_cast<size_t>(configuration.bracket_pairs.size()));
    for (const auto& pair : configuration.bracket_pairs) {
      pairs.push_back(pair);
    }
    d_->core->setBracketPairs(std::move(pairs));
  }

  ::sweeteditor::Vector<::sweeteditor::BracketPair> auto_closing_pairs;
  auto_closing_pairs.reserve(static_cast<size_t>(configuration.auto_closing_pairs.size()));
  for (const auto& pair : configuration.auto_closing_pairs) {
    auto_closing_pairs.push_back(pair);
  }
  d_->core->setAutoClosingPairs(std::move(auto_closing_pairs));
  flush();
}

const LanguageConfiguration& SweetEditorWidget::languageConfiguration() const noexcept {
  return d_->language_configuration;
}

void SweetEditorWidget::setEditorIconProvider(const EditorIconProvider* provider) {
  d_->icon_provider = provider;
  viewport()->update();
}

const EditorIconProvider* SweetEditorWidget::editorIconProvider() const noexcept {
  return d_->icon_provider;
}

void SweetEditorWidget::setMetadata(const EditorMetadata& metadata) {
  d_->metadata = metadata;
}

const EditorMetadata& SweetEditorWidget::metadata() const noexcept {
  return d_->metadata;
}

void SweetEditorWidget::addDecorationProvider(DecorationProvider* provider) {
  d_->decoration_manager.addProvider(provider);
}

void SweetEditorWidget::removeDecorationProvider(DecorationProvider* provider) {
  d_->decoration_manager.removeProvider(provider);
}

void SweetEditorWidget::addCompletionProvider(CompletionProvider* provider) {
  d_->completion_manager.addProvider(provider);
}

void SweetEditorWidget::removeCompletionProvider(CompletionProvider* provider) {
  d_->completion_manager.removeProvider(provider);
  if (d_->completion_manager.empty()) {
    dismissCompletion();
  }
}

void SweetEditorWidget::triggerCompletion() {
  triggerCompletionInternal(CompletionTriggerKind::Invoked);
}

void SweetEditorWidget::showCompletionItems(const QList<CompletionItem>& items) {
  d_->completion_manager.showItems(items);
}

void SweetEditorWidget::dismissCompletion() {
  d_->completion_manager.dismiss();
}

void SweetEditorWidget::addNewLineActionProvider(NewLineActionProvider* provider) {
  d_->newline_manager.addProvider(provider);
}

void SweetEditorWidget::removeNewLineActionProvider(NewLineActionProvider* provider) {
  d_->newline_manager.removeProvider(provider);
}

void SweetEditorWidget::undo() {
  const auto result = d_->core->undo();
  if (result.changed) {
    dispatchTextEditResult(result);
  }
  flush();
  syncCompletionPopup(result.changed, false);
  syncInputMethodState();
}

void SweetEditorWidget::redo() {
  const auto result = d_->core->redo();
  if (result.changed) {
    dispatchTextEditResult(result);
  }
  flush();
  syncCompletionPopup(result.changed, false);
  syncInputMethodState();
}

bool SweetEditorWidget::canUndo() const {
  return d_->core->canUndo();
}

bool SweetEditorWidget::canRedo() const {
  return d_->core->canRedo();
}

void SweetEditorWidget::setReadOnly(bool read_only) {
  d_->settings.setReadOnly(read_only);
}

bool SweetEditorWidget::isReadOnly() const {
  return d_->core->isReadOnly();
}

void SweetEditorWidget::setWrapMode(::sweeteditor::WrapMode mode) {
  d_->settings.setWrapMode(mode);
}

void SweetEditorWidget::setTabSize(uint32_t tab_size) {
  d_->settings.setTabSize(tab_size);
}

void SweetEditorWidget::setScale(float scale) {
  d_->settings.setScale(scale);
}

float SweetEditorWidget::scale() const {
  return d_->core->getViewState().scale;
}

void SweetEditorWidget::setCursorPosition(const ::sweeteditor::TextPosition& position) {
  d_->core->setCursorPosition(position);
  d_->core->ensureCursorVisible();
  flush();
  syncInputMethodState();
}

::sweeteditor::TextPosition SweetEditorWidget::cursorPosition() const {
  return d_->core->getCursorPosition();
}

void SweetEditorWidget::setSelection(const ::sweeteditor::TextRange& range) {
  d_->core->setSelection(range);
  d_->core->ensureCursorVisible();
  flush();
  syncInputMethodState();
}

::sweeteditor::TextRange SweetEditorWidget::selection() const {
  return d_->core->getSelection();
}

bool SweetEditorWidget::hasSelection() const {
  return d_->core->hasSelection();
}

void SweetEditorWidget::clearSelection() {
  d_->core->clearSelection();
  flush();
  syncInputMethodState();
}

void SweetEditorWidget::selectAll() {
  d_->core->selectAll();
  flush();
  syncInputMethodState();
}

void SweetEditorWidget::scrollToLine(size_t line, ::sweeteditor::ScrollBehavior behavior) {
  d_->core->scrollToLine(line, behavior);
  flush();
  syncInputMethodState();
}

void SweetEditorWidget::gotoPosition(size_t line, size_t column) {
  d_->core->gotoPosition(line, column);
  flush();
  syncInputMethodState();
}

::sweeteditor::EditorCore& SweetEditorWidget::editorCore() noexcept {
  return *d_->core;
}

const ::sweeteditor::EditorCore& SweetEditorWidget::editorCore() const noexcept {
  return *d_->core;
}

const ::sweeteditor::EditorRenderModel& SweetEditorWidget::renderModel() const noexcept {
  const_cast<SweetEditorWidget*>(this)->ensureRenderModelUpToDate();
  return d_->render_model;
}

void SweetEditorWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);

  ensureRenderModelUpToDate();

  QPainter painter(viewport());
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setFont(font());

  d_->renderer.paint(
    painter,
    viewport()->rect(),
    d_->theme,
    d_->render_model,
    font(),
    d_->icon_provider,
    d_->settings.selectionHandlesEnabled()
  );
}

void SweetEditorWidget::resizeEvent(QResizeEvent* event) {
  QAbstractScrollArea::resizeEvent(event);
  syncViewport();
  flush();
  syncCompletionPopup(false, false);
  syncInputMethodState();
}

void SweetEditorWidget::changeEvent(QEvent* event) {
  QAbstractScrollArea::changeEvent(event);
  if (event->type() == QEvent::FontChange) {
    if (!d_->syncing_platform_font) {
      d_->settings.syncFontFromWidget(font());
    }
    d_->text_measurer->setBaseFont(font());
    d_->font_metrics_dirty = true;
    syncViewport();
    flush();
    syncCompletionPopup(false, false);
    syncInputMethodState();
  }
}

void SweetEditorWidget::focusInEvent(QFocusEvent* event) {
  QAbstractScrollArea::focusInEvent(event);
  viewport()->update();
  updateMicroFocus();
}

void SweetEditorWidget::focusOutEvent(QFocusEvent* event) {
  QAbstractScrollArea::focusOutEvent(event);
  if (d_->left_button_down) {
    d_->left_button_down = false;
    if (viewport()->mouseGrabber() == viewport()) {
      viewport()->releaseMouse();
    }
    if (d_->has_last_pointer_position) {
      handleGestureResult(
        d_->core->handleGestureEvent(makeMouseGesture(
          ::sweeteditor::EventType::MOUSE_UP,
          d_->last_pointer_position,
          Qt::NoModifier
        )),
        {}
      );
    } else {
      d_->animation_timer.stop();
    }
  }
  if (d_->core->isComposing()) {
    d_->core->compositionCancel();
    flush();
    syncInputMethodState();
  }
  dismissCompletion();
  viewport()->update();
}

void SweetEditorWidget::mousePressEvent(QMouseEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("mouseDown"));
  setFocus(Qt::MouseFocusReason);
  d_->last_pointer_position = event->position();
  d_->has_last_pointer_position = true;

  if (d_->completion_popup != nullptr && d_->completion_popup->contains(event->position().toPoint()) == false
      && d_->completion_popup->isShowing()) {
    dismissCompletion();
  }

  if (event->button() == Qt::LeftButton) {
    d_->left_button_down = true;
    viewport()->grabMouse();
    handleGestureResult(
      d_->core->handleGestureEvent(makeMouseGesture(::sweeteditor::EventType::MOUSE_DOWN, event->position(), event->modifiers())),
      event->globalPosition().toPoint()
    );
    event->accept();
    return;
  }

  if (event->button() == Qt::RightButton) {
    handleGestureResult(
      d_->core->handleGestureEvent(makeMouseGesture(::sweeteditor::EventType::MOUSE_RIGHT_DOWN, event->position(), event->modifiers())),
      event->globalPosition().toPoint()
    );
    event->accept();
    return;
  }

  QAbstractScrollArea::mousePressEvent(event);
}

void SweetEditorWidget::mouseDoubleClickEvent(QMouseEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("mouseDoubleClick"));
  setFocus(Qt::MouseFocusReason);
  d_->last_pointer_position = event->position();
  d_->has_last_pointer_position = true;

  if (d_->completion_popup != nullptr && d_->completion_popup->contains(event->position().toPoint()) == false
      && d_->completion_popup->isShowing()) {
    dismissCompletion();
  }

  if (event->button() == Qt::LeftButton) {
    d_->left_button_down = true;
    viewport()->grabMouse();
    handleGestureResult(
      d_->core->handleGestureEvent(makeMouseGesture(::sweeteditor::EventType::MOUSE_DOWN, event->position(), event->modifiers())),
      event->globalPosition().toPoint()
    );
    event->accept();
    return;
  }

  QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void SweetEditorWidget::mouseMoveEvent(QMouseEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("mouseMove"));
  d_->last_pointer_position = event->position();
  d_->has_last_pointer_position = true;
  if (d_->left_button_down) {
    handleGestureResult(
      d_->core->handleGestureEvent(makeMouseGesture(::sweeteditor::EventType::MOUSE_MOVE, event->position(), event->modifiers())),
      event->globalPosition().toPoint()
    );
    event->accept();
    return;
  }

  QAbstractScrollArea::mouseMoveEvent(event);
}

void SweetEditorWidget::mouseReleaseEvent(QMouseEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("mouseUp"));
  d_->last_pointer_position = event->position();
  d_->has_last_pointer_position = true;
  if (event->button() == Qt::LeftButton && d_->left_button_down) {
    d_->left_button_down = false;
    if (viewport()->mouseGrabber() == viewport()) {
      viewport()->releaseMouse();
    }
    handleGestureResult(
      d_->core->handleGestureEvent(makeMouseGesture(::sweeteditor::EventType::MOUSE_UP, event->position(), event->modifiers())),
      event->globalPosition().toPoint()
    );
    event->accept();
    return;
  }

  QAbstractScrollArea::mouseReleaseEvent(event);
}

void SweetEditorWidget::wheelEvent(QWheelEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("mouseWheel"));
  d_->last_pointer_position = event->position();
  d_->has_last_pointer_position = true;
  ::sweeteditor::GestureEvent gesture = makeMouseGesture(
    ::sweeteditor::EventType::MOUSE_WHEEL,
    event->position(),
    event->modifiers()
  );

  const QPoint pixel_delta = event->pixelDelta();
  const QPoint angle_delta = event->angleDelta();
  const QPointF delta = !pixel_delta.isNull()
    ? QPointF(pixel_delta)
    : QPointF(angle_delta);

  gesture.wheel_delta_x = static_cast<float>(delta.x());
  gesture.wheel_delta_y = static_cast<float>(delta.y());
  handleGestureResult(d_->core->handleGestureEvent(gesture), event->globalPosition().toPoint());
  event->accept();
}

void SweetEditorWidget::keyPressEvent(QKeyEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("keyPress"));
  if (d_->completion_popup != nullptr && d_->completion_popup->handleKey(event->key())) {
    event->accept();
    return;
  }

  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && handleNewLineAction()) {
    event->accept();
    return;
  }

  ::sweeteditor::KeyEvent key_event;
  key_event.modifiers = toKeyModifiers(event->modifiers());
  if (isPlainTextInput(event)) {
    key_event.text = toUtf8(event->text());
    key_event.key_code = ::sweeteditor::KeyCode::NONE;
  } else {
    key_event.key_code = toKeyCode(event->key());
  }

  const auto result = d_->core->handleKeyEvent(key_event);
  if (result.command == ::sweeteditor::EditorCommand::TRIGGER_COMPLETION) {
    flush();
    syncInputMethodState();
    triggerCompletionInternal(CompletionTriggerKind::Invoked);
    event->accept();
    return;
  }

  if (result.command != ::sweeteditor::EditorCommand::NONE && handleClipboardCommand(result.command)) {
    event->accept();
    return;
  }

  if (result.handled || result.content_changed || result.cursor_changed || result.selection_changed) {
    dispatchKeyEventResult(result);
    flush();
    syncCompletionPopup(result.content_changed, result.cursor_changed);
    syncInputMethodState();
    if (result.content_changed && !d_->core->isInLinkedEditing()) {
      const QString text = event->text();
      if (d_->completion_manager.isTriggerCharacter(text)) {
        triggerCompletionInternal(CompletionTriggerKind::Character, text);
      } else if (d_->completion_popup != nullptr && d_->completion_popup->isShowing()) {
        triggerCompletionInternal(CompletionTriggerKind::Retrigger);
      } else if (isCompletionWordInput(text)) {
        triggerCompletionInternal(CompletionTriggerKind::Invoked);
      }
    }
    event->accept();
    return;
  }

  QAbstractScrollArea::keyPressEvent(event);
}

void SweetEditorWidget::inputMethodEvent(QInputMethodEvent* event) {
  InputPerfScope perf(d_->renderer, QStringLiteral("ime"));
  if (isReadOnly()) {
    if (d_->core->isComposing()) {
      d_->core->compositionCancel();
      flush();
      syncInputMethodState();
    }
    event->accept();
    return;
  }

  bool text_changed = false;
  ::sweeteditor::TextEditResult edit_result;
  bool has_edit_result = false;

  if (!event->commitString().isEmpty()) {
    if (d_->core->isComposing()) {
      edit_result = d_->core->compositionEnd(toUtf8(event->commitString()));
    } else {
      edit_result = d_->core->insertText(toUtf8(event->commitString()));
    }
    has_edit_result = true;
    text_changed = edit_result.changed;
  }

  if (!event->preeditString().isEmpty()) {
    if (!d_->core->isComposing()) {
      d_->core->compositionStart();
    }
    d_->core->compositionUpdate(toUtf8(event->preeditString()));
  } else if (d_->core->isComposing() && event->commitString().isEmpty()) {
    d_->core->compositionCancel();
  }

  if (has_edit_result && edit_result.changed) {
    dispatchTextEditResult(edit_result);
  }
  flush();
  syncCompletionPopup(text_changed, false);
  syncInputMethodState();
  if (text_changed && !d_->core->isInLinkedEditing()) {
    const QString committed_text = event->commitString();
    if (d_->completion_manager.isTriggerCharacter(committed_text)) {
      triggerCompletionInternal(CompletionTriggerKind::Character, committed_text);
    } else if (d_->completion_popup != nullptr && d_->completion_popup->isShowing()) {
      triggerCompletionInternal(CompletionTriggerKind::Retrigger);
    } else if (isCompletionWordInput(committed_text)) {
      triggerCompletionInternal(CompletionTriggerKind::Invoked);
    }
  }
  event->accept();
}

QVariant SweetEditorWidget::inputMethodQuery(Qt::InputMethodQuery query) const {
  switch (query) {
    case Qt::ImEnabled:
      return !isReadOnly();
    case Qt::ImCursorRectangle: {
      const auto rect = d_->core->getCursorScreenRect();
      const QPoint offset = viewport()->geometry().topLeft();
      return QRectF(rect.x + offset.x(), rect.y + offset.y(), 2.0, rect.height);
    }
    case Qt::ImCursorPosition:
      return static_cast<int>(d_->document->getCharIndexFromPosition(d_->core->getCursorPosition()));
    case Qt::ImAnchorPosition:
      return static_cast<int>(d_->document->getCharIndexFromPosition(
        d_->core->hasSelection() ? d_->core->getSelection().start : d_->core->getCursorPosition()
      ));
    case Qt::ImSurroundingText:
      return text();
    case Qt::ImCurrentSelection:
      return toQString(d_->core->getSelectedText());
    case Qt::ImFont:
      return font();
    case Qt::ImHints:
      return static_cast<int>(Qt::ImhNoPredictiveText);
    default:
      return {};
  }
}

void SweetEditorWidget::requestDecorationRefresh() {
  if (d_->decoration_manager.empty()) {
    return;
  }

  d_->decoration_manager.requestRefresh();
}

void SweetEditorWidget::syncViewport() {
  d_->core->setViewport({
    static_cast<float>(viewport()->width()),
    static_cast<float>(viewport()->height()),
  });
}

void SweetEditorWidget::ensureRenderModelUpToDate() {
  if (!d_->render_model_dirty && !d_->font_metrics_dirty) {
    return;
  }
  std::optional<PerfStepRecorder> build_perf;
  if (d_->renderer.isPerfOverlayEnabled()) {
    build_perf.emplace(PerfStepRecorder::start());
  }
  if (d_->font_metrics_dirty) {
    d_->core->onFontMetricsChanged();
    d_->font_metrics_dirty = false;
  }
  if (build_perf.has_value()) {
    build_perf->mark(PerfStepRecorder::STEP_PREP);
    d_->renderer.perfMeasureStats().reset();
    d_->text_measurer->setPerfStats(&d_->renderer.perfMeasureStats());
  } else {
    d_->text_measurer->setPerfStats(nullptr);
  }
  d_->render_model = {};
  d_->core->buildRenderModel(d_->render_model);
  d_->text_measurer->setPerfStats(nullptr);
  d_->render_model_dirty = false;
  if (build_perf.has_value()) {
    build_perf->mark(PerfStepRecorder::STEP_BUILD);
    build_perf->mark(PerfStepRecorder::STEP_METRICS);
    build_perf->finish();
    d_->renderer.perfOverlay().recordBuild(*build_perf, d_->renderer.perfMeasureStats().buildSummary());
    if (build_perf->totalMs() >= PerfOverlay::WARN_BUILD_MS
        || build_perf->getStepMs(PerfStepRecorder::STEP_BUILD) >= PerfOverlay::WARN_BUILD_MS
        || d_->renderer.perfMeasureStats().shouldLogBuild()) {
      qDebug().noquote()
        << QStringLiteral("[PERF][Build] total=%1ms prep=%2 build=%3 metrics=%4 | %5")
             .arg(build_perf->totalMs(), 0, 'f', 2)
             .arg(build_perf->getStepMs(PerfStepRecorder::STEP_PREP), 0, 'f', 2)
             .arg(build_perf->getStepMs(PerfStepRecorder::STEP_BUILD), 0, 'f', 2)
             .arg(build_perf->getStepMs(PerfStepRecorder::STEP_METRICS), 0, 'f', 2)
             .arg(d_->renderer.perfMeasureStats().buildSummary());
    }
  }
}

void SweetEditorWidget::invalidateRenderModel() noexcept {
  d_->render_model_dirty = true;
}

void SweetEditorWidget::syncPlatformScale(float scale) {
  const QFont platform_font = scaledFont(d_->settings.font(), scale);
  d_->syncing_platform_font = true;
  setFont(platform_font);
  d_->syncing_platform_font = false;
}

std::pair<int, int> SweetEditorWidget::decorationVisibleLineRange() {
  ensureRenderModelUpToDate();
  if (d_->render_model.lines.empty()) {
    return {0, -1};
  }

  int start_line = std::numeric_limits<int>::max();
  int end_line = -1;
  for (const auto& line : d_->render_model.lines) {
    const int logical_line = static_cast<int>(line.logical_line);
    start_line = std::min(start_line, logical_line);
    end_line = std::max(end_line, logical_line);
  }

  return {start_line == std::numeric_limits<int>::max() ? 0 : start_line, end_line};
}

int SweetEditorWidget::decorationTotalLineCount() const {
  return d_->document != nullptr ? static_cast<int>(d_->document->getLineCount()) : 0;
}

const ::sweeteditor::Document* SweetEditorWidget::decorationDocument() const noexcept {
  return d_->document.get();
}

const ::sweeteditor::Document* SweetEditorWidget::completionDocument() const noexcept {
  return d_->document.get();
}

QString SweetEditorWidget::completionLineText(const ::sweeteditor::TextPosition& position) const {
  if (d_->document == nullptr || position.line >= d_->document->getLineCount()) {
    return {};
  }
  return toQString(d_->document->getLineU16Text(position.line));
}

std::optional<::sweeteditor::TextRange> SweetEditorWidget::completionWordRange() const {
  const ::sweeteditor::TextRange range = d_->core->getWordRangeAtCursor();
  if (range.start.line == range.end.line && range.start.column == range.end.column) {
    return std::nullopt;
  }
  return range;
}

void SweetEditorWidget::dispatchTextEditResult(const ::sweeteditor::TextEditResult& result) {
  if (!result.changed) {
    return;
  }

  d_->decoration_manager.onTextChanged(result.changes.empty() ? nullptr : &result.changes);
  Q_EMIT textChanged();
}

void SweetEditorWidget::dispatchKeyEventResult(const ::sweeteditor::KeyEventResult& result) {
  if (result.content_changed) {
    dispatchTextEditResult(result.edit_result);
  }

  if (result.cursor_changed) {
    emitCursorSignal(d_->core->getCursorPosition());
  }

  if (result.selection_changed) {
    if (d_->core->hasSelection()) {
      const auto selection = d_->core->getSelection();
      emitSelectionSignal(true, &selection);
    } else {
      emitSelectionSignal(false, nullptr);
    }
  }
}

void SweetEditorWidget::dispatchGestureResult(const ::sweeteditor::GestureResult& result, const QPoint& global_pos) {
  const QPoint resolved_global_pos = global_pos.isNull()
    ? viewport()->mapToGlobal(QPoint(static_cast<int>(result.tap_point.x), static_cast<int>(result.tap_point.y)))
    : global_pos;

  switch (result.type) {
    case ::sweeteditor::GestureType::LONG_PRESS:
      Q_EMIT longPressed(
        static_cast<int>(result.cursor_position.line),
        static_cast<int>(result.cursor_position.column),
        resolved_global_pos
      );
      emitCursorSignal(result.cursor_position);
      break;
    case ::sweeteditor::GestureType::DOUBLE_TAP:
      Q_EMIT doubleTapped(
        static_cast<int>(result.cursor_position.line),
        static_cast<int>(result.cursor_position.column),
        result.has_selection,
        resolved_global_pos
      );
      emitCursorSignal(result.cursor_position);
      if (result.has_selection) {
        emitSelectionSignal(true, &result.selection);
      }
      break;
    case ::sweeteditor::GestureType::TAP:
      emitCursorSignal(result.cursor_position);
      dismissCompletion();
      switch (result.hit_target.type) {
        case ::sweeteditor::HitTargetType::INLAY_HINT_TEXT:
          Q_EMIT inlayHintClicked(
            static_cast<int>(result.hit_target.line),
            static_cast<int>(result.hit_target.column),
            0,
            0,
            resolved_global_pos
          );
          break;
        case ::sweeteditor::HitTargetType::INLAY_HINT_ICON:
          Q_EMIT inlayHintClicked(
            static_cast<int>(result.hit_target.line),
            static_cast<int>(result.hit_target.column),
            1,
            result.hit_target.icon_id,
            resolved_global_pos
          );
          break;
        case ::sweeteditor::HitTargetType::INLAY_HINT_COLOR:
          Q_EMIT inlayHintClicked(
            static_cast<int>(result.hit_target.line),
            static_cast<int>(result.hit_target.column),
            2,
            result.hit_target.color_value,
            resolved_global_pos
          );
          break;
        case ::sweeteditor::HitTargetType::GUTTER_ICON:
          Q_EMIT gutterIconClicked(static_cast<int>(result.hit_target.line), result.hit_target.icon_id);
          break;
        case ::sweeteditor::HitTargetType::FOLD_GUTTER:
          Q_EMIT foldToggled(static_cast<int>(result.hit_target.line), true);
          break;
        case ::sweeteditor::HitTargetType::FOLD_PLACEHOLDER:
          Q_EMIT foldToggled(static_cast<int>(result.hit_target.line), false);
          break;
        default:
          break;
      }
      break;
    case ::sweeteditor::GestureType::SCROLL:
    case ::sweeteditor::GestureType::FAST_SCROLL:
      Q_EMIT scrollChanged(result.view_scroll_x, result.view_scroll_y);
      d_->decoration_manager.onScrollChanged();
      dismissCompletion();
      break;
    case ::sweeteditor::GestureType::SCALE:
      d_->settings.scale_ = result.view_scale;
      syncPlatformScale(result.view_scale);
      Q_EMIT scaleChanged(result.view_scale);
      break;
    case ::sweeteditor::GestureType::DRAG_SELECT:
      dismissCompletion();
      emitSelectionSignal(result.has_selection, result.has_selection ? &result.selection : nullptr);
      break;
    case ::sweeteditor::GestureType::CONTEXT_MENU:
      Q_EMIT editorContextMenuRequested(resolved_global_pos);
      break;
    default:
      break;
  }
}

void SweetEditorWidget::emitCursorSignal(const ::sweeteditor::TextPosition& cursor) {
  Q_EMIT cursorChanged(static_cast<int>(cursor.line), static_cast<int>(cursor.column));
}

void SweetEditorWidget::emitSelectionSignal(bool has_selection, const ::sweeteditor::TextRange* selection) {
  const ::sweeteditor::TextRange empty_selection {};
  const ::sweeteditor::TextRange& range = selection != nullptr ? *selection : empty_selection;
  Q_EMIT selectionChanged(
    static_cast<int>(range.start.line),
    static_cast<int>(range.start.column),
    static_cast<int>(range.end.line),
    static_cast<int>(range.end.column),
    has_selection
  );
}

void SweetEditorWidget::syncCompletionPopup(bool text_changed, bool cursor_changed) {
  const bool has_selection = d_->core->hasSelection();
  if (d_->completion_popup != nullptr && d_->completion_popup->isShowing()) {
    if (has_selection || (cursor_changed && !text_changed)) {
      dismissCompletion();
    } else {
      repositionCompletionPopup();
    }
  }
}

void SweetEditorWidget::syncInputMethodState() {
  updateMicroFocus();
}

bool SweetEditorWidget::handleNewLineAction() {
  if (d_->document == nullptr || d_->newline_manager.empty()) {
    return false;
  }

  const auto action = d_->newline_manager.createAction(*d_->document, d_->metadata, d_->core->getCursorPosition());
  if (!action.has_value()) {
    return false;
  }

  const auto result = d_->core->insertText(toUtf8(action->text));
  if (result.changed) {
    dispatchTextEditResult(result);
  }
  flush();
  syncCompletionPopup(result.changed, false);
  syncInputMethodState();
  return true;
}

bool SweetEditorWidget::handleClipboardCommand(::sweeteditor::EditorCommand command) {
  QClipboard* clipboard = QApplication::clipboard();
  if (clipboard == nullptr) {
    return false;
  }

  switch (command) {
    case ::sweeteditor::EditorCommand::COPY:
      clipboard->setText(toQString(d_->core->getSelectedText()));
      return true;
    case ::sweeteditor::EditorCommand::CUT:
      clipboard->setText(toQString(d_->core->getSelectedText()));
      {
        const auto result = d_->core->deleteText(d_->core->getSelection());
        if (result.changed) {
          dispatchTextEditResult(result);
        }
        flush();
        syncCompletionPopup(result.changed, false);
        syncInputMethodState();
      }
      return true;
    case ::sweeteditor::EditorCommand::PASTE:
      {
        const auto result = d_->core->insertText(toUtf8(clipboard->text()));
        if (result.changed) {
          dispatchTextEditResult(result);
        }
        flush();
        syncCompletionPopup(result.changed, false);
        syncInputMethodState();
      }
      return true;
    default:
      return false;
  }
}

void SweetEditorWidget::handleGestureResult(const ::sweeteditor::GestureResult& result, const QPoint& global_pos) {
  if (result.needs_animation) {
    if (!d_->animation_timer.isActive()) {
      d_->animation_timer.start();
    }
  } else {
    d_->animation_timer.stop();
  }

  dispatchGestureResult(result, global_pos);
  flush();
  syncCompletionPopup(false, false);
  syncInputMethodState();
}

void SweetEditorWidget::triggerCompletionInternal(CompletionTriggerKind kind, const QString& trigger_character) {
  if (d_->document == nullptr || d_->completion_manager.empty() || d_->core->hasSelection()) {
    dismissCompletion();
    return;
  }

  if (kind != CompletionTriggerKind::Invoked && kind != CompletionTriggerKind::Character
      && kind != CompletionTriggerKind::Retrigger) {
    return;
  }

  if (kind == CompletionTriggerKind::Invoked
      && trigger_character.isEmpty()
      && (d_->completion_popup == nullptr || !d_->completion_popup->isShowing())
      && toQString(d_->core->getWordAtCursor()).isEmpty()) {
    dismissCompletion();
    return;
  }

  d_->completion_manager.triggerCompletion(kind, trigger_character);
}

void SweetEditorWidget::showCompletionPopup(const QList<CompletionItem>& items) {
  if (d_->completion_popup == nullptr) {
    hideCompletionPopup();
    return;
  }

  repositionCompletionPopup();
  d_->completion_popup->updateItems(items);
}

void SweetEditorWidget::hideCompletionPopup() {
  if (d_->completion_popup != nullptr) {
    d_->completion_popup->dismiss();
  }
}

void SweetEditorWidget::applyCompletionItem(const CompletionItem& item) {
  const bool is_snippet = item.insert_text_format == CompletionItem::INSERT_TEXT_FORMAT_SNIPPET;
  const QString text = item.text_edit.has_value()
    ? item.text_edit->new_text
    : (item.insert_text.isEmpty() ? item.label : item.insert_text);

  std::optional<::sweeteditor::TextRange> replace_range;
  if (item.text_edit.has_value()) {
    replace_range = item.text_edit->range;
  } else {
    const ::sweeteditor::TextRange word_range = d_->core->getWordRangeAtCursor();
    if (!(word_range.start.line == word_range.end.line && word_range.start.column == word_range.end.column)) {
      replace_range = word_range;
    }
  }

  bool changed = false;
  if (is_snippet) {
    if (replace_range.has_value()) {
      const auto delete_result = d_->core->deleteText(*replace_range);
      if (delete_result.changed) {
        dispatchTextEditResult(delete_result);
        changed = true;
      }
    }

    const auto insert_result = d_->core->insertSnippet(toUtf8(text));
    if (insert_result.changed) {
      dispatchTextEditResult(insert_result);
      changed = true;
    }
  } else {
    const auto result = replace_range.has_value()
      ? d_->core->replaceText(*replace_range, toUtf8(text))
      : d_->core->insertText(toUtf8(text));
    if (result.changed) {
      dispatchTextEditResult(result);
      changed = true;
    }
  }

  flush();
  syncCompletionPopup(changed, false);
  syncInputMethodState();
}

void SweetEditorWidget::repositionCompletionPopup() {
  if (d_->completion_popup == nullptr) {
    return;
  }

  const auto cursor_rect = d_->core->getCursorScreenRect();
  d_->completion_popup->updateCursorPosition(cursor_rect.x, cursor_rect.y, cursor_rect.height);
}

} // namespace sweeteditor::qt
