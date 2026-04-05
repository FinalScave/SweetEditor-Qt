#include "DemoWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  DemoWindow window;
  window.show();

  return app.exec();
}
