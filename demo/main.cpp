#include <QApplication>

#include <SweetEditorWidget.h>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  sweeteditor::qt::SweetEditorWidget window;
  window.applyTheme(sweeteditor::qt::EditorTheme::dark());
  window.resize(900, 600);
  window.setWindowTitle("SweetEditorQt Demo");
  window.show();

  return app.exec();
}
