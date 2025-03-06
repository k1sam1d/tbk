#include "Antonov.h"
#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    Antonov mainWindow;
    mainWindow.setWindowTitle("AntonovCNC");

    QWidget* centralWidget = new QWidget(&mainWindow);
    mainWindow.setCentralWidget(centralWidget);

    mainWindow.resize(964, 895);
    mainWindow.show();

    return app.exec();
}
