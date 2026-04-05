#include "DecorationProviderManager.h"

#include <SweetEditorWidget.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace sweeteditor::qt {

namespace {

template<typename T>
::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<T>>> toBatchEntries(
  const std::map<size_t, ::sweeteditor::Vector<T>>& map) {
  ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<T>>> entries;
  entries.reserve(map.size());
  for (const auto& [line, items] : map) {
    entries.push_back({line, items});
  }
  return entries;
}

template<typename T>
void appendLineMap(std::map<size_t, ::sweeteditor::Vector<T>>& target,
                   const std::map<size_t, ::sweeteditor::Vector<T>>& source) {
  for (const auto& [line, items] : source) {
    auto& dst = target[line];
    dst.insert(dst.end(), items.begin(), items.end());
  }
}

} // namespace

DecorationProviderManager::DecorationProviderManager(SweetEditorWidget* editor) {
  bind(editor);
}

DecorationProviderManager::~DecorationProviderManager() {
  if (debounce_timer_ != nullptr) {
    debounce_timer_->stop();
  }
  if (scroll_refresh_timer_ != nullptr) {
    scroll_refresh_timer_->stop();
  }
  providers_.clear();
  states_.clear();
  pending_text_changes_.clear();
  scroll_refresh_scheduled_ = false;
  pending_scroll_refresh_ = false;
}

void DecorationProviderManager::bind(SweetEditorWidget* editor) {
  editor_ = editor;
  if (editor_ == nullptr || debounce_timer_ != nullptr) {
    return;
  }

  debounce_timer_ = new QTimer(editor_);
  debounce_timer_->setSingleShot(true);
  QObject::connect(debounce_timer_, &QTimer::timeout, editor_, [this]() {
    doRefresh();
  });

  scroll_refresh_timer_ = new QTimer(editor_);
  scroll_refresh_timer_->setSingleShot(true);
  QObject::connect(scroll_refresh_timer_, &QTimer::timeout, editor_, [this]() {
    scroll_refresh_scheduled_ = false;
    if (debounce_timer_ != nullptr) {
      debounce_timer_->stop();
    }
    doRefresh();
    scroll_refresh_clock_.restart();
    if (pending_scroll_refresh_) {
      pending_scroll_refresh_ = false;
      scheduleScrollRefresh();
    }
  });

  scroll_refresh_clock_.start();
}

void DecorationProviderManager::addProvider(DecorationProvider* provider) {
  if (provider == nullptr
      || std::find(providers_.begin(), providers_.end(), provider) != providers_.end()) {
    return;
  }

  providers_.push_back(provider);
  states_[provider] = ProviderState {};
  requestRefresh();
}

void DecorationProviderManager::removeProvider(DecorationProvider* provider) {
  providers_.erase(std::remove(providers_.begin(), providers_.end(), provider), providers_.end());
  states_.erase(provider);

  if (editor_ == nullptr) {
    return;
  }

  if (providers_.empty()) {
    clearManagedDecorations();
    editor_->flush();
    return;
  }

  applyMerged(true);
}

bool DecorationProviderManager::empty() const noexcept {
  return providers_.empty();
}

void DecorationProviderManager::requestRefresh() {
  scheduleRefresh(0, nullptr);
}

void DecorationProviderManager::onDocumentLoaded() {
  scheduleRefresh(0, nullptr);
}

void DecorationProviderManager::onScrollChanged() {
  scheduleScrollRefresh();
}

void DecorationProviderManager::onTextChanged(const std::vector<::sweeteditor::TextChange>* changes) {
  scheduleRefresh(50, changes);
}

void DecorationProviderManager::scheduleRefresh(int delay_ms,
                                                const std::vector<::sweeteditor::TextChange>* changes) {
  if (editor_ == nullptr || providers_.empty() || debounce_timer_ == nullptr) {
    return;
  }

  if (changes != nullptr) {
    pending_text_changes_.insert(
      pending_text_changes_.end(),
      changes->begin(),
      changes->end()
    );
  }

  if (scroll_refresh_timer_ != nullptr && scroll_refresh_scheduled_) {
    scroll_refresh_timer_->stop();
    scroll_refresh_scheduled_ = false;
  }
  pending_scroll_refresh_ = false;

  debounce_timer_->stop();
  debounce_timer_->setInterval(std::max(1, delay_ms == 0 ? 1 : delay_ms));
  debounce_timer_->start();
}

void DecorationProviderManager::scheduleScrollRefresh() {
  if (editor_ == nullptr || providers_.empty() || scroll_refresh_timer_ == nullptr) {
    return;
  }

  if (!scroll_refresh_clock_.isValid()) {
    scroll_refresh_clock_.start();
  }

  const qint64 elapsed_ms = scroll_refresh_clock_.elapsed();
  const int min_interval_ms = scrollRefreshMinIntervalMs();
  const int delay_ms = elapsed_ms >= min_interval_ms
    ? 1
    : std::max(1, static_cast<int>(min_interval_ms - elapsed_ms));

  if (scroll_refresh_scheduled_) {
    pending_scroll_refresh_ = true;
    return;
  }

  scroll_refresh_scheduled_ = true;
  scroll_refresh_timer_->stop();
  scroll_refresh_timer_->setInterval(delay_ms);
  scroll_refresh_timer_->start();
}

void DecorationProviderManager::doRefresh() {
  if (editor_ == nullptr || providers_.empty()) {
    return;
  }

  const DecorationContext context = buildContext();
  for (DecorationProvider* provider : providers_) {
    if (provider == nullptr) {
      continue;
    }

    auto& state = states_[provider];
    state.snapshot = provider->provideDecorations(context);
    state.has_snapshot = true;
  }

  applyMerged(false);
}

void DecorationProviderManager::applyMerged(bool clear_managed_first) {
  if (editor_ == nullptr) {
    return;
  }

  StyleSpanMap syntax_spans;
  StyleSpanMap semantic_spans;
  InlayHintMap inlay_hints;
  DiagnosticMap diagnostics;
  std::optional<::sweeteditor::Vector<::sweeteditor::IndentGuide>> indent_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::BracketGuide>> bracket_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::FlowGuide>> flow_guides;
  std::optional<::sweeteditor::Vector<::sweeteditor::SeparatorGuide>> separator_guides;
  ::sweeteditor::Vector<::sweeteditor::FoldRegion> fold_regions;
  GutterIconMap gutter_icons;
  PhantomTextMap phantom_texts;

  DecorationApplyMode syntax_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode semantic_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode inlay_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode diagnostic_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode indent_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode bracket_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode flow_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode separator_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode fold_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode gutter_mode = DecorationApplyMode::MERGE;
  DecorationApplyMode phantom_mode = DecorationApplyMode::MERGE;

  for (DecorationProvider* provider : providers_) {
    const auto it = states_.find(provider);
    if (it == states_.end() || !it->second.has_snapshot) {
      continue;
    }

    const DecorationResult& result = it->second.snapshot;
    syntax_mode = mergeMode(syntax_mode, result.syntax_spans_mode);
    if (result.syntax_spans.has_value()) {
      appendStyleSpanMap(syntax_spans, *result.syntax_spans);
    }

    semantic_mode = mergeMode(semantic_mode, result.semantic_spans_mode);
    if (result.semantic_spans.has_value()) {
      appendStyleSpanMap(semantic_spans, *result.semantic_spans);
    }

    inlay_mode = mergeMode(inlay_mode, result.inlay_hints_mode);
    if (result.inlay_hints.has_value()) {
      appendInlayHintMap(inlay_hints, *result.inlay_hints);
    }

    diagnostic_mode = mergeMode(diagnostic_mode, result.diagnostics_mode);
    if (result.diagnostics.has_value()) {
      appendDiagnosticMap(diagnostics, *result.diagnostics);
    }

    indent_mode = mergeMode(indent_mode, result.indent_guides_mode);
    if (result.indent_guides.has_value()) {
      indent_guides = *result.indent_guides;
    }

    bracket_mode = mergeMode(bracket_mode, result.bracket_guides_mode);
    if (result.bracket_guides.has_value()) {
      bracket_guides = *result.bracket_guides;
    }

    flow_mode = mergeMode(flow_mode, result.flow_guides_mode);
    if (result.flow_guides.has_value()) {
      flow_guides = *result.flow_guides;
    }

    separator_mode = mergeMode(separator_mode, result.separator_guides_mode);
    if (result.separator_guides.has_value()) {
      separator_guides = *result.separator_guides;
    }

    fold_mode = mergeMode(fold_mode, result.fold_regions_mode);
    if (result.fold_regions.has_value()) {
      fold_regions.insert(fold_regions.end(), result.fold_regions->begin(), result.fold_regions->end());
    }

    gutter_mode = mergeMode(gutter_mode, result.gutter_icons_mode);
    if (result.gutter_icons.has_value()) {
      appendGutterIconMap(gutter_icons, *result.gutter_icons);
    }

    phantom_mode = mergeMode(phantom_mode, result.phantom_texts_mode);
    if (result.phantom_texts.has_value()) {
      appendPhantomTextMap(phantom_texts, *result.phantom_texts);
    }
  }

  auto& core = editor_->editorCore();
  if (clear_managed_first) {
    clearManagedDecorations();
  }

  const auto clearSpanRange = [&](::sweeteditor::SpanLayer layer) {
    const auto empty = buildEmptyRangeMap<::sweeteditor::StyleSpan>(last_visible_start_line_, last_visible_end_line_);
    if (!empty.empty()) {
      core.setBatchLineSpans(layer, buildBatchStyleEntries(empty));
    }
  };
  const auto clearInlayRange = [&]() {
    const auto empty = buildEmptyRangeMap<::sweeteditor::InlayHint>(last_visible_start_line_, last_visible_end_line_);
    if (!empty.empty()) {
      core.setBatchLineInlayHints(buildBatchInlayEntries(empty));
    }
  };
  const auto clearDiagnosticRange = [&]() {
    const auto empty = buildEmptyRangeMap<::sweeteditor::DiagnosticSpan>(last_visible_start_line_, last_visible_end_line_);
    if (!empty.empty()) {
      core.setBatchLineDiagnostics(buildBatchDiagnosticEntries(empty));
    }
  };
  const auto clearGutterRange = [&]() {
    const auto empty = buildEmptyRangeMap<::sweeteditor::GutterIcon>(last_visible_start_line_, last_visible_end_line_);
    if (!empty.empty()) {
      core.setBatchLineGutterIcons(buildBatchGutterEntries(empty));
    }
  };
  const auto clearPhantomRange = [&]() {
    const auto empty = buildEmptyRangeMap<::sweeteditor::PhantomText>(last_visible_start_line_, last_visible_end_line_);
    if (!empty.empty()) {
      core.setBatchLinePhantomTexts(buildBatchPhantomEntries(empty));
    }
  };

  if (!clear_managed_first) {
    if (syntax_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearHighlights(::sweeteditor::SpanLayer::SYNTAX);
    } else if (syntax_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearSpanRange(::sweeteditor::SpanLayer::SYNTAX);
    }

    if (semantic_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearHighlights(::sweeteditor::SpanLayer::SEMANTIC);
    } else if (semantic_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearSpanRange(::sweeteditor::SpanLayer::SEMANTIC);
    }

    if (inlay_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearInlayHints();
    } else if (inlay_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearInlayRange();
    }

    if (diagnostic_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearDiagnostics();
    } else if (diagnostic_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearDiagnosticRange();
    }

    if (gutter_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearGutterIcons();
    } else if (gutter_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearGutterRange();
    }

    if (phantom_mode == DecorationApplyMode::REPLACE_ALL) {
      core.clearPhantomTexts();
    } else if (phantom_mode == DecorationApplyMode::REPLACE_RANGE) {
      clearPhantomRange();
    }
  }

  if (!syntax_spans.empty()) {
    core.setBatchLineSpans(::sweeteditor::SpanLayer::SYNTAX, buildBatchStyleEntries(syntax_spans));
  }
  if (!semantic_spans.empty()) {
    core.setBatchLineSpans(::sweeteditor::SpanLayer::SEMANTIC, buildBatchStyleEntries(semantic_spans));
  }
  if (!inlay_hints.empty()) {
    core.setBatchLineInlayHints(buildBatchInlayEntries(inlay_hints));
  }
  if (!diagnostics.empty()) {
    core.setBatchLineDiagnostics(buildBatchDiagnosticEntries(diagnostics));
  }
  if (!gutter_icons.empty()) {
    core.setBatchLineGutterIcons(buildBatchGutterEntries(gutter_icons));
  }
  if (!phantom_texts.empty()) {
    core.setBatchLinePhantomTexts(buildBatchPhantomEntries(phantom_texts));
  }

  if (clear_managed_first || indent_mode == DecorationApplyMode::REPLACE_ALL || indent_mode == DecorationApplyMode::REPLACE_RANGE) {
    auto guides = indent_guides.value_or(::sweeteditor::Vector<::sweeteditor::IndentGuide> {});
    core.setIndentGuides(std::move(guides));
  } else if (indent_guides.has_value()) {
    auto guides = *indent_guides;
    core.setIndentGuides(std::move(guides));
  }

  if (clear_managed_first || bracket_mode == DecorationApplyMode::REPLACE_ALL || bracket_mode == DecorationApplyMode::REPLACE_RANGE) {
    auto guides = bracket_guides.value_or(::sweeteditor::Vector<::sweeteditor::BracketGuide> {});
    core.setBracketGuides(std::move(guides));
  } else if (bracket_guides.has_value()) {
    auto guides = *bracket_guides;
    core.setBracketGuides(std::move(guides));
  }

  if (clear_managed_first || flow_mode == DecorationApplyMode::REPLACE_ALL || flow_mode == DecorationApplyMode::REPLACE_RANGE) {
    auto guides = flow_guides.value_or(::sweeteditor::Vector<::sweeteditor::FlowGuide> {});
    core.setFlowGuides(std::move(guides));
  } else if (flow_guides.has_value()) {
    auto guides = *flow_guides;
    core.setFlowGuides(std::move(guides));
  }

  if (clear_managed_first || separator_mode == DecorationApplyMode::REPLACE_ALL || separator_mode == DecorationApplyMode::REPLACE_RANGE) {
    auto guides = separator_guides.value_or(::sweeteditor::Vector<::sweeteditor::SeparatorGuide> {});
    core.setSeparatorGuides(std::move(guides));
  } else if (separator_guides.has_value()) {
    auto guides = *separator_guides;
    core.setSeparatorGuides(std::move(guides));
  }

  if (clear_managed_first || fold_mode == DecorationApplyMode::REPLACE_ALL || fold_mode == DecorationApplyMode::REPLACE_RANGE) {
    core.setFoldRegions(std::move(fold_regions));
  } else if (!fold_regions.empty()) {
    core.setFoldRegions(std::move(fold_regions));
  }

  editor_->flush();
}

void DecorationProviderManager::clearManagedDecorations() {
  if (editor_ == nullptr) {
    return;
  }

  auto& core = editor_->editorCore();
  core.clearHighlights();
  core.clearInlayHints();
  core.clearPhantomTexts();
  core.clearGutterIcons();
  core.clearGuides();
  core.clearDiagnostics();
  core.setFoldRegions(::sweeteditor::Vector<::sweeteditor::FoldRegion> {});
}

DecorationContext DecorationProviderManager::buildContext() {
  DecorationContext context;
  if (editor_ == nullptr) {
    return context;
  }

  const auto [visible_start_line, visible_end_line] = editor_->decorationVisibleLineRange();
  last_visible_start_line_ = visible_start_line;
  last_visible_end_line_ = visible_end_line;

  const int total_line_count = editor_->decorationTotalLineCount();
  int context_start_line = visible_start_line;
  int context_end_line = visible_end_line;
  if (total_line_count > 0 && visible_end_line >= visible_start_line) {
    const int overscan_lines = calculateOverscanLines(visible_start_line, visible_end_line);
    context_start_line = std::max(0, visible_start_line - overscan_lines);
    context_end_line = std::min(total_line_count - 1, visible_end_line + overscan_lines);
  }

  context.visible_start_line = context_start_line;
  context.visible_end_line = context_end_line;
  context.total_line_count = total_line_count;
  context.text_changes = pending_text_changes_;
  context.language_configuration = &editor_->languageConfiguration();
  context.editor_metadata = &editor_->metadata();
  context.document = editor_->decorationDocument();
  pending_text_changes_.clear();
  return context;
}

int DecorationProviderManager::calculateOverscanLines(int visible_start_line, int visible_end_line) const {
  const int viewport_line_count = visible_end_line >= visible_start_line
    ? (visible_end_line - visible_start_line + 1)
    : 0;
  if (viewport_line_count <= 0) {
    return 0;
  }

  return std::max(0, static_cast<int>(std::ceil(
    static_cast<double>(viewport_line_count) * decorationOverscanViewportMultiplier()
  )));
}

int DecorationProviderManager::scrollRefreshMinIntervalMs() const {
  if (editor_ == nullptr) {
    return 0;
  }
  return std::max(0, editor_->settings().decorationScrollRefreshMinIntervalMs());
}

float DecorationProviderManager::decorationOverscanViewportMultiplier() const {
  if (editor_ == nullptr) {
    return 0.0f;
  }
  return std::max(0.0f, editor_->settings().decorationOverscanViewportMultiplier());
}

DecorationApplyMode DecorationProviderManager::mergeMode(DecorationApplyMode current,
                                                         DecorationApplyMode next) {
  return modePriority(next) > modePriority(current) ? next : current;
}

int DecorationProviderManager::modePriority(DecorationApplyMode mode) {
  switch (mode) {
    case DecorationApplyMode::MERGE:
      return 0;
    case DecorationApplyMode::REPLACE_RANGE:
      return 1;
    case DecorationApplyMode::REPLACE_ALL:
      return 2;
  }

  return 0;
}

void DecorationProviderManager::appendStyleSpanMap(StyleSpanMap& target, const StyleSpanMap& source) {
  appendLineMap(target, source);
}

void DecorationProviderManager::appendInlayHintMap(InlayHintMap& target, const InlayHintMap& source) {
  appendLineMap(target, source);
}

void DecorationProviderManager::appendDiagnosticMap(DiagnosticMap& target, const DiagnosticMap& source) {
  appendLineMap(target, source);
}

void DecorationProviderManager::appendGutterIconMap(GutterIconMap& target, const GutterIconMap& source) {
  appendLineMap(target, source);
}

void DecorationProviderManager::appendPhantomTextMap(PhantomTextMap& target, const PhantomTextMap& source) {
  appendLineMap(target, source);
}

::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::StyleSpan>>>
DecorationProviderManager::buildBatchStyleEntries(const StyleSpanMap& map) {
  return toBatchEntries(map);
}

::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::InlayHint>>>
DecorationProviderManager::buildBatchInlayEntries(const InlayHintMap& map) {
  return toBatchEntries(map);
}

::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::DiagnosticSpan>>>
DecorationProviderManager::buildBatchDiagnosticEntries(const DiagnosticMap& map) {
  return toBatchEntries(map);
}

::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::GutterIcon>>>
DecorationProviderManager::buildBatchGutterEntries(const GutterIconMap& map) {
  return toBatchEntries(map);
}

::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::PhantomText>>>
DecorationProviderManager::buildBatchPhantomEntries(const PhantomTextMap& map) {
  return toBatchEntries(map);
}

template<typename T>
std::map<size_t, ::sweeteditor::Vector<T>> DecorationProviderManager::buildEmptyRangeMap(int start_line,
                                                                                          int end_line) {
  std::map<size_t, ::sweeteditor::Vector<T>> result;
  if (end_line < start_line) {
    return result;
  }

  for (int line = start_line; line <= end_line; ++line) {
    result.emplace(static_cast<size_t>(line), ::sweeteditor::Vector<T> {});
  }
  return result;
}

} // namespace sweeteditor::qt
