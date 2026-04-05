#pragma once

#include <sweeteditor/foundation.h>

#include <QStringList>
#include <QWidget>

#include <memory>

class QComboBox;
class QLabel;

namespace sweeteditor::qt {
class SweetEditorWidget;
}

class DemoWindow final : public QWidget {
  Q_OBJECT

public:
  explicit DemoWindow(QWidget* parent = nullptr);
  ~DemoWindow() override;

private:
  struct Private;

  void setupUi();
  void setupDemoProviders();
  void setupFileSelector();
  void syncTheme();
  void registerTextStyles();
  void updateStatus(const QString& message);
  void loadDemoFile(const QString& file_path);
  void loadDemoText(const QString& display_name, const QString& text, const QString& file_path = {});
  void cycleWrapMode();

  static QString normalizeNewlines(QString text);
  static QString resolveAssetsRoot();
  static QStringList listDemoFiles(const QString& assets_root);
  static QString defaultSampleCode();

  sweeteditor::qt::SweetEditorWidget* editor_ {nullptr};
  QComboBox* file_combo_ {nullptr};
  QLabel* status_label_ {nullptr};
  QString assets_root_;
  QStringList demo_files_;
  bool is_dark_theme_ {true};
  ::sweeteditor::WrapMode wrap_mode_preset_ {::sweeteditor::WrapMode::NONE};
  bool suppress_file_selection_ {false};
  std::unique_ptr<Private> d_;
};
