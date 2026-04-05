#pragma once

#include <QString>

#include <QColor>
#include <QFont>

#include <array>
#include <chrono>

class QPainter;

namespace sweeteditor::qt {

class MeasurePerfStats {
public:
  void reset();

  void recordText(double elapsed_ms, int text_len, int font_style);
  void recordInlay(double elapsed_ms, int text_len);
  void recordIcon(double elapsed_ms, int icon_id);

  bool shouldLogBuild() const;
  QString buildSummary() const;

private:
  qint64 text_count_ {0};
  double text_ms_total_ {0.0};
  double text_ms_max_ {0.0};
  int text_max_len_ {0};
  int text_max_style_ {0};

  qint64 inlay_count_ {0};
  double inlay_ms_total_ {0.0};
  double inlay_ms_max_ {0.0};
  int inlay_max_len_ {0};

  qint64 icon_count_ {0};
  double icon_ms_total_ {0.0};
  double icon_ms_max_ {0.0};
  int icon_max_id_ {0};
};

class PerfStepRecorder {
public:
  static constexpr int MAX_STEPS = 32;

  static constexpr const char* STEP_PREP = "prep";
  static constexpr const char* STEP_BUILD = "build";
  static constexpr const char* STEP_METRICS = "metrics";
  static constexpr const char* STEP_CLEAR = "clear";
  static constexpr const char* STEP_CURRENT = "current";
  static constexpr const char* STEP_SELECTION = "selection";
  static constexpr const char* STEP_LINES = "lines";
  static constexpr const char* STEP_GUIDES = "guides";
  static constexpr const char* STEP_COMPOSITION = "comp";
  static constexpr const char* STEP_DIAGNOSTICS = "diag";
  static constexpr const char* STEP_LINKED = "linked";
  static constexpr const char* STEP_BRACKET = "bracket";
  static constexpr const char* STEP_CURSOR = "cursor";
  static constexpr const char* STEP_GUTTER = "gutter";
  static constexpr const char* STEP_LINE_NO = "lineNo";
  static constexpr const char* STEP_SCROLLBAR = "scrollbar";
  static constexpr const char* STEP_POPUP = "popup";

  static PerfStepRecorder start();

  void mark(const char* step_name);
  void finish();

  double totalMs() const;
  double getStepMs(const char* step_name) const;
  bool anyStepOver(double threshold_ms) const;

  int stepCount() const noexcept;
  QString stepName(int index) const;
  double stepMsByIndex(int index) const;

private:
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  PerfStepRecorder();

  std::array<QString, MAX_STEPS> step_names_;
  std::array<Duration, MAX_STEPS> step_durations_ {};
  Clock::time_point start_time_;
  Clock::time_point last_time_;
  Clock::time_point end_time_ {};
  int step_count_ {0};
};

class PerfOverlay {
public:
  static constexpr double WARN_BUILD_MS = 8.0;
  static constexpr double WARN_PAINT_MS = 8.0;
  static constexpr double WARN_INPUT_MS = 3.0;
  static constexpr double WARN_PAINT_STEP_MS = 2.0;
  static constexpr double WARN_MEASURE_SINGLE_MS = 1.0;

  PerfOverlay() = default;
  ~PerfOverlay();
  PerfOverlay(const PerfOverlay&) = delete;
  PerfOverlay& operator=(const PerfOverlay&) = delete;

  void setEnabled(bool enabled);
  bool isEnabled() const noexcept;

  void recordBuild(const PerfStepRecorder& build_perf, const QString& measure_summary);
  void recordDraw(const PerfStepRecorder& draw_perf);
  void recordInput(const QString& tag, double input_ms);

  void draw(QPainter& painter, int view_width) const;

private:
  void updateFrameStats();

  bool enabled_ {false};
  double current_fps_ {0.0};
  double last_build_ms_ {0.0};
  double last_draw_ms_ {0.0};
  double last_total_ms_ {0.0};
  QString last_measure_summary_;
  QString last_input_tag_;
  double last_input_ms_ {0.0};
  PerfStepRecorder* last_build_perf_ {nullptr};
  PerfStepRecorder* last_draw_perf_ {nullptr};
  QFont overlay_font_ {QStringLiteral("Consolas"), 8};
  QColor background_color_ {0, 0, 0, 180};
  QColor ok_text_color_ {0, 255, 0};
  QColor warn_text_color_ {255, 96, 96};
};

} // namespace sweeteditor::qt
