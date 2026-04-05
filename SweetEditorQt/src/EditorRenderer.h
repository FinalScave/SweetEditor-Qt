#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QRect>

#include <EditorTheme.h>
#include <Perf.h>

#include <cstdint>
#include <unordered_map>

#include <sweeteditor/visual.h>

class QPainter;

namespace sweeteditor::qt {

class EditorIconProvider;

class EditorRenderer {
public:
  void setPerfOverlayEnabled(bool enabled);
  bool isPerfOverlayEnabled() const noexcept;

  MeasurePerfStats& perfMeasureStats() noexcept;
  const MeasurePerfStats& perfMeasureStats() const noexcept;

  PerfOverlay& perfOverlay() noexcept;
  const PerfOverlay& perfOverlay() const noexcept;

  void paint(QPainter& painter,
             const QRect& bounds,
             const EditorTheme& theme,
             const ::sweeteditor::EditorRenderModel& model,
             const QFont& base_font,
             const EditorIconProvider* icon_provider,
             bool selection_handles_enabled);

private:
  struct FontCacheEntry {
    QFont font;
    QFontMetricsF metrics;

    explicit FontCacheEntry(QFont value)
      : font(std::move(value))
      , metrics(font) {
    }
  };

  void resetFontCaches(const QFont& base_font);
  const FontCacheEntry& fontCacheEntry(const QFont& base_font, int32_t font_style, bool inlay);

  bool font_cache_initialized_ {false};
  QFont cached_base_font_;
  std::unordered_map<int32_t, FontCacheEntry> text_font_cache_;
  std::unordered_map<int32_t, FontCacheEntry> inlay_font_cache_;
  MeasurePerfStats perf_measure_stats_;
  PerfOverlay perf_overlay_;
};

} // namespace sweeteditor::qt
