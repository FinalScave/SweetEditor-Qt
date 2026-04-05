#include "QtTextMeasurer.h"

#include <QFontMetricsF>
#include <QString>

#include <utility>

namespace {

QString toQString(const U16String& text) {
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

float QtTextMeasurer::measureWidth(const U16String& text, int32_t font_style) {
  QFont font = base_font_;
  font.setBold((font_style & 0x01) != 0);
  font.setItalic((font_style & 0x02) != 0);
  return measure(text, font);
}

float QtTextMeasurer::measureInlayHintWidth(const U16String& text) {
  QFont font = base_font_;
  font.setPointSizeF(font.pointSizeF() * 0.92);
  return measure(text, font);
}

float QtTextMeasurer::measureIconWidth(int32_t) {
  return 0.0f;
}

::sweeteditor::FontMetrics QtTextMeasurer::getFontMetrics() {
  const QFontMetricsF metrics(base_font_);
  return {
    static_cast<float>(metrics.ascent()),
    static_cast<float>(metrics.descent()),
  };
}

float QtTextMeasurer::measure(const U16String& text, const QFont& font) const {
  const QFontMetricsF metrics(font);
  return static_cast<float>(metrics.horizontalAdvance(toQString(text)));
}

} // namespace sweeteditor::qt
