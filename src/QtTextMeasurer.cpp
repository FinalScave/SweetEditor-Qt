#include "QtTextMeasurer.h"

#include <QFontMetricsF>
#include <QString>

#include <chrono>
#include <utility>

namespace {

QString toQString(const ::sweeteditor::U16String& text) {
#ifdef _WIN32
  return QString::fromWCharArray(text.data(), static_cast<qsizetype>(text.size()));
#else
  return QString::fromUtf16(reinterpret_cast<const char16_t*>(text.data()), static_cast<qsizetype>(text.size()));
#endif
}

} // namespace

namespace sweeteditor::qt {

QtTextMeasurer::QtTextMeasurer(QFont base_font)
  : base_font_(std::move(base_font)) {}

void QtTextMeasurer::setBaseFont(const QFont& font) {
  base_font_ = font;
}

const QFont& QtTextMeasurer::baseFont() const noexcept {
  return base_font_;
}

void QtTextMeasurer::setPerfStats(MeasurePerfStats* stats) noexcept {
  perf_stats_ = stats;
}

MeasurePerfStats* QtTextMeasurer::perfStats() const noexcept {
  return perf_stats_;
}

float QtTextMeasurer::measureWidth(const U16String& text, int32_t font_style) {
  const auto start = std::chrono::steady_clock::now();
  QFont font = base_font_;
  font.setBold((font_style & 0x01) != 0);
  font.setItalic((font_style & 0x02) != 0);
  const float width = measure(text, font);
  if (perf_stats_ != nullptr) {
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start
    ).count();
    perf_stats_->recordText(elapsed_ms, static_cast<int>(text.size()), font_style);
  }
  return width;
}

float QtTextMeasurer::measureInlayHintWidth(const U16String& text) {
  const auto start = std::chrono::steady_clock::now();
  QFont font = base_font_;
  font.setPointSizeF(font.pointSizeF() * 0.92);
  const float width = measure(text, font);
  if (perf_stats_ != nullptr) {
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start
    ).count();
    perf_stats_->recordInlay(elapsed_ms, static_cast<int>(text.size()));
  }
  return width;
}

float QtTextMeasurer::measureIconWidth(int32_t) {
  const auto start = std::chrono::steady_clock::now();
  const float width = static_cast<float>(QFontMetricsF(base_font_).height());
  if (perf_stats_ != nullptr) {
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start
    ).count();
    perf_stats_->recordIcon(elapsed_ms, 0);
  }
  return width;
}

::sweeteditor::FontMetrics QtTextMeasurer::getFontMetrics() {
  const QFontMetricsF metrics(base_font_);
  return {
    static_cast<float>(-metrics.ascent()),
    static_cast<float>(metrics.descent()),
  };
}

float QtTextMeasurer::measure(const U16String& text, const QFont& font) const {
  const QFontMetricsF metrics(font);
  return static_cast<float>(metrics.horizontalAdvance(toQString(text)));
}

} // namespace sweeteditor::qt
