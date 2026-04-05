#include "EditorRenderer.h"

#include <EditorIconProvider.h>
#include <Perf.h>

#include <algorithm>
#include <cmath>
#include <QBrush>
#include <QColor>
#include <QDebug>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QString>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace {

QString toQString(const ::sweeteditor::U16String& text) {
#ifdef _WIN32
  return QString::fromWCharArray(text.data(), static_cast<qsizetype>(text.size()));
#else
  return QString::fromUtf16(reinterpret_cast<const char16_t*>(text.data()), static_cast<qsizetype>(text.size()));
#endif
}

QColor colorFromArgb(int32_t argb, const QColor& fallback = {}) {
  if (argb == 0) {
    return fallback;
  }

  const auto packed = static_cast<uint32_t>(argb);
  return QColor::fromRgb(
    static_cast<int>((packed >> 16) & 0xFFU),
    static_cast<int>((packed >> 8) & 0xFFU),
    static_cast<int>(packed & 0xFFU),
    static_cast<int>((packed >> 24) & 0xFFU)
  );
}

QRectF toQRectF(const ::sweeteditor::Rect& rect) {
  return {
    rect.origin.x,
    rect.origin.y,
    rect.width,
    rect.height,
  };
}

QColor diagnosticColor(int32_t severity, const sweeteditor::qt::EditorTheme& theme, int32_t override_color) {
  if (override_color != 0) {
    return colorFromArgb(override_color);
  }

  switch (severity) {
    case 0: return theme.diagnostic_error;
    case 1: return theme.diagnostic_warning;
    case 2: return theme.diagnostic_info;
    case 3: return theme.diagnostic_hint;
    default: return theme.diagnostic_hint;
  }
}

QFont applyFontStyle(QFont font, int32_t font_style) {
  font.setBold((font_style & ::sweeteditor::FONT_STYLE_BOLD) != 0);
  font.setItalic((font_style & ::sweeteditor::FONT_STYLE_ITALIC) != 0);
  font.setStrikeOut((font_style & ::sweeteditor::FONT_STYLE_STRIKETHROUGH) != 0);
  return font;
}

QFont styledFont(const QFont& base_font, int32_t font_style) {
  QFont font(base_font);
  return applyFontStyle(font, font_style);
}

QFont inlayFont(const QFont& base_font, int32_t font_style) {
  QFont font(base_font);
  if (font.pointSizeF() > 0.0) {
    font.setPointSizeF(std::max(1.0, font.pointSizeF() * 0.92));
  } else if (font.pixelSize() > 0) {
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * 0.92))));
  }
  return applyFontStyle(font, font_style);
}

QColor scaledAlpha(QColor color, float factor) {
  color.setAlphaF(std::clamp(color.alphaF() * static_cast<qreal>(factor), 0.0, 1.0));
  return color;
}

void fillRoundedRect(QPainter& painter, const QRectF& rect, qreal radius, const QColor& color) {
  if (!rect.isValid() || rect.isEmpty() || !color.isValid() || color.alpha() == 0) {
    return;
  }

  QPainterPath path;
  path.addRoundedRect(rect, radius, radius);
  painter.fillPath(path, color);
}

QColor currentLineAccentColor(const sweeteditor::qt::EditorTheme& theme) {
  return theme.current_line_number.isValid() ? theme.current_line_number : theme.gutter_foreground;
}

bool paintGutterIcon(QPainter& painter,
                     const QRectF& rect,
                     int32_t icon_id,
                     const QColor& tint,
                     const sweeteditor::qt::EditorIconProvider* icon_provider) {
  return icon_provider != nullptr && icon_provider->paintIcon(painter, icon_id, rect, tint);
}

void drawArrowHead(QPainter& painter,
                   const QColor& color,
                   const QPointF& from,
                   const QPointF& to,
                   qreal arrow_len,
                   qreal arrow_angle) {
  const qreal dx = to.x() - from.x();
  const qreal dy = to.y() - from.y();
  const qreal length = std::sqrt((dx * dx) + (dy * dy));
  if (length < 1.0) {
    return;
  }

  const qreal ux = dx / length;
  const qreal uy = dy / length;
  const qreal cos_a = std::cos(arrow_angle);
  const qreal sin_a = std::sin(arrow_angle);
  const QPointF p1(
    to.x() - (arrow_len * ((ux * cos_a) - (uy * sin_a))),
    to.y() - (arrow_len * ((uy * cos_a) + (ux * sin_a)))
  );
  const QPointF p2(
    to.x() - (arrow_len * ((ux * cos_a) + (uy * sin_a))),
    to.y() - (arrow_len * ((uy * cos_a) - (ux * sin_a)))
  );

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawPolygon(QPolygonF {to, p1, p2});
  painter.restore();
}

void drawGuideSegment(QPainter& painter,
                      const ::sweeteditor::GuideSegment& guide,
                      const sweeteditor::qt::EditorTheme& theme) {
  const QColor color = guide.type == ::sweeteditor::GuideType::SEPARATOR ? theme.separator : theme.guide;
  const qreal line_width = guide.type == ::sweeteditor::GuideType::INDENT ? 1.0 : 1.2;

  painter.save();
  QPen pen(color, line_width);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  if (guide.style == ::sweeteditor::GuideStyle::DASHED) {
    pen.setStyle(Qt::DashLine);
  }
  painter.setPen(pen);

  const QPointF start(guide.start.x, guide.start.y);
  const QPointF end(guide.end.x, guide.end.y);
  if (guide.style == ::sweeteditor::GuideStyle::DOUBLE) {
    const qreal dx = end.x() - start.x();
    const qreal dy = end.y() - start.y();
    const qreal length = std::sqrt((dx * dx) + (dy * dy));
    QPointF normal(0.0, 0.0);
    if (length >= 1.0) {
      normal = QPointF((-dy / length) * 1.5, (dx / length) * 1.5);
    }
    painter.drawLine(start + normal, end + normal);
    painter.drawLine(start - normal, end - normal);
  } else if (guide.arrow_end) {
    const qreal arrow_angle = M_PI * 28.0 / 180.0;
    const qreal arrow_len = guide.type == ::sweeteditor::GuideType::FLOW ? 9.0 : 8.0;
    const qreal arrow_depth = arrow_len * std::cos(arrow_angle);
    const qreal dx = end.x() - start.x();
    const qreal dy = end.y() - start.y();
    const qreal length = std::sqrt((dx * dx) + (dy * dy));
    const qreal trim = arrow_depth + (line_width * 0.5);
    if (length > trim) {
      const qreal ratio = (length - trim) / length;
      painter.drawLine(start, QPointF(start.x() + (dx * ratio), start.y() + (dy * ratio)));
    }
    drawArrowHead(painter, color, start, end, arrow_len, arrow_angle);
  } else {
    painter.drawLine(start, end);
  }
  painter.restore();
}

void drawDiagnosticDecoration(QPainter& painter,
                              const QRectF& rect,
                              const QColor& color,
                              int32_t severity) {
  if (!rect.isValid() || rect.isEmpty()) {
    return;
  }

  painter.save();
  QPen pen(color, severity == 3 ? 2.0 : 3.0);
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);

  const qreal start_x = rect.left();
  const qreal end_x = rect.right();
  const qreal base_y = rect.bottom() - 1.0;

  if (severity == 3) {
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(QPointF(start_x, base_y), QPointF(end_x, base_y));
    painter.restore();
    return;
  }

  QPainterPath path(QPointF(start_x, base_y));
  constexpr qreal half_wave = 7.0;
  constexpr qreal amplitude = 3.5;
  qreal x = start_x;
  int step = 0;
  while (x < end_x) {
    const qreal next_x = std::min(x + half_wave, end_x);
    const qreal mid_x = (x + next_x) * 0.5;
    const qreal peak_y = (step % 2 == 0) ? (base_y - amplitude) : (base_y + amplitude);
    const qreal c1x = x + ((2.0 / 3.0) * (mid_x - x));
    const qreal c1y = base_y + ((2.0 / 3.0) * (peak_y - base_y));
    const qreal c2x = next_x + ((2.0 / 3.0) * (mid_x - next_x));
    const qreal c2y = base_y + ((2.0 / 3.0) * (peak_y - base_y));
    path.cubicTo(QPointF(c1x, c1y), QPointF(c2x, c2y), QPointF(next_x, base_y));
    x = next_x;
    ++step;
  }
  painter.drawPath(path);
  painter.restore();
}

void drawSelectionHandle(QPainter& painter,
                         const ::sweeteditor::SelectionHandle& handle,
                         const QColor& color) {
  if (!handle.visible) {
    return;
  }

  painter.save();
  painter.setPen(QPen(color, 1.5));
  painter.drawLine(
    QPointF(handle.position.x, handle.position.y - handle.height),
    QPointF(handle.position.x, handle.position.y)
  );
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  painter.drawEllipse(QPointF(handle.position.x, handle.position.y), 4.0, 4.0);
  painter.restore();
}

void drawFoldMarker(QPainter& painter,
                    const QRectF& rect,
                    ::sweeteditor::FoldState fold_state,
                    const QColor& color) {
  if (!rect.isValid() || rect.isEmpty() || fold_state == ::sweeteditor::FoldState::NONE) {
    return;
  }

  const qreal center_x = rect.left() + (rect.width() * 0.5);
  const qreal center_y = rect.top() + (rect.height() * 0.5);
  const qreal half_size = std::min(rect.width(), rect.height()) * 0.28;

  QPainterPath path;
  if (fold_state == ::sweeteditor::FoldState::COLLAPSED) {
    path.moveTo(center_x - (half_size * 0.5), center_y - half_size);
    path.lineTo(center_x + (half_size * 0.5), center_y);
    path.lineTo(center_x - (half_size * 0.5), center_y + half_size);
  } else {
    path.moveTo(center_x - half_size, center_y - (half_size * 0.5));
    path.lineTo(center_x, center_y + (half_size * 0.5));
    path.lineTo(center_x + half_size, center_y - (half_size * 0.5));
  }

  painter.save();
  QPen pen(color, std::max<qreal>(1.0, rect.height() * 0.1));
  pen.setCapStyle(Qt::RoundCap);
  pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(pen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);
  painter.restore();
}

void drawCurrentLineDecoration(QPainter& painter,
                               float top,
                               float width,
                               float line_height,
                               ::sweeteditor::CurrentLineRenderMode mode,
                               const sweeteditor::qt::EditorTheme& theme) {
  if (mode == ::sweeteditor::CurrentLineRenderMode::NONE || width <= 0.0f || line_height <= 0.0f) {
    return;
  }

  const QRectF rect(0.0, top, width, line_height);
  if (mode == ::sweeteditor::CurrentLineRenderMode::BACKGROUND) {
    painter.fillRect(rect, theme.current_line);
    return;
  }

  painter.save();
  painter.setBrush(Qt::NoBrush);
  painter.setPen(QPen(theme.current_line, 1.0));
  painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
  painter.restore();
}

} // namespace

namespace sweeteditor::qt {

void EditorRenderer::setPerfOverlayEnabled(bool enabled) {
  perf_overlay_.setEnabled(enabled);
}

bool EditorRenderer::isPerfOverlayEnabled() const noexcept {
  return perf_overlay_.isEnabled();
}

MeasurePerfStats& EditorRenderer::perfMeasureStats() noexcept {
  return perf_measure_stats_;
}

const MeasurePerfStats& EditorRenderer::perfMeasureStats() const noexcept {
  return perf_measure_stats_;
}

PerfOverlay& EditorRenderer::perfOverlay() noexcept {
  return perf_overlay_;
}

const PerfOverlay& EditorRenderer::perfOverlay() const noexcept {
  return perf_overlay_;
}

void EditorRenderer::resetFontCaches(const QFont& base_font) {
  font_cache_initialized_ = true;
  cached_base_font_ = base_font;
  text_font_cache_.clear();
  inlay_font_cache_.clear();
}

const EditorRenderer::FontCacheEntry& EditorRenderer::fontCacheEntry(const QFont& base_font,
                                                                     int32_t font_style,
                                                                     bool inlay) {
  if (!font_cache_initialized_ || cached_base_font_ != base_font) {
    resetFontCaches(base_font);
  }

  auto& cache = inlay ? inlay_font_cache_ : text_font_cache_;
  auto it = cache.find(font_style);
  if (it == cache.end()) {
    QFont font = inlay ? inlayFont(base_font, font_style) : styledFont(base_font, font_style);
    it = cache.emplace(font_style, FontCacheEntry(std::move(font))).first;
  }
  return it->second;
}

void EditorRenderer::paint(QPainter& painter,
                           const QRect& bounds,
                           const EditorTheme& theme,
                           const ::sweeteditor::EditorRenderModel& model,
                           const QFont& base_font,
                           const EditorIconProvider* icon_provider,
                           bool selection_handles_enabled) {
  PerfStepRecorder draw_perf = PerfStepRecorder::start();
  const bool perf_enabled = perf_overlay_.isEnabled();
  auto mark_step = [&](const char* step_name) {
    if (perf_enabled) {
      draw_perf.mark(step_name);
    }
  };

  painter.save();
  painter.setClipRect(bounds);
  painter.fillRect(bounds, theme.background);
  mark_step(PerfStepRecorder::STEP_CLEAR);

  std::unordered_map<const ::sweeteditor::U16String*, QString> text_cache;
  text_cache.reserve(128);
  auto cachedQString = [&text_cache](const ::sweeteditor::U16String& text) -> const QString& {
    auto it = text_cache.find(&text);
    if (it == text_cache.end()) {
      it = text_cache.emplace(&text, toQString(text)).first;
    }
    return it->second;
  };

  std::unordered_set<size_t> gutter_icon_lines;
  if (icon_provider != nullptr && !model.gutter_icons.empty()) {
    gutter_icon_lines.reserve(model.gutter_icons.size());
    for (const auto& icon : model.gutter_icons) {
      gutter_icon_lines.insert(icon.logical_line);
    }
  }

  const QFontMetricsF base_metrics(base_font);
  const float default_line_height = model.cursor.height > 0.0f
    ? model.cursor.height
    : static_cast<float>(base_metrics.height());
  drawCurrentLineDecoration(
    painter,
    model.current_line.y,
    model.viewport_width,
    default_line_height,
    model.current_line_render_mode,
    theme
  );
  mark_step(PerfStepRecorder::STEP_CURRENT);

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(theme.selection);
  for (const auto& rect : model.selection_rects) {
    painter.drawRect(toQRectF(rect));
  }
  painter.restore();
  mark_step(PerfStepRecorder::STEP_SELECTION);

  for (const auto& line : model.lines) {
    for (const auto& run : line.runs) {
      const bool is_inlay = run.type == ::sweeteditor::VisualRunType::INLAY_HINT;
      const auto& font_entry = fontCacheEntry(base_font, run.style.font_style, is_inlay);
      const QFont& font = font_entry.font;
      const QFontMetricsF& metrics = font_entry.metrics;
      const QRectF background_rect(
        run.x,
        run.y - metrics.ascent(),
        run.width,
        metrics.height()
      );

      if (run.type == ::sweeteditor::VisualRunType::FOLD_PLACEHOLDER) {
        const QRectF pill_rect(
          run.x + run.margin,
          background_rect.top(),
          std::max(0.0f, run.width - (run.margin * 2.0f)),
          background_rect.height()
        );
        fillRoundedRect(painter, pill_rect, pill_rect.height() * 0.2, theme.fold_placeholder_background);
        if (!run.text.empty()) {
          painter.setFont(font);
          painter.setPen(theme.fold_placeholder_text);
          painter.drawText(QPointF(run.x + run.margin + run.padding, run.y), cachedQString(run.text));
        }
        continue;
      }

      if (is_inlay) {
        const QRectF pill_rect(
          run.x + run.margin,
          background_rect.top(),
          std::max(0.0f, run.width - (run.margin * 2.0f)),
          background_rect.height()
        );

        if (run.color_value != 0) {
          const qreal block_size = std::min(pill_rect.width(), pill_rect.height());
          const QRectF block_rect(pill_rect.left(), pill_rect.top(), block_size, block_size);
          painter.fillRect(block_rect, colorFromArgb(run.color_value, theme.accent));
          continue;
        }

        fillRoundedRect(painter, pill_rect, pill_rect.height() * 0.2, theme.inlay_hint_background);

        if (run.icon_id != 0 && run.text.empty()) {
          const qreal icon_size = std::min(pill_rect.width(), pill_rect.height());
          const QRectF icon_rect(
            pill_rect.left() + ((pill_rect.width() - icon_size) * 0.5),
            pill_rect.top() + ((pill_rect.height() - icon_size) * 0.5),
            icon_size,
            icon_size
          );
          bool painted = false;
          if (icon_provider != nullptr) {
            painted = icon_provider->paintIcon(painter, run.icon_id, icon_rect, theme.inlay_hint_icon);
          }
          if (!painted) {
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(theme.inlay_hint_icon);
            painter.drawEllipse(icon_rect.adjusted(2.0, 2.0, -2.0, -2.0));
            painter.restore();
          }
          continue;
        }

        if (!run.text.empty()) {
          painter.setFont(font);
          painter.setPen(colorFromArgb(run.style.color, theme.inlay_hint_text));
          painter.drawText(QPointF(run.x + run.margin + run.padding, run.y), cachedQString(run.text));
        }
        continue;
      }

      const QColor background = colorFromArgb(run.style.background_color);
      if (background.isValid() && background.alpha() > 0) {
        painter.fillRect(background_rect, background);
      }

      if (run.text.empty()) {
        continue;
      }

      painter.setFont(font);
      QColor foreground = colorFromArgb(run.style.color, theme.foreground);
      if (run.type == ::sweeteditor::VisualRunType::PHANTOM_TEXT) {
        foreground = theme.phantom_text;
      }
      painter.setPen(foreground);
      painter.drawText(QPointF(run.x, run.y), cachedQString(run.text));
    }
  }
  mark_step(PerfStepRecorder::STEP_LINES);

  painter.save();
  painter.setBrush(Qt::NoBrush);
  for (const auto& guide : model.guide_segments) {
    drawGuideSegment(painter, guide, theme);
  }
  painter.restore();
  mark_step(PerfStepRecorder::STEP_GUIDES);

  if (model.composition_decoration.active) {
    painter.setPen(QPen(theme.composition, 1.5));
    const QRectF rect = toQRectF(model.composition_decoration.rect);
    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
  }
  mark_step(PerfStepRecorder::STEP_COMPOSITION);

  for (const auto& diagnostic : model.diagnostic_decorations) {
    const QRectF rect = toQRectF(diagnostic.rect);
    drawDiagnosticDecoration(
      painter,
      rect,
      diagnosticColor(diagnostic.severity, theme, diagnostic.color),
      diagnostic.severity
    );
  }
  mark_step(PerfStepRecorder::STEP_DIAGNOSTICS);

  painter.setBrush(Qt::NoBrush);
  for (const auto& rect : model.linked_editing_rects) {
    const QColor stroke = rect.is_active ? theme.linked_editing_active : theme.linked_editing_inactive;
    painter.setPen(QPen(stroke, rect.is_active ? 2.0 : 1.0));
    if (rect.is_active) {
      QColor fill = scaledAlpha(theme.linked_editing_active, 0.18f);
      painter.fillRect(toQRectF(rect.rect), fill);
    }
    painter.drawRect(toQRectF(rect.rect));
  }
  mark_step(PerfStepRecorder::STEP_LINKED);
  for (const auto& rect : model.bracket_highlight_rects) {
    painter.fillRect(toQRectF(rect), theme.bracket_highlight_background);
    painter.setPen(QPen(theme.bracket_highlight_border, 1.5));
    painter.drawRect(toQRectF(rect));
  }
  mark_step(PerfStepRecorder::STEP_BRACKET);

  if (model.cursor.visible) {
    painter.fillRect(
      QRectF(
        model.cursor.position.x,
        model.cursor.position.y,
        2.0,
        std::max(model.cursor.height, default_line_height)
      ),
      theme.cursor
    );
  }

  if (selection_handles_enabled) {
    drawSelectionHandle(painter, model.selection_start_handle, theme.cursor);
    drawSelectionHandle(painter, model.selection_end_handle, theme.cursor);
  }
  mark_step(PerfStepRecorder::STEP_CURSOR);

  if (model.gutter_visible && model.split_x > 0.0f) {
    painter.fillRect(QRectF(0.0, 0.0, model.split_x, model.viewport_height), theme.background);
    drawCurrentLineDecoration(
      painter,
      model.current_line.y,
      model.split_x,
      default_line_height,
      model.current_line_render_mode,
      theme
    );
    if (model.split_line_visible) {
      painter.setPen(QPen(theme.split_line, 1.0));
      painter.drawLine(QPointF(model.split_x, 0.0), QPointF(model.split_x, model.viewport_height));
    }
  }
  mark_step(PerfStepRecorder::STEP_GUTTER);

  const size_t active_logical_line = model.cursor.text_position.line;
  const bool overlay_icon_mode = model.max_gutter_icons == 0;
  painter.setFont(base_font);
  const QFontMetricsF line_number_metrics(base_font);
  for (const auto& line : model.lines) {
    if (line.wrap_index != 0 || line.is_phantom_line) {
      continue;
    }

    const QColor line_color = line.logical_line == active_logical_line
      ? currentLineAccentColor(theme)
      : theme.gutter_foreground;

    const bool has_icons = gutter_icon_lines.find(line.logical_line) != gutter_icon_lines.end();
    if (overlay_icon_mode && has_icons) {
      continue;
    }

    painter.setPen(line_color);
    const QRectF line_number_rect(
      line.line_number_position.x,
      line.line_number_position.y - line_number_metrics.ascent(),
      120.0,
      std::ceil(line_number_metrics.height())
    );
    painter.drawText(line_number_rect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(static_cast<qulonglong>(line.logical_line + 1)));
  }
  mark_step(PerfStepRecorder::STEP_LINE_NO);

  if (icon_provider != nullptr) {
    size_t last_overlay_icon_line = static_cast<size_t>(-1);
    for (const auto& icon : model.gutter_icons) {
      if (overlay_icon_mode && icon.logical_line == last_overlay_icon_line) {
        continue;
      }

      const QRectF rect = toQRectF(icon.rect);
      const QColor tint = icon.logical_line == active_logical_line
        ? currentLineAccentColor(theme)
        : theme.gutter_foreground;
      paintGutterIcon(painter, rect, icon.icon_id, tint, icon_provider);
      if (overlay_icon_mode) {
        last_overlay_icon_line = icon.logical_line;
      }
    }
  }

  for (const auto& marker : model.fold_markers) {
    const QRectF rect = toQRectF(marker.rect).adjusted(3.0, 3.0, -3.0, -3.0);
    const QColor tint = marker.logical_line == active_logical_line
      ? currentLineAccentColor(theme)
      : theme.gutter_foreground;
    drawFoldMarker(painter, rect, marker.fold_state, tint);
  }
  mark_step(PerfStepRecorder::STEP_POPUP);

  const auto vertical_track = toQRectF(model.vertical_scrollbar.track);
  const auto vertical_thumb = toQRectF(model.vertical_scrollbar.thumb);
  const auto horizontal_track = toQRectF(model.horizontal_scrollbar.track);
  const auto horizontal_thumb = toQRectF(model.horizontal_scrollbar.thumb);
  const bool has_vertical_scrollbar = model.vertical_scrollbar.visible
    && vertical_track.isValid()
    && !vertical_track.isEmpty()
    && vertical_thumb.isValid()
    && !vertical_thumb.isEmpty();
  const bool has_horizontal_scrollbar = model.horizontal_scrollbar.visible
    && horizontal_track.isValid()
    && !horizontal_track.isEmpty()
    && horizontal_thumb.isValid()
    && !horizontal_thumb.isEmpty();

  auto paintScrollbar = [&painter, &theme](const ::sweeteditor::ScrollbarModel& scrollbar) {
    const QColor track = theme.scrollbar_track;
    const QColor thumb = scrollbar.thumb_active ? theme.scrollbar_thumb_active : theme.scrollbar_thumb;

    painter.fillRect(toQRectF(scrollbar.track), track);
    painter.fillRect(toQRectF(scrollbar.thumb), thumb);
  };

  if (has_vertical_scrollbar) {
    paintScrollbar(model.vertical_scrollbar);
  }
  if (has_horizontal_scrollbar) {
    paintScrollbar(model.horizontal_scrollbar);
  }
  if (has_vertical_scrollbar && has_horizontal_scrollbar) {
    painter.fillRect(
      QRectF(vertical_track.x(), horizontal_track.y(), vertical_track.width(), horizontal_track.height()),
      theme.scrollbar_track
    );
  }
  painter.restore();
  mark_step(PerfStepRecorder::STEP_SCROLLBAR);

  if (perf_enabled) {
    draw_perf.finish();
    perf_overlay_.recordDraw(draw_perf);
    if (draw_perf.totalMs() >= PerfOverlay::WARN_PAINT_MS
        || draw_perf.anyStepOver(PerfOverlay::WARN_PAINT_STEP_MS)) {
      qDebug().noquote()
        << QStringLiteral("[PERF][Paint] total=%1ms clear=%2 current=%3 selection=%4 lines=%5 guides=%6 comp=%7 diag=%8 linked=%9 bracket=%10 cursor=%11 gutter=%12 lineNo=%13 scrollbar=%14 popup=%15")
             .arg(draw_perf.totalMs(), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_CLEAR), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_CURRENT), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_SELECTION), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_LINES), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_GUIDES), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_COMPOSITION), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_DIAGNOSTICS), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_LINKED), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_BRACKET), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_CURSOR), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_GUTTER), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_LINE_NO), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_SCROLLBAR), 0, 'f', 2)
             .arg(draw_perf.getStepMs(PerfStepRecorder::STEP_POPUP), 0, 'f', 2);
    }
    perf_overlay_.draw(painter, bounds.width());
  }
}

} // namespace sweeteditor::qt
