#include <Perf.h>

#include <QFontMetrics>
#include <QPainter>
#include <QStringList>

#include <algorithm>

namespace {

constexpr int kMargin = 8;
constexpr int kPaddingH = 10;
constexpr int kPaddingV = 8;
constexpr int kLineSpacing = 2;

bool isWarnLine(const QString& line) {
  return line.contains(QStringLiteral("SLOW")) || line.contains(QLatin1Char('!'));
}

QStringList wrapText(const QFontMetrics& metrics, int max_width, const QString& text) {
  if (text.isEmpty()) {
    return {};
  }
  if (metrics.horizontalAdvance(text) <= max_width) {
    return {text};
  }

  QStringList lines;
  QString current;
  const QStringList words = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
  for (const QString& word : words) {
    const QString candidate = current.isEmpty() ? word : (current + QLatin1Char(' ') + word);
    if (!current.isEmpty() && metrics.horizontalAdvance(candidate) > max_width) {
      lines.push_back(current);
      current = QStringLiteral("  ") + word;
    } else {
      current = candidate;
    }
  }
  if (!current.isEmpty()) {
    lines.push_back(current);
  }
  return lines;
}

void appendStepLines(QStringList& lines,
                     const QFontMetrics& metrics,
                     int max_width,
                     const QString& prefix,
                     const sweeteditor::qt::PerfStepRecorder* recorder) {
  if (recorder == nullptr || recorder->stepCount() == 0) {
    return;
  }

  const QString continuation_prefix = QStringLiteral("  ");
  QString current = prefix;
  for (int i = 0; i < recorder->stepCount(); ++i) {
    const double step_ms = recorder->stepMsByIndex(i);
    QString entry = QStringLiteral("%1=%2").arg(recorder->stepName(i)).arg(step_ms, 0, 'f', 1);
    if (step_ms >= sweeteditor::qt::PerfOverlay::WARN_PAINT_STEP_MS) {
      entry += QLatin1Char('!');
    }

    const QString candidate = current.length() <= prefix.length()
      ? current + entry
      : current + QStringLiteral(" ") + entry;
    if (current.length() > prefix.length() && metrics.horizontalAdvance(candidate) > max_width) {
      lines.push_back(current);
      current = continuation_prefix + entry;
    } else {
      if (current.length() > prefix.length() && current.length() > continuation_prefix.length()) {
        current += QLatin1Char(' ');
      }
      current += entry;
    }
  }

  if (!current.isEmpty()) {
    lines.push_back(current);
  }
}

} // namespace

namespace sweeteditor::qt {

PerfOverlay::~PerfOverlay() {
  delete last_build_perf_;
  delete last_draw_perf_;
}

void MeasurePerfStats::reset() {
  text_count_ = 0;
  text_ms_total_ = 0.0;
  text_ms_max_ = 0.0;
  text_max_len_ = 0;
  text_max_style_ = 0;
  inlay_count_ = 0;
  inlay_ms_total_ = 0.0;
  inlay_ms_max_ = 0.0;
  inlay_max_len_ = 0;
  icon_count_ = 0;
  icon_ms_total_ = 0.0;
  icon_ms_max_ = 0.0;
  icon_max_id_ = 0;
}

void MeasurePerfStats::recordText(double elapsed_ms, int text_len, int font_style) {
  ++text_count_;
  text_ms_total_ += elapsed_ms;
  if (elapsed_ms > text_ms_max_) {
    text_ms_max_ = elapsed_ms;
    text_max_len_ = text_len;
    text_max_style_ = font_style;
  }
}

void MeasurePerfStats::recordInlay(double elapsed_ms, int text_len) {
  ++inlay_count_;
  inlay_ms_total_ += elapsed_ms;
  if (elapsed_ms > inlay_ms_max_) {
    inlay_ms_max_ = elapsed_ms;
    inlay_max_len_ = text_len;
  }
}

void MeasurePerfStats::recordIcon(double elapsed_ms, int icon_id) {
  ++icon_count_;
  icon_ms_total_ += elapsed_ms;
  if (elapsed_ms > icon_ms_max_) {
    icon_ms_max_ = elapsed_ms;
    icon_max_id_ = icon_id;
  }
}

bool MeasurePerfStats::shouldLogBuild() const {
  return text_ms_total_ >= 2.0 || inlay_ms_total_ >= 1.0;
}

QString MeasurePerfStats::buildSummary() const {
  return QStringLiteral(
           "measureText=%1/%2ms max=%3ms(len=%4,style=%5) "
           "measureInlay=%6/%7ms max=%8ms(len=%9) "
           "measureIcon=%10/%11ms max=%12ms(id=%13)")
    .arg(text_count_)
    .arg(text_ms_total_, 0, 'f', 2)
    .arg(text_ms_max_, 0, 'f', 2)
    .arg(text_max_len_)
    .arg(text_max_style_)
    .arg(inlay_count_)
    .arg(inlay_ms_total_, 0, 'f', 2)
    .arg(inlay_ms_max_, 0, 'f', 2)
    .arg(inlay_max_len_)
    .arg(icon_count_)
    .arg(icon_ms_total_, 0, 'f', 2)
    .arg(icon_ms_max_, 0, 'f', 2)
    .arg(icon_max_id_);
}

PerfStepRecorder PerfStepRecorder::start() {
  return PerfStepRecorder();
}

PerfStepRecorder::PerfStepRecorder()
  : start_time_(Clock::now()),
    last_time_(start_time_) {}

void PerfStepRecorder::mark(const char* step_name) {
  const Clock::time_point now = Clock::now();
  if (step_count_ < MAX_STEPS) {
    step_names_[step_count_] = QString::fromLatin1(step_name);
    step_durations_[step_count_] = now - last_time_;
    ++step_count_;
  }
  last_time_ = now;
}

void PerfStepRecorder::finish() {
  if (end_time_ == Clock::time_point {}) {
    end_time_ = Clock::now();
  }
}

double PerfStepRecorder::totalMs() const {
  const Clock::time_point end = end_time_ == Clock::time_point {} ? Clock::now() : end_time_;
  return std::chrono::duration<double, std::milli>(end - start_time_).count();
}

double PerfStepRecorder::getStepMs(const char* step_name) const {
  const QString target = QString::fromLatin1(step_name);
  for (int i = 0; i < step_count_; ++i) {
    if (step_names_[i] == target) {
      return stepMsByIndex(i);
    }
  }
  return 0.0;
}

bool PerfStepRecorder::anyStepOver(double threshold_ms) const {
  for (int i = 0; i < step_count_; ++i) {
    if (stepMsByIndex(i) >= threshold_ms) {
      return true;
    }
  }
  return false;
}

int PerfStepRecorder::stepCount() const noexcept {
  return step_count_;
}

QString PerfStepRecorder::stepName(int index) const {
  return index >= 0 && index < step_count_ ? step_names_[index] : QString {};
}

double PerfStepRecorder::stepMsByIndex(int index) const {
  if (index < 0 || index >= step_count_) {
    return 0.0;
  }
  return std::chrono::duration<double, std::milli>(step_durations_[index]).count();
}

void PerfOverlay::setEnabled(bool enabled) {
  enabled_ = enabled;
}

bool PerfOverlay::isEnabled() const noexcept {
  return enabled_;
}

void PerfOverlay::recordBuild(const PerfStepRecorder& build_perf, const QString& measure_summary) {
  delete last_build_perf_;
  last_build_perf_ = new PerfStepRecorder(build_perf);
  last_build_ms_ = build_perf.totalMs();
  last_measure_summary_ = measure_summary;
  updateFrameStats();
}

void PerfOverlay::recordDraw(const PerfStepRecorder& draw_perf) {
  delete last_draw_perf_;
  last_draw_perf_ = new PerfStepRecorder(draw_perf);
  last_draw_ms_ = draw_perf.totalMs();
  updateFrameStats();
}

void PerfOverlay::recordInput(const QString& tag, double input_ms) {
  last_input_tag_ = tag;
  last_input_ms_ = input_ms;
}

void PerfOverlay::draw(QPainter& painter, int view_width) const {
  if (!enabled_ || view_width <= (kMargin * 2)) {
    return;
  }

  painter.save();
  painter.setFont(overlay_font_);
  const QFontMetrics metrics(overlay_font_);
  const int max_width = std::max(0, view_width - (kMargin * 2) - (kPaddingH * 2));
  if (max_width <= 0) {
    painter.restore();
    return;
  }

  QStringList lines;
  lines.push_back(QStringLiteral("FPS: %1").arg(current_fps_, 0, 'f', 0));

  const bool slow_frame = last_total_ms_ >= 16.6
    || last_build_ms_ >= WARN_BUILD_MS
    || last_draw_ms_ >= WARN_PAINT_MS;
  lines.push_back(
    QStringLiteral("Frame: %1ms (build=%2 draw=%3)%4")
      .arg(last_total_ms_, 0, 'f', 2)
      .arg(last_build_ms_, 0, 'f', 2)
      .arg(last_draw_ms_, 0, 'f', 2)
      .arg(slow_frame ? QStringLiteral(" SLOW") : QString {})
  );

  appendStepLines(lines, metrics, max_width, QStringLiteral("Build: "), last_build_perf_);
  appendStepLines(lines, metrics, max_width, QStringLiteral("Draw: "), last_draw_perf_);

  if (!last_measure_summary_.isEmpty()) {
    lines.append(wrapText(metrics, max_width, last_measure_summary_));
  }

  if (!last_input_tag_.isEmpty()) {
    lines.push_back(
      QStringLiteral("Input[%1]: %2ms%3")
        .arg(last_input_tag_)
        .arg(last_input_ms_, 0, 'f', 2)
        .arg(last_input_ms_ >= WARN_INPUT_MS ? QStringLiteral(" SLOW") : QString {})
    );
  }

  int content_width = 0;
  for (const QString& line : lines) {
    content_width = std::max(content_width, metrics.horizontalAdvance(line));
  }

  const int line_height = metrics.height() + kLineSpacing;
  const QRect panel_rect(
    kMargin,
    kMargin,
    std::min(content_width + (kPaddingH * 2), view_width - (kMargin * 2)),
    static_cast<int>(lines.size()) * line_height + (kPaddingV * 2)
  );
  painter.fillRect(panel_rect, background_color_);

  int y = panel_rect.top() + kPaddingV + metrics.ascent();
  for (const QString& line : lines) {
    painter.setPen(isWarnLine(line) ? warn_text_color_ : ok_text_color_);
    painter.drawText(panel_rect.left() + kPaddingH, y, line);
    y += line_height;
  }

  painter.restore();
}

void PerfOverlay::updateFrameStats() {
  last_total_ms_ = last_build_ms_ + last_draw_ms_;
  current_fps_ = last_total_ms_ > 0.0 ? 1000.0 / last_total_ms_ : 0.0;
}

} // namespace sweeteditor::qt
