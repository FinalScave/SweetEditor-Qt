#pragma once

#include <DecorationProvider.h>

#include <QElapsedTimer>
#include <QTimer>

#include <unordered_map>
#include <vector>

namespace sweeteditor::qt {

class SweetEditorWidget;

class DecorationProviderManager {
public:
  explicit DecorationProviderManager(SweetEditorWidget* editor = nullptr);
  DecorationProviderManager(const DecorationProviderManager&) = delete;
  DecorationProviderManager& operator=(const DecorationProviderManager&) = delete;
  ~DecorationProviderManager();

  void bind(SweetEditorWidget* editor);
  void addProvider(DecorationProvider* provider);
  void removeProvider(DecorationProvider* provider);
  bool empty() const noexcept;

  void requestRefresh();
  void onDocumentLoaded();
  void onScrollChanged();
  void onTextChanged(const std::vector<::sweeteditor::TextChange>* changes);

private:
  struct ProviderState {
    DecorationResult snapshot;
    bool has_snapshot {false};
  };

  using StyleSpanMap = DecorationLineMap<::sweeteditor::StyleSpan>;
  using InlayHintMap = DecorationLineMap<::sweeteditor::InlayHint>;
  using DiagnosticMap = DecorationLineMap<::sweeteditor::DiagnosticSpan>;
  using GutterIconMap = DecorationLineMap<::sweeteditor::GutterIcon>;
  using PhantomTextMap = DecorationLineMap<::sweeteditor::PhantomText>;

  void scheduleRefresh(int delay_ms, const std::vector<::sweeteditor::TextChange>* changes);
  void scheduleScrollRefresh();
  void doRefresh();
  void applyMerged(bool clear_managed_first);
  void clearManagedDecorations();
  DecorationContext buildContext();
  int calculateOverscanLines(int visible_start_line, int visible_end_line) const;
  int scrollRefreshMinIntervalMs() const;
  float decorationOverscanViewportMultiplier() const;
  static DecorationApplyMode mergeMode(DecorationApplyMode current, DecorationApplyMode next);
  static int modePriority(DecorationApplyMode mode);
  static void appendStyleSpanMap(StyleSpanMap& target, const StyleSpanMap& source);
  static void appendInlayHintMap(InlayHintMap& target, const InlayHintMap& source);
  static void appendDiagnosticMap(DiagnosticMap& target, const DiagnosticMap& source);
  static void appendGutterIconMap(GutterIconMap& target, const GutterIconMap& source);
  static void appendPhantomTextMap(PhantomTextMap& target, const PhantomTextMap& source);
  static ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::StyleSpan>>> buildBatchStyleEntries(
    const StyleSpanMap& map);
  static ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::InlayHint>>> buildBatchInlayEntries(
    const InlayHintMap& map);
  static ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::DiagnosticSpan>>> buildBatchDiagnosticEntries(
    const DiagnosticMap& map);
  static ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::GutterIcon>>> buildBatchGutterEntries(
    const GutterIconMap& map);
  static ::sweeteditor::Vector<std::pair<size_t, ::sweeteditor::Vector<::sweeteditor::PhantomText>>> buildBatchPhantomEntries(
    const PhantomTextMap& map);
  template<typename T>
  static std::map<size_t, ::sweeteditor::Vector<T>> buildEmptyRangeMap(int start_line, int end_line);

  SweetEditorWidget* editor_ {nullptr};
  std::vector<DecorationProvider*> providers_;
  std::unordered_map<DecorationProvider*, ProviderState> states_;
  std::vector<::sweeteditor::TextChange> pending_text_changes_;
  QTimer* debounce_timer_ {nullptr};
  QTimer* scroll_refresh_timer_ {nullptr};
  bool scroll_refresh_scheduled_ {false};
  bool pending_scroll_refresh_ {false};
  int last_visible_start_line_ {0};
  int last_visible_end_line_ {-1};
  QElapsedTimer scroll_refresh_clock_;
};

} // namespace sweeteditor::qt
