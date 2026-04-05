#pragma once

#include <QFont>

#include <sweeteditor/layout.h>

namespace sweeteditor::qt {

class QtTextMeasurer final : public ::sweeteditor::TextMeasurer {
public:
  explicit QtTextMeasurer(QFont base_font = {});

  void setBaseFont(const QFont& font);
  const QFont& baseFont() const noexcept;

  float measureWidth(const U16String& text, int32_t font_style) override;
  float measureInlayHintWidth(const U16String& text) override;
  float measureIconWidth(int32_t icon_id) override;
  ::sweeteditor::FontMetrics getFontMetrics() override;

private:
  float measure(const U16String& text, const QFont& font) const;

  QFont base_font_;
};

} // namespace sweeteditor::qt
